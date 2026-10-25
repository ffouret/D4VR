#!/usr/bin/env python3
"""Validate private stereo MRT targets with GPU references and a sync negative control."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import subprocess
import probe
import frame_trace
import vulkan_trace


def run(args) -> dict:
    inputs = [args.fixture,args.original,args.instrumented,args.fragment]
    if args.compute:
        inputs.append(args.compute)
    if args.descriptor_buffers and (not args.recorded_replay or not args.compute):
        raise ValueError('Descriptor buffers require recorded replay and the compute echo shader')
    if args.dynamic_state and not args.recorded_replay:
        raise ValueError('Dynamic state requires recorded replay')
    if args.pipeline_recipes and not args.recorded_replay:
        raise ValueError('Pipeline recipes require recorded replay')
    if not all(p.is_file() for p in inputs) or args.output.exists():
        raise ValueError('Provide the built fixture/shaders and a new private output directory')
    manifest = json.loads((args.validation_layer/'VkLayer_khronos_validation.json').read_text())
    if manifest['layer']['name'] != 'VK_LAYER_KHRONOS_validation':
        raise ValueError('Expected the Khronos validation layer')
    if args.trace_layer:
        trace_manifest = json.loads((args.trace_layer/vulkan_trace.MANIFEST).read_text())
        if trace_manifest['layer']['name'] != vulkan_trace.LAYER or not (args.trace_layer/vulkan_trace.LIBRARY).is_file():
            raise ValueError('Expected the built D4VR trace layer and manifest')
    args.output.mkdir(parents=True,mode=0o700)
    runtime = args.runtime.resolve() if args.runtime else None
    cases = [('native',False,False,False,False),('native-validation',True,False,False,False),
             ('sync-negative-control',True,False,True,False)]
    if args.descriptor_buffers:
        cases.append(('descriptor-offset-negative-control',True,False,False,True))
    if args.trace_layer:
        cases.append(('metadata-disabled',True,False,False,False))
    if runtime:
        cases.append(('steam-runtime-validation',True,True,False,False))
    results = []
    for name,validation,in_runtime,negative,negative_compute in cases:
        bridge = Path('/run/host/usr/bin/flatpak-spawn')
        command = [str(bridge),'--host','/usr/bin/env'] if bridge.exists() else ['/usr/bin/env']
        for key in ('VK_INSTANCE_LAYERS','VK_ADD_LAYER_PATH','VK_LAYER_PATH','VK_LAYER_ENABLES','VK_LAYER_VALIDATE_SYNC',
                    'VK_LOADER_DEBUG','D4VR_EYE_STATE_FILE','D4VR_TRACE_DIR','D4VR_FRAME_TRACE','D4VR_TRACE_XR_BINDING'):
            command += ['-u',key]
        layers = []; paths = []
        if validation:
            layers.append('VK_LAYER_KHRONOS_validation');paths.append(str(args.validation_layer.resolve()))
            command += ['VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT','VK_LAYER_VALIDATE_SYNC=1','VK_LOADER_DEBUG=layer']
        metadata_directory = args.output/(name+'-frames')
        if args.trace_layer:
            layers.append(vulkan_trace.LAYER);paths.append(str(args.trace_layer.resolve()))
            metadata_directory.mkdir(mode=0o700);request = metadata_directory/'frame.request';request.write_text('1\n');os.chmod(request,0o600)
            command.append('D4VR_TRACE_DIR='+str(metadata_directory.resolve()))
            if name != 'metadata-disabled':command.append('D4VR_FRAME_TRACE=1')
        if layers:
            command += ['VK_INSTANCE_LAYERS='+':'.join(layers),'VK_ADD_LAYER_PATH=' + ':'.join(paths)]
        if in_runtime:
            command += [str(runtime),'--batch','--']
        command += [str(p.resolve()) for p in inputs[:4]]
        if args.compute:
            command += ['--compute',str(args.compute.resolve())]
        if args.split_passes:
            command.append('--split-passes')
        if args.recorded_replay:
            command.append('--recorded-replay')
        if args.descriptor_buffers:
            command.append('--descriptor-buffers')
        if args.dynamic_state:
            command.append('--dynamic-state')
        if args.pipeline_recipes:
            command.append('--pipeline-recipes')
        if negative:
            command.append('--negative-sync-control')
        if negative_compute:
            command.append('--negative-compute-control')
        log = args.output/(name+'.log')
        with log.open('xb') as stream:
            os.chmod(log,0o600)
            result = subprocess.run(command,stdout=stream,stderr=subprocess.STDOUT,timeout=60)
        text = log.read_text(errors='replace')
        payloads = [json.loads(line) for line in text.splitlines() if line.startswith('{')]
        if result.returncode or not payloads:
            raise ValueError(f'{name}: GPU fixture failed; inspect its private log')
        proof = payloads[-1]
        loaded = 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in text and 'Inserted device layer "VK_LAYER_KHRONOS_validation"' in text
        hazard = 'SYNC-HAZARD-WRITE-AFTER-WRITE' in text
        if validation and not loaded:
            raise ValueError(f'{name}: validation layer load was not verified')
        if negative:
            if not hazard or proof.get('negative_sync_control') is not True or proof.get('stereo_mrt_fixture_passed') is not False:
                raise ValueError('The intentional missing dependency did not trigger synchronization validation')
        elif negative_compute:
            if ('VUID-' in text or 'Validation Error' in text or 'SYNC-HAZARD-' in text
                    or proof.get('negative_compute_control') is not True or proof.get('incorrect_compute_samples_detected') != 8
                    or proof.get('stereo_mrt_fixture_passed') is not False):
                raise ValueError('The valid but incorrect compute descriptor offset was not detected')
        else:
            if 'VUID-' in text or 'Validation Error' in text or 'SYNC-HAZARD-' in text:
                raise ValueError(f'{name}: Vulkan validation reported an error')
            if (proof.get('stereo_mrt_fixture_passed') is not True or proof.get('color_reference_different_bytes') != 0
                    or proof.get('stencil_reference_different_bytes') != 0 or proof.get('eye_renderings') != 16
                    or proof.get('binocular_disocclusion_texels',0) == 0
                    or proof.get('max_normalized_depth_difference',1) > 4/0xffffff
                    or proof.get('original_targets_preserved_defined_bytes',0) == 0):
                raise ValueError(f'{name}: stereo target reference/isolation checks did not pass')
            if args.split_passes and proof.get('clear_followed_by_load_verified') is not True:
                raise ValueError(f'{name}: split pass continuation was not verified')
            if args.recorded_replay and (proof.get('recorded_graphics_replay_verified') is not True
                    or proof.get('captured_scopes_replayed',0) == 0 or proof.get('following_draw_reference_compared_bytes',0) == 0
                    or proof.get('forwarded_original_reference_compared_bytes',0) == 0):
                raise ValueError(f'{name}: captured replay and following-draw restoration were not verified')
            if args.descriptor_buffers and (proof.get('descriptor_buffer_replay_verified') is not True
                    or proof.get('following_compute_reference_words') != 32):
                raise ValueError(f'{name}: descriptor-buffer replay and following compute restoration were not verified')
            if args.dynamic_state and (proof.get('dynamic_state_replay_verified') is not True
                    or proof.get('declared_dynamic_states') != 12 or proof.get('following_draw_depth_write_disabled_verified') is not True):
                raise ValueError(f'{name}: dynamic-state replay and following-draw depth restoration were not verified')
            if args.pipeline_recipes and (proof.get('pipeline_recipe_variants_verified') is not True
                    or proof.get('original_shader_modules_destroyed_before_variants') is not True):
                raise ValueError(f'{name}: recipe-created variants after module destruction were not verified')
        metadata = None
        if args.trace_layer:
            if not vulkan_trace.report(metadata_directory)['trace_loaded']:
                raise ValueError(f'{name}: D4VR layer load not verified')
            frames = list(metadata_directory.glob('frame-*.json'))
            if name == 'metadata-disabled':
                if frames:raise ValueError('Metadata was captured without explicit opt-in')
                metadata = {'disabled_capture_verified':True}
            else:
                if len(frames) != 1:raise ValueError('Expected one bounded metadata capture')
                data = frame_trace.read_frame(frames[0]);summary = frame_trace.summarize(data)
                states = summary['pipeline_state_declarations']
                if not states or any(not p['effective_dynamic_states_known'] for p in states):
                    raise ValueError('Monolithic fixture declarations were not captured completely')
                largest = max(len(p['declarations'][0]['states']) for p in states)
                if args.dynamic_state and largest != 12:raise ValueError('Dynamic pipeline declaration does not match fixture')
                attachments = [a for p in summary['passes'] for a in p['attachments'] if a['role'] in ('color','depth','stencil')]
                clears = [a for a in attachments if a['load_op'] == 1]
                if not clears or any(a['clear_value_words'] is None for a in clears):raise ValueError('Clear values absent from fixture metadata')
                if any(a['clear_value_words'] != [0x3f800000,0,0,0] for a in clears if a['role']=='depth'):
                    raise ValueError('Depth clear bits do not match original fixture')
                metadata = {'bounded_capture_verified':True,'monolithic_pipeline_count':len(states),'largest_dynamic_declaration':largest,
                            'clear_value_sets':len(clears),'graphics_state_snapshot_complete':False}
        results.append({'case':name,'exit_code':result.returncode,'validation_layer_loaded':loaded,
                        'intentional_sync_hazard_detected':hazard,
                        'intentional_wrong_compute_offset_detected':negative_compute and proof.get('incorrect_compute_samples_detected') == 8,'fixture':proof,
                        'frame_metadata':metadata})
        print(f'{name}: passed',flush=True)
    report = {'schema_version':1,'scope':'original synthetic scene only','cases':results,
              'input_sha256':{p.name:probe.digest(p) for p in inputs},
              'sync_validation_negative_control_passed':True,'game_draw_replay_implemented':False,'game_stereo_submitted':False}
    if args.trace_layer:report['trace_layer_sha256']=probe.digest(args.trace_layer/vulkan_trace.LIBRARY)
    probe.write_json(args.output/'proof.json',report)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('fixture','original','instrumented','fragment','validation-layer','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--runtime',type=Path)
    parser.add_argument('--split-passes',action='store_true')
    parser.add_argument('--recorded-replay',action='store_true')
    parser.add_argument('--descriptor-buffers',action='store_true')
    parser.add_argument('--compute',type=Path)
    parser.add_argument('--dynamic-state',action='store_true')
    parser.add_argument('--pipeline-recipes',action='store_true')
    parser.add_argument('--trace-layer',type=Path)
    args = parser.parse_args(); run(args); return 0


if __name__ == '__main__':
    try: raise SystemExit(main())
    except (OSError,ValueError,KeyError,subprocess.SubprocessError) as error:
        print(f'D4VR MRT validation: {error}'); raise SystemExit(2)
