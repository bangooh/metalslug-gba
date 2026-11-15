#!/usr/bin/env python3
"""Exact offline opacity masks, including all source reflection variants."""
from pathlib import Path
import hashlib, json
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
def generate():
    src=ROOT/'generated/native-bg-blocks.4bpp'
    raw=np.frombuffer(src.read_bytes(),np.uint8).reshape(-1,4,2)
    px=np.empty((len(raw),4,4),np.uint8)
    px[:,:,::2]=raw&15;px[:,:,1::2]=raw>>4
    bits=(1<<np.arange(16,dtype=np.uint16)).reshape(4,4)
    masks=np.empty((len(raw),4),dtype='<u2')
    for flip in range(4):
        p=px[:,::-1 if flip&2 else 1,::-1 if flip&1 else 1]
        masks[:,flip]=((p!=0)*bits).sum(axis=(1,2))
        rebuilt=((masks[:,flip,None]>>np.arange(16))&1).reshape(-1,4,4)
        assert np.array_equal(rebuilt,p!=0)
    dest=ROOT/'generated/native-bg-opacity.u16'
    dest.write_bytes(masks.tobytes())
    result=dict(blocks=len(raw),variants=4,bytes=dest.stat().st_size,
        source_sha256=hashlib.sha256(src.read_bytes()).hexdigest(),
        sha256=hashlib.sha256(dest.read_bytes()).hexdigest(),exact=True)
    (ROOT/'generated/bg-opacity.json').write_text(json.dumps(result,indent=2)+'\n')
    return result
if __name__=='__main__':print(json.dumps(generate()))
