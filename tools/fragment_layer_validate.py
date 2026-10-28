"""Real WSI stage binding and mutable packet checks, without game or XR."""
import argparse
import json
from pathlib import Path
import subprocess
import pose_layer_validate
import probe


def validate(a):
    if a.output.exists():raise ValueError('Use a new private output directory')
    a.output.mkdir(parents=True,mode=0o700)
    positive=argparse.Namespace(**vars(a));positive.output=a.output/'marked';positive.fragment_packet_check=True;positive.skip_legacy=True
    proof=pose_layer_validate.validate(positive)
    negatives=[]
    for runtime in (False,True) if a.runtime else (False,):
        negative=argparse.Namespace(**vars(positive));negative.output=a.output/('unmarked-runtime' if runtime else 'unmarked-native');negative.runtime=a.runtime if runtime else None;negative.controls=('normal',);negative.fragment_marker=False
        try:pose_layer_validate.validate(negative)
        except ValueError as e:
            if str(e)!='Fragment packet differs from vertex packet':raise
        else:raise ValueError('An unmarked fragment silently acquired the camera address')
        negatives.append({'environment':'steam-runtime' if runtime else 'native','unmarked_fragment_mismatch_detected':True})
    root=Path(__file__).resolve().parents[1]
    result={'schema_version':1,'synthetic_scene':True,'real_wsi_loader':True,'mutable_gpu_packets':True,'vertex_fragment_packet_equality':True,'pose_capture':proof,'negative_controls':negatives,'game_rendered':False,'actual_steamvr_submission':False,'validation_errors':0,'source_sha256':{p:probe.digest(root/p) for p in ['tools/pose_layer_validate.py','tools/fragment_layer_validate.py','native/tests/present_fragment_eye.vert','native/tests/present_fragment_eye.frag','native/vulkan/eye_control.cpp','native/vulkan/library_variants.cpp','native/vulkan/pipeline_recipe.cpp','native/vulkan/eye_packet_update.hpp']}}
    probe.write_json(a.output/'proof.json',result);return result


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('fixture','compiler','layer','validation-layer','unit','forwarding','output'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--runtime',type=Path);a=p.parse_args();a.calibrated=True
    try:print(json.dumps(validate(a),indent=2))
    except (OSError,ValueError,subprocess.SubprocessError) as e:raise SystemExit(str(e))
