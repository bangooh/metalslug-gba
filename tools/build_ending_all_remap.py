"""Ending-only original-index remaps, with exact sunset window palettes."""
from pathlib import Path
import sys,re,json
import numpy as np
from compile_m1_native_maps import body,numbers
R=Path(__file__).resolve().parents[1];G=R/'generated';P=G/'native-background';OUT=R/'validation-ending-all';OUT.mkdir(exist_ok=True)
orig=np.array(numbers(body((G/'drawing-data.c').read_text(),'native_source_palettes')),np.uint16).reshape(-1,16);profiles=np.fromfile(P/'profiles.u16','<u2').reshape(14,256)
pal=np.array(numbers(body((G/'ending-background-palette.inc').read_text(),'ending_background_palette')),np.uint16).reshape(14,12,16)
s=(G/'scene-programs.c').read_text();manifest=json.loads((P/'manifest.json').read_text());rows=[r for r in manifest['rows']if r['file']=='scene-programs.c']
raw=np.fromfile(G/'native-scaled-background-12x12.4bpp','u1').reshape(-1,12,6);px=np.empty((len(raw),12,12),np.uint16);px[:,:,::2]=raw&15;px[:,:,1::2]=raw>>4
sets=set();windows=[]
for image in [48,49]:
 row=rows[image];w,h=row['width'],row['height'];cells=np.array(numbers(body(s,row['cells'])),np.uint16).reshape(w,h,2);a=np.zeros((h*12,w*12),np.uint16)
 for x in range(w):
  for y in range(h):
   t,v=map(int,cells[x,y]);t|=((v>>4)&15)<<16;a[y*12:y*12+12,x*12:x*12+12]=(v>>8)*16+px[t,::-1 if v&2 else 1,::-1 if v&1 else 1]
 q=np.lib.stride_tricks.sliding_window_view(a,(8,8))[::4,::4].reshape(-1,64);rgb=orig[profiles[12,q//16],q%16];valid=q%16!=0
 for v,m in zip(rgb,valid):sets.add(tuple(map(int,np.unique(v[m]))))
 windows.append((image,q,rgb,valid))
groups=[]
for v in sorted(sets,key=lambda a:(-len(a),a)):
 assert len(v)<=15;c=set(v);fits=[(len(g|c),i)for i,g in enumerate(groups)if len(g|c)<=15]
 if fits:groups[min(fits)[1]]|=c
 else:groups.append(c)
assert len(groups)<=11
pal[12]=0
for bank,colors in enumerate(groups):pal[12,bank,1:len(colors)+1]=sorted(colors)
pal[12,11]=orig[1145];pal[12,11,0]=0
ps=[0,1,4,5,12];remap=np.zeros((5,118,12,16),np.uint8);cost=np.zeros((5,118,12),np.uint16);allow=np.zeros((118,12),np.uint16)
def rgb(c):return np.stack([c&31,(c>>5)&31,(c>>10)&31],-1).astype('i4')
for pi,p in enumerate(ps):
 for slot in range(118):
  n=int(profiles[p,slot])
  if n==65535:continue
  d=((rgb(orig[n])[None,:,None,:]-rgb(pal[p,:,1:])[:,None,:,:])**2).sum(-1)
  remap[pi,slot,:,1:]=np.argmin(d[:,1:,:],axis=2)+1;cost[pi,slot]=np.minimum(d[:,1:,:].min(-1).sum(-1),65535)
  if p==12:
   for b in range(12):allow[slot,b]=sum((1<<i)for i in range(1,16)if d[b,i].min()==0)
packed=remap[:,:,:,::2]|remap[:,:,:,1::2]<<4
# Keep only distinct slot tables; unbound slots share zero rows.
records=np.concatenate([packed.reshape(-1,96),cost.view('u1').reshape(-1,24)],axis=1)
unique,index=np.unique(records,axis=0,return_inverse=True)
# IDs include the cost/remap together. Compact output avoids duplicating unbound rows.
idx=index.reshape(5,118).astype('<u2');maps=unique[:,:96].reshape(-1,12,8);costs=unique[:,96:].copy().view('<u2').reshape(-1,12)
def arr(name,a,typ):
 def fmt(v):return str(int(v))if np.ndim(v)==0 else '{'+','.join(fmt(x)for x in v)+'}'
 return 'static const '+typ+' '+name+''.join('['+str(n)+']'for n in a.shape)+'='+fmt(a)+';\n'
(G/'ending-runtime-remap.inc').write_text(arr('ending_remap_ids',idx,'uint16_t')+arr('ending_remaps',maps,'uint8_t')+arr('ending_costs',costs,'uint16_t')+arr('ending_exact_masks',allow,'uint16_t'))
(G/'ending-background-palette.inc').write_text(arr('ending_background_palette',pal,'uint16_t'))
results=[]
for image,q,source,valid in windows:
 masks=np.zeros((len(q),4),np.uint16)
 for j,start in enumerate([0,4,32,36]):
  inds=q[:,[start+dy*8+dx for dy in range(4)for dx in range(4)]]%16
  for k in range(16):masks[:,j]|=np.where(inds[:,k]!=0,1<<inds[:,k],0).astype('u2')
 slots=q[:,[0,4,32,36]]//16;fits=np.ones((len(q),12),bool)
 for j in range(4):fits&=(allow[slots[:,j]]&masks[:,j,None])==masks[:,j,None]
 assert fits.any(axis=1).all();chosen=fits.argmax(axis=1);decoded=pal[12,chosen[:,None],remap[4,q//16,chosen[:,None],q%16]]
 assert np.array_equal(decoded[valid],source[valid]);results.append(dict(image=image,windows=len(q),opaque_samples=int(valid.sum()),rgb_mismatches=0))
report=dict(exact_sunset=results,sunset_banks=len(groups),table_bytes=idx.nbytes+maps.nbytes+costs.nbytes+allow.nbytes)
(OUT/'remap-report.json').write_text(json.dumps(report,indent=2));print(report)
