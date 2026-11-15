"""Restore scene 10's original mechanical backdrop (P-ROM $0E29A0).
Uses the same original sprite decoder as tools/opening_reference.
"""
import sys,zlib,struct,json
from pathlib import Path
import numpy as np
root=Path(__file__).resolve().parents[1]
upstream=Path(sys.argv[1]) if len(sys.argv)>1 else root.parent/'upstream'
sys.path.insert(0,str(upstream/'assets/tools'));import sprites as S
p=S.PRom();tiles=S.Tiles();im=np.zeros((160,240),dtype='u1');ix=(np.arange(12)*4+2)//3
for x in range(20):
 for y in range(9):
  at=0xe29a0+(x*9+y)*4;tid=p.W(at);attr=p.W(at+2);tid|=((attr>>4)&15)<<16
  a=tiles.get(tid);a=a[:,::-1] if attr&1 else a;a=a[::-1] if attr&2 else a;a=a[ix[:,None],ix]
  bank=(attr>>8)-16;assert 0<=bank<3
  im[32+y*12:44+y*12,x*12:x*12+12]=np.where(a,208+bank*16+a,0)
packed=zlib.compress(im.tobytes(),9,wbits=-15);g=root/'generated';(g/'opening-background.deflate').write_bytes(packed)
colors=[]
for bank in (0x198,0x199,0x19a):
 for i in range(16):
  off=0x1ce00+bank*64+i*4;r,gg,b=p.d[off:off+3];colors.append(int(r)|int(gg)<<5|int(b)<<10)
(g/'opening-background.h').write_text('#include <stdint.h>\nextern const uint8_t opening_background_packed[];\nstatic const uint16_t opening_background_palette[48]={'+','.join(map(str,colors))+'};\n')
print('Mechanical background compressed bytes:',len(packed))
