"""Build-only closed scene identity and complete command interception surface.

There is no runtime shader allow-list file: the layer compares complete live
module bytes with arrays compiled from our own fixture sources in this build.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import re
import struct
import subprocess

EXISTING = set('CmdExecuteCommands CmdDraw CmdDrawIndexed CmdDrawIndirect CmdDrawIndexedIndirect CmdDrawIndirectCount CmdDrawIndexedIndirectCount CmdExecuteGeneratedCommandsEXT CmdExecuteGeneratedCommandsNV CmdDispatch CmdDispatchIndirect CmdBeginRenderPass CmdBeginRenderPass2 CmdBeginRendering CmdEndRenderPass CmdEndRenderPass2 CmdEndRendering CmdBindPipeline'.split())
MODELED = set('CmdPushConstants CmdSetViewport CmdSetScissor CmdSetStencilReference'.split())
EXISTING.update(('CmdBindDescriptorBuffersEXT','CmdSetDescriptorBufferOffsetsEXT','CmdBindDescriptorSets'))
EXISTING.update(('CmdBlitImage','CmdBlitImage2','CmdCopyImage','CmdCopyImage2','CmdCopyImageToBuffer','CmdCopyImageToBuffer2'))

def generate(compiler: Path, header: Path, output: Path) -> None:
    output.mkdir(parents=True, exist_ok=True)
    root = Path(__file__).resolve().parents[1]
    arrays = ['#pragma once\n#include <cstdint>\nnamespace d4vr::poc::code {\n']
    for name, source, define in [('vertex','layer_poc.vert',0),('stereo','layer_poc.vert',1),('fragment','stereo_mrt.frag',None)]:
        target = output / (name + '.spv')
        command = [str(compiler), '-V', '--target-env', 'vulkan1.2']
        if define is not None:
            command += ['-DD4VR_POC_STEREO=' + str(define)]
        subprocess.run(command + [str(root / 'native/tests' / source), '-o', str(target)], check=True)
        data = target.read_bytes()
        words = list(struct.unpack('<' + 'I' * (len(data)//4), data))
        if name == 'stereo':
            # OpModuleProcessed belongs in the debug section before annotations.
            marker = b'D4VR_STEREO_ABI_1\0'
            marker += b'\0' * (-len(marker)%4)
            instruction = [((1+len(marker)//4)<<16)|330, *struct.unpack('<'+'I'*(len(marker)//4),marker)]
            k = 5
            while k < len(words) and words[k]&65535 not in (71,72,19,20,21,22):
                k += words[k]>>16
            words[k:k] = instruction
            target.write_bytes(struct.pack('<'+'I'*len(words),*words))
        arrays += [f'inline constexpr uint32_t {name}[] = {{\n', ','.join(hex(w)+'u' for w in words), '\n};\n']
    arrays += ['}\n']
    (output/'poc_shader_code.hpp').write_text(''.join(arrays))
    text = header.read_text()
    commands = re.findall(r'typedef void \(VKAPI_PTR \*PFN_vk(Cmd\w+)\)\((.*?)\);',text,re.S)
    if len(commands) < 150:
        raise ValueError('Incomplete Vulkan command declaration surface')
    generated, matches = [], []
    for name,args in commands:
        if name in EXISTING or name in MODELED:
            continue
        if name.endswith(('KHR','EXT')) and name.removesuffix('KHR').removesuffix('EXT') in EXISTING:
            # The existing layer already intercepts promoted KHR aliases.
            if name.endswith('KHR'):
                continue
        params = args.split(',')
        names = [re.search(r'(\w+)\s*(?:\[.*?\])?\s*$',p).group(1) for p in params]
        if not params[0].strip().startswith('VkCommandBuffer '):
            raise ValueError('Unexpected first command parameter')
        barrier = name in {'CmdPipelineBarrier','CmdPipelineBarrier2','CmdPipelineBarrier2KHR','CmdCopyImageToBuffer','CmdCopyImageToBuffer2','CmdCopyImageToBuffer2KHR'}
        generated += [f'VKAPI_ATTR void VKAPI_CALL poc_{name}({args}) {{\n',
            f' auto d=deviceFor({names[0]}); if(d->poc) d->poc->unsupported({names[0]},'+('true' if barrier else 'false')+');\n',
            f' auto fn=reinterpret_cast<PFN_vk{name}>(d->get(d->handle,"vk{name}")); fn('+','.join(names)+');\n}\n']
        matches += [f' if(std::strcmp(name,"vk{name}")==0) return reinterpret_cast<PFN_vkVoidFunction>(poc_{name});\n']
    (output/'poc_command_wrappers.inc').write_text(''.join(generated))
    (output/'poc_command_matches.inc').write_text(''.join(matches))

if __name__ == '__main__':
    p=argparse.ArgumentParser();p.add_argument('--compiler',type=Path,required=True);p.add_argument('--header',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();generate(a.compiler,a.header,a.output)
