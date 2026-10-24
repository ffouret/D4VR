#!/usr/bin/env python3
"""Validate private stereo MRT targets with GPU references and a sync negative control."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import subprocess
import probe


def run(args) -> dict:
    inputs = [args.fixture,args.original,args.instrumented,args.fragment]
    if not all(p.is_file() for p in inputs) or args.output.exists():
        raise ValueError('Provide the built fixture/shaders and a new private output directory')
    manifest = json.loads((args.validation_layer/'VkLayer_khronos_validation.json').read_text())
    if manifest['layer']['name'] != 'VK_LAYER_KHRONOS_validation':
        raise ValueError('Expected the Khronos validation layer')
    args.output.mkdir(parents=True,mode=0o700)
    runtime = args.runtime.resolve() if args.runtime else None
    cases = [('native',False,False,False),('native-validation',True,False,False),
             ('sync-negative-control',True,False,True)]
    if runtime:
        cases.append(('steam-runtime-validation',True,True,False))
    results = []
    for name,validation,in_runtime,negative in cases:
        bridge = Path('/run/host/usr/bin/flatpak-spawn')
        command = [str(bridge),'--host','/usr/bin/env'] if bridge.exists() else ['/usr/bin/env']
        for key in ('VK_INSTANCE_LAYERS','VK_ADD_LAYER_PATH','VK_LAYER_PATH','VK_LAYER_ENABLES','VK_LAYER_VALIDATE_SYNC',
                    'VK_LOADER_DEBUG','D4VR_EYE_STATE_FILE','D4VR_TRACE_DIR','D4VR_FRAME_TRACE','D4VR_TRACE_XR_BINDING'):
            command += ['-u',key]
        if validation:
            command += ['VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation','VK_ADD_LAYER_PATH='+str(args.validation_layer.resolve()),
                        'VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT','VK_LAYER_VALIDATE_SYNC=1','VK_LOADER_DEBUG=layer']
        if in_runtime:
            command += [str(runtime),'--batch','--']
        command += [str(p.resolve()) for p in inputs]
        if negative:
            command.append('--negative-sync-control')
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
        else:
            if 'VUID-' in text or 'Validation Error' in text or 'SYNC-HAZARD-' in text:
                raise ValueError(f'{name}: Vulkan validation reported an error')
            if (proof.get('stereo_mrt_fixture_passed') is not True or proof.get('color_reference_different_bytes') != 0
                    or proof.get('stencil_reference_different_bytes') != 0 or proof.get('eye_renderings') != 16
                    or proof.get('binocular_disocclusion_texels',0) == 0
                    or proof.get('max_normalized_depth_difference',1) > 4/0xffffff
                    or proof.get('original_targets_preserved_defined_bytes',0) == 0):
                raise ValueError(f'{name}: stereo target reference/isolation checks did not pass')
        results.append({'case':name,'exit_code':result.returncode,'validation_layer_loaded':loaded,
                        'intentional_sync_hazard_detected':hazard,'fixture':proof})
        print(f'{name}: passed',flush=True)
    report = {'schema_version':1,'scope':'original synthetic scene only','cases':results,
              'input_sha256':{p.name:probe.digest(p) for p in inputs},
              'sync_validation_negative_control_passed':True,'game_draw_replay_implemented':False,'game_stereo_submitted':False}
    probe.write_json(args.output/'proof.json',report)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('fixture','original','instrumented','fragment','validation-layer','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--runtime',type=Path)
    args = parser.parse_args(); run(args); return 0


if __name__ == '__main__':
    try: raise SystemExit(main())
    except (OSError,ValueError,KeyError,subprocess.SubprocessError) as error:
        print(f'D4VR MRT validation: {error}'); raise SystemExit(2)
