#!/usr/bin/env python3
"""Offline boss animation/data conversion. No game/CPU execution."""
import argparse,hashlib,json,struct,re
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(M2_WAIT=0x2e0148,M2_INTRO=0x2e015a,M2_PUFF=0x2dfe06,M2_SHELL=0x2dfcf0,
 M2_TRAIL=0x2dfed6,M2_FLASH=0x2dfd44,M2_SPARK=0x29e440,M2_BURST=0x2dfef4,
 FX_SMALL=0x2dd2d4,FX_FIRE=0x2dd594,FX_SMOKE=0x2dd37e,FX_BLAST=0x2dd4ba,
 FX_METAL_A=0x2de43a,FX_METAL_B=0x2de5f4,FX_SMOKE_ALT=0x2df066,FX_GRENADE=0x2df180,
 FX_SURFACE=0x2de7c2,FX_SIEGE_DEBRIS_A=0x2de100,FX_SIEGE_DEBRIS_B=0x2de1ec,
 CRAB_IDLE=0x2e04d6,CRAB_FORWARD=0x2e050a,CRAB_BACK=0x2e057e,CRAB_DIE=0x2e064a,
 CRAB_FALL=0x2e075a,CRAB_CLAW0=0x2e0770,CRAB_CLAW1=0x2e081e,CRAB_LEG=0x2e08ac,
 CRAB_STEP=0x2e08cc,CRAB_DETACH=0x2e094a,CRAB_SHARD=0x2e0ab0,
 GS_A=0x2d4c42,GS_B=0x2d4c52,GS_C=0x2d4c7e,GS_D=0x2d4c9c,GS_E=0x2d4cc8,GS_F=0x2d4ce6,
 GS_HIT=0x2d4d20,GS_CREW=0x2d4d6e,GS_GUN=0x2d4e64,GS_AIM=0x2d4e70,
 GS_DROP=0x2d4e9a,GS_DROP_B=0x2d4ea0,GS_FLY=0x2d4ee4,GS_WRECK0=0x2d4fc4,GS_WRECK1=0x2d501a,GS_WRECK2=0x2d5070,
 GS_SHELL=0x2dd8b6,GS_SMOKE_MARK=0x2da750,GS_SMOKE=0x2da5c4,GS_FIRE=0x2da680)
ROOTS.update(FX_FRAGMENT59_A=0x2ddf9e,FX_FRAGMENT59_B=0x2de014,FX_TRAIL77=0x2dd4d6,FX_SPARK_DRIFT=0x2de4b0,FX_FLASH=0x2dd6a6)
TABLES=dict(M2_BODY=(0x2df7d6,3),M2_TURRET=(0x2df850,7),M2_RECOIL=(0x2df86c,7),M2_DEBRIS=(0x2df7e2,8),
 CRAB_OPEN=(0x2e0456,2),CRAB_CLOSE=(0x2e044e,2),CRASH=(0x2d4106,16),CRASH_B=(0x2d4146,16))
FX_ENTRIES=[0x78218,0x78236,0x78254,0x78272,0x78290,0x782ae,0x78740,0x7875e,0x785f6,0x78650,0x7868c]
FX_ROOTS=[0x2ddad0,0x2ddb62,0x2ddbf4,0x2ddc86,0x2ddd18,0x2dddaa,0x2de5f4,0x2de66a,0x2de34e,0x2de3c4,0x2de43a]
PATTERNS=dict(WRECK0=0x2df73e,WRECK1=0x2df750,WRECK2=0x2df762,WRECK3=0x2df774,
 SMALL=0x2df786,DEBRIS=0x2df798,IMPACT=0x2df7aa,BODY=0x2df7bc,DEATH_FIRE=0x2e0124,DEATH_BLAST=0x2e0136,
 CRAB_DEATH=0x2e0348,GS_DEATH=0x2d49d0,M5_HIT=0x2d543a,M5_DEATH=0x2d544c,
 TURRET_BODY0=0x293eb4,TURRET_BODY1=0x293ec6,TURRET_CANNON=0x293ff2,FINAL_DEATH=0x2d91f8,TANK_HIT=0x2db268,TANK_DEBRIS=0x2db27a,TANK_EXPLOSION=0x2db28c,BAZOOKA_DEATH=0x2ca3e0,BAZOOKA_PUFF=0x2ca3f2,CARRIER_BODY=0x2e2ae8,CARRIER_DEBRIS=0x2e2afa,CARRIER_HIT=0x2e2b0c)
