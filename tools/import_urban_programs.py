#!/usr/bin/env python3
"""All urban static placement families and source patch callbacks, source-only."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(BUILDING=0x298936,BUILDING2=0x298958,BUILDING_WRECK=0x29897a,COLUMN=0x29899e,WALL_CHILD=0x2989b4,WALL_CHILD2=0x2989ca,WALL_CHILD_WRECK=0x2989f4,WALL=0x298a10,WALL2=0x298a26,WALL_WRECK=0x298a3c,RANKED=0x298a52,STALL=0x298aa8,STALL_WRECK=0x298b24,FRAGILE=0x298b34,FRAGILE_BREAK=0x298b4a,FRAGILE_WRECK=0x298b78,BREAKABLE=0x298b88,HIT_STAGE=0x298b9e,SMALL=0x299050,SMALL_WRECK=0x299066,NEON=0x29908c,NEON_WRECK=0x2990a2,MULTISTAGE=0x2990b8,MULTISTAGE4=0x299d6e)
PATCHES=list(range(0x29a552,0x29b29a,20))
PATTERNS=list(range(0x29a278,0x29a482,18))
STAMPS=[0x29b29a,0x29b2b2,0x29b2ca,0x29b2de,0x29b2f2,0x29b306,0x29b31a,0x29b32e,0x29b358,0x29b368,0x29b380,0x29b3b2]
FACTORIES={0x77e10:'SMALL',0x77f6a:'FIRE',0x77fd6:'SMOKE',0x77efe:'BLAST',**{at:'DEBRIS'+str(i) for i,at in enumerate([0x78218,0x78236,0x78254,0x78272,0x78290,0x782ae,0x78740,0x7875e])},**{at:'LOCAL'+str(i) for i,at in enumerate([0x78434,0x78452,0x78470,0x7848e,0x784ac,0x784ca])}}
def vals(xs):return ','.join(map(str,xs))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);args=p.parse_args();raw=(args.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw);entries={k:c.entry(v) for k,v in ROOTS.items()};stages=[[c.entry(c.long(at+4*i)) for i in range(4)] for at in [0x2990ce,0x2990de,0x2990ee]];hit=[c.entry(c.long(0x298c46+4*i)) for i in range(8)];debris=[c.entry(at) for at in [0x2ddf9e,0x2de014,0x2de08a,0x2de100,0x2de176,0x2de1ec]];c.compile()
 if {(x['source_offset'],x['width']) for x in c.fields}!={(0x72,2),(0x20,1)} or c.parameters:raise ValueError('unreviewed graph')
 callback_patches=[]
 for v in c.callbacks:
  at=v['source']
  if c.word(at)!=0x45f9 or c.word(at+6)!=0x4eb9 or c.long(at+8)!=0x5022a or c.word(at+12)!=0x4e75:raise ValueError(('callback body',hex(at)))
  callback_patches.append(PATCHES.index(c.long(at+2)))
 h=['#ifndef NATIVE_URBAN_PROGRAMS_H','#define NATIVE_URBAN_PROGRAMS_H','#include "../src/bosses.h"','#include "../src/scene_video.h"','enum UrbanAnimation {'+','.join('URA_'+k for k in ROOTS)+',URA_COUNT};','enum UrbanField {'+','.join('URF_'+format(v['source_offset'],'X')+'_'+str(v['width']) for v in c.fields)+'};','extern const struct AnimationProgram urban_program;','extern const uint16_t urban_entries[URA_COUNT],urban_stages[3][4],urban_hit[8],urban_debris[6],urban_rank[2][64];','extern const struct BossBindingSet urban_bindings['+str(len(c.bindings))+'];','extern const uint16_t urban_callback_patches['+str(len(callback_patches))+'];','extern const struct InteractionContact urban_blocker_contact;']
 out=c.c_source('urban_program','urban-programs.h')
 def array(ty,name,data,local=False):out.append(('static ' if local else '')+'const '+ty+' '+name+'={'+data+'};')
 defs=[];attacks=[];sets=[]
 for i,b in enumerate(c.bindings):
  at=b['source'];records=defs if b['kind']=='DEFENSE' else attacks;first=len(records)
  for n in range(32):
   if c.byte(at)==255:break
   rect='{'+vals(c.signed(at+10+2*j) for j in range(4))+'}'
   if b['kind']=='DEFENSE':
    f=c.long(at+2);name='0'
    if f!=0xffffffff:name=f'filter_{i}_{n}';array('uint8_t',name+'[36]',vals(c.byte(f+j) for j in range(36)),True)
    records.append('{'+rect+','+name+','+str(c.byte(at+1))+'}')
   elif b['kind']=='ATTACK':
    group,kind,damage,strength,shape,offscreen=struct.unpack_from('>BBhBBB',raw,c.offset(at))
    if group>4 or kind>34 or shape not in (0,4):raise ValueError('attack format')
    records.append('{'+rect+','+vals([damage,group,kind,strength,shape,offscreen])+'}')
   else:raise ValueError('binding kind')
   at+=80
  else:raise ValueError('hit terminator')
  sets.append('{'+('defs+'+str(first)+',0' if b['kind']=='DEFENSE' else '0,attacks+'+str(first))+','+str(n)+'}')
 array('struct HitDefense','defs[]',','.join(defs),True)
 if attacks:array('struct HitAttack','attacks[]',','.join(attacks),True)
 array('struct BossBindingSet','urban_bindings[]',','.join(sets))
 array('uint16_t','urban_entries[URA_COUNT]',vals(entries.values()));array('uint16_t','urban_stages[3][4]',','.join('{'+vals(xs)+'}' for xs in stages));array('uint16_t','urban_hit[8]',vals(hit));array('uint16_t','urban_debris[6]',vals(debris));array('uint16_t','urban_callback_patches[]',vals(callback_patches))
 patch_rows=[]
 for i,at in enumerate(PATCHES):
  target=c.long(at);im=c.long(at+4);x,y,sx,sy,width,height=[c.signed(at+j) for j in range(8,20,2)];iw,ih=c.word(im),c.word(im+2);stream=c.long(im+4)
  if target not in (0x106f6c,0x107fe8) or min(sx,sy)<0 or min(width,height)<=0 or sy+height>ih or sx+width>iw:raise ValueError(('patch bounds',hex(at)))
  data=[c.word(stream+4*((sx+col)*ih+sy+row)+2*k) for col in range(width) for row in range(height) for k in range(2)]
  array('uint16_t',f'patch_cells{i}[{len(data)}]',vals(data),True);patch_rows.append('{patch_cells'+str(i)+','+vals([x,y,width,height,0 if target==0x106f6c else 1])+'}')
 h+=['enum UrbanPatch {'+','.join('URV_P'+format(at,'X') for at in PATCHES)+',URV_COUNT};','extern const struct SceneTilePatch urban_patches[URV_COUNT];'];array('struct SceneTilePatch','urban_patches[URV_COUNT]',','.join(patch_rows))
 h+=['enum UrbanPattern {'+','.join('URP_P'+format(at,'X') for at in PATTERNS)+',URP_COUNT};','enum UrbanFactory {'+','.join('URFX_'+v for v in FACTORIES.values())+'};','extern const struct BossPatternData urban_patterns[URP_COUNT];','extern const uint8_t urban_pattern_choices[URP_COUNT][16];']
 rows=[];choices=[];fact=list(FACTORIES)
 for at in PATTERNS:
  random=c.byte(at+13);p=c.long(at+14);ids=[fact.index(c.long(p+4*i)) for i in range(16)] if random else [fact.index(p)]*16
  rows.append('{'+vals([c.signed(at+4),c.signed(at+8),c.word(at+6),c.word(at+10),c.word(at+2),c.byte(at),c.byte(at+1),random,ids[0]])+'}');choices.append('{'+vals(ids)+'}')
 array('struct BossPatternData','urban_patterns[URP_COUNT]',','.join(rows));array('uint8_t','urban_pattern_choices[URP_COUNT][16]',','.join(choices))
 h+=['struct UrbanStamp {const uint8_t*cells;int16_t dx,dy;uint16_t width,height;};','enum UrbanStampId {'+','.join('URS_S'+format(at,'X') for at in STAMPS)+',URS_COUNT};','extern const struct UrbanStamp urban_stamps[URS_COUNT];']
 rows=[]
 for i,at in enumerate(STAMPS):
  dx,dy,width,height=[c.signed(at+2*j) for j in range(4)]
  if min(width,height)<=0:raise ValueError('stamp dimensions')
  array('uint8_t',f'stamp_cells{i}[{width*height}]',vals(c.byte(at+8+j) for j in range(width*height)),True);rows.append('{stamp_cells'+str(i)+','+vals([dx,dy,width,height])+'}')
 array('struct UrbanStamp','urban_stamps[URS_COUNT]',','.join(rows))
 at=0x29b3c0
 if c.byte(at)!=128:raise ValueError('contact marker')
 array('struct InteractionContact','urban_blocker_contact','{'+vals(c.signed(at+6+2*j) for j in range(4))+'},'+vals([c.signed(at+2),c.byte(at+1)]))
 ranks=[]
 for at in [0x2c0196,0x2c0114]:
  if c.word(at)!=0:raise ValueError('rank mode')
  ranks.append('{'+vals(c.word(at+2+2*i) for i in range(64))+'}')
 array('uint16_t','urban_rank[2][64]',','.join(ranks))
 h.append('#endif');ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,pattern_sources=PATTERNS,patch_sources=PATCHES,stamp_sources=STAMPS,callback_patch_indices=callback_patches,implementation=['src/urban.c','src/urban_effects.c','src/scene_rgb_fade.c','src/banners.c'],complete_port=False,validation='Source conversion only; no gameplay tests',source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in [(0x53f96,0x55bd0),(0x78434,0x78506),(0x52580,0x526b8),(0x46608,0x46682)]])
 for name,v in [('urban-programs.h','\n'.join(h)),('urban-programs.c','\n'.join(out)),('urban-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(v+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),patches=len(PATCHES),patterns=len(PATTERNS),stamps=len(STAMPS))))
if __name__=='__main__':main()
