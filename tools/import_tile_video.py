#!/usr/bin/env python3
"""Offline BG4x4/FIX2x2 fragments from approved75% indexed intermediates.

Read EVERY typed scene/patch table, including separate M2/M5 patches. No
recorded route, emulator, color quantizer, inferred missing tile or game test.
The runtime copies already scaled fragments; it never rescales source pixels.
"""
import hashlib,json,re,struct
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1];G=R/'generated'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 ledger=json.loads((G/'scaled-drawing-source.json').read_text())
 for name in ['native-scaled-background-12x12.4bpp','native-scaled-fixed-6x6.4bpp']:
  if sha(G/name)!=ledger['files'][name]['sha256']:raise ValueError('Changed input '+name)
 used={0,65536};records=[];sources={};flag_counts={}
 for path in sorted(G.glob('*.c')):
  s=path.read_text();arrays={}
  for m in re.finditer(r'(?:static\s+)?const uint16_t (\w+)\[[^]]*\]\s*=\s*\{([^{}]*)\};',s):
   arrays[m[1]]=[int(x.strip(),0) for x in m[2].split(',') if x.strip()]
  for m in re.finditer(r'const struct (SceneImage|SceneTilePatch|BossBackgroundPatch) (\w+)(?:\[[^]]*\])?\s*=\s*\{(.*?)\};',s,re.S):
   body=re.findall(r'\{([^{}]+)\}',m[3]) or [m[3]]
   for row in body:
    values=[x.strip() for x in row.split(',')]
    name=values[0];width,height=map(int,values[1:3] if m[1]=='SceneImage' else values[3:5])
    cells=arrays[name]
    if len(cells)!=width*height*2:raise ValueError('Unreviewed cell extent '+path.name+':'+name)
    for tile,attr in zip(cells[::2],cells[1::2]):
     bank=(attr>>4)&15
     if bank>1:raise ValueError('C-ROM bank outside imported source')
     count=8 if attr&8 else 4 if attr&4 else 1
     base=tile&~(count-1)
     used.update((bank<<16)|(base+i) for i in range(count))
     # Tile-only clear retains the original attributes, including auto bits.
     used.update((bank<<16)|i for i in range(count))
     flag_counts[str(attr&255)]=flag_counts.get(str(attr&255),0)+1
    records.append(dict(file=path.name,table=m[2],cells=name,width=width,height=height))
    sources[path.name]=sha(path)
 expected={'SceneImage':53,'SceneTilePatch':651,'BossBackgroundPatch':8}
 # Count by actual types separately, not a heuristic name match.
 counts={k:0 for k in expected}
 for path in sorted(G.glob('*.c')):
  for m in re.finditer(r'const struct (SceneImage|SceneTilePatch|BossBackgroundPatch) \w+(?:\[[^]]*\])?\s*=\s*\{(.*?)\};',path.read_text(),re.S):
   counts[m[1]]+=len(re.findall(r'\{([^{}]+)\}',m[2])) or 1
 if counts!=expected:raise ValueError('Typed image/patch inventory changed: '+str(counts))
 raw=np.frombuffer((G/'native-scaled-background-12x12.4bpp').read_bytes(),dtype=np.uint8).reshape(131072,12,6)
 pixels=np.empty((131072,12,12),np.uint8);pixels[:,:,::2]=raw&15;pixels[:,:,1::2]=raw>>4
 ids=sorted(used);lookup=np.full(131072,65535,dtype='<u2');refs=[];blocks=[];block_ids={}
 for i,tid in enumerate(ids):
  if i>=65535:raise ValueError('BG tile index width')
  lookup[tid]=i;rebuilt=np.zeros((12,12),np.uint8)
  for y in range(3):
   for x in range(3):
    part=pixels[tid,y*4:y*4+4,x*4:x*4+4];packed=(part[:,::2]|part[:,1::2]<<4).tobytes()
    if packed not in block_ids:block_ids[packed]=len(blocks);blocks.append(packed)
    refs.append(block_ids[packed]);decoded=np.frombuffer(blocks[refs[-1]],dtype=np.uint8).reshape(4,2)
    rebuilt[y*4:y*4+4,x*4:x*4+4:2]=decoded&15
    rebuilt[y*4:y*4+4,x*4+1:x*4+4:2]=decoded>>4
  if not np.array_equal(pixels[tid],rebuilt):raise ValueError('BG fragment round trip')
 if len(blocks)>=2**32:raise ValueError('BG fragment index width')
 (G/'native-bg-lookup.u16').write_bytes(lookup.tobytes())
 (G/'native-bg-fragments.u32').write_bytes(struct.pack('<'+'I'*len(refs),*refs))
 (G/'native-bg-blocks.4bpp').write_bytes(b''.join(blocks))
 # Existing FIX intermediate has6x6 active pixels padded to8x8, GBA rows.
 raw=np.frombuffer((G/'native-scaled-fixed-6x6.4bpp').read_bytes(),dtype=np.uint8).reshape(4096,8,4)
 pix=np.empty((4096,8,8),np.uint8);pix[:,:,::2]=raw&15;pix[:,:,1::2]=raw>>4
 fix=[]
 for im in pix:
  if im[6:,:].any() or im[:,6:].any():raise ValueError('FIX padding')
  out=np.zeros((6,6),np.uint8)
  for y in range(3):
   for x in range(3):
    a=im[y*2:y*2+2,x*2:x*2+2].flatten();word=sum(int(c)<<(4*i) for i,c in enumerate(a));fix.append(word)
    out[y*2:y*2+2,x*2:x*2+2]=np.array([(word>>(4*i))&15 for i in range(4)]).reshape(2,2)
  if not np.array_equal(out,im[:6,:6]):raise ValueError('FIX fragment round trip')
 (G/'native-fix-fragments.u16').write_bytes(struct.pack('<'+'H'*len(fix),*fix))
 mask=[sum(bool(any(fix[i*9:(i+1)*9]))<<(i%32) for i in range(b*32,b*32+32)) for b in range(128)]
 (G/'native-fix-visible.inc').write_text(','.join(hex(x)+'u' for x in mask)+'\n')
 h='#ifndef NATIVE_TILE_VIDEO_DATA_H\n#define NATIVE_TILE_VIDEO_DATA_H\n#include <stdint.h>\n'
 h+=f'enum {{NATIVE_BG_TILE_COUNT={len(ids)},NATIVE_BG_BLOCK_COUNT={len(blocks)}}};\n'
 h+='extern const uint16_t native_bg_lookup[131072];\nextern const uint32_t native_bg_fragments[NATIVE_BG_TILE_COUNT][9];\n'
 h+='extern const uint8_t native_bg_blocks[NATIVE_BG_BLOCK_COUNT][8];\nextern const uint8_t native_bg_preflipped[3][NATIVE_BG_BLOCK_COUNT][8];\nextern const uint16_t native_bg_opacity[NATIVE_BG_BLOCK_COUNT][4];\nextern const uint16_t native_fix_fragments[4096][9];\n#endif\n'
 (G/'tile-video-data.h').write_text(h)
 from import_bg_opacity import generate
 generate()
 from prepare_background_variants import generate as preflip
 preflip()
 names=['native-bg-lookup.u16','native-bg-fragments.u32','native-bg-blocks.4bpp','native-fix-fragments.u16','native-fix-visible.inc','tile-video-data.h']
 report=dict(version=(R/'VERSION').read_text().strip(),scale=ledger['scale'],source_canvas=[320,224],viewport=[240,160],crop=ledger['crop'],
  source_tile_count=131072,bg_tiles=len(ids),bg_blocks=len(blocks),fix_tiles=4096,inventory=counts,records=records,
  source_hashes=sources,intermediate_ledger_sha256=sha(G/'scaled-drawing-source.json'),attribute_low_byte_counts=flag_counts,
  auto_animation='attribute bit3: low3 tile bits; else bit2: low2; all phases included; physical VBlank clock supplied by native_video_vblank',
  conversion_roundtrip=True,gameplay_executed=False,quantized=False,
  approved_player_sha256=sha(G/'player.tiles'),
  limitations=['Fragment inventory only; connected4BG compositor implementation is src/background_video.c',
   'FIX/HUD use shared OBJ in src/fixed_object.c; physical budgets and timing need integrated verification',
   'Ordinary FIX rows2/29 retain approved4+4 crop; native session credits/LEVEL explicitly use row28'],
  files={n:dict(bytes=(G/n).stat().st_size,sha256=sha(G/n)) for n in names})
 (G/'tile-video-source.json').write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items() if k not in ['records','source_hashes','attribute_low_byte_counts','files']}))
if __name__=='__main__':main()
