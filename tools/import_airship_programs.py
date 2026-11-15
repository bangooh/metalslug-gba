#!/usr/bin/env python3
"""Offline airshipress assets. Source identities are provenance, never runtime PCs."""
import argparse,hashlib,json,struct,re
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(PROP0=0x2ede92,PROP1=0x2edea2,BUNKER=0x2edeb2,BUNKER_HIT=0x2edec2,DEPOT=0x2ee2c6,BODY=0x2edf8e,BODY_CHILD=0x2edf9e,GROUND=0x2edfae,GROUND_CHILD=0x2edfec,CASING=0x2ee4e8,SOLID0=0x2ee164,SOLID1=0x2ee176,PROP2=0x2ee188,
 DROP0=0x2ee194,DROP1=0x2ee1a0,DROP2=0x2ee1ac,DROP3=0x2ee1b8,TOWER_A=0x2ee1e8,TOWER_A_HIT=0x2ee1d4,TOWER_A_FINAL=0x2ee212,TOWER_A_FINAL_HIT=0x2ee1fe,
 TOWER_B=0x2ee23c,TOWER_B_HIT=0x2ee228,TOWER_B_MID=0x2ee266,TOWER_B_MID_HIT=0x2ee252,TOWER_B_FINAL=0x2ee290,TOWER_B_FINAL_HIT=0x2ee27c,
 PORT=0x2ee5be,PORT_B=0x2ee58e,PORT_B_HIT=0x2ee59e,PORT_B_WRECK=0x2ee5ae,SPARK=0x2ded26,IMPACT=0x2c52ba,SPLASH=0x2de7c2,CROUCH=0x38fa8,RICOCHET=0x2d4666,
 DEBRIS0=0x2dde3c,DEBRIS1=0x2ddf28,DEBRIS2=0x2ddf9e)
