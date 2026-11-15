from pathlib import Path
import zlib,struct,hashlib,json,numpy as np
from PIL import Image
p=Path(__file__).resolve().parents[1]/'generated'
def unpack(n,fmt):
 b=zlib.decompress((p/('opening-'+n+'.deflate')).read_bytes(),-15);return list(struct.iter_unpack(fmt,b))
t=zlib.decompress((p/'opening-tiles.deflate').read_bytes(),-15)
cells=unpack('cells','<hhH');maps=unpack('maps','<HH');draws=unpack('draws','<HhhBBBB');pals=unpack('palettes','<16H');frames=unpack('frames','<4H');refs=unpack('palrefs','<H')
f=frames[-1];out=np.zeros((160,240,3),dtype=np.uint8); ids=np.zeros((160,240),dtype=np.uint8)
for j,d in enumerate(draws[f[0]:f[0]+f[1]]):
 mp,dx,dy,sx,sy,flip,bank=d;begin,count=maps[mp];n=0
 for x,y,tid in cells[begin:begin+count]:
  left=dx+((-x-12 if flip&1 else x)*(sx+1)//256);top=dy+((-y-12 if flip&2 else y)*(sy+1)//256)
  for iy in range(12):
   for ix in range(12):
    xx=left+ix;yy=top+iy
    if not(0<=xx<240 and 0<=yy<160):continue
    xx0=11-ix if flip&1 else ix;yy0=11-iy if flip&2 else iy
    v=t[tid*72+yy0*6+xx0//2]>>(4*(xx0%2))&15
    if v:
     ids[yy,xx]=bank*16+v;q=pals[refs[f[2]+bank][0]][v];out[yy,xx]=[(q&31)*255//31,((q>>5)&31)*255//31,((q>>10)&31)*255//31]
     if yy>=152:n+=1



g=p;tiles=[bytes(32)];lookup={tiles[0]:0};maps=np.zeros((3,32,32),dtype='<u2')
for y in range(20):
 for x in range(30):
  a=ids[y*8:y*8+8,x*8:x*8+8];banks=sorted(set((a[a%16!=0]//16).flat))
  assert len(banks)<=3
  for layer,bank in enumerate(banks):
   pix=np.where(a//16==bank,a%16,0).flatten();raw=bytes(int(pix[i])|(int(pix[i+1])<<4) for i in range(0,64,2))
   if raw not in lookup:lookup[raw]=len(tiles);tiles.append(raw)
   maps[2-layer,y,x]=lookup[raw]|int(bank)<<12
raw=b''.join(tiles);assert len(raw)<=28864
raw+=bytes(28864-len(raw));palette=[0]*256
for i in range(f[3]):palette[i*16:i*16+16]=pals[refs[f[2]+i][0]]
for name,data in [('title.tiles',raw),('title.maps',maps.tobytes()),('title.pal',struct.pack('<256H',*palette))]:
 (g/name).write_bytes(data)
 if name!='title.pal':
  co=zlib.compressobj(9,wbits=-15);(g/(name+'.deflate')).write_bytes(co.compress(data)+co.flush())
print('tiles',len(tiles))
(g/'title-source.json').write_text(json.dumps({'source':'Final opening frame, 75% C-ROM pixels; approved native-size subtitle and TM glyphs restored','trace_sha256':json.loads((g/'opening-replay-source.json').read_text())['trace_sha256'],'tiles':len(tiles)},indent=2))
