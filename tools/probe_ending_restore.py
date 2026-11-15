"""Reconstruct source-scaled ending canvases and measure lossless 4bpp limits."""
from pathlib import Path
import json,re,zlib,sys
import numpy as np
from PIL import Image
from compile_m1_native_maps import body,numbers
R=Path(__file__).resolve().parents[1];G=R/'generated';P=G/'native-background';O=R/'validation-ending-restore';O.mkdir(exist_ok=True)
s=(G/'scene-programs.c').read_text();meta=(G/'native-bg-assets.c').read_text();manifest=json.loads((P/'manifest.json').read_text());rows=[r for r in manifest['rows'] if r['file']=='scene-programs.c'];regions=[numbers(x)for x in re.findall(r'\{([^{}]+)\}',body(meta,'native_bg_regions'))];variants=[numbers(x)for x in re.findall(r'\{([^{}]+)\}',body(meta,'native_bg_variants'))];rids=[i for i,r in enumerate(manifest['rows'])if r['file']=='scene-programs.c']
profiles=np.fromfile(P/'profiles.u16','<u2').reshape(14,256);original=np.array(numbers(body((G/'drawing-data.c').read_text(),'native_source_palettes')),np.uint16).reshape(-1,16)
raw=np.fromfile(G/'native-scaled-background-12x12.4bpp','u1').reshape(-1,12,6);px=np.empty((len(raw),12,12),np.uint16);px[:,:,::2]=raw&15;px[:,:,1::2]=raw>>4
raw=np.fromfile(P/'tiles.4bpp','u1').reshape(-1,32);tiles=np.empty((len(raw),64),np.uint8);tiles[:,::2]=raw&15;tiles[:,1::2]=raw>>4
banks=np.fromfile(P/'tile-banks.u8','u1');pal=np.array(numbers(body((G/'ending-background-palette.inc').read_text(),'ending_background_palette')),np.uint16).reshape(14,12,16)
raw=np.fromfile(P/'maps.u24','u1').reshape(-1,3);maps=raw[:,0].astype('u4')|raw[:,1].astype('u4')<<8|raw[:,2].astype('u4')<<16
report=[]
def rgba(rgb,opaque):
 out=np.empty(rgb.shape+(4,),np.uint8)
 for k in range(3):out[:,:,k]=((rgb>>(5*k))&31)*255//31
 out[:,:,3]=opaque.astype('u1')*255
 return out
for image,p in [(0,0),(15,1),(34,4),(37,5),(39,5),(41,5),(50,12)]:
 row=rows[image];w,h=row['width'],row['height'];cells=np.array(numbers(body(s,row['cells'])),np.uint16).reshape(w,h,2);rgb=np.zeros((h*12,w*12),np.uint16);opaque=np.zeros_like(rgb,bool)
 for x in range(w):
  for y in range(h):
   t,a=map(int,cells[x,y]);t|=((a>>4)&15)<<16;v=px[t,::-1 if a&2 else 1,::-1 if a&1 else 1];source=int(profiles[p,a>>8]);assert source!=65535
   rgb[y*12:y*12+12,x*12:x*12+12]=original[source,v];opaque[y*12:y*12+12,x*12:x*12+12]=v!=0
 q=np.lib.stride_tricks.sliding_window_view(rgb,(8,8))[::4,::4].reshape(-1,64);mask=np.lib.stride_tricks.sliding_window_view(opaque,(8,8))[::4,::4].reshape(-1,64)
 first=variants[regions[rids[image]][1]][0];ids=maps[first:first+len(q)];dec=pal[p,banks[ids,None],tiles[ids]]
 counts=np.array([len(np.unique(v[m])) for v,m in zip(q,mask)])
 # Exact local bank/index representation: for <=15 colours each tile can be decoded exactly.
 packed=[];sets=set();too_many=0
 for v,m in zip(q,mask):
  colors=np.unique(v[m]);sets.add(tuple(map(int,colors)))
  if len(colors)>15:too_many+=1;continue
  indices=np.zeros(64,np.uint8);indices[m]=np.searchsorted(colors,v[m])+1
  restored=np.zeros(64,np.uint16);restored[m]=colors[indices[m]-1];assert np.array_equal(restored[m],v[m])
  packed.append((indices[::2]|indices[1::2]<<4).tobytes())
 # Greedy union supplies an upper bound, NOT the minimum palette-bank count.
 groups=[]
 for colors in sorted(sets,key=lambda a:(-len(a),a)):
  if len(colors)>15:continue
  c=set(colors);fits=[(len(g|c),i)for i,g in enumerate(groups)if len(g|c)<=15]
  if fits:groups[min(fits)[1]]|=c
  else:groups.append(c)
 Image.fromarray(rgba(rgb,opaque)).save(O/f'image-{image:02d}-source-restored.png')
 # Show a same-coordinate source window and its GBA tile colours, enlarged nearest.
 ww=w*3-1;hh=h*3-1;old=np.zeros_like(rgb);om=np.zeros_like(opaque)
 for y in range(hh):
  for x in range(ww):
   at=y*ww+x;old[y*4:y*4+8,x*4:x*4+8]=dec[at].reshape(8,8);om[y*4:y*4+8,x*4:x*4+8]=(tiles[ids[at]]!=0).reshape(8,8)
 canvas=Image.new('RGBA',(min(w*12,384)*2,min(h*12,192)),(40,40,40,255));crop=(0,0,min(w*12,384),min(h*12,192));canvas.paste(Image.fromarray(rgba(old,om)).crop(crop),(0,0));canvas.paste(Image.fromarray(rgba(rgb,opaque)).crop(crop),(crop[2],0));canvas.resize((canvas.width*2,canvas.height*2),Image.Resampling.NEAREST).save(O/f'image-{image:02d}-before-restored.png')
 report.append(dict(image=image,profile=p,windows=len(q),opaque_pixels=int(mask.sum()),mismatching_opaque_samples=int(((q!=dec)&mask).sum()),max_colors=int(counts.max()),windows_over_15=int(too_many),exact_local_tiles=len(packed),unique_local_palettes=len(sets),greedy_bank_upper_bound=len(groups),local_tiles_zlib_bytes=len(zlib.compress(b''.join(packed),9)),source_rgb555_alpha_zlib_bytes=len(zlib.compress(rgb.astype('<u2').tobytes()+np.packbits(opaque).tobytes(),9))))
(O/'report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
