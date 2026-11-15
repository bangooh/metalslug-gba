#!/usr/bin/env python3
"""Source scene-three siege data, cropped tile writes and collision stamps.
Conversion only. Runtime handlers are ordinary C and never dispatch source PCs.
"""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(DRIFT=0x2e6cdc,GATE=0x2e6c00,GATE_BROKEN=0x2e6c16,ARCH=0x2e6d28,
 COLUMN_A=0x2e6d38,COLUMN_B=0x2e6da4,COLUMN_HIT1_A=0x2e6d4e,COLUMN_HIT1_B=0x2e6dba,
 COLUMN_HIT2_A=0x2e6d6a,COLUMN_HIT2_B=0x2e6dd6,COLUMN_WRECK_A=0x2e6d86,COLUMN_WRECK_B=0x2e6df2,
 REACTIVE=0x2e6e10,SOLID_A=0x2e6ebc,SOLID_B=0x2e6ecc,GUARD_ROOT=0x2e6edc,GUARD_FOLLOW=0x2e6eec,
 GUARD_A=0x2e6efc,GUARD_B=0x2e6f12,BOX=0x2e6f28,BOX_WRECK=0x2e6f3e,TILE_A=0x2e6f54,TILE_B=0x2e6f6a,
 BOUNCE=0x2e6f80,BOUNCE_HIT=0x2e6f96,BOUNCE_WRECK=0x2e6fba,ROW8=0x2e6fca,
 SINGLE=0x2e6fe0,SINGLE_HIT=0x2e6ff6,SINGLE_TILE=0x2e7006,ROW9=0x2e701c,BREAKABLE=0x2e7032,
 FINALE=0x2e7048,CLOSING=0x2e7486,BODY=0x2e7566,PARACHUTE=0x2e75c0,REWARD_DECOR=0x2e77ba,
 CREW=0x2c10d8,CREW_FALL=0x2b5b92,CREW_LAND=0x2b5c22,CREW_RISE=0x2b5cfa,CREW_RUN=0x2b604a,
 CHUNK=0x2de4b0,SHARD_A=0x2f40c6,SHARD_B=0x2f40f2,SHARD_C=0x2f411e,SHARD_LAND=0x2f40d2)
TABLES=dict(FINALE=(0x2e7556,4),STEP=(0x2e77ca,18),FLYING=(0x2980fc,4))
FIXED=dict(COLUMN=('ATTACK',0x2e9698),GUARDIAN=('DEFENSE',0x2e9256),BODY=('DEFENSE',0x2e915a),REWARD=('DEFENSE',0x2e9202),NUKE=('ATTACK',0x2ecd80))
PATCHES=dict(REACTIVE=0x2e9c8c,TILE_A=0x2e9c50,TILE_B=0x2e9c64,SINGLE=0x2e9c78,BREAK_A=0x2e9ca0,BREAK_B=0x2e9cb4,UNUSED=0x2e9f20,FINALE=0x2ea790,
 STEP0=0x2ea7a4,STEP1=0x2ea7b8,STEP2=0x2ea7cc,STEP3=0x2ea7e0,STEP4=0x2ea7f4,STEP5=0x2ea808,STEP6=0x2ea81c,STEP7=0x2ea830,STEP8=0x2ea844)
PATCH_TABLES=dict(ROW8=(0x2eaa60,8,4),ROW9=(0x2eaae0,9,1),REWARD=(0x2eab04,8,1),PANELS=(0x2eab24,45,2))
STAMPS=dict(GATE=0x2eac8c,COLUMN_A=0x2eaca0,COLUMN_B=0x2eacac,ROW=0x2eacb8,BODY=0x2eacc4,CLOSE=0x2eacd4,BREAK=0x2eae76)
PATTERNS=dict(GUARD=0x2e9a0c,COLUMN_A=0x2e9a1e,COLUMN_B=0x2e9a30,COLUMN_BLAST=0x2e987e,BOX=0x2e9a94,TILE=0x2e9aa6,
 BREAK=0x2e9b00,BOUNCE_FIRE=0x2e9890,BOUNCE_DEBRIS=0x2e9a82,ROW=0x2e9aca,AUX=0x2e9adc,FINALE_SMOKE=0x2e98d8,FINALE_HUMANS=0x2e98ea,
 BODY_FIRE=0x2e98a2,BODY_BLAST=0x2e98b4,BODY_DEBRIS=0x2e9aee,STEP_SMOKE=0x2e993c,REWARD=0x2e9ab8)
