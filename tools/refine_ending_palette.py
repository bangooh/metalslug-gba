"""Ending-only palette refinement; preserves shared tile indices and all gameplay palettes."""
from pathlib import Path
import sys,json,re,numpy as np
from compile_m1_native_maps import body,numbers
G=Path(__file__).resolve().parents[1]/'generated';P=G/'native-background'
m=json.loads((P/'manifest.json').read_text());s=(G/'scene-programs.c').read_text();meta=(G/'native-bg-assets.c').read_text()
regions=[numbers(x)for x in re.findall(r'\{([^{}]+)\}',body(meta,'native_bg_regions'))];variants=[numbers(x)for x in re.findall(r'\{([^{}]+)\}',body(meta,'native_bg_variants'))]
profiles=np.fromfile(P/'profiles.u16','<u2').reshape(14,256);orig=np.array(numbers(body((G/'drawing-data.c').read_text(),'native_source_palettes')),np.uint16).reshape(-1,16)
pairs=np.array(numbers(body(s,'scene_palette_pairs')),np.uint16).reshape(-1,2);cmd=[(numbers(a),int(b),int(c),int(d))for a,b,c,d in re.findall(r'\{\{([^{}]*)\},(\d+),(\d+),(\d+)\}',body(s,'scene_commands'))];desc=[numbers(x)for x in re.findall(r'\{([^{}]+)\}',body(s,'scene_descriptors'))]
selected=set()
for entry in [14,15]:
 state=np.full(256,65535,np.uint16);seen=set();at=desc[entry][0];profile=12
 while at not in seen and at<len(cmd):
  seen.add(at);v,n,op,plane=cmd[at]
  if op==10:
   for slot,pal in pairs[v[0]:v[0]+v[1]]:state[int(slot)&255]=65535 if pal==65535 else int(pal)+1024
   scores=[sum(16 if j<20 else 1 for j in range(16,118)if state[j]!=65535 and state[j]==p[j])for p in profiles];profile=int(np.argmax(scores))
  if op==0:selected.add((profile,v[0]))
  at=n
raw=np.fromfile(G/'native-scaled-background-12x12.4bpp','u1').reshape(-1,12,6);px=np.empty((len(raw),12,12),np.uint16);px[:,:,::2]=raw&15;px[:,:,1::2]=raw>>4
raw=np.fromfile(P/'tiles.4bpp','u1').reshape(-1,32);tiles=np.empty((len(raw),64),np.uint8);tiles[:,::2]=raw&15;tiles[:,1::2]=raw>>4
banks=np.fromfile(P/'tile-banks.u8','u1');pal=np.fromfile(P/'palettes.rgb555','<u2').reshape(14,12,16);new=pal.copy();raw=np.fromfile(P/'maps.u24','u1').reshape(-1,3);maps=raw[:,0].astype('u4')|raw[:,1].astype('u4')<<8|raw[:,2].astype('u4')<<16
sceneids=[i for i,r in enumerate(m['rows'])if r['file']=='scene-programs.c'];hist={}
for p,image in sorted(selected):
 if p==12:continue
 rid=sceneids[image];row=m['rows'][rid];w,h=row['width'],row['height'];cells=np.array(numbers(body(s,row['cells'])),np.uint16).reshape(w,h,2);im=np.zeros((h*12,w*12),np.uint16)
 for x in range(w):
  for y in range(h):
   tile,a=map(int,cells[x,y]);tile|=((a>>4)&15)<<16;v=px[tile,::-1 if a&2 else 1,::-1 if a&1 else 1];im[y*12:y*12+12,x*12:x*12+12]=np.where(v,(a>>8)*16+v,0)
 q=np.lib.stride_tricks.sliding_window_view(im,(8,8))[::4,::4].reshape(-1,64);lookup=np.zeros(4096,np.uint16)
 for slot,n in enumerate(profiles[p]):
  if n!=65535:lookup[slot*16:slot*16+16]=orig[n]
 first=variants[regions[rid][1]][0];ids=maps[first:first+len(q)];keys=banks[ids,None].astype('u4')*16+tiles[ids];valid=q!=0
 hh=hist.setdefault(p,np.zeros((192,32768),np.uint32));packed=(keys[valid]<<15)|lookup[q[valid]];u,c=np.unique(packed,return_counts=True);hh.reshape(-1)[u]+=c.astype('u4')
def rgb(c):return np.array([c&31,c>>5&31,c>>10&31],float).T
report=[]
for p,h in hist.items():
 before=after=0
 for k,counts in enumerate(h):
  if k%16==0:continue
  ids=np.flatnonzero(counts)
  if not len(ids):continue
  weights=counts[ids];pts=rgb(ids);mean=np.average(pts,axis=0,weights=weights);old=int(pal[p].flat[k]);candidates=np.append(ids,old);best=int(candidates[np.argmin(((rgb(candidates)-mean)**2).sum(1))]);new[p].flat[k]=best
  before+=float((((pts-rgb(old))**2).sum(1)*weights).sum());after+=float((((pts-rgb(best))**2).sum(1)*weights).sum())
 report.append(dict(profile=p,before=before,after=after,reduction_percent=round(100*(1-after/before),2)if before else 0,changed=int((new[p]!=pal[p]).sum())))
assert np.array_equal(new[12],pal[12])
(G/'ending-background-palette.inc').write_text('/* Ending-only fitted source colours; shared textures unchanged. */\nstatic const uint16_t ending_background_palette[14][12][16]={\n'+',\n'.join('{'+','.join('{'+','.join(map(str,bank))+'}'for bank in profile)+'}'for profile in new)+'\n};\n')
print(json.dumps(dict(images=sorted(selected),profiles=report),indent=2))
