#!/usr/bin/env python3
"""Authored M3-M6 catalogs. Inputs are scene commands and patch tables, never replay traces."""
import argparse,json,re
from pathlib import Path
import numpy as np
from compile_m1_native_maps import ROOT,G,body,numbers
p=argparse.ArgumentParser();p.add_argument('--mission',type=int,choices=(3,4,5,6),required=True);p.add_argument('--animation-base',action='store_true',help='Recreate the stable pre-wall-repair animation catalog');args=p.parse_args();mission=args.mission;program={3:3,4:2,5:4,6:5}[mission]
# Reuse deterministic tile/transition compiler after defining complete authored maps.
source=(ROOT/'tools/build_m1_cartridge_maps.py').read_text()
source=source.replace("parser.add_argument('--mission',type=int,choices=(1,2),default=1)","parser.add_argument('--mission',type=int,choices=(3,4,5,6),required=True)")
source=source.replace("(mission-1)*102:mission*102",f"{program*102}:{(program+1)*102}")
a=source.index('image_records=');b=source.index('# Clear records',a)
setup='''
dynamic_slots=DYNAMIC_SLOTS
commands=[(numbers(m[1]),int(m[2]),int(m[3]),int(m[4])) for m in re.finditer(r'\\{\\{([^{}]*)\\},(\\d+),(\\d+),(\\d+)\\}',body(s,'scene_commands'))]
descriptors=[numbers(x) for x in re.findall(r'\\{([^{}]*)\\}',body(s,'scene_descriptors'))]
at=descriptors[PROGRAM][0];seen=set();bindings=[];chains=[[] for _ in range(4)];clear_before=set();cleared=set()
def signed(x):return x-65536 if x>=32768 else x
while at not in seen:
 seen.add(at);v,nxt,op,plane=commands[at]
 if op==11:cleared.add(plane)
 if op==0:
  if v[0] in [x[0] for x in bindings]:raise ValueError(('repeated image needs explicit variant',v[0]))
  i=len(bindings);bindings.append((v[0],plane,signed(v[2])*3//4,signed(v[3])*3//4));chains[plane].append(i)
  if plane in cleared:clear_before.add(i);cleared.remove(plane)
 at=nxt
image_records=[scene_records[b[0]] for b in bindings]
images=[canvas(r['file'],r['cells'],r['width'],r['height']) for r in image_records]
patches=[];patch_sources=[]
families=FAMILIES
for f,table,start,stop in families:
 text=(G/f).read_text()
 records=re.findall(r'\\{([^{}]+)\\}',body(text,table))
 for i,m in enumerate(records):
  if not start<=i<stop:continue
  fields=m.split(',');name=fields[0];x,y,w,h,plane=map(int,fields[1:])
  tile=canvas(f,name,w,h)
  for image,(_,layer,ox,oy) in enumerate(bindings):
   if layer!=plane:continue
   px=x
   # M3's upper fortress writes use aliases in the 32-column VRAM ring.
   # $2E9CA0/$2E9F20/$2EA790 address columns 173/184/211, equivalent
   # to 13/24/51 in image29. Treating them as absolute map coordinates
   # places the collapse outside the map and leaves its wall untouched.
   if PROGRAM==3 and table=='siege_patches' and i in (4,6,7) and bindings[image][0]==29:
    px-=160
   patches.append((table,i,px*12+ox,y*12+oy,plane,tile));patch_sources.append((table,i,image))

text=(G/'m5-programs.c').read_text();m=re.search(r'const struct SceneTilePatch m5_turret_death_patch=\\{([^{}]+)\\};',text);fields=m[1].split(',');name=fields[0];x,y,w,h,plane=map(int,fields[1:]);tile=canvas('m5-programs.c',name,w,h)
for image,(_,layer,ox,oy) in enumerate(bindings):
 if layer==plane:
  patches.append(('m5_turret_death_patch',0,x*12+ox,y*12+oy,plane,tile));patch_sources.append(('m5_turret_death_patch',0,image))
'''.replace('DYNAMIC_SLOTS',repr({3:[39,78,79,80,81],4:[26,29,64,65,72],5:[],6:[16,17,18,29]}[mission])).replace('PROGRAM',str(program)).replace('FAMILIES',repr({3:[('siege-programs.c','siege_patches',0,154),('m4-programs.c','m4_patches',0,22)],4:[('urban-programs.c','urban_patches',0,170),('village-programs.c','village_patches',32,44)],5:[('fort-programs.c','fort_patches',0,137)],6:[('airship-programs.c','airship_patches',0,33)]}[mission]))
if args.animation_base:
 setup=setup.replace("if PROGRAM==3 and table=='siege_patches'".replace('PROGRAM',str(program)), "if False and table=='siege_patches'")
