#!/usr/bin/env python3
"""M1 ROM maps plus closed destruction transitions; no gameplay-route inputs."""
import sys, json, re, struct, argparse
from pathlib import Path
from collections import defaultdict, deque
import numpy as np
from compile_m1_native_maps import ROOT, G, body, numbers
from prepare_native_background import rgb, colors, centers
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--mission',type=int,choices=(1,2),default=1)
mission=parser.parse_args().mission
dynamic_slots=set(range(0x5b,0x61))|set(range(0x72,0x75)) if mission==1 else set()
prefix=f'm{mission}'
O=G/f'{prefix}-cartridge'; O.mkdir(exist_ok=True)
s=(G/'scene-programs.c').read_text()
pairs=np.array(numbers(body(s,'scene_palette_pairs'))).reshape(-1,2)
bind=np.full(256,-1,int)
for slot,pal,flags in np.array(numbers(body((G/'session-data.c').read_text(),'session_palettes'))).reshape(-1,3):bind[slot]=pal
for slot,pal in pairs[(mission-1)*102:mission*102]:bind[slot&255]=-1 if pal==65535 else 1024+pal
from native_m1_hit_banks import hit_tables
hit_lookup,_=hit_tables()
palettes=np.array(numbers(body((G/'drawing-data.c').read_text(),'native_source_palettes')),np.uint16).reshape(-1,16)
raw=np.fromfile(G/'native-scaled-background-12x12.4bpp',np.uint8).reshape(-1,12,6)
pix=np.empty((len(raw),12,12),np.uint8);pix[:,:,::2]=raw&15;pix[:,:,1::2]=raw>>4
ledger=json.loads((G/'tile-video-source.json').read_text())['records']
def canvas(file,name,w,h):
 a=np.array(numbers(body((G/file).read_text(),name)),np.uint32).reshape(w,h,2);out=np.full((h*12,w*12),-1,np.int32)
 for x in range(w):
  for y in range(h):
   t,at=map(int,a[x,y]);p=pix[t+((at>>4)&15)*65536,::-1 if at&2 else 1,::-1 if at&1 else 1];b=bind[at>>8]
   if np.any(p):
    if b<0: raise ValueError(('unbound',file,name,at>>8))
    values=palettes[b,p].astype(np.int32)&32767
    if (at>>8) in globals().get('dynamic_slots',()):values=np.array([0]+[hit_lookup[(at>>8)*16+i] if mission==1 else 32768+(at>>8)*16+i for i in range(1,16)])[p]
    out[y*12:y*12+12,x*12:x*12+12]=np.where(p,values,-1)
 return out
scene_records=[r for r in ledger if r['file']=='scene-programs.c']
image_records=scene_records[slice(0,15) if mission==1 else slice(15,21)]
if mission==2:image_records.append(scene_records[52]) # $06896A/$0689E6 arena relink
images=[canvas(r['file'],r['cells'],r['width'],r['height']) for r in image_records]
patches=[]
families=[('opening-programs.c','opening_patches'),('script151-programs.c','script151_patches')] if mission==1 else [('village-programs.c','village_patches'),('boss-programs.c','boss2_background')]
for f,table in families:
 text=(G/f).read_text()
 for i,m in enumerate(re.findall(r'\{([^{}]+)\}',body(text,table))):
  # M2's authored placement list has walls/houses/tower (0..31), no gates.
  # Gate records32..43 and all urban records belong to other missions.
  if mission==2 and table=='village_patches' and i>=32:continue
  fields=m.split(',');name=fields[0];values=list(map(int,fields[1:]));
  if table=='boss2_background':values.append(0)
  x,y,w,h,plane=values;patches.append((table,i,x*12,y*12,plane,canvas(f,name,w,h)))
for i,(x,y,w,h,plane) in enumerate([(0,0,3,10,0),(0,0,8,10,0)] if mission==1 else []):
 patches.append(('clear',i,x*12,y*12,plane,np.full((h*12,w*12),-1,np.int32)))
# Clear records are authored operations, compiled as transparent rectangles.
print('images',len(images),'patches',len(patches),flush=True)
allc=np.concatenate([a[a>=0] for a in images]+[p[-1][p[-1]>=0] for p in patches]);cs,ws=np.unique(allc,return_counts=True)
dynamic=cs[cs>=32768];static=cs[cs<32768];static_weights=ws[cs<32768]
if len(static)>191-len(dynamic): target=np.unique(colors(centers(rgb(static),static_weights,191-len(dynamic),30)))
else:target=static
nearest=np.argmin(((rgb(static)[:,None]-rgb(target)[None])**2).sum(2),axis=1)
lut=np.zeros(36864,np.uint8);lut[static]=nearest+1
palette=np.zeros(192,dtype='<u2');palette[1:len(target)+1]=target
semantic=np.zeros(192,dtype='<u2')
for j,token in enumerate(dynamic,len(target)+1):
 slot,index=divmod(int(token)-32768,16);lut[token]=j;palette[j]=palettes[bind[slot],index]&32767;semantic[j]=slot*16+index
