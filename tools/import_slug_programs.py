#!/usr/bin/env python3
"""Compile Slug body drawing dependencies and typed mount/hit data offline.

This is not a vehicle state-machine translator or a gameplay validation.
Every runtime reference is a native index; source pointers stay in the ledger.
"""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
TABLES={
 'IDLE_FLAT':(0x2792f4,5),'JUMP_CROUCH':(0x279308,5),'JUMP_LAUNCH':(0x27931c,5),
 'JUMP_LAND':(0x279330,5),'DRIVE_ALT':(0x279344,5),'BRAKE_RIGHT':(0x279358,5),
 'DESTROYED_SLIDE':(0x27936c,5),'KNOCKED':(0x279394,5),'DECEL_A':(0x2793bc,5),
 'CRUISE_RIGHT':(0x2793d0,5),'BRAKE':(0x2793e4,5),'DECEL_B':(0x2793f8,5),
 'DRIVE':(0x27940c,5),'DRIVE_B':(0x279420,5),'CROUCH_CRUISE_RIGHT':(0x279434,5),
 'CROUCH_CRUISE_LEFT':(0x279448,5),'ACCEL_RIGHT':(0x27945c,5),
 'CROUCH_ACCEL_RIGHT':(0x279470,5),'CROUCH_ACCEL_LEFT':(0x279484,5),
 'FIRE_FLAT':(0x279498,5),'FIRE_RECOIL':(0x2794e8,5),'BRAKE_LEFT':(0x2794fc,5),
 'CRUISE_LEFT':(0x279524,5),'ACCEL_LEFT':(0x279538,5),
 'HIT_REACT_A':(0x27954c,5),'HIT_REACT_B':(0x279560,5),
 'HIT_LAUNCH_A':(0x279574,5),'HIT_LAUNCH_B':(0x279588,5),
 'HIT_LAND_A':(0x27959c,5),'HIT_LAND_B':(0x2795b0,5),
 'HUNKER':(0x279600,5),'IDLE_ENTER_A':(0x279614,5),'IDLE_ENTER_B':(0x279628,5),
 'ANGLED_NORMAL':(0x2b0c30,17),'ANGLED_STALL':(0x2b0c74,17),
 'ANGLED_IDLE_ENTER':(0x2b0cb8,17),'ANGLED_SLOPE_MOUNT':(0x2b0cfc,17),
 'ANGLED_PLAYER_MOUNT':(0x2b0d40,17),'ANGLED_UNOCCUPIED':(0x2b0dc8,17),
}
FIELDS={(0x8c,1):'ACTION_FLAGS',(0x3b,1):'POSE',(0x91,1):'ACCEL_TIMER'}
CALLBACKS={0x31e5e:'DUST_FLAT',0x31e94:'DUST_DOWN',0x31ed6:'DUST_UP',
 0x31f16:'DUST_STEEP_DOWN',0x31f5c:'DUST_STEEP_UP',
 0x2a5d4:'TERRAIN_SOUND_A',0x2a606:'TERRAIN_SOUND_B',
 0x2a310:'SOUND_1092',0x2a2f8:'SOUND_108F',
 0x31d90:'SPARK',0x31dc2:'SPARK_LEFT',0x31dfe:'SPARK_RIGHT',0x2a638:'DESTROYED_BUBBLE'}
ATTACKS={'NORMAL':0x29698,'SHORT':0x2973c,'DESTROYED_A':0x29790,
 'DESTROYED_B':0x29834,'DESTROYED_C':0x298d8}