TABLES=dict(TENT=(0x2edf76,6),CRATE=(0x2ee13c,5),CRATE_WRECK=(0x2ee150,5),GUN_IDLE=(0x2ee318,8),GUN_ACTIVE=(0x2ee3f0,8),OVERLAY=(0x2ee4c8,8),ROCK=(0x2ee1c4,4),SMOKE=(0x2ee2be,2),THROWN=(0x2ee7a8,3),RICOCHET_IMPACT=(0x31d1e,2))
FIXED=dict(BUNKER=('DEFENSE',0x2ee7b4),BARREL=('DEFENSE',0x2ee808),TOWER=('DEFENSE',0x2eeb50),TOWER_ACTIVE=('DEFENSE',0x2eeba4),PORT=('DEFENSE',0x2eebf8),CROUCH=('DEFENSE',0x38fbc),DEPOT_BLAST=('ATTACK',0x2ef00c),BODY=('ATTACK',0x2eee24),TOWER_BLAST=('ATTACK',0x2eeec8),RICOCHET=('ATTACK',0x2d3e42))
PATCHES={f'P{x:X}':x for x in [0x2ef34c,0x2ef360,0x2ef374,0x2ef388,*range(0x2ef3ac,0x2ef5f0,20)]}
STAMPS={f'S{x:X}':x for x in [0x2ef5f0,0x2ef5fc,0x2ef60c,0x2ef626,0x2ef63e,0x2ef66c,0x2ef688,0x2ef696,0x2ef6a4,0x2ef6c4,0x2ef6e4,0x2ef704,0x2ef740,0x2ef77a,0x2ef79a]}
PATTERNS={f'P{x:X}':x for x in [0x2ef1ac,0x2ef1be,0x2ef1d0,0x2ef1e2,0x2ef274,0x2ef286,0x2ef298,0x2ef2ea]}
FACTORIES={0x77f6a:'FIRE',0x77fd6:'SMOKE',0x78236:'DEBRIS1',0x78254:'DEBRIS2',0x78272:'DEBRIS3',0x78290:'DEBRIS4',0x782ae:'DEBRIS5',0x78650:'DEBRIS9',0x7868c:'DEBRIS10',0x782cc:'LOCAL0',0x78308:'LOCAL1',0x78326:'LOCAL2'}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw)
 for i,at in {254:0x8b34c,255:0x8b366,256:0x8b380,257:0x8b39a,258:0x88ff4,270:0x8b258,323:0x38f14,324:0x38f48}.items():
  if c.long(0xe8000+i*4)!=at:raise ValueError('template mapping')
 entries={k:c.entry(v) for k,v in ROOTS.items()};tables={k:[c.entry(c.long(at+4*i)) if c.long(at+4*i)!=0xffffffff else 65535 for i in range(n)] for k,(at,n) in TABLES.items()};fixed={k:c.binding(*v) for k,v in FIXED.items()}
 c.compile()
 if c.fields or c.parameters:raise ValueError(('unreviewed owner field',c.fields))
 if set(x['source'] for x in c.callbacks)!=set([0x8b45c,0x8b490,0x8b4c4,0x8b50e]):raise ValueError(('callbacks',c.callbacks))
 callback_patches=[]
 # Each callback and field is implemented by airship.c; unexpected additions fail.
 h=['#ifndef NATIVE_AIRSHIP_PROGRAMS_H','#define NATIVE_AIRSHIP_PROGRAMS_H','#include "../src/bosses.h"','#include "../src/scene_video.h"','#include "../src/airship.h"',
 'enum AirshipAnimation {'+','.join('ASA_'+k for k in entries)+',ASA_COUNT};',
 'enum AirshipCallback {'+','.join('ASC_'+format(x['source'],'X') for x in c.callbacks)+'};',

 'enum AirshipBinding {'+','.join('ASB_'+k+'='+str(v) for k,v in fixed.items())+'};',
 'enum AirshipPattern {'+','.join('ASP_'+k for k in PATTERNS)+',ASP_COUNT};',
 'enum AirshipFactory {'+','.join('ASFX_'+v for v in FACTORIES.values())+'};',
 'extern const struct AnimationProgram airship_program;','extern const uint16_t airship_entries[ASA_COUNT];',
 'extern const struct BossBindingSet airship_bindings['+str(len(c.bindings))+'];',
 *['extern const uint16_t airship_'+k.lower()+'['+str(n)+'];' for k,(_,n) in TABLES.items()],

 'extern const struct BossPatternData airship_patterns[ASP_COUNT];','extern const uint8_t airship_pattern_choices[ASP_COUNT][16];',
 'extern const uint16_t airship_rank[64];','extern const int16_t airship_bounds[12][4],airship_crouch_rect[4],airship_muzzle[8][2];',
 'struct AirshipStamp {const uint8_t*cells;int16_t dx,dy;uint16_t width,height;};',
 'enum AirshipStampId {'+','.join('ASS_'+k for k in STAMPS)+',ASS_COUNT};','extern const struct AirshipStamp airship_stamps[ASS_COUNT];',
 ]
 out=c.c_source('airship_program','airship-programs.h');defs=[];attacks=[];sets=[];filters={}
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
 array('struct BossBindingSet','airship_bindings[]',','.join(sets));array('uint16_t','airship_entries[ASA_COUNT]',vals(entries.values()))
 for k,v in tables.items():array('uint16_t','airship_'+k.lower()+'['+str(len(v))+']',vals(v))

 patch_ids={};patch_rows=[]
 def patch(at):
  if at in patch_ids:return patch_ids[at]
  i=len(patch_ids);patch_ids[at]=i;target=c.long(at);im=c.long(at+4);x,y,sx,sy,width,height=[c.signed(at+j) for j in range(8,20,2)]
  iw,ih=c.word(im),c.word(im+2);stream=c.long(im+4)
  if target not in (0x106f6c,0x107fe8) or min(sx,sy)<0 or min(width,height)<=0 or sx+width>iw or sy+height>ih:raise ValueError(('patch bounds',hex(at)))
  data=[c.word(stream+4*((sx+col)*ih+sy+row)+2*k) for col in range(width) for row in range(height) for k in range(2)]
  array('uint16_t',f'patch_cells{i}[{len(data)}]',vals(data));patch_rows.append('{patch_cells'+str(i)+','+vals([x,y,width,height,0 if target==0x106f6c else 1])+'}');return i
 named={k:patch(at) for k,at in PATCHES.items()}
 cb_ids=[patch(at) for at in callback_patches]


 h+=['enum AirshipPatchId {'+','.join('ASV_'+k+'='+str(v) for k,v in named.items())+'};','extern const struct SceneTilePatch airship_patches['+str(len(patch_ids))+'];']
 array('struct SceneTilePatch','airship_patches[]',','.join(patch_rows))
 stamp_rows=[]
 for i,at in enumerate(STAMPS.values()):
  dx,dy,width,height=[c.signed(at+2*j) for j in range(4)]
  if width<=0 or height<=0:raise ValueError('stamp dimensions')
  array('uint8_t',f'stamp_cells{i}[{width*height}]',vals(c.byte(at+8+j) for j in range(width*height)))
  stamp_rows.append('{stamp_cells'+str(i)+','+vals([dx,dy,width,height])+'}')
 array('struct AirshipStamp','airship_stamps[]',','.join(stamp_rows))
 patterns=[];choices=[];fact=list(FACTORIES)
 for at in PATTERNS.values():
  random=c.byte(at+13);p=c.long(at+14);ids=[fact.index(c.long(p+4*i)) for i in range(16)] if random else [fact.index(p)]*16
  patterns.append('{'+vals([c.signed(at+4),c.signed(at+8),c.word(at+6),c.word(at+10),c.word(at+2),c.byte(at),c.byte(at+1),random,ids[0]])+'}');choices.append('{'+vals(ids)+'}')
 array('struct BossPatternData','airship_patterns[]',','.join(patterns));array('uint8_t','airship_pattern_choices[ASP_COUNT][16]',','.join(choices))
 if c.word(0x2c06aa)!=0:raise ValueError('rank mode')
 array('uint16_t','airship_rank[64]',vals(c.word(0x2c06ac+2*i) for i in range(64)))
 array('int16_t','airship_bounds[12][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in [0x2ef7a8,0x2ef7b2,0x2ef7bc,0x2ef7c6,0x2ef7d0,0x2ef7da,0x2ef7e4,0x38f9e,0x2df238,0x2df21a,0x2df224,0x2df224]))
 array('int16_t','airship_crouch_rect[4]',vals(c.signed(0x38fb4+2*i) for i in range(4)))
 array('int16_t','airship_muzzle[8][2]',','.join('{'+vals(c.signed(0x2ef7ee+4*i+2*j) for j in range(2))+'}' for i in range(8)))
 handlers={0x88f74:'PROP0',0x88f8e:'PROP1',0x890cc:'CAMP',0x89e8e:'BARRELS',0x8a006:'ROCKRAIN',0x893ac:'BODY',0x8a10c:'TOWER_A',0x8a74e:'TOWER_B',0x89c44:'CRATE3',0x89e2c:'PROP2',0x89c58:'CRATE4'}
 rows=[]
 for i in range(128):
  at=0x97422+20*i
  if c.word(at)==65535:break
  if c.byte(at)!=1 or c.long(at+6) not in handlers:raise ValueError(('unreviewed placement',hex(at),hex(c.long(at+6))))
  rows.append('{'+vals([c.signed(at+2),c.signed(at+4),c.word(at+10),c.word(at+12)])+',AIRSHIP_'+handlers[c.long(at+6)]+','+str(c.byte(at+1))+'}')
 else:raise ValueError('placement terminator')
 h.append('struct AirshipPlacement {int16_t x,y;uint16_t config,priority;uint8_t entry,flags;};')
 h.append('extern const struct AirshipPlacement airship_scene5['+str(len(rows))+'];')
 array('struct AirshipPlacement','airship_scene5['+str(len(rows))+']',','.join(rows))
 h.append('#endif');ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,tables=TABLES,patch_sources=patch_ids,stamp_sources=STAMPS,pattern_sources=PATTERNS,validation='Source conversion only; executable coverage tracked separately',complete_port=False,templates={254:0x8b34c,255:0x8b366,256:0x8b380,257:0x8b39a,258:0x88ff4,270:0x8b258,323:0x38f14,324:0x38f48},implementation=['src/airship.c','src/airship_structures.c','src/airship_body.c','src/airship_effects.c'],source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in [(0x88f74,0x8b944),(0x38f14,0x39148),(0x631d0,0x63226),(0x6df32,0x6dfe8)]])
 for name,data in [('airship-programs.h','\n'.join(h)),('airship-programs.c','\n'.join(out)),('airship-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(data+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),patches=len(patch_ids),stamps=len(STAMPS))))
if __name__=='__main__':main()