FACTORIES={0x537aa:'CHUNK',0x8ee96:'SHARD_A',0x8eeac:'SHARD_B',0x8eec2:'SHARD_C',0x77e10:'SMALL',0x77f6a:'FIRE',0x77efe:'BLAST',0x77fd6:'SMOKE',0x77f8e:'FIRE_BLUE',0x77fb2:'SMOKE_BLUE',0x78506:'DEBRIS_AD_A',0x78542:'DEBRIS_AD_B',0x78632:'DEBRIS77_A',0x786c8:'DEBRIS77_B',0x4a802:'HUMAN_LAUNCHED',0x4a18c:'HUMAN_CORPSE',**{x:'DEBRIS'+str(i) for i,x in enumerate([0x78218,0x78236,0x78254,0x78272,0x78290,0x782ae,0x78740,0x7875e])}}
CALLBACK_PATCH={0x863d6:'REACTIVE',0x86438:'STEP0',0x86446:'STEP1',0x86454:'STEP2',0x86462:'STEP3',0x86470:'STEP4',0x8647e:'STEP5',0x8648c:'STEP6',0x8649a:'STEP7',0x864a8:'STEP8'}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw);entries={k:c.entry(v) for k,v in ROOTS.items()};tables={k:[c.entry(c.long(at+i*4)) if c.long(at+i*4)!=0xffffffff else 65535 for i in range(n)] for k,(at,n) in TABLES.items()};fixed={k:c.binding(*v) for k,v in FIXED.items()};c.compile()
 if set(x['source'] for x in c.callbacks)!=set(CALLBACK_PATCH) or c.parameters:raise ValueError(('unreviewed callback',c.callbacks))
 if {(x['source_offset'],x['width']) for x in c.fields}!={(0x20,1),(0x3a,1)}:raise ValueError(('unreviewed fields',c.fields))
 h=['#ifndef NATIVE_SIEGE_PROGRAMS_H','#define NATIVE_SIEGE_PROGRAMS_H','#include "../src/bosses.h"','#include "../src/scene_video.h"','#include "../src/siege.h"',
 'enum SiegeAnimation {'+','.join('SGA_'+k for k in entries)+',SGA_COUNT};',
 'enum SiegeCallback {'+','.join('SGC_'+format(x['source'],'X') for x in c.callbacks)+'};',
 'enum SiegeField {'+','.join('SGF_'+format(x['source_offset'],'X') for x in c.fields)+'};',
 'enum SiegeBinding {'+','.join('SGB_'+k+'='+str(v) for k,v in fixed.items())+'};',
 'enum SiegePattern {'+','.join('SGP_'+k for k in PATTERNS)+',SGP_COUNT};',
 'enum SiegeFactory {'+','.join('SGFCT_'+v for v in FACTORIES.values())+'};',
 'extern const struct AnimationProgram siege_program;','extern const uint16_t siege_entries[SGA_COUNT];',
 'extern const struct BossBindingSet siege_bindings['+str(len(c.bindings))+'];',
 *['extern const uint16_t siege_'+k.lower()+'['+str(n)+'];' for k,(_,n) in TABLES.items()],
 'extern const struct BossPatternData siege_patterns[SGP_COUNT];','extern const uint8_t siege_pattern_choices[SGP_COUNT][16];',
 'extern const uint16_t siege_body_rank[64];','extern const int16_t siege_chunk_bounds[4];',
 'struct SiegeStamp {const uint8_t*cells;int16_t dx,dy;uint16_t width,height;};',
 'enum SiegeStampId {'+','.join('SGS_'+k for k in STAMPS)+',SGS_COUNT};','extern const struct SiegeStamp siege_stamps[SGS_COUNT];',
 'struct SiegeSpawn {int16_t x,y;uint16_t priority;uint8_t variant,facing;};','extern const struct SiegeSpawn siege_guards[12],siege_steps[9];',
 'extern const struct InteractionContact siege_body_contact;',
 'struct SiegePlacement {int16_t x,y;uint16_t config,priority;uint8_t entry,flags;};','extern const struct SiegePlacement siege_scene3[16];']
 out=c.c_source('siege_program','siege-programs.h');defs=[];attacks=[];sets=[];filters={}
 def array(ty,name,data):out.append(('static ' if name.startswith(('patch_cells','stamp_cells')) else '')+'const '+ty+' '+name+'={'+data+'};')
 for b in c.bindings:
  q=b['source'];records=defs if b['kind']=='DEFENSE' else attacks;first=len(records)
  for count in range(256):
   if c.byte(q)==255:break
   rect='{'+vals(c.signed(q+10+2*i) for i in range(4))+'}'
   if b['kind']=='DEFENSE':
    at=c.long(q+2);name='0'
    if at!=0xffffffff:
     if at not in filters:
      filters[at]='filter_'+str(len(filters));out.append('static const uint8_t '+filters[at]+'[36]={'+vals(c.byte(at+i) for i in range(36))+'};')
     name=filters[at]
    records.append('{'+rect+','+name+','+str(c.byte(q+1))+'}')
   elif b['kind']=='ATTACK':
    group,kind,damage,strength,shape,offscreen=struct.unpack_from('>BBhBBB',raw,c.offset(q))
    if group>4 or kind>34 or shape not in (0,4):raise ValueError('attack format')
    records.append('{'+rect+','+vals([damage,group,kind,strength,shape,offscreen])+'}')
   else:raise ValueError(b)
   q+=80
  else:raise ValueError('unterminated hit table')
  sets.append('{'+('defs+'+str(first)+',0' if b['kind']=='DEFENSE' else '0,attacks+'+str(first))+','+str(count)+'}')
 out+=['static const struct HitDefense defs[]={'+','.join(defs)+'};','static const struct HitAttack attacks[]={'+','.join(attacks)+'};']
 array('struct BossBindingSet','siege_bindings[]',','.join(sets));array('uint16_t','siege_entries[SGA_COUNT]',vals(entries.values()))
 for k,v in tables.items():array('uint16_t','siege_'+k.lower()+'['+str(len(v))+']',vals(v))
 # Every rectangle is cropped offline from its original column-major image.
 patch_ids={};patch_rows=[]
 def patch(at):
  if at in patch_ids:return patch_ids[at]
  i=len(patch_ids);patch_ids[at]=i;target=c.long(at);im=c.long(at+4);x,y,sx,sy,width,height=[c.signed(at+j) for j in range(8,20,2)]
  iw,ih=c.word(im),c.word(im+2);stream=c.long(im+4)
  if target not in (0x106f6c,0x107fe8) or min(sx,sy)<0 or min(width,height)<=0 or sx+width>iw or sy+height>ih:raise ValueError(('patch bounds',hex(at)))
  data=[c.word(stream+4*((sx+col)*ih+sy+row)+2*k) for col in range(width) for row in range(height) for k in range(2)]
  array('uint16_t',f'patch_cells{i}[{len(data)}]',vals(data));patch_rows.append('{patch_cells'+str(i)+','+vals([x,y,width,height,0 if target==0x106f6c else 1])+'}');return i
 named={k:patch(at) for k,at in PATCHES.items()}
 for k,(at,n,m) in PATCH_TABLES.items():
  rows=[[patch(c.long(at+4*(i*m+j))) for j in range(m)] for i in range(n)]
  h.append('extern const uint16_t siege_patch_'+k.lower()+f'[{n}][{m}];');array('uint16_t','siege_patch_'+k.lower()+f'[{n}][{m}]',','.join('{'+vals(row)+'}' for row in rows))
 h+=['enum SiegePatchId {'+','.join('SGV_'+k+'='+str(v) for k,v in named.items())+'};','extern const struct SceneTilePatch siege_patches['+str(len(patch_ids))+'];', 'extern const uint16_t siege_callback_patch['+str(len(c.callbacks))+'];']
 array('struct SceneTilePatch','siege_patches[]',','.join(patch_rows));array('uint16_t','siege_callback_patch[]',vals(named[CALLBACK_PATCH[x['source']]] for x in c.callbacks))
 stamp_rows=[]
 for i,at in enumerate(STAMPS.values()):
  dx,dy,width,height=[c.signed(at+2*j) for j in range(4)]
  if width<=0 or height<=0:raise ValueError('stamp dimensions')
  array('uint8_t',f'stamp_cells{i}[{width*height}]',vals(c.byte(at+8+j) for j in range(width*height)))
  stamp_rows.append('{stamp_cells'+str(i)+','+vals([dx,dy,width,height])+'}')
 array('struct SiegeStamp','siege_stamps[]',','.join(stamp_rows))
 patterns=[];choices=[];fact=list(FACTORIES)
 for at in PATTERNS.values():
  random=c.byte(at+13);p=c.long(at+14);ids=[fact.index(c.long(p+4*i)) for i in range(16)] if random else [fact.index(p)]*16
  patterns.append('{'+vals([c.signed(at+4),c.signed(at+8),c.word(at+6),c.word(at+10),c.word(at+2),c.byte(at),c.byte(at+1),random,ids[0]])+'}');choices.append('{'+vals(ids)+'}')
 array('struct BossPatternData','siege_patterns[]',','.join(patterns));array('uint8_t','siege_pattern_choices[SGP_COUNT][16]',','.join(choices))
 if c.word(0x2c0218)!=0:raise ValueError('rank mode')
 array('uint16_t','siege_body_rank[64]',vals(c.word(0x2c021a+2*i) for i in range(64)))
 array('int16_t','siege_chunk_bounds[4]',vals(c.signed(0x298736+2*i) for i in range(4)))
 for name,at,n in [('guards',0x2eaf1c,12),('steps',0x2eafe2,9)]:
  rows=['{'+vals([c.signed(at+8*i),c.signed(at+8*i+2),c.word(at+8*i+4),c.byte(at+8*i+6),c.byte(at+8*i+7)])+'}' for i in range(n)]
  if c.word(at+8*n)!=65535:raise ValueError('spawn terminator')
  array('struct SiegeSpawn','siege_'+name+f'[{n}]',','.join(rows))
 # $028998 contact record; its seven words are decoded like the existing tank contact converter.
 at=0x2eb06c
 if c.byte(at)!=0x80:raise ValueError('contact marker')
 array('struct InteractionContact','siege_body_contact','{'+vals(c.signed(at+6+2*i) for i in range(4))+'},'+vals([c.signed(at+2),c.byte(at+1)]))
 placement_handlers={0x834a4:'GATE',0x842d0:'BREAKABLE',0x83fa8:'ROW8',0x83596:'ARCH',0x84396:'FINALE',0x84ca4:'REWARD',0x835f2:'COLUMN0',0x8396a:'REACTIVE'}
 rows=[];placement_sources=[]
 for i in range(16):
  at=0x9718a+i*20
  if c.byte(at)!=1 or c.long(at+6) not in placement_handlers:raise ValueError('unreviewed placement')
  rows.append('{'+vals([c.signed(at+2),c.signed(at+4),c.word(at+10),c.word(at+12)])+',SIEGE_'+placement_handlers[c.long(at+6)]+','+str(c.byte(at+1))+'}');placement_sources.append(at)
 if c.word(0x9718a+16*20)!=65535:raise ValueError('scene3 terminator')
 array('struct SiegePlacement','siege_scene3[16]',','.join(rows))
 h.append('#endif');ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,tables=TABLES,patch_sources=patch_ids,stamp_sources=STAMPS,pattern_sources=PATTERNS,validation='Source conversion only; executable coverage is tracked separately',complete_port=False,placement_sources=placement_sources,implementation=['src/siege.c','src/siege_finale.c','src/siege_effects.c'],source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in [(0x832ec,0x84db0),(0x85fb0,0x865dc),(0x537aa,0x53964),(0x8ee96,0x8efce),(0x5ea1c,0x5ea52)]])
 for name,data in [('siege-programs.h','\n'.join(h)),('siege-programs.c','\n'.join(out)),('siege-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(data+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),patches=len(patch_ids),stamps=len(STAMPS))))
if __name__=='__main__':main()
