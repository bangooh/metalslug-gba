#!/usr/bin/env python3
"""Compile final tile palettes/indices; no runtime color search. Deterministic."""
from pathlib import Path
import numpy as np,json,argparse
from prepare_native_background import rgb,colors,centers
R=Path(__file__).resolve().parents[1];D=R/'generated/m1-cartridge'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--mission',type=int,choices=(1,2,3,4,5,6),default=1)
parser.add_argument('--requantize',action='store_true',help='Explicitly redesign all M1 palettes instead of preserving approved pixels')
parser.add_argument('--preserve-existing',action='store_true',help='Keep approved palette banks, including dynamic source-slot identities')
parser.add_argument('--approved-dir',type=Path,help='Explicit freshly generated palette catalog')
args=parser.parse_args()
prefix=f'm{args.mission}';D=R/f'generated/{prefix}-cartridge'
a=np.fromfile(D/'tiles.bin',np.uint8).reshape(-1,64)
pal=np.fromfile(D/'palette.bin','<u2');pts=rgb(pal[1:])
hist=np.zeros((len(a),len(pts)),np.float32)
for i,t in enumerate(a):hist[i]=np.bincount(t,minlength=192)[1:]
w=hist.sum(1);mean=hist@pts/np.maximum(w[:,None],1)
seed=centers(mean,w+1,12);groups=np.argmin(((mean[:,None]-seed)**2).sum(2),axis=1)
semantic=np.fromfile(D/'semantic.bin','<u2') if (D/'semantic.bin').exists() else np.zeros(192,np.uint16)
bank_semantic=np.zeros((12,15),np.uint16)
approved=args.approved_dir or R/f'art/approved-{prefix}-4bpp';preserved={}
if semantic.any() and not args.preserve_existing:
 from native_dynamic_banks import compile_banks
 if args.mission==1:
  from native_m1_hit_banks import compile_banks
 groups,banks,dist,bank_semantic=compile_banks(hist,pts,semantic,groups)
elif not args.requantize and approved.exists():
 old_a=np.fromfile(approved/'tiles.bin',np.uint8).reshape(-1,64)
 old_tiles=np.fromfile(approved/'tiles4.bin',np.uint8).reshape(-1,32)
 old_ids=np.fromfile(approved/'ids4.bin','<u2');old_attrs=np.fromfile(approved/'attrs4.bin','<u2')
 assert len(old_a)==len(old_ids)==len(old_attrs)
 for t,ident,attr in zip(old_a,old_ids,old_attrs):
  packed_tile=old_tiles[ident];v=np.empty(64,np.uint8);v[::2]=packed_tile&15;v[1::2]=packed_tile>>4;v=v.reshape(8,8)
  if attr&0x400:v=v[:,::-1]
  if attr&0x800:v=v[::-1]
  preserved[t.tobytes()]=(int(attr>>12),v.copy())
 banks=rgb(np.fromfile(approved/'palette4.bin','<u2').reshape(12,16)[:,1:])
 dist=((pts[:,None,None]-banks[None])**2).sum(3)
 if semantic.any():
  bank_semantic=np.fromfile(approved/'semantic4.bin','<u2').reshape(12,16)[:,1:]
  dist=np.where((semantic[1:]//16)[:,None,None]==(bank_semantic//16)[None],dist,1e12)
 groups=np.argmin(hist@dist.min(2),axis=1)
else:
 for iteration in range(12):
  banks=[]
  for b in range(12):
   weights=hist[groups==b].sum(0);mask=weights>0
   banks.append(rgb(colors(centers(pts[mask],weights[mask],15,20))))
  banks=np.array(banks);dist=((pts[:,None,None]-banks[None])**2).sum(3)
  errors=dist.min(2).T
  new=np.argmin(hist@errors.T,axis=1)
  print(iteration,float((hist*errors[new]).sum()/w.sum()),flush=True)
  if np.array_equal(new,groups):break
  groups=new
lookup=np.argmin(dist,axis=2).T+1
packed=[];index={};ids=[];attrs=[];err=[];retained=0
for i,t in enumerate(a):
 bank=int(groups[i]);v=np.where(t>0,lookup[bank,np.maximum(t.astype(int)-1,0)],0).astype(np.uint8).reshape(8,8)
 if t.tobytes() in preserved:bank,v=preserved[t.tobytes()];retained+=1
 variants=[v,v[:,::-1],v[::-1],v[::-1,::-1]]
 raw=[(x[:,::2]|x[:,1::2]<<4).tobytes() for x in variants];key=min(raw);flip=raw.index(key)
 if key not in index:index[key]=len(packed);packed.append(key)
 ids.append(index[key]);attrs.append((bank<<12)|(flip<<10))
 valid=t>0
 if valid.any():err.extend(abs(pts[t[valid]-1]-banks[bank,v.ravel()[valid]-1]).max(1))
(D/'tiles4.bin').write_bytes(b''.join(packed));np.array(ids,'<u2').tofile(D/'ids4.bin');np.array(attrs,'<u2').tofile(D/'attrs4.bin')
p=np.zeros((12,16),'<u2');p[:,1:]=colors(banks);p.tofile(D/'palette4.bin')
q=np.zeros((12,16),'<u2');q[:,1:]=bank_semantic;q.tofile(D/'semantic4.bin')
report=dict(logical_tiles=len(a),physical_tiles=len(packed),mean_max_channel_error=float(np.mean(err)),max_channel_error=float(np.max(err)),p99_channel_error=float(np.percentile(err,99)))
report.update(approved_tiles_preserved=retained,new_tiles=len(a)-retained,palette_preserved=bool(preserved))
(D/'report4.json').write_text(json.dumps(report,indent=2));print(report)

header=R/f'generated/{prefix}-assets.h'
s=header.read_text()
if f'{prefix}_tiles4' not in s:s=s.replace(f'extern const uint32_t {prefix}_tiles[];',f'extern const uint32_t {prefix}_tiles[],{prefix}_tiles4[];\nextern const uint16_t {prefix}_ids4[],{prefix}_attrs4[],{prefix}_palette4[192];')
if args.mission!=2 and f'{prefix}_semantic4' not in s:s+=f'\nextern const uint16_t {prefix}_semantic4[192];\n'
header.write_text(s)
p=R/f'm1_native/{"assets" if args.mission==1 else f"m{args.mission}_assets"}.S';s=p.read_text()
if args.mission==4:s=s.replace('cart_m4_palette','m4_palette')
for name in ['tiles4','ids4','attrs4','palette4']+(['semantic4'] if args.mission!=2 else []):
 if f'.global {prefix}_{name}\n' not in s:s+=f'.section .rodata.{prefix}_{name},"a",%progbits\n.balign 4\n.global {prefix}_{name}\n{prefix}_{name}:\n.incbin "generated/{prefix}-cartridge/{name}.bin"\n'
if args.mission==4:s=s.replace('m4_palette','cart_m4_palette')
p.write_text(s)
