"""Lossless per-frame opaque occlusion, evaluated offline at exact GBA sampling."""
from pathlib import Path
import numpy as np,struct,zlib,re,json
R=Path(__file__).resolve().parents[1];G=R/'generated'
def unpack(n,f):return list(struct.iter_unpack(f,zlib.decompress((G/('opening-'+n+'.deflate')).read_bytes(),-15)))
cs=unpack('cells','<hhH');ms=unpack('maps','<HH');ds=unpack('draws','<HhhBBBB');fs=unpack('frames','<4H')
t=np.frombuffer(zlib.decompress((G/'opening-tiles.deflate').read_bytes(),-15),np.uint8).reshape(-1,12,6);tiles=np.empty((len(t),12,12),np.uint8);tiles[:,:,::2]=t&15;tiles[:,:,1::2]=t>>4
s=(R/'src/native_opening.c').read_text();rs=np.array([int(x)for x in re.findall(r'\d+',re.search(r'resample\[13\]\[12\]=\{(.*?)\};',s,re.S)[1])]).reshape(13,12)
data=bytearray();offsets=[];sizes=[];stats=[];shared={}
for f in fs:
 jobs=[]
 for mid,dx,dy,sx,sy,flip,bank in ds[f[0]:f[0]+f[1]]:
  start,n=ms[mid]
  for x,y,tid in cs[start:start+n]:
   if flip&1:x=-x-12
   if flip&2:y=-y-12
   l=dx+x*(sx+1)//256;r=dx+(x+12)*(sx+1)//256;top=dy+y*(sy+1)//256;b=dy+(y+12)*(sy+1)//256
   x0=max(l,0);x1=min(r,240);y0=max(top,0);y1=min(b,160)
   if x1<=x0 or y1<=y0:jobs.append(None);continue
   xx=rs[r-l,x0-l:x1-l];yy=rs[b-top,y0-top:y1-top]
   if flip&1:xx=11-xx
   if flip&2:yy=11-yy
   mask=tiles[tid][yy[:,None],xx]!=0;jobs.append((x0,x1,y0,y1,mask))
 covered=np.zeros((160,240),bool);bits=np.zeros(len(jobs),np.uint8)
 for i in range(len(jobs)-1,-1,-1):
  if jobs[i]is None:continue
  x0,x1,y0,y1,mask=jobs[i];dst=covered[y0:y1,x0:x1]
  if np.any(mask&~dst):bits[i]=1;dst|=mask
 raw=np.packbits(bits,bitorder='little').tobytes()
 if raw not in shared:shared[raw]=len(data);data.extend(zlib.compress(raw,9,wbits=-15))
 offsets.append(shared[raw]);sizes.append(len(raw));stats.append([len(jobs),int(bits.sum())])
(G/'opening-visibility.deflate').write_bytes(data)
(G/'opening-visibility.h').write_text('#include <stdint.h>\nextern const uint8_t opening_visibility_packed[];\nstatic const uint16_t opening_visibility_offsets[]={'+','.join(map(str,offsets))+'};\nstatic const uint16_t opening_visibility_sizes[]={'+','.join(map(str,sizes))+'};\nenum {OPENING_VISIBILITY_MAX='+str(max(sizes))+'};\n')
assert len(data)<65536 and max(sizes)<=1024
p=R/'src/opening_replay_assets.S';s=p.read_text();label='.section .rodata.opening_visibility,"a",%progbits\n.balign 4\n.global opening_visibility_packed\nopening_visibility_packed:\n.incbin "generated/opening-visibility.deflate"\n'
if 'opening_visibility_packed:'not in s:p.write_text(s+label)
(G/'opening-visibility-source.json').write_text(json.dumps({'bytes':len(data),'max_frame_bytes':max(sizes),'cells_total_kept':stats},indent=2));print(len(data),max(sizes),'cells',np.sum(stats,axis=0))