PATTERNS.update({f'SCRIPT151_{i}':at for i,at in enumerate(range(0x74922,0x749e8,18))})
PATTERNS.update(ROOF_DEATH=0x295bd8,BOAT_SMOKE=0x295c7a,BOAT_FIRE=0x295c8c,BOAT_DEBRIS=0x295c9e)
PATTERNS.update(TURRET142_HIT=0x2c33b4,TURRET142_DEATH=0x2c3406)
PATTERNS.update(BOSS120_DEATH=0x286112)
PATTERNS.update(MORTAR_HIT=0x2c5686,CANNON_HIT_A=0x2c5a76,CANNON_HIT_B=0x2c5a88,CANNON_DEATH=0x2c5a64)
PATTERNS.update(WALKER_FIRE=0x2d40a2,WALKER_IMPACT=0x2d40b4)
PATTERNS.update(GROUND_NEST_DEATH=0x2c3788,ROCKET_VEHICLE_FIRE=0x2ccf4a,ROCKET_VEHICLE_DEBRIS=0x2ccf5c)
PATTERNS.update(PLATFORM_DEATH_A=0x290ea4,PLATFORM_DEATH_B=0x290eb6,PLATFORM_HIT_A=0x290ec8,PLATFORM_HIT_B=0x290eec,PLATFORM_HIT_C=0x290eda)
FIXED=dict(M2_BURST_A=('ATTACK',0x2e001e),M2_BURST_B=('ATTACK',0x2e0072),M2_BURST_C=('ATTACK',0x2e00c6),
 GS_BODY=('DEFENSE',0x2d46c6),GS_GUN=('DEFENSE',0x2d471a),GS_B_LOW=('DEFENSE',0x2d476e),GS_B_HIGH=('DEFENSE',0x2d47c2),
 GS_CRASH=('ATTACK',0x2d4816),GS_B_LOW_HIT=('ATTACK',0x2d490e),GS_B_HIGH_HIT=('ATTACK',0x2d4962))