DEFENSES={'UNOCCUPIED':0x2999e,'CROUCH':0x29a14,'BASE':0x29a68}
DEFENSE_TABLES={'NORMAL':0x29ffc,'CROUCH':0x2a024,'CROUCH_A':0x2a038,'CROUCH_B':0x2a04c}
def values(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args()
 raw=(a.source/'build/mslug_prom.bin').read_bytes()
 expected=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']
 if hashlib.sha256(raw).hexdigest()!=expected:raise ValueError('P-ROM fingerprint')
 c=AnimationCompiler(raw)
 initial=c.entry(0x2792b0)
 tables=[]
 for name,(at,count) in TABLES.items():
  ptrs=[c.long(at+4*i) for i in range(count)]
  tables.append(dict(name=name,source=at,pointers=ptrs,entries=[65535 if v==0xffffffff else c.entry(v) for v in ptrs]))
 # $02E924/$02EB14 select twice: slope [0..4], then display phase [0..7].
 # A $FFFFFFFF pointer means retain the current stream, not empty animation.
 turns=[]
 for name,at in [('TO_DRIVE',0x2794d4),('TO_DRIVE_ALT',0x2794c0)]:
  rows=[]
  for i in range(5):
   sub=c.long(at+4*i);ptrs=[c.long(sub+4*j) for j in range(8)]
   rows.append(dict(source=sub,pointers=ptrs,entries=[65535 if v==0xffffffff else c.entry(v) for v in ptrs]))
  turns.append(dict(name=name,source=at,rows=rows))
 c.compile()
 field_names=[FIELDS[(v['source_offset'],v['width'])] for v in c.fields]
 callback_names=[CALLBACKS[v['source']] for v in c.callbacks]
 # $70 points to an eight-byte drawing parameter record. Its first long is
 # six signed mount coordinates; the other long is zero in this entire graph.
 mounts=[];mount_ids={};parameter_fixups=[]
 for index,encoded in enumerate(c.parameters):
  at,reserved=struct.unpack('>II',bytes.fromhex(encoded))
  if reserved:raise ValueError(f'unreviewed mount parameter {index}')
  if at not in mount_ids:
   mount_ids[at]=len(mounts);mounts.append(dict(source=at,offsets=[c.signed(at+2*i) for i in range(6)]))
  parameter_fixups.append(mount_ids[at])
 for node in c.nodes:
  if node['op']=='PARAMETERS':node['value']=parameter_fixups[node['value']]
 fixed_defenses={name:c.binding('DEFENSE',at) for name,at in DEFENSES.items()}
 defense_tables={name:[c.binding('DEFENSE',c.long(at+4*i)) for i in range(5)] for name,at in DEFENSE_TABLES.items()}
 # No attack binding occurs in these body streams; controller attacks are
 # separate data, with all records (including the group-4 projectile probe).
 attacks=[];attack_sets=[];attack_ledger=[]
 for name,at in ATTACKS.items():
  first=len(attacks);q=at
  for count in range(256):
   if c.byte(q)==255:break
   group,kind,damage,strength,shape,offscreen=struct.unpack_from('>BBhBBB',raw,c.offset(q))
   if shape not in (0,4) or group>=5 or kind>34:raise ValueError(f'attack {q:06x}')
   rect=[c.signed(q+10+2*i) for i in range(4)]
   attacks.append('{{'+values(rect)+'},'+values([damage,group,kind,strength,shape,offscreen])+'}');q+=80
  else:raise ValueError('unterminated attack set')
  attack_sets.append('{attack_records+'+str(first)+','+str(count)+'}')
  attack_ledger.append(dict(name=name,source=at,first=first,count=count))
 h=['#ifndef SLUG_PROGRAMS_H','#define SLUG_PROGRAMS_H','#include "../src/animation.h"','#include "../src/hitbox.h"',
    'struct SlugAnimationTable {const uint16_t *entries;uint8_t count;};',
    'struct SlugMount {int16_t turret_x,turret_y,secondary_x,secondary_y,cannon_x,cannon_y;};',
    'struct SlugDefenseSet {const struct HitDefense *records;uint16_t count;};',
    'enum SlugAnimationTableId {']
 h += [f' SANI_{v["name"]}={i},' for i,v in enumerate(tables)]+[' SANI_TABLE_COUNT','};']
 h += ['enum SlugAnimationField {']+[f' SAF_{n}={i},' for i,n in enumerate(field_names)]+['};']
 h += ['enum SlugAnimationCallback {']+[f' SAC_{n}={i},' for i,n in enumerate(callback_names)]+['};']
 h += ['enum SlugAttackId {']+[f' SATT_{name}={i},' for i,name in enumerate(ATTACKS)]+[' SATT_COUNT','};']
 h += ['enum SlugDefenseId {']+[f' SDEF_{name}={i},' for name,i in fixed_defenses.items()]+['};']
 h += ['enum SlugDefenseTableId {']+[f' SDEFT_{name}={i},' for i,name in enumerate(defense_tables)]+[' SDEFT_COUNT','};']
 h += [f'#define SLUG_DEFENSE_BINDINGS {len(c.bindings)}',f'#define SLUG_MOUNT_COUNT {len(mounts)}',
       f'#define SLUG_INITIAL_ENTRY {initial}',
       'extern const struct AnimationProgram slug_animation_program;',
       'extern const struct SlugAnimationTable slug_animation_tables[SANI_TABLE_COUNT];',
       'extern const uint16_t slug_turn_entries[2][5][8];',
       'extern const struct SlugMount slug_mounts[SLUG_MOUNT_COUNT];',
       'extern const struct SlugDefenseSet slug_defense_sets[SLUG_DEFENSE_BINDINGS];',
       'extern const uint16_t slug_defense_tables[SDEFT_COUNT][5];',
       'extern const struct HitAttackSet slug_attack_sets[SATT_COUNT];','#endif']
 out=c.c_source('slug_animation_program','slug-programs.h')
 for i,t in enumerate(tables):out.append(f'static const uint16_t table_{i}[]={{'+values(t['entries'])+'};')
 out.append('const struct SlugAnimationTable slug_animation_tables[]={'+','.join('{table_'+str(i)+','+str(len(t['entries']))+'}' for i,t in enumerate(tables))+'};')
 out.append('const uint16_t slug_turn_entries[2][5][8]={'+','.join('{'+','.join('{'+values(row['entries'])+'}' for row in t['rows'])+'}' for t in turns)+'};')
 out.append('const struct SlugMount slug_mounts[]={'+','.join('{'+values(v['offsets'])+'}' for v in mounts)+'};')
 defenses=[];defense_sets=[];filters={}
 for binding in c.bindings:
  if binding['kind']!='DEFENSE':raise ValueError('unexpected Slug binding')
  first=len(defenses);q=binding['source']
  for count in range(256):
   if c.byte(q)==255:break
   filter_at=c.long(q+2);filter_name='0'
   if filter_at!=0xffffffff:
    if filter_at not in filters:
     filters[filter_at]=f'defense_filter_{len(filters)}'
     out.append(f'static const uint8_t {filters[filter_at]}[36]={{'+values(c.byte(filter_at+i) for i in range(36))+'};')
    filter_name=filters[filter_at]
   rect=[c.signed(q+10+2*i) for i in range(4)]
   defenses.append('{{'+values(rect)+'},'+filter_name+','+str(c.byte(q+1))+'}');q+=80
  else:raise ValueError('unterminated defense set')
  binding['defense_count']=count;defense_sets.append('{defense_records+'+str(first)+','+str(count)+'}')
 out += ['static const struct HitDefense defense_records[]={'+','.join(defenses)+'};',
         'const struct SlugDefenseSet slug_defense_sets[]={'+','.join(defense_sets)+'};',
         'const uint16_t slug_defense_tables[SDEFT_COUNT][5]={'+','.join('{'+values(row)+'}' for row in defense_tables.values())+'};',
         'static const struct HitAttack attack_records[]={'+','.join(attacks)+'};',
         'const struct HitAttackSet slug_attack_sets[]={'+','.join(attack_sets)+'};']
 ledger=c.ledger();ledger.update(prom_sha256=expected,initial=dict(source=0x2792b0,entry=initial),
  tables=tables,turn_tables=turns,mounts=mounts,parameter_to_mount=parameter_fixups,
  fixed_defenses=fixed_defenses,defense_tables=defense_tables,attacks=attack_ledger,
  complete=False,status='Body animation dependencies compiled; native animation owner/motion wrappers implemented separately; mounted locomotion/hit-reaction controller implemented separately; remaining controller families, rendering and gameplay integration pending')
 (R/'generated/slug-programs.h').write_text('\n'.join(h)+'\n')
 (R/'generated/slug-programs.c').write_text('\n'.join(out)+'\n')
 (R/'generated/slug-program-source.json').write_text(json.dumps(ledger,indent=2)+'\n')
 print(json.dumps(dict(tables=len(tables),turn_selectors=80,commands=len(c.nodes),drawings=len(c.drawings),
  fields=field_names,callbacks=callback_names,mounts=len(mounts),defense_sets=len(defense_sets),attack_records=len(attacks))))
if __name__=='__main__':main()
