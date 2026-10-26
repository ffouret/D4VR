"""Held monoscopic selection across real WSI presents; no XR or game."""
import argparse,json,subprocess
from pathlib import Path
import pose_layer_validate,probe


def validate(a):
    if a.output.exists():raise ValueError('Use a new private output directory')
    a.output.mkdir(mode=0o700,parents=True)
    unit=subprocess.run([*__import__('paired_capture').host_command(),str(a.hold_unit.resolve())],capture_output=True,text=True,check=True,timeout=20)
    positive=argparse.Namespace(**vars(a));positive.output=a.output/'held';positive.skip_legacy=True;positive.hold_view=True;positive.fragment_packet_check=True;positive.calibrated=True
    checks=pose_layer_validate.validate(positive)
    negatives=[]
    for runtime in (False,True) if a.runtime else (False,):
        control=argparse.Namespace(**vars(positive));control.output=a.output/('unheld-runtime' if runtime else 'unheld-native');control.runtime=a.runtime if runtime else None;control.controls=('normal',);control.disable_hold=True
        try:pose_layer_validate.validate(control)
        except ValueError as e:
            if str(e) not in ('Prior held mapping/provenance missing','View not held between capture requests'):raise
        else:raise ValueError('Original unheld driver passed held-view proof')
        negatives.append({'environment':'steam-runtime' if runtime else 'native','unheld_interval_detected':True})
    regression=argparse.Namespace(**vars(a));regression.output=a.output/'default-regression';regression.skip_legacy=True;regression.controls=('normal','cancel');regression.calibrated=True
    default=pose_layer_validate.validate(regression)
    root=Path(__file__).resolve().parents[1]
    result={'schema_version':1,'synthetic_scene':True,'real_wsi_loader':True,'actual_xr_pose_source':False,'actual_steamvr_submission':False,'game_rendered':False,'validation_errors':0,'unit':json.loads(unit.stdout),'held_cases':checks,'negative_controls':negatives,'default_driver_regression':default,'valid_stereo_request_not_admitted':True,'host_present_span_is_not_gpu_time':True,'fixture_sha256':probe.digest(a.fixture),'forwarding_sha256':probe.digest(a.forwarding),'hold_unit_sha256':probe.digest(a.hold_unit),'source_sha256':{p:probe.digest(root/p) for p in ['native/core/fixed_view_hold.hpp','native/vulkan/diagnostic_capture.cpp','native/vulkan/diagnostic_capture.hpp','native/vulkan/trace_layer.cpp','native/tests/present_layer_vulkan.cpp','native/tests/fixed_view_hold_tests.cpp','tools/pose_layer_validate.py','tools/fixed_view_validate.py']}}
    probe.write_json(a.output/'proof.json',result);return result

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('fixture','compiler','layer','validation-layer','unit','forwarding','hold-unit','output'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--runtime',type=Path)
    try:print(json.dumps(validate(p.parse_args()),indent=2))
    except (OSError,ValueError,subprocess.SubprocessError) as e:raise SystemExit(str(e))