CALLBACKS={0x7c918,0x283ca,0x434ce,0x434dc,*range(0x7cd06,0x7ce14,30),
 0x7ca60,0x7cab0,0x7cafa,0x7cb44,0x7cb8e,0x7cbd8,0x7cc22,0x7cc6c,0x7ccb6,
 *range(0x7ce92,0x7cee2,10),0x6f13c,0x6f10c,0x6f124}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);args=p.parse_args()
 raw=(args.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw);entries={k:c.entry(v) for k,v in ROOTS.items()}
 tables={k:[c.entry(c.long(at+4*i)) for i in range(n)] for k,(at,n) in TABLES.items()}
 debris=[c.entry(at) for at in FX_ROOTS];fixed={k:c.binding(*v) for k,v in FIXED.items()}
 crab_attacks=[]
 for i in range(8):
  at=c.long(0x2e141a+4*i);row=[]
  while c.long(at)!=0xffffffff:
   row.append(c.binding('ATTACK',c.long(at)));at+=4
   if len(row)>6:raise ValueError('crab attack bound')
  crab_attacks.append(row+[65535]*(7-len(row)))
 c.compile()
 if {v['source'] for v in c.callbacks}!=CALLBACKS or c.parameters:raise ValueError('unreviewed boss callback')
 if {(v['source_offset'],v['width']) for v in c.fields}!={(0x20,1),(0x2a,2),(0x70,2),(0x5c,4),(0x3b,1),(0x7d,1)}:raise ValueError('unreviewed boss field')
 h=['#ifndef BOSS_PROGRAMS_H','#define BOSS_PROGRAMS_H','#include "../src/animation.h"','#include "../src/hitbox.h"',
 'enum BossAnimation {'+','.join('BA_'+k for k in entries)+',BA_COUNT};',
 'enum BossCallback {'+','.join('BC_'+format(v['source'],'X') for v in c.callbacks)+'};',
 'enum BossField {'+','.join('BF_'+format(v['source_offset'],'X') for v in c.fields)+'};',
 'enum BossBinding {'+','.join('BB_'+k+'='+str(v) for k,v in fixed.items())+'};',
 'extern const uint8_t boss_script151_choices[16];',
 'extern const uint8_t boss_turret142_choices[2][16];',
 'extern const uint8_t boss_rocket_vehicle_fx_choices[16],boss_walker_fx_choices[16];',
 'extern const uint8_t boss_tank_fx_choices[16],boss_bazooka_fx_choices[16];',
 'enum BossPattern {'+','.join('BP_'+k for k in PATTERNS)+',BP_COUNT};',
 'struct BossBindingSet {const struct HitDefense*defense;const struct HitAttack*attack;uint16_t count;};',
 'struct BossPatternData {int16_t dx,dy;uint16_t spread_x,spread_y,life;uint8_t follow,period,random,kind;};',
 'struct BossBackgroundPatch {const uint16_t*tiles;int16_t x,y;uint16_t width,height;};',
 'extern const struct AnimationProgram boss_program;',
 'extern const struct BossBindingSet boss_bindings['+str(len(c.bindings))+'];',
 'extern const uint16_t boss_crash_palettes[4];',
 'extern const uint16_t boss_entries[BA_COUNT],boss_fx_debris_entries[11];',
 *['extern const uint16_t boss_'+k.lower()+'['+str(n)+'];' for k,(_,n) in TABLES.items()],
 'extern const struct BossPatternData boss_patterns[BP_COUNT];',
 'extern const uint8_t boss_fx_choices[16],boss_gunship_fx_choices[16],boss_turret_fx_choices[16],boss2_curve[176][2];',
 'extern const int16_t boss2_sectors[7][4],boss2_muzzles[7][2],boss_fx_bounds[11][4];',
 'extern const int16_t boss_trig[2][256];',
 'extern const uint16_t boss2_rank[3][8][4][2],boss2_smoke_periods[11],boss2_scroll;',
 'extern const struct BossBackgroundPatch boss2_background[8];',
 'extern const uint16_t crab_rank[2][8][4][2],crab_attacks[8][7];',
 'extern const uint8_t crab_delays[7][256];',
 'extern const int16_t crab_bounds[2][4],crab_angles[9],crab_accel[3][9],crab_offsets[3][9][2];',
 '#include "../src/flight_path.h"','#include "interaction-programs.h"',
 'extern const struct FlightProgram gunship_flight_program;',
 'extern const uint16_t gunship_paths[8],gunship_rank[7][8][4][2],gunship_sounds[6];',
 'extern const int16_t gunship_bounds[4],gunship_offsets[8][4],gunship_depth[4],gunship_speeds[32];',
 'extern const struct InteractionContact gunship_contact;','#endif']
 out=c.c_source('boss_program','boss-programs.h');defenses=[];attacks=[];sets=[];filters={}
 for b in c.bindings:
  q=b['source'];records=defenses if b['kind']=='DEFENSE' else attacks;first=len(records)
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
    if group>4 or kind>34 or shape not in (0,4):raise ValueError('boss attack format')
    records.append('{'+rect+','+vals([damage,group,kind,strength,shape,offscreen])+'}')
   else:raise ValueError(b)
   q+=80
  else:raise ValueError('unterminated hit table')
  sets.append('{'+('defenses+'+str(first)+',0' if b['kind']=='DEFENSE' else '0,attacks+'+str(first))+','+str(count)+'}')
 out+=['static const struct HitDefense defenses[]={'+','.join(defenses)+'};','static const struct HitAttack attacks[]={'+','.join(attacks)+'};',
 'const struct BossBindingSet boss_bindings[]={'+','.join(sets)+'};']
 def array(ty,name,data):out.append('const '+ty+' '+name+'={'+data+'};')
 array('uint16_t','boss_crash_palettes[4]',vals(c.word(0x2d4284+2*i) for i in range(4)))
 array('uint16_t','boss_entries[BA_COUNT]',vals(entries.values()))
 for k,v in tables.items():array('uint16_t','boss_'+k.lower()+'['+str(len(v))+']',vals(v))
 array('uint16_t','boss_fx_debris_entries[11]',vals(debris))
 array('uint8_t','boss_fx_choices[16]',vals(FX_ENTRIES.index(c.long(0x295cf8+4*i)) for i in range(16)))
 array('uint8_t','boss_gunship_fx_choices[16]',vals(FX_ENTRIES.index(c.long(0x2d49e2+4*i)) for i in range(16)))
 array('uint8_t','boss_walker_fx_choices[16]',vals(FX_ENTRIES.index(c.long(0x2d40c6+4*i)) for i in range(16)))
 array('uint8_t','boss_rocket_vehicle_fx_choices[16]',vals(FX_ENTRIES.index(c.long(0x2ccf76+4*i)) for i in range(16)))
 array('uint8_t','boss_tank_fx_choices[16]',vals(FX_ENTRIES.index(c.long(0x2db29e+4*i)) for i in range(16)))
 array('uint8_t','boss_bazooka_fx_choices[16]',vals(FX_ENTRIES.index(c.long(0x2ca40c+4*i)) for i in range(16)))
 array('uint8_t','boss_turret_fx_choices[16]',vals(FX_ENTRIES.index(c.long(0x294004+4*i)) for i in range(16)))
 kinds={0x748e2:5,0x78740:"BFX_DEBRIS6",0x77e10:0,0x77f6a:1,0x77fd6:2,0x77efe:3,0x7c996:4,0x295cf8:5,0x2d49e2:5,0x294004:5,0x2db29e:5,0x2ca40c:5,0x2ccf76:5,0x2d40c6:5}
 kinds.update({0x2c33c6:5,0x2c3418:5})
 kinds.update({0x5379c:"BFX_SPARK_DRIFT",0x537aa:"BFX_SPARK_SCATTER"})
 out.append('#include "../src/bosses.h"')
 choices={0x77fd6:'BFX_SMOKE',**{at:f'BFX_DEBRIS{i}' for i,at in enumerate([0x78218,0x78236,0x78254,0x78272,0x78290,0x782ae])},0x77efe:'BFX_BLAST',0x77f6a:'BFX_FIRE',0x78704:'BFX_DEBRIS28',0x78740:'BFX_DEBRIS6',0x7875e:'BFX_DEBRIS7'}
 array('uint8_t','boss_script151_choices[16]',vals(choices[c.long(0x748e2+4*i)] for i in range(16)))
 array('uint8_t','boss_turret142_choices[2][16]',','.join('{'+vals(choices[c.long(at+4*i)] for i in range(16))+'}' for at in (0x2c33c6,0x2c3418)))
 rows=[]
 for at in PATTERNS.values():
  if c.long(at+14) not in kinds:raise ValueError('unreviewed FX kind')
  rows.append('{'+vals([c.signed(at+4),c.signed(at+8),c.word(at+6),c.word(at+10),c.word(at+2),c.byte(at),c.byte(at+1),c.byte(at+13),kinds[c.long(at+14)]])+'}')
 array('struct BossPatternData','boss_patterns[BP_COUNT]',','.join(rows))
 array('uint8_t','boss2_curve[176][2]',','.join('{'+vals(c.byte(0x2df8a4+2*i+j) for j in range(2))+'}' for i in range(176)))
 for name,at,n,m in [('boss2_sectors',0x2df818,7,4),('boss2_muzzles',0x2df888,7,2)]:
  array('int16_t',name+f'[{n}][{m}]',','.join('{'+vals(c.signed(at+2*(i*m+j)) for j in range(m))+'}' for i in range(n)))
 array('int16_t','boss_fx_bounds[11][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x2df1f2,0x2df1fc,0x2df206,0x2df210,0x2df21a,0x2df224,0x2d403e,0x2e011a,0x5dd4c,0x2df238,0x298736)))
 array('int16_t','boss_trig[2][256]',','.join('{'+vals(c.signed(at+2*i) for i in range(256))+'}' for at in (0x2c072c,0x2c07ac)))
 ranks=(0x2bcf54,0x2bcfd6,0x2bd058)
 if any(c.word(at)!=0 for at in ranks):raise ValueError('boss rank table format')
 array('uint16_t','boss2_rank[3][8][4][2]',','.join('{'+','.join('{'+','.join('{'+vals(c.word(at+2+16*r+4*f+2*p) for p in range(2))+'}' for f in range(4))+'}' for r in range(8))+'}' for at in ranks))
 array('uint16_t','boss2_smoke_periods[11]',vals(c.word(0x2df802+2*i) for i in range(11)))
 array('uint16_t','boss2_scroll',str(c.word(0x2bcf52)))
 patches=[]
 for i in range(8):
  at=0x29856c+20*i;image=c.long(at+4)
  width=c.word(at+16)
  if c.long(at)!=0x106f6c or [c.word(at+v) for v in (12,14,18)]!=[0,0,12] or c.word(image+2)!=12 or width!=c.word(image):raise ValueError('boss background patch layout')
  tiles=c.long(image+4);array('uint16_t',f'boss2_patch_tiles_{i}[{24*width}]',vals(c.word(tiles+2*j) for j in range(24*width)))
  patches.append('{boss2_patch_tiles_'+str(i)+','+vals([c.signed(at+8),c.signed(at+10),width,12])+'}')
 array('struct BossBackgroundPatch','boss2_background[8]',','.join(patches))
 array('uint16_t','crab_attacks[8][7]',','.join('{'+vals(row)+'}' for row in crab_attacks))
 array('uint8_t','crab_delays[7][256]',','.join('{'+vals(c.byte(c.long(0x2e035a+4*i)+j) for j in range(256))+'}' for i in range(7)))
 array('int16_t','crab_bounds[2][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x2e0b5c,0x2e0340)))
 array('int16_t','crab_angles[9]',vals(c.signed(0x2e03e6+2*i) for i in range(9)))
 array('int16_t','crab_accel[3][9]',','.join('{'+vals(c.signed(c.long(0x2e03f8+4*i)+2*j) for j in range(9))+'}' for i in range(3)))
 array('int16_t','crab_offsets[3][9][2]',','.join('{'+','.join('{'+vals(c.signed(c.long(0x2e045e+4*i)+4*j+2*k) for k in range(2))+'}' for j in range(9))+'}' for i in range(3)))
 ranks=(0x2bda80,0x2bdb02)
 if any(c.word(at)!=0 for at in ranks):raise ValueError('crab rank format')
 array('uint16_t','crab_rank[2][8][4][2]',','.join('{'+','.join('{'+','.join('{'+vals(c.word(at+2+16*r+4*f+2*p) for p in range(2))+'}' for f in range(4))+'}' for r in range(8))+'}' for at in ranks))
 ranks=[0x2b91fe,0x2b9488,0x2b950a]+[c.long(0x2d4a4e+4*i) for i in range(4)]
 if any(c.word(at)!=0 for at in ranks):raise ValueError('gunship rank format')
 array('uint16_t','gunship_rank[7][8][4][2]',','.join('{'+','.join('{'+','.join('{'+vals(c.word(at+2+16*r+4*f+2*p) for p in range(2))+'}' for f in range(4))+'}' for r in range(8))+'}' for at in ranks))
 for name,at,n in [('gunship_sounds',0x2d4a22,6),('gunship_bounds',0x2d49b6,4),('gunship_depth',0x2d4a9e,4),('gunship_speeds',0x2d4aa6,32)]:
  array('uint16_t' if name=='gunship_sounds' else 'int16_t',name+f'[{n}]',vals(c.word(at+2*i) if name=='gunship_sounds' else c.signed(at+2*i) for i in range(n)))
 array('int16_t','gunship_offsets[8][4]',','.join('{'+vals(c.signed(0x2d4a5e+8*i+2*j) for j in range(4))+'}' for i in range(8)))
 at=0x6eb84
 if c.byte(at)!=128:raise ValueError('gunship contact')
 array('struct InteractionContact','gunship_contact','{'+vals(c.signed(at+6+2*i) for i in range(4))+'},'+vals([c.signed(at+2),c.byte(at+1)]))
 pending=[];seen={};path_rows=[];path_calls=[]
 def entry(at):
  if at not in seen:seen[at]=len(pending);pending.append(at)
  return seen[at]
 path_entries=[entry(c.long(0x2d4a2e+4*i)) for i in range(8)]
 for at in pending:
  op=c.byte(at);value=target=length=width=increment=angle=0;nxt=65535
  if op==0:kind='ANGLE';angle=c.byte(at+1);nxt=entry(at+2)
  elif op==1:kind='DISTANCE';length=c.word(at+2);nxt=entry(at+4)
  elif op==2:kind='ARC';length=c.word(at+4);increment=struct.unpack('b',bytes([c.byte(at+1)]))[0];angle=c.byte(at+2);nxt=entry(at+6)
  elif op==3:kind='WAIT';length=c.word(at+2);nxt=entry(at+4)
  elif op==4:kind='JUMP';target=entry(c.long(at+2))
  elif op==7:
   kind='FIELD';field=c.word(at+2)
   if field not in (0x3a,0x7f):raise ValueError('unreviewed flight field')
   target=(0x3a,0x7f).index(field);width=1;value=c.byte(at+1);nxt=entry(at+4)
  elif op==8:
   kind='CALL';callback=c.long(at+2)
   if callback not in (0x6eece,0x6f0b8,0x6f0c6,0x6f0e2,0x6f0f0):raise ValueError('unreviewed flight call')
   if callback not in path_calls:path_calls.append(callback)
   target=path_calls.index(callback);nxt=entry(at+6)
  elif op==9:kind='SOUND';value=c.word(at+2);nxt=entry(at+4)
  elif op==10:kind='FINISH'
  else:raise ValueError(f'unreviewed flight operation {op}')
  path_rows.append('{'+vals([value,nxt,target,length])+',FP_'+kind+','+vals([width,increment,angle])+'}')
 h.insert(-1,'enum GunshipPathCallback {'+','.join('GPC_'+format(x,'X') for x in path_calls)+'};')
 out.append('static const struct FlightCommand gunship_flight_commands[]={'+','.join(path_rows)+'};')
 array('struct FlightProgram','gunship_flight_program','gunship_flight_commands,'+str(len(path_rows)))
 array('uint16_t','gunship_paths[8]',vals(path_entries))
 templates={75:0x6e52a,132:0x7ba7c,138:0x7cf02,139:0x7cf0c,140:0x7dc3e}
 if any(c.long(0xe8000+k*4)!=v for k,v in templates.items()):raise ValueError('boss template')
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,tables=TABLES,patterns=PATTERNS,templates=templates,
  implementation=['src/boss_common.c','src/boss_fx.c','src/boss2.c','src/crab.c','src/gunship.c','src/flight_path.c'],
  flight_commands=[dict(source=at,native_index=i) for at,i in seen.items()],flight_callbacks=path_calls,
  source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in ((0x7ba7c,0x7dfa8),(0x77c7e,0x789f0),(0x62536,0x625da),(0x5379c,0x53884),(0x6dce0,0x6df04),(0x6e52a,0x6f162),(0x723da,0x726d4),(0x78f8a,0x79250))],
  validation='Source extraction and ARM compilation only; no gameplay or regression tests',complete_port=False)
 for name,data in [('boss-programs.h','\n'.join(h)),('boss-programs.c','\n'.join(out)),('boss-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(data+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),defenses=len(defenses),attacks=len(attacks))))
if __name__=='__main__':main()