source=source[:a]+setup+source[b:]
a=source.index('# Persist already-loaded');b=source.index('tiles=[bytes(64)]',a)
source=source[:a]+'''
# Common canvas offset allows negative authored anchors without clipping pixels.
shiftx=-min(0,min(b[2] for b in bindings));shifty=-min(0,min(b[3] for b in bindings))
origins=[(b[2]+shiftx,b[3]+shifty) for b in bindings]
# A full32-column static source image remains in the512px ring after its
# input ends. Bake one extra period, including the4px boundary phase.
repeat_mask=0
for image,r in enumerate(image_records):
 if r['width']==32 and not any(bindings[z[2]][1]==bindings[image][1] for z in patch_sources):
  repeat_mask|=1<<image
  images[image]=np.tile(images[image],(1,2))
for chain in chains:
 previous=np.zeros((0,0),np.uint8)
 for image in chain:
  if image in clear_before:previous=np.zeros((0,0),np.uint8)
  x,y=origins[image];a=images[image]
  joined=np.zeros((max(previous.shape[0],y+a.shape[0]),max(previous.shape[1],x+a.shape[1])),np.uint8)
  joined[:previous.shape[0],:previous.shape[1]]=previous
  joined[y:y+a.shape[0],x:x+a.shape[1]]=a
  images[image]=joined;previous=joined
patches=[(t,i,x+shiftx,y+shifty,p,a) for t,i,x,y,p,a in patches]
'''+source[b:]
# Scope closure to this plane's potential patches; other images cannot receive them.
source=source.replace('for xx,yy,pw,ph,plane,pi in patchmaps:', 'for patchno,(xx,yy,pw,ph,plane,pi) in enumerate(patchmaps):\n    if patch_sources[patchno][2]!=image:continue')
# Also include older same-plane variants to retain destruction across rebindings.
source=source.replace('if patch_sources[patchno][2]!=image:continue','if bindings[patch_sources[patchno][2]][1]!=bindings[image][1]:continue')
a=source.index("header='''");b=source.index('err=abs(',a)
source=source[:a]+'''
header=f"#ifndef M{mission}_ASSETS_H\\n#define M{mission}_ASSETS_H\\n#include \\\"m1-assets.h\\\"\\n#include \\\"../src/scene_video.h\\\"\\n"
header+=f"extern const struct M1Image {prefix}_images[{len(images)}];\\nextern const struct M1Patch {prefix}_patches[];\\nextern const struct M1Op {prefix}_ops[];\\nextern const uint32_t {prefix}_tiles[],{prefix}_tiles4[];\\nextern const uint8_t {prefix}_opacity[];\\nextern const uint16_t {prefix}_maps[],{prefix}_patchmaps[],{prefix}_transitions[][2],{prefix}_palette[192],{prefix}_ids4[],{prefix}_attrs4[],{prefix}_palette4[192];\\n"
header+=f"enum {{M{mission}_TILE_COUNT={len(tiles)},M{mission}_PATCH_COUNT={len(patches)},M{mission}_IMAGE_COUNT={len(images)},M{mission}_REPEAT_X_MASK={repeat_mask}}};\\n#endif\\n"
(G/f'{prefix}-assets.h').write_text(header)
c=f'#include "{prefix}-assets.h"\\n'
for typ,name,rs in [('M1Image','images',maprows),('M1Patch','patches',patchrows),('M1Op','ops',oprows)]:c+=f'const struct {typ} {prefix}_{name}[]={{'+','.join('{'+','.join(map(str,r))+'}' for r in rs)+'};\\n'
(G/f'{prefix}-assets.c').write_text(c)
asm=''
for name,file in [('opacity','opacity'),('tiles','tiles'),('maps','maps'),('patchmaps','patches'),('transitions','transitions'),('palette','palette'),('tiles4','tiles4'),('ids4','ids4'),('attrs4','attrs4'),('palette4','palette4')]:asm+=f'.section .rodata.{prefix}_{name},"a",%progbits\\n.balign 4\\n.global {prefix}_{name}\\n{prefix}_{name}:\\n.incbin "generated/{prefix}-cartridge/{file}.bin"\\n'
(ROOT/f'm1_native/{prefix}_assets.S').write_text(asm)
(O/'bindings.json').write_text(json.dumps(dict(program=PROGRAM,images=bindings,patches=patch_sources,clear_before=sorted(clear_before),repeat_x_mask=repeat_mask),indent=2))
'''.replace('DYNAMIC_SLOTS',repr({3:[39,78,79,80,81],4:[26,29,64,65,72],5:[],6:[16,17,18,29]}[mission])).replace('PROGRAM',str(program))+source[b:]
if args.animation_base:
 source=source.replace('images[image]=np.tile(images[image],(1,2))','images[image]=images[image]')
source=source.replace('mission=parser.parse_args().mission', "parser.add_argument('--animation-base',action='store_true');mission=parser.parse_args().mission")
exec(compile(source,str(ROOT/'tools/build_m1_cartridge_maps.py'),'exec'),{'__name__':'__main__'})

if mission==4:
 for n in ["m4-assets.h","m4-assets.c"]:
  f=G/n;f.write_text(f.read_text().replace("m4_patches","cart_m4_patches").replace("m4_palette","cart_m4_palette"))
 f=ROOT/"m1_native/m4_assets.S";f.write_text(f.read_text().replace("m4_palette","cart_m4_palette"))

(G/f"m{mission}-cartridge/assets-unpacked.c").write_text((G/f"m{mission}-assets.c").read_text())
