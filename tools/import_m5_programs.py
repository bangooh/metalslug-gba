#!/usr/bin/env python3
"""Compile reviewed M5 animation, rank and authored flight data offline."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(INTRO=0x2d889e,ENTRANCE=0x2d5e02,IDLE=0x2d55ce,PICK=0x2d55da,PICK_UP=0x2d5666,
 ATTACK_A=0x2d5e7a,END_A=0x2d6002,ATTACK_B=0x2d5a78,END_B=0x2d5c00,ATTACK_C=0x2d5cf2,DIE=0x2d60f6,
 ROTOR=0x2d61a8,ROTOR_FAST=0x2d638a,PROJECTILE=0x2d6d70,FX_IDLE=0x2d81f8,FX_A=0x2d78e8,FX_B=0x2d6fcc,
 SPARK=0x2d8204,MUZZLE=0x2d82ba,MUZZLE_END=0x2d8320,SOLDIER=0x29b816,TURN=0x29bfb8,THROW=0x29ba70,
 CREW_ENTER=0x2e2286,CREW_RAISE=0x29bd40,GRENADIER=0x2d83ae,HURT=0x2d8714,EJECT=0x2d875c,
 BOUNCE=0x28732c,SETTLED=0x28735e,FX_8770=0x2d8770,FX_87AE=0x2d87ae,WIND=0x2d87d8,
 SMOKE=0x2dd8b6,SMOKE_SYNC=0x2dd944,ROCKET=0x2d883c,SHOT_FLASH=0x288200,EJECT_PIECE=0x288320,
 TC_IDLE=0x290f6e,TC_EMPTY=0x290f0e,TC_RECOIL=0x29bfc4,TC_BODY=0x2912d6,TC_BODY_FIRE=0x291526,
 TC_DRIVER=0x29276a,TC_READY=0x292a48,TC_FIRE=0x292c92,TC_CANNON=0x2927f8,TC_SHELL=0x2911bc,
 ST_PART=0x2c8536,ST_HOLD=0x2c8542,ST_SWAY=0x2c85a2,ST_LIGHT=0x2c8506,ST_LIGHT_ON=0x2c8512,
 ST_HULL=0x2c84fa,ST_NOSE=0x2c84ee,ST_HATCH=0x2c85e0)
TABLES=dict(PART=(0x2d5372,16),HOVER=(0x2d53b2,4),BOMB=(0x2d53c2,4),THROW=(0x2d5402,6),SHOT=(0x2861bc,6),ST_OPEN=(0x2c8626,6),ST_CLOSE=(0x2c8686,6))
RANKS=dict(HP=0x2b9b22,BOMB_HP=0x2b9c24,BOMB_SPEED=0x2b9ca6,CREW_COUNT=0x2b9d28,
 CREW_WAIT=0x2b9dfa,GRENADE_WAIT=0x2b9ecc,GRENADE_COUNT=0x2b9f4e,GRENADE_REPEAT=0x2b9fd0,
 CHOICE0=0x2ba0e4,CHOICE1=0x2ba166,ROCKET_COUNT=0x2ba37e,ROCKET_WAIT=0x2ba410,
 SLUG_COUNT=0x2ba492,SLUG_WAIT=0x2ba514,GATE=0x2ba596,SHOT_SPEED=0x2ba052,
 TC_HP=0x2b7798,TC_SPEED=0x2b781a,TC_DRIVER_HP=0x2b789c,TC_WAIT=0x2b791e)
# Sector $76 has 32 projectile directions; the grenade aim quantizer clamps
# to 0..10. Each reviewed stream contains precisely this many 18-byte rows.
SELECTORS={0x2d78ea:32,0x2d6fce:32,0x2d83bc:11,0x2d6d84:32,0x2d7b2e:32,
 0x2d7212:32,0x2d7d72:32,0x2d7456:32,0x2d7fb6:32,0x2d769a:32}
SELECTORS.update({at:32 for at in (0x292a4a,0x292c94,0x290f74,0x2912de,0x29152e,
 0x292800,0x292ed8,0x291772,0x29311c,0x2919bc,0x293360,0x291c00,0x2935a4,
 0x291e4a,0x2937e8,0x29208e,0x293a2c,0x2922d8,0x293c70,0x29251c)})
def vals(xs):return ','.join(map(str,xs))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);args=p.parse_args()
 raw=(args.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw,SELECTORS);entries={k:c.entry(v) for k,v in ROOTS.items()}
 tables={k:[65535 if c.long(at+4*i)==0xffffffff else c.entry(c.long(at+4*i)) for i in range(n)] for k,(at,n) in TABLES.items()}
 body=c.binding('DEFENSE',0x2d50c6);shot_def=c.binding('DEFENSE',0x285e12);shot_attack=c.binding('ATTACK',0x285fb6)
 stage_def=c.binding('DEFENSE',0x2c8466);turret_attack=c.binding('ATTACK',0x293ed8);c.compile()
 expected_calls={0x70e78,0x70ee0,0x4a110,0x4a120,0x420e4,0x71038,0x283ca,0x42366,0x70f22,0x70ed6,
  0x70f04,0x55bce,0x70ef2,0x70ebe,0x70e3c,0x70f4c,0x70f70,0x70fe8,0x70f9a,0x70fbe}
 expected_fields={(0x3a,1),(0x3b,1),(0x34,1),(0x85,1),(0x76,2),(0x34,2),(0x84,1),(0x8a,1),(0x20,1)}
 if {x['source'] for x in c.callbacks}!=expected_calls or c.parameters:raise ValueError('unreviewed M5 callback/parameter')
 if {(x['source_offset'],x['width']) for x in c.fields}!=expected_fields:raise ValueError('unreviewed M5 field')
 templates={29:0x4cbea,111:0x6f1f6,112:0x6f17e,113:0x6f198}
 if any(c.long(0xe8000+4*i)!=at for i,at in templates.items()):raise ValueError('M5 template mapping')
 h=['#ifndef NATIVE_M5_PROGRAMS_H','#define NATIVE_M5_PROGRAMS_H','#include "../src/bosses.h"','#include "../src/scene_video.h"',
 'enum M5Animation {'+','.join('M5A_'+k for k in entries)+',M5A_COUNT};',
 'enum M5Callback {'+','.join('M5C_'+format(v['source'],'X') for v in c.callbacks)+'};',
 'enum M5Field {'+','.join('M5F_'+format(v['source_offset'],'X')+'_'+str(v['width']) for v in c.fields)+'};',
 'enum M5RankIndex {'+','.join('M5R_'+k for k in RANKS)+',M5R_IDLE0,M5R_IDLE1,M5R_IDLE2,M5R_IDLE3,M5R_COUNT};',
 'struct M5Binding {const struct HitDefense*defense;const struct HitAttack*attack;const struct InteractionContact*contact;uint16_t count;};',
 'struct M5Rank {const uint16_t*values;const uint8_t*choices;};',
 '#define M5_BODY_DEFENSE '+str(body),
 '#define M5_SHOT_DEFENSE '+str(shot_def),'#define M5_SHOT_ATTACK '+str(shot_attack),
 '#define M5_STAGE_DEFENSE '+str(stage_def),'#define M5_TURRET_ATTACK '+str(turret_attack),
 'extern const struct AnimationProgram m5_program;','extern const uint16_t m5_entries[M5A_COUNT];',
 'extern const struct M5Binding m5_bindings['+str(len(c.bindings))+'];',
 *['extern const uint16_t m5_'+k.lower()+'['+str(n)+'];' for k,(_,n) in TABLES.items()],
 'extern const struct M5Rank m5_rank[M5R_COUNT];',
 'extern const int16_t m5_hp[16][4],m5_bounds[2][4],m5_targets[5][8][2],m5_spark_offsets[32][2];',
 'extern const uint16_t m5_palettes[2][8],m5_intro_speed,m5_shot_angles[6];','extern const int16_t m5_shot_offsets[6][2];',
 'extern const uint8_t m5_motion[8],m5_actions[8];',
 'extern const int16_t m5_turret_offsets[3][32][2],m5_turret_pose[32],m5_turret_bounds[3][4];',
 'extern const struct SceneTilePatch m5_turret_death_patch;',
 'struct M5TerrainStamp {int16_t dx,dy;uint16_t width,height;const uint8_t*cells;};',
 'extern const struct M5TerrainStamp m5_nose_stamp;',
 'extern const struct FlightProgram m5_flight_program;','extern const uint16_t m5_paths[8];']
 out=c.c_source('m5_program','m5-programs.h');defs=[];attacks=[];contacts=[];sets=[];filters={}
 def array(ty,name,data):out.append('const '+ty+' '+name+'={'+data+'};')
 for b in c.bindings:
  q=b['source']
  if b['kind']=='CONTACT':
   if c.byte(q)!=128:raise ValueError('contact kind')
   n=len(contacts);contacts.append('{{'+vals(c.signed(q+6+2*i) for i in range(4))+'},'+vals([c.signed(q+2),c.byte(q+1)])+'}')
   sets.append('{0,0,contacts+'+str(n)+',1}');continue
  records=defs if b['kind']=='DEFENSE' else attacks;first=len(records)
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
    if group>4 or kind>34 or shape not in (0,4):raise ValueError('attack record')
    records.append('{'+rect+','+vals([damage,group,kind,strength,shape,offscreen])+'}')
   else:raise ValueError(b)
   q+=80
  else:raise ValueError('unterminated hit table')
  sets.append('{'+('defs+'+str(first)+',0' if b['kind']=='DEFENSE' else '0,attacks+'+str(first))+',0,'+str(count)+'}')
 out+=['static const struct HitDefense defs[]={'+','.join(defs)+'};','static const struct HitAttack attacks[]={'+','.join(attacks)+'};']
 if contacts:out.append('static const struct InteractionContact contacts[]={'+','.join(contacts)+'};')
 out.append('const struct M5Binding m5_bindings[]={'+','.join(sets)+'};')
 array('uint16_t','m5_entries[M5A_COUNT]',vals(entries.values()))
 for k,v in tables.items():array('uint16_t','m5_'+k.lower()+'['+str(len(v))+']',vals(v))
 rank_rows=[];rank_sources=list(RANKS.values())+[c.long(0x2d53f2+4*i) for i in range(4)]
 for i,at in enumerate(rank_sources):
  mode=c.word(at)
  if mode==2:
   choices=[c.byte(at+2+j) for j in range(64)]
   array('uint8_t',f'm5_rank_choices_{i}[64]',vals(choices))
   array('uint16_t',f'm5_rank_values_{i}[{(max(choices)+1)*8}]',vals(c.word(at+66+2*j) for j in range((max(choices)+1)*8)))
   rank_rows.append('{m5_rank_values_'+str(i)+',m5_rank_choices_'+str(i)+'}')
  else:
   if mode not in (0,1):raise ValueError('rank mode')
   array('uint16_t',f'm5_rank_values_{i}[64]',vals(c.word(at+2+2*j) for j in range(64)))
   rank_rows.append('{m5_rank_values_'+str(i)+',0}')
 array('struct M5Rank','m5_rank[M5R_COUNT]',','.join(rank_rows))
 array('int16_t','m5_hp[16][4]',','.join('{'+vals(c.signed(0x2b9ba4+8*i+2*j) for j in range(4))+'}' for i in range(16)))
 array('int16_t','m5_bounds[2][4]',','.join('{'+vals(c.signed(at+2*j) for j in range(4))+'}' for at in (0x2d5316,0x2d531e)))
 array('int16_t','m5_targets[5][8][2]',','.join('{'+','.join('{'+vals(c.signed(at+4*i+2*j) for j in range(2))+'}' for i in range(8))+'}' for at in (0x2d547e,0x2d549e,0x2d54be,0x2d54fe,0x2d551e)))
 array('int16_t','m5_spark_offsets[32][2]',','.join('{'+vals(c.signed(0x2d554e+4*i+2*j) for j in range(2))+'}' for i in range(32)))
 array('uint16_t','m5_palettes[2][8]',','.join('{'+vals(c.word(at+2*i) for i in range(8))+'}' for at in (0x2d552e,0x2d553e)))
 array('uint16_t','m5_intro_speed',str(c.word(0x400)))
 stamp=0x2c84c2;sw=c.word(stamp+4);sh=c.word(stamp+6)
 array('uint8_t',f'm5_nose_cells[{sw*sh}]',vals(c.byte(stamp+8+i) for i in range(sw*sh)))
 array('struct M5TerrainStamp','m5_nose_stamp',vals([c.signed(stamp),c.signed(stamp+2),sw,sh])+',m5_nose_cells')
 array('int16_t','m5_turret_offsets[3][32][2]',','.join('{'+','.join('{'+vals(c.signed(at+4*i+2*j) for j in range(2))+'}' for i in range(32))+'}' for at in (0x294044,0x2940c4,0x294144)))
 # $04D034 compound-owner turret destruction patch.
 at=0x295682;target=c.long(at);im=c.long(at+4);x,y,sx,sy,width,height=[c.signed(at+j) for j in range(8,20,2)]
 iw,ih=c.word(im),c.word(im+2);stream=c.long(im+4)
 if target not in (0x106f6c,0x107fe8) or min(sx,sy)<0 or min(width,height)<=0 or sx+width>iw or sy+height>ih:raise ValueError('turret patch bounds')
 cells=[c.word(stream+4*((sx+col)*ih+sy+row)+2*k) for col in range(width) for row in range(height) for k in range(2)]
 array('uint16_t','m5_turret_patch_cells[]',vals(cells));array('struct SceneTilePatch','m5_turret_death_patch','m5_turret_patch_cells,'+vals([x,y,width,height,0 if target==0x106f6c else 1]))
 array('int16_t','m5_turret_pose[32]',vals(c.signed(0x2941c4+2*i) for i in range(32)))
 array('int16_t','m5_turret_bounds[3][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x293fd4,0x293fde,0x293fe8)))
 array('uint16_t','m5_shot_angles[6]',vals(c.word(0x2861ec+2*i) for i in range(6)))
 array('int16_t','m5_shot_offsets[6][2]',','.join('{'+vals(c.signed(0x2861d4+4*i+2*j) for j in range(2))+'}' for i in range(6)))
 motion=[0x70aee,0x70b70,0x70b78,0x70b80,0x70b86,0x70bc2]
 array('uint8_t','m5_motion[8]',vals(motion.index(c.long(0x2d541a+4*i)) for i in range(8)))
 actions=[0xffffffff,0x6f416,0x6f902,0x6f59a,0x6f67e,0x6f7f0,0x6f590]
 array('uint8_t','m5_actions[8]',vals(actions.index(c.long(0x2d53d2+4*i)) for i in range(8)))
 pending=[];seen={};path_rows=[];calls=[];fields=[]
 def entry(at):
  if at not in seen:seen[at]=len(pending);pending.append(at)
  return seen[at]
 paths=[entry(c.long(0x2d545e+4*i)) for i in range(8)]
 for at in pending:
  op=c.byte(at);value=target=length=width=increment=angle=0;nxt=65535
  if op==0:kind='ANGLE';angle=c.byte(at+1);nxt=entry(at+2)
  elif op==1:kind='DISTANCE';length=c.word(at+2);nxt=entry(at+4)
  elif op==2:kind='ARC';length=c.word(at+4);increment=struct.unpack('b',bytes([c.byte(at+1)]))[0];angle=c.byte(at+2);nxt=entry(at+6)
  elif op==3:kind='WAIT';length=c.word(at+2);nxt=entry(at+4)
  elif op==4:kind='JUMP';target=entry(c.long(at+2))
  elif op in (5,6,7):
   kind='FIELD';width={5:4,6:2,7:1}[op];value=c.byte(at+1) if width==1 else c.word(at+2) if width==2 else c.long(at+4)
   field=c.word(at+(4 if width==2 else 2));key=(field,width)
   if key==(0,4):value=[0x6ff32,0x6fef0].index(value)
   elif key!=(0x89,1):raise ValueError('unreviewed M5 path field')
   if key not in fields:fields.append(key)
   target=fields.index(key);nxt=entry(at+(4 if width==1 else 6 if width==2 else 8))
  elif op==8:
   kind='CALL';callback=c.long(at+2)
   if callback not in calls:calls.append(callback)
   target=calls.index(callback);nxt=entry(at+6)
  elif op==9:kind='SOUND';value=c.word(at+2);nxt=entry(at+4)
  elif op==10:kind='FINISH'
  else:raise ValueError(f'flight operation {op} at {at:x}')
  path_rows.append('{'+vals([value,nxt,target,length])+',FP_'+kind+','+vals([width,increment,angle])+'}')
 if calls:h.append('enum M5PathCallback {'+','.join('M5PC_'+format(x,'X') for x in calls)+'};')
 if fields:h.append('enum M5PathField {'+','.join('M5PF_'+format(x,'X')+'_'+str(y) for x,y in fields)+'};')
 out.append('static const struct FlightCommand m5_flight_commands[]={'+','.join(path_rows)+'};')
 array('struct FlightProgram','m5_flight_program','m5_flight_commands,'+str(len(path_rows)));array('uint16_t','m5_paths[8]',vals(paths));h.append('#endif')
 source_files=['asm/enemies/bosses/gunship_m5boss_finalboss_06dfe8.s','asm/enemies/vehicles/turret_car_props_04cbd4.s',
  'asm/enemies/soldiers/barrel_paratrooper_shield_tank_066000.s','asm/video/camera_relink_wrap_scroll_06896a.s']
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,tables=TABLES,rank_sources=rank_sources,templates=templates,
  implementation=['src/m5.c','src/m5_children.c','src/m5_stage.c','src/m5_turret.c'],
  source_files={file:hashlib.sha256((args.source/file).read_bytes()).hexdigest() for file in source_files},
  source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in ((0x6f17e,0x71068),(0x682d0,0x68ba2),(0x4cbd4,0x4d6ec),(0x415a2,0x417ac),(0x42188,0x42206),(0x41bc6,0x41c1a))],
  path_fields=fields,path_callbacks=calls,path_commands=[dict(source=at,native_index=i) for at,i in seen.items()],
  validation='Source conversion only, not gameplay validation',complete_port=False)
 for name,data in [('m5-programs.h','\n'.join(h)),('m5-programs.c','\n'.join(out)),('m5-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(data+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),fields=c.fields,callbacks=c.callbacks,path_fields=fields,path_calls=calls)))
if __name__=='__main__':main()
