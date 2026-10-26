#!/usr/bin/env python3
"""Prepare one-session, sequential geometric-eye image capture. No live VR claim."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import time
import zlib
import camera_capture
import eye_control
import paired_capture
import probe
import shader_capture
import vulkan_trace


def atomic(path: Path,value: str):
    tmp=path.with_suffix(path.suffix+'.tmp');tmp.write_text(value);tmp.chmod(0o600);tmp.replace(path)


def prepare(game: Path,launcher: Path,shaders: Path,layer: Path,output: Path) -> dict:
    proof=json.loads((layer/'sequential-validation.json').read_text())
    if (proof.get('purpose')!='sequential_presentation_capture' or proof.get('wsi_loader_capture_passed') is not True
            or proof.get('original_pixels_restored') is not True or proof.get('forwarding_tests_passed') is not True
            or proof.get('layer_sha256')!=probe.digest(layer/vulkan_trace.LIBRARY)):
        raise ValueError('Provide the GPU-validated sequential capture layer')
    manifest=json.loads((layer/vulkan_trace.MANIFEST).read_text())['layer']
    if manifest.get('name')!=vulkan_trace.LAYER or manifest.get('library_path')!='./'+vulkan_trace.LIBRARY:
        raise ValueError('Unexpected sequential capture layer manifest')
    bundle=camera_capture.validate_bundle(shaders);expected=probe.profile()
    if (bundle.get('purpose')!='eye_transform_diagnostic' or bundle.get('eye_state_abi')!=1
            or bundle.get('eye_state_specialization_id')!=4 or bundle['game_version']!=expected['game_version']
            or bundle.get('gpu_validation',{}).get('stereo_gpu_fixture_passed') is not True):
        raise ValueError('A matching, GPU-validated game eye-transform bundle is required')
    game,output=game.resolve(),output.resolve()
    if output.exists() or output==game or game in output.parents:raise ValueError('Use a new private directory outside the game')
    if shader_capture.launcher_processes():raise ValueError('Close Diablo and Battle.net before preparation')
    source=paired_capture.launcher_source(launcher.read_text(),bundle['proton_path'],game,output,True)
    audit=probe.check_target(game,expected);output.mkdir(parents=True,mode=0o700)
    capture=shader_capture.prepare(game,launcher,output,expected)
    private_layer=output/'layer';private_layer.mkdir(mode=0o700)
    for name in (vulkan_trace.LIBRARY,vulkan_trace.MANIFEST):shutil.copyfile(layer/name,private_layer/name)
    if probe.digest(private_layer/vulkan_trace.LIBRARY)!=proof['layer_sha256']:raise ValueError('Layer changed during copy')
    for eye,name in enumerate(('left','right')):
        path=output/(name+'.bin');path.write_bytes(eye_control.encode(eye,separation=.128));path.chmod(0o600)
    (output/'vendor-cache').mkdir(mode=0o700);atomic(output/'eye.selection','2\n')
    script=output/'battle-net-sequential.sh';script.write_text(source);script.chmod(0o700)
    variables={'VK_INSTANCE_LAYERS':vulkan_trace.LAYER,'VK_ADD_LAYER_PATH':str(private_layer),'D4VR_TRACE_DIR':str(output),
        'D4VR_PAIR_LEFT_FILE':str(output/'left.bin'),'D4VR_PAIR_RIGHT_FILE':str(output/'right.bin'),'D4VR_SEQUENTIAL_CAPTURE_DIR':str(output),
        'VKD3D_SHADER_OVERRIDE':probe.windows_path(shaders.resolve()/'overrides'),'VKD3D_SHADER_CACHE_PATH':capture['variables']['VKD3D_SHADER_CACHE_PATH'],
        'VKD3D_SHADER_DEBUG':'info','VKD3D_DEBUG':'info','VKD3D_LOG_FILE':probe.windows_path(output/'sequential.log')}
    capture.update(phase='prepared',purpose='sequential_diablo_capture',variables=variables,private_launcher=str(script),game_audit_before=audit,
        shader_bundle=str(shaders.resolve()),shader_bundle_sha256=probe.digest(shaders/'bundle.json'),layer_sha256=proof['layer_sha256'],
        game_files_changed=False,game_stereo_submitted=False,same_instant_pair=False,eye_separation_view_units=.128)
    probe.write_json(output/'capture.json',capture);return capture


def launch(output: Path):
    capture=json.loads((output/'capture.json').read_text())
    if capture['purpose'] not in ('sequential_diablo_capture','observational_depth_audit') or capture['phase']!='prepared':raise ValueError('Session is not prepared')
    if shader_capture.launcher_processes():raise ValueError('Close Diablo and Battle.net before launch')
    capture['phase']='running';probe.write_json(output/'capture.json',capture)
    cmd=paired_capture.command(capture);observations=[]
    with (output/'launcher-private.log').open('xb') as log:
        os.chmod(log.name,0o600);process=subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT)
        while True:
            observations.append({'processes':paired_capture.owned_processes(output),'elapsed_monotonic':time.monotonic()})
            probe.write_json(output/'process-observations.json',{'observations':observations[-1800:]})
            if process.poll() is not None:break
            try:process.wait(timeout=2)
            except subprocess.TimeoutExpired:pass
    capture.update(phase='launcher_exited',launcher_exit_code=process.returncode,
        game_audit_after=probe.check_target(Path(capture['game_directory']),probe.profile()))
    probe.write_json(output/'capture.json',capture)


def read_image(path: Path) -> tuple[dict,bytes]:
    if path.is_symlink():raise ValueError('Image manifest symlink refused')
    d=json.loads(path.read_text());eye=d.get('eye');w,h=d.get('width'),d.get('height')
    if (type(eye) is not int or eye not in (0,1) or type(w) is not int or type(h) is not int or not 0<w<=4096 or not 0<h<=4096
            or type(d.get('sequence')) is not int or d['sequence']<=0 or type(d.get('present')) is not int or d['present']<=0
            or d.get('format') not in (37,43,44,50,58,64) or d.get('bytes')!=w*h*4 or d.get('single_eye_interval') is not True
            or d.get('selected_eye_mask')!=1<<eye or d.get('selected_draw_commands',0)<=0 or d.get('scope')!='original_presentation_blit'
            or d.get('same_instant_pair') is not False or d.get('game_stereo_submitted') is not False):
        raise ValueError('Image does not prove a supported single-eye presentation interval')
    pixels=path.with_suffix('.pixels')
    if pixels.is_symlink() or pixels.stat().st_size!=w*h*4:raise ValueError('Invalid pixel payload')
    return d,pixels.read_bytes()


def rgba(data: bytes,fmt: int) -> bytes:
    if fmt in (37,43):return data
    out=bytearray(len(data))
    if fmt in (44,50):
        out[0::4]=data[2::4];out[1::4]=data[1::4];out[2::4]=data[0::4];out[3::4]=data[3::4]
    elif fmt in (58,64):
        for i,(word,) in enumerate(struct.iter_unpack('<I',data)):
            channels=[round(((word>>n)&1023)*255/1023) for n in (0,10,20)]
            if fmt==58:channels.reverse()
            out[i*4:i*4+4]=bytes(channels+[round((word>>30)*255/3)])
    else:raise ValueError('Unsupported presentation format')
    return bytes(out)


def png(path: Path,width: int,height: int,data: bytes):
    if len(data)!=width*height*4:raise ValueError('RGBA size mismatch')
    def chunk(name,value):return struct.pack('>I',len(value))+name+value+struct.pack('>I',zlib.crc32(name+value)&0xffffffff)
    rows=b''.join(b'\0'+data[y*width*4:(y+1)*width*4] for y in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',width,height,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b''));path.chmod(0o600)


def preview_rgba(data: bytes,fmt: int) -> bytes:
    # Presentation RGB is already composed. Backbuffer alpha is not window
    # transparency: Diablo writes zero over most terrain. Keep raw pixels
    # untouched, but make the exported presentation preview fully opaque.
    out=bytearray(rgba(data,fmt));out[3::4]=b'\xff'*(len(out)//4);return bytes(out)


def pair(output: Path,require_game: bool=True) -> dict:
    groups={}
    for path in output.glob('image-*.json'):
        match=re.fullmatch(r'image-(\d+)-(\d+)-(\d+)\.json',path.name)
        if not match:continue
        d,b=read_image(path)
        if int(match.group(3))!=d['sequence']:raise ValueError('Image filename/request sequence mismatch')
        groups.setdefault(match.group(1,2),[]).append((path,d,b))
    candidates=[(key,sorted(value,key=lambda x:x[1]['sequence'])) for key,value in groups.items() if {x[1]['eye'] for x in value}=={0,1}]
    if len(candidates)!=1:raise ValueError('Expected one complete pair from the same process/device')
    key,images=candidates[0]
    if len(images) not in (2,4) or [x[1]['eye'] for x in images] != [0,1]*(len(images)//2):raise ValueError('Expected left/right or repeated left/right captures')
    if require_game:
        observed=json.loads((output/'process-observations.json').read_text())['observations']
        pids={str(p['pid']) for row in observed for p in row['processes'] if 'diablo iv.exe' in p['names']}
        if key[0] not in pids:raise ValueError('Captured process was not observed as Diablo IV.exe')
    left,right=images[:2]
    if any(left[1][k]!=x[1][k] for k in ('width','height','format') for x in images[1:]):raise ValueError('Eye formats/extents differ')
    if any(a[1]['sequence']>=b[1]['sequence'] or a[1]['present']>=b[1]['present'] for a,b in zip(images,images[1:])):raise ValueError('Eye order is invalid')
    width,height=left[1]['width'],left[1]['height'];colors=[preview_rgba(x[2],x[1]['format']) for x in (left,right)]
    for name,color in zip(('left','right'),colors):png(output/(name+'.png'),width,height,color)
    joined=b''.join(colors[0][y*width*4:(y+1)*width*4]+colors[1][y*width*4:(y+1)*width*4] for y in range(height))
    png(output/'pair.png',width*2,height,joined)
    report={'schema_version':1,'scope':'sequential_original_render_images','synthetic_scene':not require_game,'game_process_observed':require_game,
        'width':width,'height':height,'format':left[1]['format'],'same_instant_pair':False,'game_stereo_submitted':False,
        'preview_alpha_policy':'opaque presentation RGB; original captured pixels retained unchanged',
        'different_pixel_bytes':sum(a!=b for a,b in zip(left[2],right[2])),
        'images':[{'metadata':str(x[0]),'eye':x[1]['eye'],'sha256':hashlib.sha256(x[2]).hexdigest(),'selected_draw_commands':x[1]['selected_draw_commands']} for x in (left,right)]}
    if len(images)==4:
        report['repeat_images']=[]
        for x,name in zip(images[2:],('left','right')):
            png(output/('repeat-'+name+'.png'),width,height,preview_rgba(x[2],x[1]['format']))
            report['repeat_images'].append({'metadata':str(x[0]),'eye':x[1]['eye'],'sha256':hashlib.sha256(x[2]).hexdigest()})
        report['repeated_eye_selections_verified']=True
        report['stationary_scene_proven']=False
    probe.write_json(output/'pair.json',report);return report


def request_pair(output: Path,repeat: bool=False):
    session=json.loads((output/'capture.json').read_text())
    if session['purpose']!='sequential_diablo_capture':raise ValueError('Not a prepared Diablo session')
    try:
        for sequence,eye in enumerate((0,1,0,1) if repeat else (0,1),1):
            atomic(output/'eye.selection',str(eye)+'\n');time.sleep(2)
            atomic(output/'present.request',str(sequence)+'\n');deadline=time.monotonic()+15
            while time.monotonic()<deadline:
                found=[p for p in output.glob('image-*.json') if json.loads(p.read_text()).get('sequence')==sequence]
                if found:
                    if len(found)!=1:raise ValueError('Multiple processes captured the same request')
                    read_image(found[0]);break
                time.sleep(.1)
            else:raise ValueError('No supported presentation capture arrived within 15 seconds')
    finally:atomic(output/'eye.selection','2\n')
    return pair(output)


def main():
    parser=argparse.ArgumentParser(description=__doc__);sub=parser.add_subparsers(dest='mode',required=True)
    p=sub.add_parser('prepare')
    for key in ('game-dir','launcher','shader-bundle','layer','output'):p.add_argument('--'+key,type=Path,required=True)
    for mode in ('launch','request','report'):
        p=sub.add_parser(mode);p.add_argument('--output',type=Path,required=True)
        if mode=='request':p.add_argument('--repeat',action='store_true',help='Repeat left/right once for temporal consistency inspection')
    args=parser.parse_args()
    if args.mode=='prepare':result=prepare(args.game_dir,args.launcher,args.shader_bundle,args.layer,args.output)
    elif args.mode=='launch':launch(args.output);return
    elif args.mode=='request':result=request_pair(args.output,args.repeat)
    else:result=pair(args.output)
    print(json.dumps(result,indent=2))


if __name__=='__main__':
    try:main()
    except (OSError,ValueError,KeyError,subprocess.SubprocessError) as error:raise SystemExit(f'D4VR sequential capture: {error}')
