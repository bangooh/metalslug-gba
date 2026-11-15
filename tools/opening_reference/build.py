"""Compile original cinematic display trace to native GBA replay tables.
Pixels are referenced from the existing lossless C-ROM inventory, not duplicated.
"""
import sys,json,struct,hashlib
from pathlib import Path
import numpy as np
sys.path.insert(0,'upstream/assets/tools');import sprites as S
p=S.PRom();tiles=S.Tiles();t=json.load(open('opening-work/trace.json'));frames=t['frames'];g=Path('src/metal-slug-native/generated')
mapids={};maps=[];cells=[];tileids={};draws=[];pals=[];palids={};headers=[];palrefs=[]
def palid(colors,fade):
 level,step,white=fade
 colors=[[c+((31-c)*level//31) if white else c-c*level//31 for c in row] for row in colors]
 colors[0]=[0,0,0];v=tuple(r|gg<<5|b<<10 for r,gg,b in colors)
 if v not in palids:palids[v]=len(pals);pals.append(v)
 return palids[v]
for f in frames[::2]:
 slots={};banks={}
 for d in f['draws']:
  pid=palid(f['palettes'][str(d['slot'])],f['fade'])
  if pid not in banks:banks[pid]=len(banks)
  slots[d['slot']]=banks[pid]
 assert len(banks)<=16
 headers.append((len(draws),len(f['draws']),len(palrefs),len(banks)));palrefs+=list(banks)
 for d in sorted(f['draws'],key=lambda x:x['priority']):
  mp=d['map']
  if mp not in mapids:
   mapids[mp]=len(maps);start=len(cells)
   for x,y,tid,_,_ in S.compose(p,tiles,[(mp,8,0,0)])[0]:
    if not np.any(tiles.get(tid)):continue
    if tid not in tileids:tileids[tid]=len(tileids)
    cells.append((x*3//4,y*3//4,tileids[tid]))
   maps.append((start,len(cells)-start))
  x=d['x'] if d['x']<32768 else d['x']-65536;y=d['y'] if d['y']<32768 else d['y']-65536
  draws.append((mapids[mp],x*3//4,(512-y-16)*3//4-4,d['sx'],d['sy'],d['flags'],slots[d['slot']]))
assert len(tileids)*72+8192<185228
h='''#ifndef OPENING_REPLAY_DATA_H
#define OPENING_REPLAY_DATA_H
#include <stdint.h>
struct OpeningCell {int16_t x,y;uint16_t tile;};
struct OpeningMap {uint16_t first,count;};
struct OpeningDraw {uint16_t map;int16_t x,y;uint8_t sx,sy,flags,bank;};
struct OpeningFrame {uint16_t first,count,pal_first,pal_count;};
struct OpeningSound {uint16_t frame,command;uint8_t stop;};
'''+f'enum {{OPENING_TILE_COUNT={len(tileids)},OPENING_FRAME_COUNT={len(headers)},OPENING_SOUND_COUNT={len(t["sounds"])}}};\n'
for typ,name in [('uint32_t','opening_tiles'),('struct OpeningCell','opening_cells'),('struct OpeningMap','opening_maps'),('struct OpeningDraw','opening_draws'),('struct OpeningFrame','opening_frames'),('uint16_t','opening_palrefs'),('struct OpeningSound','opening_sounds')]:h+=f'extern const {typ} {name}[];\n'
h+='extern const uint16_t opening_backdrops[];\nextern const uint16_t opening_palettes[][16];\n#endif\n';(g/'opening-replay-data.h').write_text(h)
def rows(x):return ',\n'.join('{'+','.join(map(str,r))+'}' for r in x)
c='#include "opening-replay-data.h"\n'
def backdrop(f):
 level,_,white=f['fade'];rgb=[v+(31-v)*level//31 if white else v-v*level//31 for v in f['backdrop']]
 return rgb[0]|rgb[1]<<5|rgb[2]<<10
c+='const uint16_t opening_backdrops[]={'+','.join(str(backdrop(f)) for f in frames[::2])+'};\n'
for typ,name,v in [('struct OpeningCell','opening_cells',cells),('struct OpeningMap','opening_maps',maps),('struct OpeningDraw','opening_draws',draws),('struct OpeningFrame','opening_frames',headers),('struct OpeningSound','opening_sounds',[(f,cmd,int(pc==0x2222)) for f,pc,cmd in t['sounds']])]:c+=f'const {typ} {name}[]={{'+rows(v)+'};\n'
c+='const uint32_t opening_tiles[]={'+','.join(map(str,tileids))+'};\nconst uint16_t opening_palrefs[]={'+','.join(map(str,palrefs))+'};\nconst uint16_t opening_palettes[][16]={'+rows(pals)+'};\n';(g/'opening-replay-data.c').write_text(c)
print(dict(tiles=len(tileids),cells=len(cells),draws=len(draws),frames=len(headers),palettes=len(pals),bytes=len(tileids)*4+len(cells)*6+len(maps)*4+len(draws)*10+len(headers)*8+len(pals)*32+len(palrefs)*2))
(g/'opening-replay-source.json').write_text(json.dumps(dict(prom_sha256=hashlib.sha256(p.d).hexdigest(),original_entry='0x08f91a',frames=420,display_hz=30,source_hz=60,pixel_source='Existing native-sprites.4bpp; no new raster assets',trace_sha256=hashlib.sha256(Path('opening-work/trace.json').read_bytes()).hexdigest(),omitted='Credit/start prompts; scene10 background is generated separately by build_opening_background.py'),indent=2))
