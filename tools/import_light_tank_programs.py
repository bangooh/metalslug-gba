#!/usr/bin/env python3
"""Native light tanks 53..55, attached crew and complete local ordnance graph."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(IDLE=0x2c8cce,DRIVE=0x2c8d6a,REVERSE=0x2c8eba,BRAKE=0x2c8fb0,RESUME=0x2c90f4,AIM_HIGH=0x2c922a,AIM_LOW=0x2c92d8,FIRE_LOW=0x2c94a8,FIRE_HIGH=0x2c961e,CLOSE=0x2c93fa,HIT_BIG=0x2c97ca,HIT=0x2c9710,DEAD=0x2c9938,DEAD_BIG=0x2c99de,DRIVER_OUT0=0x2c9b2c,DRIVER_OUT1=0x2c9b88,DRIVER_HIT=0x2c997c,DRIVER_DIE=0x4acfe,SMOKE=0x2c9bbe,SMOKE_BACK=0x2c9c60,MISSILE=0x2c9cf8,SHELL=0x2d45fc,TRAIL=0x2d4628)
TABLES=dict(DRIVER=(0x2c8b96,11),TURRET=(0x2c8bc2,15))
RANKS=dict(COOLDOWN=0x2b7f76,PERIOD=0x2b7ff8,REPEATS=0x2b807a,TURRET_HP=0x2b7df0,SHELL=0x2b7e72,MISSILE=0x2b7ef4)
FIXED=dict(BODY=('DEFENSE',0x2c86e0),DRIVE=('ATTACK',0x2c89cc),WRECK=('ATTACK',0x2c8a74))
TEMPLATES={53:0x68c1e,54:0x68c2a,55:0x68c38}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',required=True,type=Path);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw,{0x2c9d0c:32,0x2c9f50:32})
 if any(c.long(0xe8000+4*i)!=at for i,at in TEMPLATES.items()):raise ValueError('template dispatch')
 entries={k:c.entry(v) for k,v in ROOTS.items()};tables={k:[65535 if c.long(at+4*i)==0xffffffff else c.entry(c.long(at+4*i)) for i in range(n)] for k,(at,n) in TABLES.items()};fixed={k:c.binding(*v) for k,v in FIXED.items()};c.compile()
 if {x['source'] for x in c.callbacks}!={0x4a0d4,0x283ca,0x69ff6,0x69fb8,0x69f68,0x4a1ba,0x6a050,0x6e4cc,0x6a044,0x69ef8,0x69f58} or c.parameters or {(x['source_offset'],x['width']) for x in c.fields}!={(0x3b,1),(0x5c,4),(0x94,2),(0x74,2),(0x82,2),(0x20,1)}:raise ValueError('unreviewed graph')
 h=['#ifndef NATIVE_LIGHT_TANK_PROGRAMS_H','#define NATIVE_LIGHT_TANK_PROGRAMS_H','#include "../src/bosses.h"',
 'enum LightTankAnimation {'+','.join('LTA_'+k for k in ROOTS)+',LTA_COUNT};','enum LightTankCallback {'+','.join('LTC_'+format(x['source'],'X') for x in c.callbacks)+'};','enum LightTankField {'+','.join('LTF_'+format(x['source_offset'],'X') for x in c.fields)+'};','enum LightTankBinding {'+','.join('LTB_'+k+'='+str(v) for k,v in fixed.items())+'};','enum LightTankRank {'+','.join('LTR_'+k for k in RANKS)+',LTR_COUNT};',
 'extern const struct AnimationProgram light_tank_program;','extern const struct BossBindingSet light_tank_bindings['+str(len(c.bindings))+'];','extern const uint16_t light_tank_entries[LTA_COUNT],light_tank_rank[LTR_COUNT][64];',
 *['extern const uint16_t light_tank_'+k.lower()+'['+str(n)+'];' for k,(_,n) in TABLES.items()],
 'extern const uint8_t light_tank_delays[2][4][8],light_tank_turn_delays[16];','extern const int16_t light_tank_bounds[3][4],light_tank_shell[2][4][3],light_tank_homing_delays[16];','extern const struct InteractionContact light_tank_contact;','struct LightTankStamp {const uint8_t*cells;int16_t dx,dy;uint16_t width,height;};','extern const struct LightTankStamp light_tank_stamps[2];']
 out=c.c_source('light_tank_program','light-tank-programs.h');defs=[];attacks=[];sets=[];filters={}
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


 array('struct BossBindingSet','light_tank_bindings[]',','.join(sets));array('uint16_t','light_tank_entries[LTA_COUNT]',vals(entries.values()))
 for k,v in tables.items():array('uint16_t','light_tank_'+k.lower()+'[]',vals(v))
 if any(c.word(at) not in (0,1) for at in RANKS.values()):raise ValueError('rank mode')
 array('uint16_t','light_tank_rank[LTR_COUNT][64]',','.join('{'+vals(c.word(at+2+2*i) for i in range(64))+'}' for at in RANKS.values()))
 array('uint8_t','light_tank_delays[2][4][8]',','.join('{'+','.join('{'+vals(c.byte(c.long(at+4*i)+j) for j in range(8))+'}' for i in range(4))+'}' for at in (0x2c8e8a,0x2c8f80)))
 array('uint8_t','light_tank_turn_delays[16]',vals(c.byte(0x2c8c2e+i) for i in range(16)))
 array('int16_t','light_tank_homing_delays[16]',vals(c.signed(0x2c8c3e+2*i) for i in range(16)))
 array('int16_t','light_tank_bounds[3][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x2c8b6c,0x2c8b7c,0x2d404e)))
 array('int16_t','light_tank_shell[2][4][3]',','.join('{'+','.join('{'+vals(c.signed(at+6*i+2*j) for j in range(3))+'}' for i in range(4))+'}' for at in (0x2c8bfe,0x2c8c16)))
 if c.byte(0x2c8b18)!=128:raise ValueError('contact format')
 array('struct InteractionContact','light_tank_contact','{'+vals(c.signed(0x2c8b1e+2*i) for i in range(4))+'},'+vals([c.signed(0x2c8b1a),c.byte(0x2c8b19)]))
 rows=[]
 for i,at in enumerate((0x2c8c7e,0x2c8ca6)):
  dims=[c.signed(at+2*j) for j in range(4)];data=[c.byte(at+8+j) for j in range(dims[2]*dims[3])];array('uint8_t',f'light_tank_stamp{i}[]',vals(data));rows.append('{light_tank_stamp'+str(i)+','+vals(dims)+'}')
 array('struct LightTankStamp','light_tank_stamps[2]',','.join(rows));h.append('#endif')
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,tables=TABLES,rank_sources=RANKS,templates=TEMPLATES,implementation=['src/light_tank.c','src/light_tank_parts.c'],validation='Source conversion only; no gameplay tests',complete_port=False,source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in [(0x68c1e,0x6a062),(0x6dbd4,0x6dc90),(0x6e4cc,0x6e50e)]])
 for name,value in [('light-tank-programs.h','\n'.join(h)),('light-tank-programs.c','\n'.join(out)),('light-tank-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),ranks=len(RANKS))))
if __name__=='__main__':main()