(O/'palette.bin').write_bytes(palette.tobytes());(O/'semantic.bin').write_bytes(semantic.tobytes())
def indexed(a):return np.where(a<0,0,lut[np.maximum(a,0)]).astype(np.uint8)
images=[indexed(a) for a in images];patches=[(*p[:-1],indexed(p[-1])) for p in patches]
# Persist already-loaded terrain across map rebinding. These anchors are
# authored SCOP_BIND operands, not values sampled from a playthrough.
anchors=([(0,128),(656,352),(0,48),(1920,128),(1856,352),(1920,352),(2752,336),(2784,240),(2912,352),(3024,48),(3392,0),(3328,32),(3776,16),(3776,224),(1520,32)] if mission==1 else [(0,320),(0,320),(1008,320),(2928,192),(3200,32),(4016,256)])
origins=[(0,0)]*len(images)
for chain in (([0,3,10,12],[1,4,5,7,8,11,13],[6,9],[2,14]) if mission==1 else ([0,4],[1,3],[2],[5])):
 previous=np.zeros((0,0),np.uint8)
 for image in chain:
  x,y=(v*3//4 for v in anchors[image]);a=images[image]
  joined=np.zeros((max(previous.shape[0],y+a.shape[0]),max(previous.shape[1],x+a.shape[1])),np.uint8)
  joined[:previous.shape[0],:previous.shape[1]]=previous
  if mission==2 and image==3:
   # SCOP_BIND/DRAW at2544 then BIND/DIRTY at2928. Both copies
   # persist in the source ring map; the second bind must not erase the first.
   first_x=2544*3//4
   joined[y:y+a.shape[0],first_x:first_x+a.shape[1]]=a
  joined[y:y+a.shape[0],x:x+a.shape[1]]=a
  images[image]=joined;previous=joined;origins[image]=(x,y)
if mission==2:
 # The boss's separate20-column image repeats every320 source pixels.
 # $0689E6 first binds it at4096 (3776+320), preserving the image19
 # columns on the left. $06896A subsequently advances by320 each wrap.
 # Compile that transition and three periods; runtime only selects an
 # origin, retaining no source-cell ring or pixel compositor.
 assert images[6].shape==(192,240)
 a=np.tile(images[6],(1,3));previous=images[4]
 joined=np.zeros((max(previous.shape[0],12+a.shape[0]),max(previous.shape[1],3072+a.shape[1])),np.uint8)
 joined[:previous.shape[0],:previous.shape[1]]=previous
 joined[12:12+a.shape[0],3072:3072+a.shape[1]]=a
 images[6]=joined;origins[6]=(3072,12)
# Opening destruction is authored in the village map3 (world x=1920),
# then the raised map7; script151 is authored in map12. Map0 ends where
# map3 begins and contains none of these village destruction cells.
# Using map0 silently applied the changes 1440 GBA pixels behind the player.
patches=[(table,i,x+origins[(3 if n<70 else 7 if n<76 else 12) if mission==1 else 0][0],y+origins[(3 if n<70 else 7 if n<76 else 12) if mission==1 else 0][1],plane,a) for n,(table,i,x,y,plane,a) in enumerate(patches)]
tiles=[bytes(64)];tileindex={tiles[0]:0}
def intern(a):
 key=a.tobytes() if hasattr(a,'tobytes') else a
 if key not in tileindex:tileindex[key]=len(tiles);tiles.append(key)
 return tileindex[key]
ops=[];opindex={};patchmaps=[];trans=defaultdict(dict)
# Mask distinguishes transparent replacement from 'leave unchanged'.
for table,i,x,y,plane,a in patches:
 x0=x//8;y0=y//8;x1=(x+a.shape[1]+7)//8;y1=(y+a.shape[0]+7)//8;ids=[]
 for ty in range(y0,y1):
  for tx in range(x0,x1):
   mask=np.zeros((8,8),bool);v=np.zeros((8,8),np.uint8)
   for py in range(8):
    for px in range(8):
     xx=tx*8+px-x;yy=ty*8+py-y
     if 0<=xx<a.shape[1] and 0<=yy<a.shape[0]:mask[py,px]=True;v[py,px]=a[yy,xx]
   key=mask.tobytes()+v.tobytes()
   if key not in opindex:opindex[key]=len(ops);ops.append((mask,v))
   ids.append(opindex[key])
 patchmaps.append((x0,y0,x1-x0,y1-y0,plane,ids))
maps=[]
for image,a in enumerate(images):
 h=(a.shape[0]+7)//8;w=(a.shape[1]+7)//8;pad=np.zeros((h*8,w*8),np.uint8);pad[:a.shape[0],:a.shape[1]]=a;ids=[]
 for y in range(h):
  for x in range(w):
   base=intern(pad[y*8:y*8+8,x*8:x*8+8]);ids.append(base)
   local=[]
   for xx,yy,pw,ph,plane,pi in patchmaps:
    if xx<=x<xx+pw and yy<=y<yy+ph:local.append(pi[(y-yy)*pw+x-xx])
   local=set(local);seen={base};q=deque([base])
   while q:
    old=q.popleft();v=np.frombuffer(tiles[old],np.uint8).reshape(8,8)
    for op in local:
     mask,val=ops[op];new=intern(np.where(mask,val,v));trans[op][old]=new
     if new not in seen:seen.add(new);q.append(new)
    if len(seen)>4096:raise ValueError(('state explosion',image,x,y,len(seen)))
 maps.append((w,h,ids));print('image',image,'tiles',len(tiles),flush=True)
assert len(tiles)<65535
# Compact ROM lookup tables sorted by old tile ID.
mapdata=[];maprows=[]
for image,(w,h,ids) in enumerate(maps):maprows.append((len(mapdata),w,h,*origins[image]));mapdata+=ids
patchdata=[];patchrows=[]
for x,y,w,h,plane,ids in patchmaps:patchrows.append((len(patchdata),x,y,w,h,plane));patchdata+=ids
trdata=[];oprows=[]
for op in range(len(ops)):
 mask,val=ops[op]
 if mask.all():oprows.append((0,0,intern(val)))
 else:
  row=sorted(trans[op].items());oprows.append((len(trdata),len(row),65535));trdata+=row
(O/'opacity.bin').write_bytes(np.packbits(np.frombuffer(b''.join(tiles),np.uint8).reshape(-1,8)>0,axis=1,bitorder='little').tobytes());(O/'tiles.bin').write_bytes(b''.join(tiles));(O/'maps.bin').write_bytes(np.array(mapdata,dtype='<u2').tobytes());(O/'patches.bin').write_bytes(np.array(patchdata,dtype='<u2').tobytes());(O/'transitions.bin').write_bytes(np.array(trdata,dtype='<u2').tobytes())
header='''#ifndef M1_ASSETS_H
#define M1_ASSETS_H
#include <stdint.h>
struct M1Image {uint32_t offset;uint16_t width,height;int16_t origin_x,origin_y;};
struct M1Patch {uint32_t offset;int16_t x,y;uint16_t width,height,plane;};
struct M1Op {uint32_t offset;uint16_t count,replace;};
extern const struct M1Image m1_images[15];
extern const struct M1Patch m1_patches[];
extern const struct M1Op m1_ops[];
extern const uint32_t m1_tiles[];
extern const uint8_t m1_opacity[];
extern const uint16_t m1_maps[],m1_patchmaps[],m1_transitions[][2],m1_palette[192];
'''+f'enum {{M1_TILE_COUNT={len(tiles)},M1_PATCH_COUNT={len(patches)}}};\n#endif\n'
if mission==2:
 header='#ifndef M2_ASSETS_H\n#define M2_ASSETS_H\n#include "m1-assets.h"\n'+header[header.index('extern const struct M1Image'):]
 header=header.replace('m1_','m2_').replace('m2_images[15]','m2_images[7]').replace('M1_TILE_COUNT','M2_TILE_COUNT').replace('M1_PATCH_COUNT','M2_PATCH_COUNT')
(G/f'{prefix}-assets.h').write_text(header)
c='#include "m1-assets.h"\n'
for typ,name,rows in [('M1Image','m1_images',maprows),('M1Patch','m1_patches',patchrows),('M1Op','m1_ops',oprows)]:c+=f'const struct {typ} {name}[]={{'+','.join('{'+','.join(map(str,r))+'}' for r in rows)+'};\n'
if mission==2:c=c.replace('m1-','m2-').replace('m1_','m2_')
(G/f'{prefix}-assets.c').write_text(c)
asm=''
for name,file in [('m1_opacity','opacity'),('m1_tiles','tiles'),('m1_maps','maps'),('m1_patchmaps','patches'),('m1_transitions','transitions'),('m1_palette','palette')]:asm+=f'.section .rodata.{name},"a",%progbits\n.balign 4\n.global {name}\n{name}:\n.incbin "generated/m1-cartridge/{file}.bin"\n'
if mission==2:asm=asm.replace('m1_','m2_').replace('m1-cartridge','m2-cartridge')
(ROOT/f'm1_native/{"assets" if mission==1 else "m2_assets"}.S').write_text(asm)
err=abs(rgb(static)-rgb(target[nearest]));report=dict(tiles=len(tiles),patches=len(patches),operations=len(ops),transitions=len(trdata),source_colors=len(cs),output_colors=len(target),max_channel_error=int(err.max()),weighted_mean_channel_error=float(np.average(err.mean(1),weights=static_weights)),status='compiled; integration and visual validation pending')
(O/'report.json').write_text(json.dumps(report,indent=2));print(report)
