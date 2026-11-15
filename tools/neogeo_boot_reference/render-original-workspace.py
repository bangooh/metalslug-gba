import sys,zipfile,zlib,struct,json
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
sys.path.insert(0,'upstream/assets/tools');import gfx
root=Path('scene-fixes/boot-animation');z=zipfile.ZipFile('scene-fixes/reference/neogeo.zip');lo=np.frombuffer(z.read('000-lo.lo'),dtype='u1');fix=gfx.fix_decode(z.read('sfix.sfix'));rom=Path('upstream/rom');tiles=gfx.spr_decode((rom/'201-c1.bin').read_bytes()[:32768],(rom/'201-c2.bin').read_bytes()[:32768]);xm=[0x0080,0x0880,0x0888,0x2888,0x288a,0x2a8a,0x2aaa,0xaaaa,0xaaea,0xbaea,0xbaeb,0xbbeb,0xbbef,0xfbef,0xfbff,0xffff]
frames=[];indexframes=[];paletteframes=[]
for frame in range(1,331,2):
 b=zlib.decompress((root/f'{frame:04d}.bin').read_bytes());v=np.frombuffer(b[:131072],'>u2');p=np.frombuffer(b[131072:],'>u2').astype('u4');rgb=((p>>8&15)*2+(p>>14&1))|(((p>>4&15)*2+(p>>13&1))<<5)|(((p&15)*2+(p>>12&1))<<10);im=np.full((256,320),4095,dtype='<u2');x=y=rows=zx=zy=0
 for i in range(1,381):
  ctl=int(v[0x8200+i]);zoom=int(v[0x8000+i]);
  if ctl&64:x=(x+zx+1)&511;zx=(zoom>>8)&15
  else:x=int(v[0x8400+i])>>7;y=512-(ctl>>7);rows=ctl&63;zx=(zoom>>8)&15;zy=zoom&255
  if not rows or 320<=x<=496:continue
  sx=x-512 if x>=480 else x;xs=[j for j in range(16) if xm[zx]&(0x8000>>j)];dst=np.arange(len(xs))+sx;keep=(dst>=0)&(dst<320);dst=dst[keep];xs=np.array(xs)[keep]
  for yy in range(16,240):
   line=(yy-y)&511
   if rows<32 and line>=rows*16:continue
   zl=line&255;inv=bool(line&256)
   if inv:zl^=255
   if rows>32:
    period=(zy+1)*2;zl%=period
    if zl>zy:zl=period-1-zl;inv=not inv
   q=int(lo[(zy<<8)|zl]);ti=q>>4;py=q&15
   if inv:py^=15;ti^=31
   tid,attr=map(int,v[i*64+ti*2:i*64+ti*2+2]);tid|=(attr<<12)&0xf0000
   assert tid<len(tiles),(frame,tid)
   a=tiles[tid,py^ (15 if attr&2 else 0),15-xs if attr&1 else xs];mask=a!=0;im[yy,dst[mask]]=(attr>>8)*16+a[mask]
 for xx in range(40):
  for yy in range(2,30):
   w=int(v[0x7000+xx*32+yy]);a=fix[w&4095];dest=im[yy*8:yy*8+8,xx*8:xx*8+8];mask=a!=0;dest[mask]=(w>>12)*16+a[mask]
 # Same center-sampled 75% conversion as existing sprite assets.
 ix=(np.arange(240)*4+2)//3;iy=16+(np.arange(160)+4)*4//3
 out=im[iy[:,None],ix];indexframes.append(out);paletteframes.append(rgb);frames.append(rgb[out])
np.save(root/'frames.npy',np.stack(frames))
np.save(root/'indexframes.npy',np.stack(indexframes));np.save(root/'paletteframes.npy',np.stack(paletteframes));print('indices',np.unique(indexframes))
