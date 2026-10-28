#!/usr/bin/env python3
"""Finite owned attachment-sRGB / image-format-BLIT control and CPU auditor.

This NEW control never changes the frozen shaders or older certificates. The
DataFormat transfer formula and Vulkan's floor-or-ceil UNORM conversion are
distinct from POLICY: a predeclared finite-case acceptance policy, not a Vulkan
precision guarantee. No game UV decode, resource identity or coverage follows.
"""
from __future__ import annotations
import argparse
from functools import lru_cache
import hashlib
import math
import os
from pathlib import Path
import struct
import subprocess

import sampled_writer_validate as base

WIDTH, HEIGHT = 2560, 1440
SENTINEL = bytes((7, 5, 3, 255))  # raw COPY, never a floating-point clear
NAMES = ('A_clamp_zero', 'B_repeat_offset',
         'B_clamp_offset_center_scissor_LOAD', 'B_clamp_offset_center_viewport_LOAD',
         'negative_height', 'B_rectangle_input')
MODES = ('unorm', 'mutable-srgb', 'srgb')
GROUPS = tuple(m+'-'+s for m in MODES for s in ('original', 'diagnostic'))
PIPELINE = dict(blend_enable=False, logic_op_enable=False, color_write_mask=15,
                sample_count=1, load_op='LOAD', store_op='STORE', rasterizer_discard=False,
                cull_mode=0, fragment_shading_rate=False, topology=3, vertex_count=3,
                instance_count=1, first_vertex=0, first_instance=0)
POLICY = dict(version=1, scope='closed_owned_finite_cases_only',
              linear_shader_code_budget=0.005, transfer_operation_relative_budget=1e-5,
              quantization='floor_or_ceil_after_clamp', outward_cpu_rounding=True,
              normative_precision_upper_bound_claimed=False,
              universal_interpolation_bound_claimed=False,
              adaptive_widening_allowed=False)
REFERENCES = dict(
    transfer='https://registry.khronos.org/DataFormat/specs/1.4/dataformat.1.4.html',
    attachment='https://docs.vulkan.org/spec/latest/chapters/framebuffer.html#framebuffer-blending',
    blit='https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBlitImage.html',
    conversion='https://docs.vulkan.org/spec/latest/chapters/fundamentals.html#fundamentals-fixedconv')
SOURCES = ('native/tests/sampled_srgb_gpu.cpp', 'native/tests/sampled_writer_gpu.cpp',
           'native/tests/sampled_writer_hash.hpp', 'tools/sampled_srgb_validate.py',
           'tests/test_sampled_srgb_validate.py', 'tools/sampled_writer_validate.py',
           'tools/sampled_uv_probe.py', 'tests/test_sampled_uv_probe.py',
           'tools/shader_write_contract.py', 'tools/spirv_probe.py', 'tools/glsl_effects.py',
           'tools/shader_effects.py', 'tools/shader_effects_validate.py', 'tools/frame_trace.py',
           'tools/probe.py', 'tools/shader_inventory.py', 'tools/pe_image.py')


def equal(actual, expected, label):
    """Metadata types are exact, including integer versus boolean."""
    if isinstance(expected, dict):
        if not isinstance(actual, dict) or set(actual) != set(expected):
            raise ValueError('Metadata keys: '+label)
        for key in expected: equal(actual[key], expected[key], label+'.'+key)
    elif isinstance(expected, (tuple, list)):
        if not isinstance(actual, list) or len(actual) != len(expected):
            raise ValueError('Metadata array: '+label)
        for i, value in enumerate(expected): equal(actual[i], value, label+'.'+str(i))
    elif type(expected) is int:
        if type(actual) is not int or actual != expected: raise ValueError('Metadata integer: '+label)
    elif type(expected) is float and type(actual) in (int, float):
        if not math.isfinite(actual) or actual != expected: raise ValueError('Metadata value: '+label)
    elif type(actual) is not type(expected) or actual != expected:
        raise ValueError('Metadata value/type: '+label)


def finite(value):
    if type(value) not in (int, float) or not math.isfinite(value):
        raise ValueError('Only finite owned operands are certified')
    return value


