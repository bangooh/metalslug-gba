#!/usr/bin/env python3
"""Rocket vehicles88/89: body and weapon poses, contacts and trajectory tables."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS={'SLOPE': 2938422, 'RIDER_SLOPE': 2946580, 'ROCKET': 2964110, 'MUZZLE': 3004114}
TABLES={'DRIVE': (441176, 4), 'DRIVE_BACK': (441248, 4), 'STOP_BACK': (441670, 4), 'STOP': (441742, 4), 'TURN': (441906, 4), 'TURN_BACK': (441978, 4), 'IDLE': (442166, 4), 'ALERT': (442410, 4), 'FIRE': (442578, 4), 'RECOVER': (442754, 4), 'HURT': (442886, 4), 'DEAD': (2936758, 4), 'RIDER_IDLE': (443826, 4), 'RIDER_DEAD': (445590, 4), 'RIDER_HURT': (445864, 4)}
SELECTORS={2946582: 17, 2947804: 16, 2947208: 16, 2947506: 16, 2962938: 16, 2960590: 16, 2961764: 16, 2958252: 16, 2953576: 16, 2955914: 16, 2938432: 17, 2946892: 17, 2963230: 16, 2960882: 16, 2962056: 16, 2958544: 16, 2953868: 16, 2956206: 16, 2938742: 17, 2963522: 16, 2961174: 16, 2962348: 16, 2958836: 16, 2954160: 16, 2956498: 16, 2963814: 16, 2961466: 16, 2962640: 16, 2959128: 16, 2954452: 16, 2956790: 16, 2959420: 16, 2954744: 16, 2957082: 16, 2959712: 16, 2955036: 16, 2957374: 16, 2960004: 16, 2955328: 16, 2957666: 16, 2960296: 16, 2955620: 16, 2957958: 16}
TEMPLATES={88:0x6ba34,89:0x6ba2c}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',required=True,type=Path);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw,SELECTORS)
 if any(c.long(0xe8000+4*i)!=at for i,at in TEMPLATES.items()):raise ValueError('template dispatch')
 entries={k:c.entry(v) for k,v in ROOTS.items()};tables={k:[65535 if c.long(at+4*i)==0xffffffff else c.entry(c.long(at+4*i)) for i in range(n)] for k,(at,n) in TABLES.items()}
 fire=[[c.entry(c.long(c.long(0x2cd02a+4*i)+4*j)) for j in range(16)] for i in range(4)]
 defense=[c.binding('DEFENSE',c.long(0x2ccfea+4*i)) for i in range(4)]
 attack=[[c.binding('ATTACK',c.long(at+4*i)) for i in range(4)] for at in (0x2cd00a,0x2cd01a)]
 c.compile()
 if {x['source'] for x in c.callbacks}!={0x6d602,0x283ca,0x6d59c,0x6d5cc} or c.parameters or {(x['source_offset'],x['width']) for x in c.fields}!={(0x8e,1),(0x8f,1),(0x8a,2),(0x34,2)}:raise ValueError('unreviewed graph')
 if [c.long(0x2cd606+4*i) for i in range(9)]!=[0x6c572,0x6c6ca,0x6c73e,0x6c8ca,0x6c97a,0x6cb06,0x6cc5c,0x6cdba,0x6c54c]:raise ValueError('weapon command mapping')
 # Repeated inline source selectors have identical content, but their frame
 # continuations remain distinct in C rather than inferred from pose data.
 for source,base in [(0x6bc1c,0x6bb58),(0x6bc64,0x6bba0),(0x6bf9a,0x6bf36),*[(at,0x6c5b2) for at in (0x6c662,0x6c6b6,0x6c786,0x6c862,0x6c8b6,0x6c9b8,0x6ca9e,0x6caf2,0x6cb44,0x6cbf4,0x6cc48)],(0x6cd46,0x6cc96)]:
  if [c.long(source+i*4) for i in range(4)]!=[c.long(base+i*4) for i in range(4)]:raise ValueError('inline selector alias')

 h=['#ifndef NATIVE_ROCKET_VEHICLE_PROGRAMS_H','#define NATIVE_ROCKET_VEHICLE_PROGRAMS_H','#include "../src/bosses.h"',
 'enum RocketVehicleAnimation {'+','.join('RVA_'+k for k in ROOTS)+',RVA_COUNT};','enum RocketVehicleTable {'+','.join('RVT_'+k for k in TABLES)+',RVT_COUNT};',
 'enum RocketVehicleCallback {'+','.join('RVC_'+format(x['source'],'X') for x in c.callbacks)+'};','enum RocketVehicleField {'+','.join('RVF_'+format(x['source_offset'],'X') for x in c.fields)+'};',
 'extern const struct AnimationProgram rocket_vehicle_program;','extern const struct BossBindingSet rocket_vehicle_bindings['+str(len(c.bindings))+'];',
 'extern const uint16_t rocket_vehicle_entries[RVA_COUNT],rocket_vehicle_tables[RVT_COUNT][4],rocket_vehicle_fire[4][16],rocket_vehicle_defense[4],rocket_vehicle_attack[2][4],rocket_vehicle_palettes[4];',
 'extern const struct InteractionContact rocket_vehicle_contacts[4];','extern const int16_t rocket_vehicle_bounds[2][4],rocket_vehicle_offsets[4][16][2],rocket_vehicle_motion[2][4][8][16][3];',
 'extern const uint8_t rocket_vehicle_ranges[4][5],rocket_vehicle_angles[2][8][33],rocket_vehicle_mirror[16];']

 out=c.c_source('rocket_vehicle_program','rocket-vehicle-programs.h');defs=[];attacks=[];sets=[];filters={}
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


 array('struct BossBindingSet','rocket_vehicle_bindings[]',','.join(sets));array('uint16_t','rocket_vehicle_entries[RVA_COUNT]',vals(entries.values()))
 array('uint16_t','rocket_vehicle_tables[RVT_COUNT][4]',','.join('{'+vals(v)+'}' for v in tables.values()))
 array('uint16_t','rocket_vehicle_fire[4][16]',','.join('{'+vals(v)+'}' for v in fire))
 array('uint16_t','rocket_vehicle_defense[4]',vals(defense));array('uint16_t','rocket_vehicle_attack[2][4]',','.join('{'+vals(v)+'}' for v in attack))
 array('uint16_t','rocket_vehicle_palettes[4]',vals(c.word(0x2ccf6e+2*i) for i in range(4)))
 contacts=[]
 for i in range(4):
  at=c.long(0x2ccffa+4*i)
  if c.byte(at)!=128:raise ValueError('contact format')
  contacts.append('{ {'+vals(c.signed(at+6+2*j) for j in range(4))+'},'+vals([c.signed(at+2),c.byte(at+1)])+'}')
 array('struct InteractionContact','rocket_vehicle_contacts[4]',','.join(contacts))
 array('int16_t','rocket_vehicle_bounds[2][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x2ccf3a,0x2ccf42)))
 array('int16_t','rocket_vehicle_offsets[4][16][2]',','.join('{'+','.join('{'+vals(c.signed(c.long(0x2cd03a+4*i)+4*j+2*k) for k in range(2))+'}' for j in range(16))+'}' for i in range(4)))
 array('int16_t','rocket_vehicle_motion[2][4][8][16][3]',','.join('{'+','.join('{'+','.join('{'+','.join('{'+vals(c.signed(c.long(c.long(at+4*i)+4*j)+6*k+2*l) for l in range(3))+'}' for k in range(16))+'}' for j in range(8))+'}' for i in range(4))+'}' for at in (0x2cd10a,0x2cd11a)))
 array('uint8_t','rocket_vehicle_ranges[4][5]',','.join('{'+vals(c.byte(c.long(0x2ccfc6+4*i)+j) for j in range(5))+'}' for i in range(4)))
 array('uint8_t','rocket_vehicle_angles[2][8][33]',','.join('{'+','.join('{'+vals(c.byte(c.long(at+4*i)+j) for j in range(33))+'}' for i in range(8))+'}' for at in (0x2cd4ea,0x2cd50a)))
 array('uint8_t','rocket_vehicle_mirror[16]',vals(c.byte(0x2cd5f6+i) for i in range(16)));h.append('#endif')

 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,tables=TABLES,selector_counts=SELECTORS,templates=TEMPLATES,implementation=['src/rocket_vehicle.c'],validation='Source conversion only; no gameplay tests',complete_port=False,source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in [(0x6ba2c,0x6d654),(0x6a96c,0x6aa04),(0x30068,0x30804)]])
 for name,value in [('rocket-vehicle-programs.h','\n'.join(h)),('rocket-vehicle-programs.c','\n'.join(out)),('rocket-vehicle-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),selectors=len(SELECTORS))))
if __name__=='__main__':main()
