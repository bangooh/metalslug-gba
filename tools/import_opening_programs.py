#!/usr/bin/env python3
"""Original opening compound props, gates and attached block/puller; no runtime PCs."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(HUT=0x294234,HUT_WRECK=0x294244,ROOF=0x294254,ROOF_DIE=0x29426a,FLAG=0x29440a,NEST=0x29441a,NEST_WRECK=0x294430,TOP=0x294446,BASE=0x29445c,BUNKER=0x294472,BRIDGE=0x294488,PILLAR=0x29449e,SHED=0x2944b4,BARRIER=0x2944ca,BARRIER_WRECK=0x2944da,POST=0x2944ea,GATE=0x2944fa,GATE2=0x29451c,GATE_WRECK=0x29453e,FORT0=0x29454e,FORT1=0x294570,FORT2=0x294592,FORT_WRECK=0x2945b4,SIDE0=0x2945d4,SIDE1=0x2945e8,SIDE2=0x2945fc,LIGHT=0x294620,GATE_ROOF=0x294636,GATE_ROOF_WRECK=0x29464c,DOOR=0x29465c,MOUNT=0x29466c,MOUNT_FALL=0x29467c,PART_A=0x2946a2,PART_B=0x2946b8,PART_C=0x2946ce,PART_D=0x2946fa,PART_E=0x294710,PART_F=0x294726,PART_G=0x29473c,PULL_RUN=0x29b744,PULL_INTRO=0x2c09ac,PULL_ACTION=0x2c0a3a,PULL_END=0x2c0bb6,BLOCK=0x2c0be2,BLOCK_FALL=0x2c0c5e,BLOCK_FLAT=0x2c0c98,BLOCK_HIT=0x2c0cc4,BLOCK_BACK=0x2c0daa,LAND_FX=0x2deea2)
FIXED=dict(ROOF_BLAST=('ATTACK',0x295fba),BARRIER_DEF=('DEFENSE',0x294e7c),BARRIER_BLAST=('ATTACK',0x296102),NUKE=('ATTACK',0x2ecd80),BLOCK_ATTACK=('ATTACK',0x2c0dba))
PATCHES=list(range(0x29556a,0x295b5a,20))
PATTERNS=list(range(0x295b5a,0x295cf8,18))
FACTORIES={0x77e10:'BFX_SMALL',0x77f6a:'BFX_FIRE',0x77fd6:'BFX_SMOKE',0x77efe:'BFX_BLAST',**{at:'BFX_DEBRIS'+str(i) for i,at in enumerate([0x78218,0x78236,0x78254,0x78272,0x78290,0x782ae,0x78740,0x7875e])}}
STAMPS=[0x295d38,0x295d78,0x295e4e,0x295dee,0x295f12,0x295e04,0x295f24,0x295e14,0x295e38,0x295f4a,0x2c0f06]
def vals(xs):return ','.join(map(str,xs))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);args=p.parse_args();raw=(args.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw);entries={k:c.entry(v) for k,v in ROOTS.items()};stages=[[65535 if c.long(at+4*i)==0xffffffff else c.entry(c.long(at+4*i)) for i in range(4)] for at in [0x2945c4,0x294610]];fixed={k:c.binding(*v) for k,v in FIXED.items()};c.compile()
 if c.fields or c.parameters or {v['source'] for v in c.callbacks}!={0x4fb74,0x5f93a}:raise ValueError('unreviewed graph')
 h=['#ifndef NATIVE_OPENING_PROGRAMS_H','#define NATIVE_OPENING_PROGRAMS_H','#include "../src/bosses.h"','#include "../src/scene_video.h"','enum OpeningAnimation {'+','.join('OPA_'+k for k in ROOTS)+',OPA_COUNT};','enum OpeningCallback {'+','.join('OPC_'+format(v['source'],'X') for v in c.callbacks)+'};','enum OpeningBinding {'+','.join('OPB_'+k+'='+str(v) for k,v in fixed.items())+'};','extern const struct AnimationProgram opening_program;','extern const uint16_t opening_entries[OPA_COUNT],opening_stages[2][4],opening_fort_rank[64];','extern const struct BossBindingSet opening_bindings['+str(len(c.bindings))+'];','extern const int16_t opening_bounds[4][4];','extern const struct HitRect opening_targets[2];','extern const struct InteractionContact opening_contacts[3];']
 out=c.c_source('opening_program','opening-programs.h')
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
 array('struct HitDefense','defs[]',','.join(defs),True);array('struct HitAttack','attacks[]',','.join(attacks),True);array('struct BossBindingSet','opening_bindings[]',','.join(sets))
 array('uint16_t','opening_entries[OPA_COUNT]',vals(entries.values()));array('uint16_t','opening_stages[2][4]',','.join('{'+vals(xs)+'}' for xs in stages))
 patch_rows=[]
 for i,at in enumerate(PATCHES):
  target=c.long(at);im=c.long(at+4);x,y,sx,sy,width,height=[c.signed(at+j) for j in range(8,20,2)];iw,ih=c.word(im),c.word(im+2);stream=c.long(im+4)
  if target not in (0x106f6c,0x107fe8) or min(sx,sy)<0 or min(width,height)<=0 or sy+height>ih or (sx+width>iw and not (im==0x295562 and sx+width<=90)):raise ValueError(('patch bounds',hex(at)))
  #51DE2 reads only image height;295562 intentionally addresses columns80..89 beyond its width80.
  data=[c.word(stream+4*((sx+col)*ih+sy+row)+2*k) for col in range(width) for row in range(height) for k in range(2)]
  array('uint16_t',f'patch_cells{i}[{len(data)}]',vals(data),True);patch_rows.append('{patch_cells'+str(i)+','+vals([x,y,width,height,0 if target==0x106f6c else 1])+'}')
 h+=['enum OpeningPatch {'+','.join('OPV_P'+format(at,'X') for at in PATCHES)+',OPV_COUNT};','extern const struct SceneTilePatch opening_patches[OPV_COUNT];'];array('struct SceneTilePatch','opening_patches[OPV_COUNT]',','.join(patch_rows))
 h+=['enum OpeningPattern {'+','.join('OPP_P'+format(at,'X') for at in PATTERNS)+',OPP_COUNT};','extern const struct BossPatternData opening_patterns[OPP_COUNT];','extern const uint8_t opening_pattern_choices[OPP_COUNT][16];']
 rows=[];choices=[]
 for at in PATTERNS:
  random=c.byte(at+13);p=c.long(at+14);ids=[FACTORIES[c.long(p+4*i)] for i in range(16)] if random else [FACTORIES[p]]*16
  rows.append('{'+vals([c.signed(at+4),c.signed(at+8),c.word(at+6),c.word(at+10),c.word(at+2),c.byte(at),c.byte(at+1),random])+','+ids[0]+'}');choices.append('{'+','.join(ids)+'}')
 array('struct BossPatternData','opening_patterns[OPP_COUNT]',','.join(rows));array('uint8_t','opening_pattern_choices[OPP_COUNT][16]',','.join(choices))
 h+=['struct OpeningStamp {const uint8_t*cells;int16_t dx,dy;uint16_t width,height;};','enum OpeningStampId {'+','.join('OPS_S'+format(at,'X') for at in STAMPS)+',OPS_COUNT};','extern const struct OpeningStamp opening_stamps[OPS_COUNT];']
 rows=[]
 for i,at in enumerate(STAMPS):
  dx,dy,width,height=[c.signed(at+2*j) for j in range(4)]
  if min(width,height)<=0:raise ValueError('stamp dimensions')
  array('uint8_t',f'stamp_cells{i}[{width*height}]',vals(c.byte(at+8+j) for j in range(width*height)),True);rows.append('{stamp_cells'+str(i)+','+vals([dx,dy,width,height])+'}')
 array('struct OpeningStamp','opening_stamps[OPS_COUNT]',','.join(rows))
 array('int16_t','opening_bounds[4][4]',','.join('{'+vals(c.signed(at+2*j) for j in range(4))+'}' for at in [0x2c0ed4,0x2c0ede,0x2c0ee8,0x2df24c]))
 array('struct HitRect','opening_targets[2]',','.join('{'+vals(c.signed(at+2*j) for j in range(4))+'}' for at in [0x2c0ef2,0x2c0efc]))
 rows=[]
 for at in [0x296272,0x2962be,0x29630a]:
  if c.byte(at)!=128:raise ValueError('contact marker')
  rows.append('{{'+vals(c.signed(at+6+2*j) for j in range(4))+'},'+vals([c.signed(at+2),c.byte(at+1)])+'}')
 array('struct InteractionContact','opening_contacts[3]',','.join(rows))
 if c.word(0x2c0010)!=0:raise ValueError('rank mode')
 array('uint16_t','opening_fort_rank[64]',vals(c.word(0x2c0012+2*i) for i in range(64)))
 h.append('#endif');ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,pattern_sources=PATTERNS,patch_sources=PATCHES,stamp_sources=STAMPS,implementation=['src/opening.c','src/opening_fort.c','src/opening_crew.c'],complete_port=False,validation='Source conversion only; no gameplay tests',source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in [(0x4d8f2,0x4f2c2),(0x4fa8a,0x5022a),(0x5f384,0x5fa00),(0x86586,0x865dc)]])
 for name,v in [('opening-programs.h','\n'.join(h)),('opening-programs.c','\n'.join(out)),('opening-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(v+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),patches=len(PATCHES),patterns=len(PATTERNS),stamps=len(STAMPS))))
if __name__=='__main__':main()