def srgb_encode(value):
    value = min(1., max(0., finite(value)))
    return 12.92*value if value <= .0031308 else 1.055*value**(1/2.4)-.055


def srgb_decode(value):
    value = min(1., max(0., finite(value)))
    return value/12.92 if value <= .04045 else ((value+.055)/1.055)**2.4


def outward(lo, hi):
    return math.nextafter(lo, -math.inf), math.nextafter(hi, math.inf)


def operation(lo, hi):
    # Chosen BEFORE physical execution. The spec's "about 1 part in 10^5"
    # outside-shader precision language is motivation, NOT a normative bound.
    e = POLICY['transfer_operation_relative_budget']*max(abs(lo), abs(hi))
    return outward(lo-e, hi+e)


@lru_cache(maxsize=32768)
def transfer_interval(interval, encode=True):
    lo, hi = interval
    finite(lo); finite(hi)
    if not 0 <= lo <= hi <= 1: raise ValueError('Transfer interval outside [0,1]')
    if lo == hi and lo in (0., 1.): return lo, hi
    # Constants and branches are the DataFormat formula. Intermediates receive
    # a relative operation budget; pow and its exponent are explicit operations.
    values = []
    if encode:
        if lo <= .0031308: values.append(operation(12.92*lo, 12.92*min(hi, .0031308)))
        if hi > .0031308:
            a, b = max(lo, .0031308), hi
            exlo, exhi = operation(1/2.4, 1/2.4)
            a, b = operation(a**exhi, b**exlo)
            a, b = operation(1.055*a, 1.055*b)
            values.append(operation(a-.055, b-.055))
    else:
        if lo <= .04045: values.append(operation(lo/12.92, min(hi, .04045)/12.92))
        if hi > .04045:
            a, b = operation(max(lo, .04045)+.055, hi+.055)
            a, b = operation(a/1.055, b/1.055)
            # The exponent 2.4 is part of the formula, rather than a runtime
            # division. Its representation is still admitted by this policy.
            exlo, exhi = operation(2.4, 2.4)
            values.append(operation(max(0., a)**exhi, max(0., b)**exlo))
    return max(0., min(v[0] for v in values)), min(1., max(v[1] for v in values))


def quantize_interval(interval):
    lo, hi = interval; finite(lo); finite(hi)
    if lo > hi: raise ValueError('Reversed interval')
    # Normative floor OR ceil; nearest is recommended, not required by Vulkan.
    return max(0, math.floor(min(1., max(0., lo))*255)), min(255, math.ceil(min(1., max(0., hi))*255))


def source_bounds(interval, view_srgb, alpha=False):
    return quantize_interval(transfer_interval(tuple(interval)) if view_srgb and not alpha else interval)


@lru_cache(maxsize=1024)
def blit_bounds(source_code, image_format, alpha=False):
    if type(source_code) is not int or not 0 <= source_code <= 255 or image_format not in (44, 50):
        raise ValueError('Unknown finite BLIT operand')
    if image_format == 44 or alpha: return source_code, source_code
    return quantize_interval(transfer_interval((source_code/255, source_code/255), False))


def destination_bounds(bounds, image_format, alpha=False):
    values = [blit_bounds(c, image_format, alpha) for c in range(bounds[0], bounds[1]+1)]
    return min(v[0] for v in values), max(v[1] for v in values)


