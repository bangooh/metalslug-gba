#!/usr/bin/env python3
"""Templates76..85 and connected riders/ordnance: source conversion, not execution."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS={'WALK_A': 2909724, 'WALK_B': 2910106, 'STOP_A': 2909972, 'STOP_B': 2910304, 'WAIT_A': 2910028, 'WAIT_B': 2910440, 'STAND_A': 2910044, 'STAND_B': 2910470, 'CHARGE_A': 2909774, 'CHARGE_B': 2910156, 'DEAD': 2909922, 'RUN': 2910562, 'LEAP': 2910608, 'THROWN': 2910702, 'WRECK': 2909950, 'PILOT_SHOT': 2911320, 'PILOT_OPEN': 2911368, 'PILOT_AIM': 2911390, 'OVERLAY': 2913556, 'OVERLAY_DEAD': 2913602, 'SMOKE': 3008224, 'DEBRIS': 2911696, 'BOUNCE': 2911722, 'MUZZLE0': 2913126, 'MUZZLE1': 2913208, 'MUZZLE2': 2913290, 'MUZZLE3': 2913372, 'MISSILE': 2911764, 'SPARK': 2913454, 'BOMB': 2912948, 'DUST': 3004428, 'FLASH': 306430, 'GRENADE_A': 2738390, 'GRENADE_B': 2738464, 'SURFACE': 2738530, 'SURFACE_LOOP': 2738626, 'SURFACE_HIT': 2738578}
TABLES={'PILOT': (2909456, 16), 'FIRE': (2909520, 4)}
SELECTORS={0x2c6ca0:8,0x2c6e28:32,0x2c706c:32}
RANKS=dict(HP_A=0x2b958c,SPEED_A=0x2b960e,BURST=0x2b9690,LONG_WAIT=0x2b9712,SHORT_WAIT=0x2b9794,SHOTS=0x2b9816,SHOT_LONG=0x2b9898,SHOT_SHORT=0x2b991a,HP_B=0x2b999c,SPEED_B=0x2b9a1e,WAIT_B=0x2b9aa0)
FIXED=dict(BODY_A=("DEFENSE",0x2c60c4),BODY_B=("DEFENSE",0x2c6118),CHARGE=("ATTACK",0x2c6310),LEAP=("ATTACK",0x2c6364))
TEMPLATES={76: 411048, 77: 411054, 78: 411180, 79: 411186, 80: 411164, 81: 411170, 82: 411128, 83: 411134, 84: 410960, 85: 411018}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',required=True,type=Path);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw,SELECTORS)
 if any(c.long(0xe8000+4*i)!=at for i,at in TEMPLATES.items()):raise ValueError('template dispatch')
 entries={k:c.entry(v) for k,v in ROOTS.items()};tables={k:[65535 if c.long(at+4*i)==0xffffffff else c.entry(c.long(at+4*i)) for i in range(n)] for k,(at,n) in TABLES.items()};fixed={k:c.binding(*v) for k,v in FIXED.items()};c.compile()
 if {x['source'] for x in c.callbacks}!={417536, 417512, 164810, 417548, 417328, 417524, 303316, 417172, 2738518, 417560, 303546, 417244} or c.parameters:raise ValueError('unreviewed graph')
 if {(x['source_offset'],x['width']) for x in c.fields}!={(136, 2), (32, 1), (116, 2), (52, 2), (92, 4), (59, 1)}:raise ValueError('unreviewed fields')
 h=['#ifndef NATIVE_ASSAULT76_PROGRAMS_H','#define NATIVE_ASSAULT76_PROGRAMS_H','#include "../src/bosses.h"',
 'enum Assault76Animation {'+','.join('R76A_'+k for k in ROOTS)+',R76A_COUNT};',
 'enum Assault76Callback {'+','.join('R76C_'+format(x['source'],'X') for x in c.callbacks)+'};',
 'enum Assault76Field {'+','.join('R76F_'+format(x['source_offset'],'X')+'_'+str(x['width']) for x in c.fields)+'};',
 'enum Assault76Binding {'+','.join('R76B_'+k+'='+str(v) for k,v in fixed.items())+'};',
 'enum Assault76Rank {'+','.join('R76R_'+k for k in RANKS)+',R76R_COUNT};',
 'extern const struct AnimationProgram assault76_program;','extern const struct BossBindingSet assault76_bindings['+str(len(c.bindings))+'];','extern const uint16_t assault76_entries[R76A_COUNT];',
 'struct Assault76RankData {const uint16_t*values;const uint8_t*choices;};','extern const struct Assault76RankData assault76_rank[R76R_COUNT];',
 *['extern const uint16_t assault76_'+k.lower()+'['+str(n)+'];' for k,(_,n) in TABLES.items()],
 'extern const int16_t assault76_bounds[4],assault76_aim[2][17],assault76_fire_sector[8],assault76_debris[4][3],assault76_targets[16][2],assault76_turn[4];',
 'extern const uint16_t assault76_bomb_palettes[4];','extern const uint8_t assault76_delays[48];',
 'struct Assault76Spawn {uint8_t kind;int16_t x,y,facing,priority;};','extern const struct Assault76Spawn assault76_spawns[5][3];','extern const uint8_t assault76_spawn_counts[5];']
 for node in c.nodes:
  if node['width']==4:
   if node['value']!=0x2c69be or node['op']!='SET':raise ValueError('unreviewed pointer')
   node['value']=0
 out=c.c_source('assault76_program','assault76-programs.h');defs=[];attacks=[];sets=[];filters={}
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
 out+=['static const struct HitDefense defs[]={'+','.join(defs)+'};']
 if attacks:out.append('static const struct HitAttack attacks[]={'+','.join(attacks)+'};')


 array('struct BossBindingSet','assault76_bindings[]',','.join(sets));array('uint16_t','assault76_entries[R76A_COUNT]',vals(entries.values()))
 for k,v in tables.items():array('uint16_t','assault76_'+k.lower()+'[]',vals(v))
 rank_rows=[]
 for i,at in enumerate(RANKS.values()):
  mode=c.word(at)
  if mode==2:
   choices=[c.byte(at+2+j) for j in range(64)]
   array('uint8_t',f'assault76_choices{i}[64]',vals(choices))
   array('uint16_t',f'assault76_values{i}[{(max(choices)+1)*8}]',vals(c.word(at+66+2*j) for j in range((max(choices)+1)*8)))
   rank_rows.append('{assault76_values'+str(i)+',assault76_choices'+str(i)+'}')
  else:
   if mode not in (0,1):raise ValueError('rank mode')
   array('uint16_t',f'assault76_values{i}[64]',vals(c.word(at+2+2*j) for j in range(64)))
   rank_rows.append('{assault76_values'+str(i)+',0}')
 array('struct Assault76RankData','assault76_rank[R76R_COUNT]',','.join(rank_rows))
 array('int16_t','assault76_bounds[4]',vals(c.signed(0x2c63b8+2*i) for i in range(4)))
 array('int16_t','assault76_aim[2][17]',','.join('{'+vals(c.signed(at+2*i) for i in range(17))+'}' for at in (0x2c6560,0x2c6582)))
 for name,at,n in [('fire_sector',0x2c65ac,8),('turn',0x2c65a4,4)]:array('int16_t',f'assault76_{name}[{n}]',vals(c.signed(at+2*i) for i in range(n)))
 for name,at,n,m in [('debris',0x2c65c4,4,3),('targets',0x2c65dc,16,2)]:array('int16_t',f'assault76_{name}[{n}][{m}]',','.join('{'+vals(c.signed(at+2*(i*m+j)) for j in range(m))+'}' for i in range(n)))
 array('uint16_t','assault76_bomb_palettes[4]',vals([0x15d,0x164,0x165,65535][c.word(0x2c65bc+2*i)&3] for i in range(4)))
 array('uint8_t','assault76_delays[48]',vals(c.byte(0x2c69be+i) for i in range(48)))
 kinds=[0x65964,0x65604,0x65614,0x65624,0x65634,0x658be,0x658ca,0x658d6,0x658e2];counts=[];spawns=[]
 for at in (0x2c6500,0x2c6460,0x2c6488,0x2c64b0,0x2c64d8):
  rows=[]
  while c.long(at)!=0xffffffff:
   rows.append('{'+vals([kinds.index(c.long(at)),*[c.signed(at+i) for i in (4,6,8,10)]])+'}');at+=12
  counts.append(len(rows));spawns.append('{'+','.join(rows)+'}')
 array('struct Assault76Spawn','assault76_spawns[5][3]',','.join(spawns));array('uint8_t','assault76_spawn_counts[5]',vals(counts));h.append('#endif')
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,templates=TEMPLATES,rank_sources=RANKS,selector_bounds=SELECTORS,implementation=['src/assault76.c','src/assault76_parts.c','src/assault76_ordnance.c'],implemented_unreachable_aliases=['064FF8 pilot-done reads ROM byte000004=0','0651E6 thrown soldier','065668 missile resume: explicit timer-owner for the original A0 write','065CE8 spark','065E6E corpse pair','065F24 next-task depth/child predicate'],pending_unreferenced_helpers=[],pointer_relocations={'2C69BE':'assault76_delays index0'},complete_port=False,validation='Source conversion only; no gameplay tests',source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in [(0x64550,0x65f40)]])
 for name,value in [('assault76-programs.h','\n'.join(h)),('assault76-programs.c','\n'.join(out)),('assault76-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),ranks=len(RANKS))))
if __name__=='__main__':main()
