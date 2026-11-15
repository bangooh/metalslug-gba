#!/usr/bin/env python3
"""Compile reviewed tank/attached-bazooka data; runtime uses semantic C states."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(PHASE2=0x2db5a4,DIE=0x2db6ce,SINK=0x2db6de,HULL=0x2db6ee,CABIN=0x2db732,LAUNCHER=0x2db8d6,
 TURRET=0x2dba40,AIM=0x2dba5a,RECOIL=0x2dba66,WRECK=0x2dba94,GUN=0x2dbabc,CASING=0x2dbd1c,GROUND=0x2dbddc,
 LAUNCH=0x2dbdee,ROCKET=0x2dbe40,TRAIL=0x2dc2e0,MUZZLE=0x2dc374,MUZZLE_B=0x2dc406,MUZZLE_C=0x2dc4bc,MUZZLE_LOOP=0x2dc50c,MUZZLE_END=0x2dc5ae,
 FRAGMENT_SMOKE=0x2d4628,SMOKE=0x2dc616,FRAGMENT=0x2d45fc,CREW_IDLE=0x2ca804,CREW_FIRE=0x2cacc0,CREW_HURT=0x2cae00,CREW_DIE=0x2caf58,
 ALLY_IDLE=0x2ca832,WEAPON=0x2cb082,WEAPON_HURT=0x2cb8ce,WEAPON_DIE=0x2cc1f0,CREW_MUZZLE=0x2dd6d2,CREW_ROCKET=0x2cc686)
ROOTS.update(BZ_IDLE=0x2ca84a,BZ_WALK=0x2ca690,BZ_BACK=0x2ca74a,BZ_STOP=0x2ca996,BZ_STOP_BACK=0x2ca898,BZ_TURN=0x2caa94,BZ_TURN_BACK=0x2cabaa,BZ_FIRE_MOVE=0x2cad10)
TABLES=dict(TURN=(0x2db0fc,8),DRIVE=(0x2db11c,8),RETREAT=(0x2db13c,8),HULL=(0x2db15c,7),CABIN=(0x2db178,7),LAUNCHER=(0x2db194,7),GUN=(0x2db1b0,7),GUN_FIRE=(0x2db1cc,7),WEAPON_FIRE=(0x2cb1ac,16))
RANKS=dict(HP=0x2baef8,THRESHOLD=0x2baf7a,TURRET_HP=0x2baffc,MINIMUM=0x2bb07e,MUZZLE_COUNT=0x2bb100,AIM_WAIT=0x2bb182,RECOIL_WAIT=0x2bb204,
 ROCKET_COUNT=0x2bb286,ROCKET_WAIT=0x2bb308,ROCKET_REPEAT=0x2bb38a,GUN_WAIT=0x2bb40c,GUN_LATE=0x2bb48e,GUN_EARLY=0x2bb510,GUN_COUNT=0x2bb592,GUN_CHOICE=0x2bb614,
 CREW_HP=0x2bd3e6,CREW_WAIT=0x2bd4ea,CREW_REPEAT=0x2bd56c,CREW_COUNT=0x2bd5ee)
RANKS.update(BZ_SPEED=0x2bd468,BZ_IDLE=0x2bd670,BZ_B_SPEED=0x2bd774,BZ_B_WAIT=0x2bd7f6,BZ_B_HP=0x2bd6f2,BZ_B_ALERT=0x2bd8fa,BZ_B_REPEAT=0x2bd878,BZ_B_COUNT=0x2bd97c,BZ_B_IDLE=0x2bd9fe)
# Weapon angle is masked to 15. Flying projectiles use angle >> 3 (0..31).
SELECTORS={a:16 for a in (0x2cb084,0x2cb8d0,0x2cc1f2,0x2cb9f4,0x2cc316,0x2cbb18,0x2cc43a,0x2cbc3c,0x2cc55e,0x2cbd60,0x2cbe84,0x2cbfa8,0x2cc0cc)}
SELECTORS.update({a:32 for a in (0x2dbe54,0x2dc098,0x2cc698)})
FIXED=dict(BODY_DEF=('DEFENSE',0x2da784),IDLE_ATK=('ATTACK',0x2dab18),MOVE_ATK=('ATTACK',0x2daa24),HULL_DEF=('DEFENSE',0x2dadb4),
 MUZZLE_ATK=('ATTACK',0x2daf54),FLAME_ATK=('ATTACK',0x2daeb0),CREW_DEF=('DEFENSE',0x2ca1e4))
FIXED.update(BZ_ATTACK_A=('ATTACK',0x2ca238),BZ_ATTACK_B=('ATTACK',0x2ca2d8))
def vals(xs):return ','.join(map(str,xs))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw,SELECTORS);entries={k:c.entry(v) for k,v in ROOTS.items()}
 tables={k:[65535 if c.long(at+4*i)==0xffffffff else c.entry(c.long(at+4*i)) for i in range(n)] for k,(at,n) in TABLES.items()}
 fixed={k:c.binding(*v) for k,v in FIXED.items()};c.compile()
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=c.callbacks,fields=c.fields,parameters=c.parameters)))
 expected_calls={0x7458a,0x74408,0x74416,0x743ca,0x283ca,0x6b974,0x743de,0x6e4cc,0x7439a,0x743ac,0x6b9a4}
 expected_fields={(0x76,1),(0x9b,1),(0x7a,1),(0x74,1),(0x7b,1),(0x77,1),(0x34,2),(0x7e,2),(0x75,1),(0x7c,2)}
 if {x['source'] for x in c.callbacks}!=expected_calls or c.parameters or {(x['source_offset'],x['width']) for x in c.fields}!=expected_fields:raise ValueError('unreviewed animation graph')
 # Both watched scripts have one random branch. Keep source bytes and
 # actor dispatch fixed until their semantic consumers are reviewed again.
 for at,repeats,period in ((0xee1b4,2,40),(0xee1da,1,100)):
  expected=bytes([1,repeats,0x10,8,4,0,period,20,0,0,3,0,0,0,0,0x80,0,0,0,228,1,0x50,0,0xc8]+[0]*8+[14,0,12,0,255,255])
  if raw[at:at+38]!=expected:raise ValueError('watch script changed')
 normal=[0x6acca,0x6ad14,0x6ad9a,0x6adf6,0x6ae74,0x6af18,0x6af68,0x6a460]
 fixed_weapon=[0x6aebc,0x6aebc,0x6ad9a,0x6aebc,0x6aebc,0x6af18,0x6af68,0x6a460]
 if [c.long(0x2ca644+4*i) for i in range(8)]!=normal or [c.long(0x2ca664+4*i) for i in range(8)]!=fixed_weapon:raise ValueError('weapon dispatch')
 templates={118:0x72fc8,119:0x73fca,64:0x6aa14,65:0x6ac5c,86:0x6a08a,87:0x6a07e,90:0x6a476,91:0x6a486,92:0x6a7bc,93:0x6a7ca}
 if any(c.long(0xe8000+4*i)!=at for i,at in templates.items()):raise ValueError('template mapping')
 h=['#ifndef NATIVE_TANK_PROGRAMS_H','#define NATIVE_TANK_PROGRAMS_H','#include "../src/bosses.h"',
 'enum TankAnimation {'+','.join('TKA_'+k for k in entries)+',TKA_COUNT};',
 'enum TankCallback {'+','.join('TKC_'+format(v['source'],'X') for v in c.callbacks)+'};',
 'enum TankField {'+','.join('TKF_'+format(v['source_offset'],'X')+'_'+str(v['width']) for v in c.fields)+'};',
 'enum TankRank {'+','.join('TKR_'+k for k in RANKS)+',TKR_COUNT};',
 'enum TankBinding {'+','.join('TKB_'+k+'='+str(v) for k,v in fixed.items())+'};',
 'extern const struct AnimationProgram tank_program;','extern const uint16_t tank_entries[TKA_COUNT];',
 'extern const struct BossBindingSet tank_bindings['+str(len(c.bindings))+'];','struct TankRankData {const uint16_t*values;const uint8_t*choices;};','extern const struct TankRankData tank_rank[TKR_COUNT];',
 *['extern const uint16_t tank_'+k.lower()+'['+str(n)+'];' for k,(_,n) in TABLES.items()],
 'extern const struct InteractionContact tank_contacts[3],tank_bazooka_contact;','extern const uint8_t tank_bazooka_ranges[4][5];','extern const int16_t tank_bounds[4][4],tank_fragment_motion[2][4][8][2],tank_crew_motion[8][16][3],tank_crew_offsets[16][2];',
 'extern const uint16_t tank_path_map[2][16],tank_crew_palettes[4];','extern const uint8_t tank_trail[9],tank_crew_angles[2][33],tank_mirror[16];',
 'extern const struct FlightProgram tank_flight_program;','extern const uint16_t tank_paths[16];',
 'struct TankTerrainStamp {int16_t dx,dy;uint16_t width,height;const uint8_t*cells;};','extern const struct TankTerrainStamp tank_stamps[2];']
 out=c.c_source('tank_program','tank-programs.h');defs=[];attacks=[];sets=[];filters={}
 def array(ty,name,data):out.append('const '+ty+' '+name+'={'+data+'};')
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
 array('struct BossBindingSet','tank_bindings[]',','.join(sets));array('uint16_t','tank_entries[TKA_COUNT]',vals(entries.values()))
 for k,v in tables.items():array('uint16_t','tank_'+k.lower()+'['+str(len(v))+']',vals(v))
 rank_rows=[]
 for i,at in enumerate(RANKS.values()):
  mode=c.word(at)
  if mode==2:
   choices=[c.byte(at+2+j) for j in range(64)]
   array('uint8_t',f'tank_choices{i}[64]',vals(choices))
   array('uint16_t',f'tank_values{i}[{(max(choices)+1)*8}]',vals(c.word(at+66+2*j) for j in range((max(choices)+1)*8)))
   rank_rows.append('{tank_values'+str(i)+',tank_choices'+str(i)+'}')
  else:
   if mode not in (0,1):raise ValueError('rank mode')
   array('uint16_t',f'tank_values{i}[64]',vals(c.word(at+2+2*j) for j in range(64)))
   rank_rows.append('{tank_values'+str(i)+',0}')
 array('struct TankRankData','tank_rank[TKR_COUNT]',','.join(rank_rows))
 contacts=[]
 for at in (0x2db018,0x2db064,0x2db0b0):
  if c.byte(at)!=128:raise ValueError('contact format')
  contacts.append('{{'+vals(c.signed(at+6+2*i) for i in range(4))+'},'+vals([c.signed(at+2),c.byte(at+1)])+'}')
 array('struct InteractionContact','tank_contacts[3]',','.join(contacts))
 if c.byte(0x2ca198)!=128:raise ValueError('bazooka contact')
 array('struct InteractionContact','tank_bazooka_contact','{'+vals(c.signed(0x2ca19e+2*i) for i in range(4))+'},'+vals([c.signed(0x2ca19a),c.byte(0x2ca199)]))
 array('uint8_t','tank_bazooka_ranges[4][5]',','.join('{'+vals(c.byte(c.long(0x2ca44c+4*i)+j) for j in range(5))+'}' for i in range(4)))
 array('int16_t','tank_bounds[4][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x2db010,0x2d404e,0x2ca3d0,0x2ca3d8)))
 array('int16_t','tank_fragment_motion[2][4][8][2]',','.join('{'+','.join('{'+','.join('{'+vals(c.signed(c.long(at+4*i)+4*j+2*k) for k in range(2))+'}' for j in range(8))+'}' for i in range(4))+'}' for at in (0x2bb696,0x2bb6a6)))
 array('int16_t','tank_crew_motion[8][16][3]',','.join('{'+','.join('{'+vals(c.signed(c.long(0x2ca4b0+4*i)+6*j+2*k) for k in range(3))+'}' for j in range(16))+'}' for i in range(8)))
 array('int16_t','tank_crew_offsets[16][2]',','.join('{'+vals(c.signed(0x2ca470+4*i+2*j) for j in range(2))+'}' for i in range(16)))
 array('uint16_t','tank_path_map[2][16]',','.join('{'+vals(c.word(at+2*i) for i in range(16))+'}' for at in (0x2db228,0x2db248)))
 array('uint16_t','tank_crew_palettes[4]',vals(c.word(0x2ca404+2*i) for i in range(4)))
 array('uint8_t','tank_crew_angles[2][33]',','.join('{'+vals(c.byte(at+i) for i in range(33))+'}' for at in (0x2ca5f0,0x2ca612)))
 array('uint8_t','tank_mirror[16]',vals(c.byte(0x2ca634+i) for i in range(16)))
 array('uint8_t','tank_trail[9]',vals(c.byte(0x74460+i) for i in range(9)))
 stamps=[]
 for i,at in enumerate((0x2db2de,0x2db364)):
  sw,sh=c.word(at+4),c.word(at+6)
  array('uint8_t',f'tank_stamp_cells{i}[{sw*sh}]',vals(c.byte(at+8+j) for j in range(sw*sh)))
  stamps.append('{'+vals([c.signed(at),c.signed(at+2),sw,sh])+f',tank_stamp_cells{i}'+'}')
 array('struct TankTerrainStamp','tank_stamps[2]',','.join(stamps))
 pending=[];seen={};rows=[];calls=[];fields=[]
 def entry(at):
  if at not in seen:seen[at]=len(pending);pending.append(at)
  return seen[at]
 paths=[entry(c.long(0x2db1e8+4*i)) for i in range(16)]
 for at in pending:
  op=c.byte(at);value=target=length=width=increment=angle=0;nxt=65535
  if op==0:kind='ANGLE';angle=c.byte(at+1);nxt=entry(at+2)
  elif op==1:kind='DISTANCE';length=c.word(at+2);nxt=entry(at+4)
  elif op==2:kind='ARC';length=c.word(at+4);increment=struct.unpack('b',bytes([c.byte(at+1)]))[0];angle=c.byte(at+2);nxt=entry(at+6)
  elif op==3:kind='WAIT';length=c.word(at+2);nxt=entry(at+4)
  elif op==4:kind='JUMP';target=entry(c.long(at+2))
  elif op in (5,6,7):
   kind='FIELD';width={5:4,6:2,7:1}[op];value=c.byte(at+1) if width==1 else c.word(at+2) if width==2 else c.long(at+4);key=(c.word(at+(4 if width==2 else 2)),width)
   if key==(0,4):value=[0x73d24,0x73d84].index(value)
   if key not in fields:fields.append(key)
   target=fields.index(key);nxt=entry(at+(4 if width==1 else 6 if width==2 else 8))
  elif op==8:
   kind='CALL';callback=c.long(at+2)
   if callback not in calls:calls.append(callback)
   target=calls.index(callback);nxt=entry(at+6)
  elif op==9:kind='SOUND';value=c.word(at+2);nxt=entry(at+4)
  elif op==10:kind='FINISH'
  else:raise ValueError(f'path operation {op} at {at:x}')
  rows.append('{'+vals([value,nxt,target,length])+',FP_'+kind+','+vals([width,increment,angle])+'}')
 if fields!=[(0x36,2),(0,4)] or calls:raise ValueError('unreviewed tank path')
 print('path',len(rows),'fields',fields,'calls',[hex(x) for x in calls])
 if fields:h.append('enum TankPathField {'+','.join('TKPF_'+format(x,'X')+'_'+str(y) for x,y in fields)+'};')
 if calls:h.append('enum TankPathCallback {'+','.join('TKPC_'+format(x,'X') for x in calls)+'};')
 out.append('static const struct FlightCommand tank_flight_commands[]={'+','.join(rows)+'};')
 array('struct FlightProgram','tank_flight_program','tank_flight_commands,'+str(len(rows)));array('uint16_t','tank_paths[16]',vals(paths));h.append('#endif')
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,tables=TABLES,rank_sources=RANKS,templates=templates,
  path_fields=fields,path_callbacks=calls,path_commands=[dict(source=at,native_index=i) for at,i in seen.items()],
  source_files={p:hashlib.sha256((a.source/p).read_bytes()).hexdigest() for p in ['asm/enemies/bosses/m5tank_finalboss_scriptedprop_071ffc.s','asm/enemies/soldiers/bazooka_rocketvehicle_walker_06a000.s','asm/video/sprite_multi_blit_043fac.s','asm/mission/event_vm_04422a.s']},
  source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in ((0x72fc8,0x745be),(0x6a07e,0x6ba2c),(0xee1b4,0xee200))],
  implementation=['src/tank.c','src/tank_parts.c','src/tank_crew.c','src/bazooka.c'],validation='Source conversion only; not gameplay validation',complete_port=False)
 for name,data in [('tank-programs.h','\n'.join(h)),('tank-programs.c','\n'.join(out)),('tank-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(data+'\n')
if __name__=='__main__':main()