def state(name):
    if name not in NAMES: raise ValueError('Unknown fixed control')
    image, sampler, shift = (0, 0, (0, 0)) if name == NAMES[0] else (1, 0, (1, -1))
    if name == 'B_repeat_offset': sampler = 1
    if name == 'B_rectangle_input': shift = (0, 0)
    vp = [0, HEIGHT, WIDTH, -HEIGHT] if name == 'negative_height' else [0, 0, WIDTH, HEIGHT]
    sc = [0, 0, WIDTH, HEIGHT]
    center = [WIDTH//4, HEIGHT//4, WIDTH//2, HEIGHT//2]
    if 'center_scissor' in name: sc = center
    if 'center_viewport' in name: vp = center
    pc = [0]*7; pc[2], pc[6] = image, sampler
    ubo = bytearray(1280); struct.pack_into('<2f', ubo, 0, shift[0]/8, shift[1]/8)
    return dict(texture_index=image, sampler_index=sampler, shift_texels=list(shift),
                texture_variant='rectangle' if name == 'B_rectangle_input' else 'original',
                ubo_row0_xy=[shift[0]/8, shift[1]/8], ubo_row0_zw=[0, 0],
                scissor=sc, viewport=vp, viewport_depth=[0, 1], push_constants=pc), struct.pack('<7I', *pc), bytes(ubo)


@lru_cache(maxsize=4)
def texture_bytes(index, rectangle=False):
    result = bytearray()
    for y in range(8):
        for x in range(8):
            if index == 0: rgba = (17+13*x, 29+19*y, 41+7*x+11*y, 255)
            else: rgba = (233-11*x, 211-17*y,
                          197-5*x-9*y if not rectangle or (2 <= x < 6 and 2 <= y < 6) else 31,
                          83+7*x+11*y if rectangle else 255)
            result.extend(rgba)
    return bytes(result)


def occupied(x, y, viewport, scissor):
    vx, vy, vw, vh = viewport; sx, sy, sw, sh = scissor
    return vx <= x < vx+vw and min(vy, vy+vh) <= y < max(vy, vy+vh) and sx <= x < sx+sw and sy <= y < sy+sh


def uv_at(x, y, viewport, shift):
    vx, vy, vw, vh = viewport
    return (x+.5-vx)/vw+shift[0]/8, 1-(y+.5-vy)/vh+shift[1]/8


def encoded_interval(value):
    finite(value)
    if value < -1: return 0., 0.
    if value > 2: return 1., 1.
    code = 1+253*((value+1)/3)
    error = POLICY['linear_shader_code_budget']
    return max(1., code-error)/255, min(254., code+error)/255


def texture_pixel(x, y, s):
    vx, vy, vw, vh = s['viewport']; shift = s['shift_texels']
    tx = ((2*(x-vx)+1)*8)//(2*vw)+shift[0]
    ty = ((2*vh-2*(y-vy)-1)*8)//(2*vh)+shift[1]
    if s['sampler_index']: tx, ty = tx % 8, ty % 8
    else: tx, ty = min(7, max(0, tx)), min(7, max(0, ty))
    texture = texture_bytes(s['texture_index'], s['texture_variant'] == 'rectangle')
    r, g, b, a = texture[(ty*8+tx)*4:(ty*8+tx+1)*4]
    return b, g, r, a


def pixel_bounds(x, y, s, mode, diagnostic=False, transfer_count=None, swapped=False):
    if not occupied(x, y, s['viewport'], s['scissor']): return tuple((c, c) for c in SENTINEL)
    pixel = texture_pixel(x, y, s); count = (mode != 'unorm') if transfer_count is None else transfer_count
    result = []
    u, v = uv_at(x, y, s['viewport'], s['shift_texels'])
    for channel, c in enumerate(pixel):
        if diagnostic and channel in (1, 2):
            value = (u if channel == 2 else v) if not swapped else (v if channel == 2 else u)
            interval = encoded_interval(value)
        else: interval = (c/255, c/255)
        if channel == 3 or (not count and (not diagnostic or channel == 0)):
            result.append((c, c)); continue
        for _ in range(int(count)):
            interval = transfer_interval(tuple(interval))
        result.append(quantize_interval(interval))
    return tuple(result)


def reference_rows(name, mode, diagnostic=False, destination=False):
    """Independent absolute CPU intervals. No observed GPU pixel is a reference."""
    s, _, _ = state(name); image_format = 50 if mode == 'srgb' else 44
    cache = {}
    for y in range(HEIGHT):
        if not occupied(WIDTH//2, y, s['viewport'], s['scissor']):
            bounds = tuple((c,c) for c in SENTINEL)
            if destination: bounds = tuple(destination_bounds(b,image_format,c==3) for c,b in enumerate(bounds))
            yield bytes(b[0] for b in bounds)*WIDTH, bytes(b[1] for b in bounds)*WIDTH
            continue
        # These controls have only eight sampled texture rows and a bounded
        # quantized G palette. This cache avoids rebuilding identical rows.
        keys = tuple(pixel_bounds(x, y, s, mode, diagnostic) for x in (0, WIDTH//2, WIDTH-1))
        key = (keys, occupied(WIDTH//2, y, s['viewport'], s['scissor']))
        # Include the exact sampled row and UV G interval; edge witnesses alone
        # are not sufficient to identify texture row transitions.
        key += (texture_pixel(0, y, s), source_bounds(encoded_interval(uv_at(0, y, s['viewport'], s['shift_texels'])[1]), mode != 'unorm'))
        if key not in cache:
            lo, hi = bytearray(), bytearray()
            for x in range(WIDTH):
                bounds = pixel_bounds(x, y, s, mode, diagnostic)
                if destination: bounds = tuple(destination_bounds(b, image_format, c == 3) for c, b in enumerate(bounds))
                lo.extend(b[0] for b in bounds); hi.extend(b[1] for b in bounds)
            cache[key] = bytes(lo), bytes(hi)
        yield cache[key]


def mismatch_count(actual, lower, upper):
    if len(actual) != len(lower) or len(actual) != len(upper): raise ValueError('Pixel row length')
    if lower == upper: return sum(a != b for a, b in zip(actual, lower)) if actual != lower else 0
    return sum(not lo <= value <= hi for value, lo, hi in zip(actual, lower, upper))


def audit_pixels(original_source, diagnostic_source, original_blit, diagnostic_blit, name, mode):
    if mode not in MODES: raise ValueError('Unknown mode')
    size = WIDTH*HEIGHT*4
    images = tuple(bytes(v) for v in (original_source, diagnostic_source, original_blit, diagnostic_blit))
    original_source, diagnostic_source, original_blit, diagnostic_blit = images
    for image in images: equal(len(image), size, 'pixel length')
    errors = dict(absolute_original_source=0, absolute_diagnostic_source=0,
                  absolute_original_blit=0, absolute_diagnostic_blit=0,
                  source_ba=0, destination_ba=0, blit_relation=0)
    row_cache = {}
    for label, image, diagnostic, destination in (
        ('absolute_original_source', original_source, False, False),
        ('absolute_diagnostic_source', diagnostic_source, True, False),
        ('absolute_original_blit', original_blit, False, True),
        ('absolute_diagnostic_blit', diagnostic_blit, True, True)):
        for y, (lo, hi) in enumerate(reference_rows(name, mode, diagnostic, destination)):
            row = image[y*WIDTH*4:(y+1)*WIDTH*4]; key = (row, lo, hi)
            if key not in row_cache: row_cache[key] = mismatch_count(row, lo, hi)
            errors[label] += row_cache[key]
    for source, dest in ((original_source, original_blit), (diagnostic_source, diagnostic_blit)):
        if mode != 'srgb':
            errors['blit_relation'] += sum(a != b for a, b in zip(source, dest)) if source != dest else 0
        else:
            # Independent source-code -> linear destination relation, in
            # addition to the independent absolute CPU intervals above.
            for channel in range(4):
                palette = [blit_bounds(c, 50, channel == 3) for c in range(256)]
                for a, b in zip(source[channel::4], dest[channel::4]):
                    errors['blit_relation'] += not palette[a][0] <= b <= palette[a][1]
    for channel in (0, 3):
        errors['source_ba'] += sum(a != b for a, b in zip(original_source[channel::4], diagnostic_source[channel::4]))
        errors['destination_ba'] += sum(a != b for a, b in zip(original_blit[channel::4], diagnostic_blit[channel::4]))
    if any(errors.values()): raise ValueError('Finite SRGB pixel control failed '+name+'/'+mode+': '+str(errors))
    s, _, _ = state(name)
    witnesses = dict(absent_transfer=0, double_transfer=0, component_swap=0, unexpected_transfer=0,
                     alpha_transfer=0, blit_wrong_source_format=0)
    for y in range(HEIGHT//16, HEIGHT, max(1, HEIGHT//8)):
        for x in range(WIDTH//16, WIDTH, max(1, WIDTH//8)):
            if not occupied(x, y, s['viewport'], s['scissor']): continue
            i = (y*WIDTH+x)*4; p = diagnostic_source[i:i+4]
            for key, count, swap in (('absent_transfer', 0, False), ('double_transfer', 2, False),
                                     ('component_swap', mode != 'unorm', True), ('unexpected_transfer', 1, False)):
                bounds = pixel_bounds(x, y, s, mode, True, count, swap)
                witnesses[key] += any(not bounds[c][0] <= p[c] <= bounds[c][1] for c in (1, 2))
            alpha = texture_pixel(x, y, s)[3]
            wrong_alpha = source_bounds((alpha/255, alpha/255), True)
            witnesses['alpha_transfer'] += not wrong_alpha[0] <= p[3] <= wrong_alpha[1]
            wrong_format = 44 if mode == 'srgb' else 50
            witnesses['blit_wrong_source_format'] += any(not blit_bounds(p[c], wrong_format)[0] <= diagnostic_blit[i+c] <= blit_bounds(p[c], wrong_format)[1] for c in (0, 1, 2))
    required = ['component_swap', 'blit_wrong_source_format']
    required += ['unexpected_transfer'] if mode == 'unorm' else ['absent_transfer', 'double_transfer']
    if name == 'B_rectangle_input': required += ['alpha_transfer']
    if any(witnesses[k] == 0 for k in required): raise ValueError('Non-discriminating finite control: '+str(witnesses))
    vx,vy,vw,vh=s['viewport'];sx,sy,sw,sh=s['scissor']
    occupied_pixels = max(0,min(WIDTH,vx+vw,sx+sw)-max(0,vx,sx))*max(0,min(HEIGHT,max(vy,vy+vh),sy+sh)-max(0,min(vy,vy+vh),sy))
    return dict(name=name, mode=mode, **{k+'_sha256': base.digest(v) for k, v in zip(
        ('original_source', 'diagnostic_source', 'original_blit', 'diagnostic_blit'), images)},
        errors=errors, fixed_witness_rejections=witnesses, written_pixels=occupied_pixels,
        unchanged_source_load_pixels=WIDTH*HEIGHT-occupied_pixels,
        preserved_source_ba_bytes=size//2, preserved_destination_ba_bytes=size//2)


def regular(folder, name, size=None):
    if not isinstance(name, str) or not name or Path(name).name != name: raise ValueError('Local file name required')
    folder = Path(folder)
    if any(p.is_symlink() for p in (folder, *folder.parents)): raise ValueError('Symlink directory refused')
    if size is None and (folder/name).stat().st_size > 16*1024*1024: raise ValueError('Metadata/log byte bound')
    return base.read(folder/name, size)


def integer(value, minimum=0):
    if type(value) is not int or value < minimum: raise ValueError('Nonnegative exact integer required')
    return value


def audit_queries(q, mode):
    equal(q['completed_before_resource_creation'], True, 'query order')
    equal(q['image_format_list_core_version'], 4202496, 'format list core')
    equal([x['format'] for x in q['format_features']], [37, 44, 50], 'format query IDs')
    # SAMPLED_IMAGE=1, COLOR_ATTACHMENT=0x80, BLIT_SRC/DST=0x400/800,
    # TRANSFER_SRC/DST=0x4000/8000. No assumption from linear-tiling flags.
    for x, needed in zip(q['format_features'], (0x8001, 0xcc80, 0xc480)):
        for key in ('linear', 'optimal', 'buffer'): integer(x[key])
        if x['optimal'] & needed != needed: raise ValueError('Required format capability absent')
    expected = [(44,19,0,[]),(44,19,8,[44,50]),(50,19,0,[]),(44,3,0,[]),(37,6,0,[])]
    equal(len(q['images']), 5, 'five exact image queries')
    for x, (fmt, usage, flags, formats) in zip(q['images'], expected):
        for key, value in dict(format=fmt, type=1, tiling=0, usage=usage, flags=flags, format_list=formats, result=0).items(): equal(x[key], value, 'image query '+key)
        required = [8, 8, 1] if fmt == 37 else [WIDTH, HEIGHT, 1]
        equal(x['extent'],required,'queried extent')
        for key in ('max_extent',):
            if len(x[key]) != 3 or any(type(v) is not int for v in x[key]): raise ValueError('Query extent')
        if any(v < n for v, n in zip(x['max_extent'], required)): raise ValueError('Image extent unsupported')
        for key in ('max_mip_levels', 'max_array_layers'): integer(x[key], 1)
        if not integer(x['sample_counts']) & 1 or integer(x['max_resource_size']) < (256 if fmt == 37 else WIDTH*HEIGHT*4): raise ValueError('Image limits')


def audit_manifest(folder, group, modules):
    raw = regular(folder, 'manifest.json'); m = base.decode(raw)
    diagnostic = group.endswith('-diagnostic'); mode = group.rsplit('-', 1)[0]
    source = 50 if mode == 'srgb' else 44; view = 44 if mode == 'unorm' else 50; mut = mode == 'mutable-srgb'
    exact = dict(schema_version=1, owned_fixture=True, game_rendered=False, diagnostic_uv=diagnostic,
        descriptor_buffer=False, state_snapshot_before_teardown=True, width=WIDTH, height=HEIGHT,
        source_image_format=source, attachment_view_format=view, pipeline_color_format=view,
        destination_image_format=44, mutable_format=mut, source_create_flags=8 if mut else 0,
        source_usage=19, destination_usage=3, view_formats=[44, 50] if mut else [],
        modules=[modules[0], modules[2 if diagnostic else 1]], pipeline=PIPELINE,
        render_area=[0, 0, WIDTH, HEIGHT], dynamic_state=dict(viewport=True, scissor=True),
        features=dict(**{key: True for key in base.FEATURES}, descriptorBuffer=False, synchronization2=False),
        descriptor_state=base.DESCRIPTORS, sampler_state=base.SAMPLERS,
        texture_state=dict(width=8, height=8, format=37, mip_levels=1,
                           files=['texture-A.rgba', 'texture-B.rgba', 'texture-B-rectangle.rgba']),
        initialization=dict(method='raw_buffer_copy', source_bgra8=list(SENTINEL), bytes=WIDTH*HEIGHT*4,
                            sha256=base.digest(SENTINEL*(WIDTH*HEIGHT))),
        counts=dict(submissions=8, draws=6, image_uploads=3, target_initializations=6,
                    source_readbacks=6, blits=6, destination_readbacks=6))
    for key, value in exact.items(): equal(m[key], value, group+'.'+key)
    integer(m['device']['device_id']); integer(m['device']['driver_version'])
    equal(m['device']['vendor_id'], 0x10de, 'device vendor')
    if integer(m['device']['api_version']) < 4206592: raise ValueError('Vulkan1.3 required')
    audit_queries(m['queries'], mode)
    validation = m['validation']
    for key in ('debug_messenger', 'synchronization_validation', 'counters_include_teardown'): equal(validation[key], True, key)
    equal(validation['errors'], 0, 'validation errors'); integer(validation['warnings']); equal(validation['log_file'], 'validation.log', 'log file')
    log = regular(folder, 'validation.log').decode('utf8')
    if any(marker in log for marker in base.ERROR_MARKERS): raise ValueError('Validation error marker')
    for file, content in (('texture-A.rgba', texture_bytes(0)), ('texture-B.rgba', texture_bytes(1)), ('texture-B-rectangle.rgba', texture_bytes(1, True))):
        equal(regular(folder, file, 256), content, 'actual texture upload')
    equal([c['name'] for c in m['cases']], list(NAMES), 'fixed cases')
    for c in m['cases']:
        name = c['name']; s, pc, ubo = state(name)
        for key, value in s.items(): equal(c[key], value, 'written state '+key)
        for key, value in (('pc', pc), ('ubo', ubo)):
            equal(c[key+'_file'], name+'.'+key+'.bin', key+' path')
            equal(regular(folder, c[key+'_file'], len(value)), value, key+' bytes')
            equal(c[key+'_sha256'], base.digest(value), key+' sha')
        equal(c['bytes'], WIDTH*HEIGHT*4, 'raw bytes')
        for label in ('source', 'blit'): equal(c[label+'_raw_file'], name+'.'+label+'.bgra', 'raw path')
        equal(c['initialization'], 'raw_buffer_copy', 'initialization')
        equal(c['source_copy_before_blit'], True, 'source read order'); equal(c['serial_fence_completed'], True, 'fence')
        equal(c['blit'], dict(source_layout=6, destination_layout=7, filter=0, region_count=1,
              source_subresource=[1,0,0,1], destination_subresource=[1,0,0,1],
              source_offsets=[[0,0,0],[WIDTH,HEIGHT,1]], destination_offsets=[[0,0,0],[WIDTH,HEIGHT,1]]), 'exact original BLIT')
    return m, base.digest(raw)


def completion():
    return dict(schema_version=1, sampled_srgb_gpu_passed=True, owned_fixture=True, game_rendered=False,
                groups=list(GROUPS), draws=36, blits=36, readbacks=72, validation_errors=0)


def audit_directory(directory, modules):
    directory = Path(directory); equal(base.decode(regular(directory, 'manifest.json')), completion(), 'root completion')
    manifests = {}; results = []; warnings = []
    for mode in MODES:
        pair = []
        for suffix in ('original', 'diagnostic'):
            group = mode+'-'+suffix; folder = directory/group
            m, sha = audit_manifest(folder, group, modules); manifests[group] = sha
            pair.append(m); warnings.append(m['validation']['warnings'])
        for index, name in enumerate(NAMES):
            images = []
            for suffix, m in zip(('original', 'diagnostic'), pair):
                c = m['cases'][index]
                for label in ('source', 'blit'):
                    raw = regular(directory/(mode+'-'+suffix), c[label+'_raw_file'], WIDTH*HEIGHT*4)
                    equal(c[label+'_sha256'], base.digest(raw), 'raw SHA'); images.append(raw)
            # Images were loaded source,blit per suffix; oracle expects the two
            # source observations before the two destination observations.
            results.append(audit_pixels(images[0], images[2], images[1], images[3], name, mode))
    return dict(owned_srgb_control_passed=True, cases=results, policy=POLICY, references=REFERENCES,
                manifest_sha256=manifests, readbacks=72, total_readback_bytes=72*WIDTH*HEIGHT*4,
                validation_errors=0, validation_warnings=warnings, game_rendered=False,
                game_numeric_uv_admitted=False, game_pixel_cause_proven=False,
                nonfinite_gpu_behavior_certified=False, effective_game_coverage_known=False,
                game_sampled_resource_known=False, xr_submission=False)


def paired_hashes(audit):
    return [[c[k+'_sha256'] for k in ('original_source','diagnostic_source','original_blit','diagnostic_blit')] for c in audit['cases']]


def launch_command(args, modules, environment):
    command = ['/usr/bin/env']
    clear = {'VK_LAYER_PATH', 'VK_INSTANCE_LAYERS', 'VK_ADD_LAYER_PATH', 'LD_PRELOAD'} | {k for k in os.environ if k.startswith('D4VR_')}
    for key in sorted(clear): command += ['-u', key]
    command += ['VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation', 'VK_ADD_LAYER_PATH='+str(args.validation_layer.resolve()),
        'VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT',
        'VK_LAYER_VALIDATE_SYNC=1', 'VK_LOADER_DEBUG=layer']
    if environment == 'steam-runtime': command += [str(args.runtime.resolve()), '--batch', '--']
    command += [str(args.fixture.resolve()), '--vertex', modules[0]['path'], '--fragment', modules[1]['path'],
                '--candidate', modules[2]['path'], '--output', str((args.output/environment).resolve())]
    return base.host_command()+command


def run(args):
    from sampled_uv_probe import verify_candidate
    paths = [args.vertex, args.fragment, args.candidate]
    expected = [args.vertex_sha256, args.fragment_sha256, args.candidate_sha256]
    if any(len(sha) != 64 or any(c not in '0123456789abcdef' for c in sha) for sha in expected): raise ValueError('Explicit lowercase SHA256 pins required')
    if args.output.exists() or args.output.is_symlink(): raise ValueError('Require fresh output')
    if any(p.is_symlink() for p in args.output.parents): raise ValueError('Symlink output parent refused')
    root = Path(__file__).resolve().parents[1]
    frozen = {s: base.digest(base.read(root/s)) for s in SOURCES}
    fixture_hash = base.digest(base.read(args.fixture)); validator_hash = base.digest(base.read(args.validator.resolve()))
    def stable():
        equal({s: base.digest(base.read(root/s)) for s in SOURCES}, frozen, 'frozen sources')
        equal(base.digest(base.read(args.fixture)), fixture_hash, 'frozen fixture')
        equal(base.digest(base.read(args.validator.resolve())), validator_hash, 'frozen validator')
        for path, sha in zip(paths, expected): equal(base.digest(base.read(path)), sha, 'frozen module')
    modules = [base.verify_module(path, sha, stage) for path, sha, stage in zip(paths, expected, (1,16,16))]
    certificate = verify_candidate(base.read(args.fragment), base.read(args.candidate), args.fragment_sha256)
    stable()
    args.output.mkdir(parents=True, mode=0o700)
    base.write_json(args.output/'private-inputs.json', dict(modules=modules, constructor=certificate,
        source_sha256=frozen, fixture_sha256=fixture_hash, validator_sha256=validator_hash, policy=POLICY, references=REFERENCES,
        physical_and_final_source_hashes_identical_required=True))
    for index, path in enumerate(paths):
        stable(); command = base.host_command()+[str(args.validator.resolve()), '--target-env', 'vulkan1.3', str(path.resolve())]
        result = subprocess.run(command, capture_output=True, text=True, timeout=30)
        (args.output/('module-'+str(index)+'-validation.log')).write_text(result.stdout+result.stderr)
        if result.returncode: raise ValueError('SPIR-V preparation refused BEFORE GPU')
        stable()
    audits = []
    for environment in ('native', 'steam-runtime'):
        stable(); command = launch_command(args, modules, environment)
        base.write_json(args.output/(environment+'-command.json'), command)
        result = subprocess.run(command, capture_output=True, text=True, timeout=1200)
        log = result.stdout+result.stderr; (args.output/(environment+'.log')).write_text(log)
        if result.returncode or any(x in log for x in base.ERROR_MARKERS): raise ValueError('Physical control failed; preserve '+environment+'.log')
        equal(base.decode(result.stdout.strip()), completion(), 'physical completion')
        stable(); audit = audit_directory(args.output/environment, modules)
        base.write_json(args.output/(environment+'-audit.json'), audit); audits.append(dict(environment=environment, **audit))
    stable(); equal(paired_hashes(audits[0]), paired_hashes(audits[1]), 'native/runtime exact bytes')
    proof = dict(schema_version=1, owned_srgb_control_passed=True, source_sha256=frozen,
        physical_source_sha256=frozen, final_source_sha256=frozen, fixture_sha256=fixture_hash,
        validator_sha256=validator_hash, policy=POLICY, references=REFERENCES, environments=audits,
        physical_readbacks=144, total_readback_bytes=144*WIDTH*HEIGHT*4, validation_errors=0,
        native_runtime_bytes_equal=True, game_bundle_authorized=False, game_numeric_uv_admitted=False,
        game_rendered=False, coverage_repaired=False, xr_submission=False,
        nonfinite_gpu_behavior_certified=False, universal_interpolation_bound_claimed=False)
    base.write_json(args.output/'proof.json', proof); return proof


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('fixture','vertex','fragment','candidate','validation-layer','runtime','output','validator'):
        p.add_argument('--'+key, type=Path, required=True)
    for key in ('vertex','fragment','candidate'): p.add_argument('--'+key+'-sha256', required=True)
    result = run(p.parse_args())
    import json
    print(json.dumps({k:v for k,v in result.items() if k not in ('source_sha256','physical_source_sha256','final_source_sha256','environments')}, indent=2))


if __name__ == '__main__': main()
