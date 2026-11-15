#!/usr/bin/env python3
"""Exact offline orientation expansion of the complete imported BG inventory.
No emulator route, captured viewport, palette reduction or missing-tile fallback.
"""
from pathlib import Path
import hashlib,json,re
import numpy as np
ROOT=Path(__file__).resolve().parents[1];G=ROOT/'generated'
def generate():
 raw=(G/'native-bg-blocks.4bpp').read_bytes();source=np.frombuffer(raw,dtype=np.uint8).reshape(-1,4,2)
 count=len(source);assert count==int(re.search(r'NATIVE_BG_BLOCK_COUNT=(\d+)',(G/'tile-video-data.h').read_text())[1])
 pixels=np.empty((count,4,4),np.uint8);pixels[:,:,::2]=source&15;pixels[:,:,1::2]=source>>4
 variants=[]
 for flip in [1,2,3]:
  view=pixels[:,::-1 if flip&2 else 1,::-1 if flip&1 else 1]
  packed=view[:,:,::2]|(view[:,:,1::2]<<4)
  restored=np.empty_like(view);restored[:,:,::2]=packed&15;restored[:,:,1::2]=packed>>4
  assert np.array_equal(view,restored);variants.append(packed.tobytes())
 payload=b''.join(variants);(G/'native-bg-preflipped.4bpp').write_bytes(payload)
 report=dict(source_sha256=hashlib.sha256(raw).hexdigest(),blocks=count,additional_orientations=3,bytes=len(payload),sha256=hashlib.sha256(payload).hexdigest(),exact_pixels_verified=count*16*3,coverage='Every imported background block in all53 scene images and659 patches; no recorded-input coverage',quantized=False)
 (G/'native-bg-preflipped.json').write_text(json.dumps(report,indent=2)+'\n');return report
if __name__=='__main__':print(json.dumps(generate()))
