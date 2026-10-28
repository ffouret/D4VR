#!/usr/bin/env python3
"""Prepare a fixed lateral eye packet for an explicit geometry diagnostic."""
from __future__ import annotations
import argparse
import math
from pathlib import Path
import struct

FORMAT = struct.Struct('<8I36f')
IDENTITY = [1.,0,0,0,0,1.,0,0,0,0,1.,0,0,0,0,1.]
GATE = [1.784022,3.171595,.00001,0.]


def encode(eye: int, sequence: int = 1, separation: float = .064, enabled: bool = True) -> bytes:
    if eye not in (0,1) or not 1 <= sequence <= 0xffffff or not math.isfinite(separation) or not 0 <= separation <= .2:
        raise ValueError('Invalid fixed-eye diagnostic parameters')
    view = IDENTITY.copy(); view[3] = (1 if eye == 0 else -1)*separation/2
    return FORMAT.pack(0x44345652,1,sequence,3 if enabled else 2,eye,0,0,0,*view,*IDENTITY,*GATE)


def decode(data: bytes) -> dict:
    if len(data) != FORMAT.size:
        raise ValueError('Expected the 176-byte eye state ABI')
    values = FORMAT.unpack(data); magic,version,sequence,flags,eye,*reserved = values[:8]
    if magic != 0x44345652 or version != 1 or flags not in (2,3) or eye not in (0,1) or not 1 <= sequence <= 0xffffff or any(reserved):
        raise ValueError('Invalid fixed-eye state header')
    floats = list(values[8:]); view, projection, gate = floats[:16],floats[16:32],floats[32:]
    expected_gate = list(struct.unpack('<4f',struct.pack('<4f',*GATE)))
    if not all(math.isfinite(v) for v in floats) or projection != IDENTITY or gate != expected_gate:
        raise ValueError('This diagnostic requires the original projection and its measured gate')
    if any(view[k] != IDENTITY[k] for k in range(16) if k != 3) or abs(view[3]) > .100001:
        raise ValueError('This diagnostic supports a fixed lateral eye translation only')
    return {'eye':eye,'sequence':sequence,'enabled':bool(flags&1),'translation_view_units':view[3],
            'uses_current_game_projection':True,'mode':'immutable_per_launch'}


def read(path: Path) -> dict:
    if path.is_symlink() or path.stat().st_size != FORMAT.size:
        raise ValueError('Invalid private eye state file')
    return decode(path.read_bytes())


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--eye',choices=('left','right'),required=True)
    parser.add_argument('--separation-view-units',type=float,default=.064)
    parser.add_argument('--sequence',type=int,default=1)
    parser.add_argument('--disabled',action='store_true')
    parser.add_argument('--output',type=Path,required=True)
    args = parser.parse_args(); data = encode(0 if args.eye == 'left' else 1,args.sequence,args.separation_view_units,not args.disabled)
    with args.output.open('xb') as stream:
        args.output.chmod(0o600); stream.write(data)
    print(read(args.output)); return 0


if __name__ == '__main__': raise SystemExit(main())
