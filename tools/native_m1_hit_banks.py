"""M1 boss palettes share one hit signal; quantize both source states together."""
from pathlib import Path
import numpy as np
from prepare_native_background import rgb,colors,centers,body,nums

def hit_tables():
 g=Path(__file__).resolve().parents[1]/'generated'
 p=np.array(nums(body((g/'drawing-data.c').read_text(),'native_source_palettes').replace('{','').replace('}','')),np.uint16).reshape(-1,16)&32767
 t=np.array(nums(body((g/'script151-programs.c').read_text(),'script151_palettes').replace('{','').replace('}',''))).reshape(2,9,2)
 lookup={};canonical={};hit={}
 for i in range(9):
  slot=int(t[0,i,0])
  for ix in range(1,16):
   normal,flash=map(int,p[1024+t[:,i,1],ix]);tag=slot*16+ix
   token=normal if normal==flash else canonical.setdefault((normal,flash),32768+tag)
   lookup[tag]=token;hit[tag]=flash
 return lookup,hit

def compile_banks(hist,pts,semantic,groups):
 _,hit=hit_tables();domain=semantic[1:]!=0
 flash=pts.copy()
 for i,t in enumerate(semantic[1:]):
  if t:flash[i]=rgb(np.array(hit[int(t)]))
 features=np.concatenate((pts,flash),axis=1);previous=None
 for iteration in range(30):
  banks=[];tags=[];features_out=[]
  for b in range(12):
   weights=hist[groups==b].sum(0);active=np.unique(domain[weights>0])
   if not len(active):
    banks.append(previous[0][b]);tags.append(previous[1][b]);features_out.append(previous[2][b]);continue
   counts={bool(d):1 for d in active}
   for k in range(15-len(active)):
    choices=[bool(d) for d in active if counts[bool(d)]<np.count_nonzero((domain==d)&(weights>0))]
    if not choices:break
    d=max(choices,key=lambda d:np.sqrt(weights[domain==d].sum())/counts[d]);counts[d]+=1
   row=[];tagrow=[];frow=[]
   for d in active:
    ix=np.flatnonzero((domain==d)&(weights>0));samples=features[ix];reps=centers(samples,weights[ix],counts[bool(d)],20)
    for rep in reps:
     if d:
      i=ix[np.argmin(((samples-rep)**2).sum(1))];row.append(pts[i]);tagrow.append(semantic[i+1]);frow.append(features[i])
     else:
      v=rgb(colors(rep[:3]));row.append(v);tagrow.append(0);frow.append(np.tile(v,2))
   while len(row)<15:row.append(row[-1]);tagrow.append(tagrow[-1]);frow.append(frow[-1])
   banks.append(row);tags.append(tagrow);features_out.append(frow)
  banks=np.array(banks);tags=np.array(tags,np.uint16);features_out=np.array(features_out)
  dist=((features[:,None,None]-features_out[None])**2).sum(3)
  dist=np.where(domain[:,None,None]==(tags[None]!=0),dist,1e12)
  new=np.argmin(hist@dist.min(2),axis=1);previous=banks,tags,features_out
  if np.array_equal(new,groups):break
  groups=new
 return groups,banks,dist,tags
