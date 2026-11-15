#!/usr/bin/env python3
"""137 air formation, wave manager, pilot and destruction: original source data conversion only."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS={'INTRO': 3026890, 'TURN': 3026902, 'HIT': 3028326, 'PILOT': 3029270, 'PILOT_LEAP': 3029282, 'PILOT_FALL': 3029324, 'OVERLAY': 3028408, 'OVERLAY_DEAD': 3028660, 'HOP': 3027036, 'ARC_LEFT': 3027118, 'ARC_RIGHT': 3027722, 'SHARD_A': 3029392, 'SHARD_B': 3029484, 'SHARD_C': 3029576, 'DEBRIS_A': 3007428, 'DEBRIS_B': 3007310, 'DEBRIS_C': 3007546, 'DEBRIS_D': 3007988, 'DEBRIS_E': 3008106, 'GLINT': 3004598, 'PATH_A': 3026934, 'PATH_B': 3026954, 'PATH_C': 3027002, 'PATH_D': 3027084, 'ARC_LEFT_PILOT': 3027420, 'ARC_RIGHT_PILOT': 3028024}
TABLES={}
SELECTORS={3028662: 16, 3027124: 16, 3027728: 16, 3028964: 16, 3027426: 16, 3028030: 16}
RANKS=dict(IDLE=0x2bea3e,PERIOD=0x2beac0,HP=0x2beb42,SPEED0=0x2bebc4,SPEED1=0x2bec46,SPEED2=0x2bee4e,SPEED3=0x2bed4a,SPEED4=0x2bedcc,SPEED5=0x2becc8,PILOT=0x2bef82)
FIXED=dict(BODY=('DEFENSE',0x2e3aa4),PILOT=('DEFENSE',0x2e3af8))
TEMPLATES={137:0x800a6}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',required=True,type=Path);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw,SELECTORS)
 if any(c.long(0xe8000+4*i)!=at for i,at in TEMPLATES.items()):raise ValueError('template dispatch')
 entries={k:c.entry(v) for k,v in ROOTS.items()};tables={k:[65535 if c.long(at+4*i)==0xffffffff else c.entry(c.long(at+4*i)) for i in range(n)] for k,(at,n) in TABLES.items()};fixed={k:c.binding(*v) for k,v in FIXED.items()};c.compile()
 if {x['source'] for x in c.callbacks}!={0x283ca} or c.parameters:raise ValueError('unreviewed graph')
 if {(x['source_offset'],x['width']) for x in c.fields}!={(0x7e,1),(0x82,1),(0x76,2)}:raise ValueError('unreviewed fields')
 h=['#ifndef NATIVE_AIR_FORMATION_PROGRAMS_H','#define NATIVE_AIR_FORMATION_PROGRAMS_H','#include "../src/bosses.h"',
 'enum AirFormationAnimation {'+','.join('AFA_'+k for k in ROOTS)+',AFA_COUNT};',
 'enum AirFormationCallback {'+','.join('AFC_'+format(x['source'],'X') for x in c.callbacks)+'};',
 'enum AirFormationField {'+','.join('AFF_'+format(x['source_offset'],'X')+'_'+str(x['width']) for x in c.fields)+'};',
 'enum AirFormationBinding {'+','.join('AFB_'+k+'='+str(v) for k,v in fixed.items())+'};',
 'enum AirFormationRank {'+','.join('AFR_'+k for k in RANKS)+',AFR_COUNT};',
 'extern const struct AnimationProgram air_formation_program;','extern const struct BossBindingSet air_formation_bindings['+str(len(c.bindings))+'];','extern const uint16_t air_formation_entries[AFA_COUNT];',
 'struct AirFormationRankData {const uint16_t*values;const uint8_t*choices;};','extern const struct AirFormationRankData air_formation_rank[AFR_COUNT];',
 *['extern const uint16_t air_formation_'+k.lower()+'['+str(n)+'];' for k,(_,n) in TABLES.items()],
 'extern const uint16_t air_formation_pilot_palettes[4];',
 'extern const uint8_t air_formation_scales[4][6],air_formation_deaths[7][8],air_formation_pattern_choices[2][16];',
 'extern const int16_t air_formation_bounds[2][4],air_formation_offsets[4][6][2],air_formation_approach[6][4],air_formation_config[2][4][4],air_formation_pilot_offsets[8][4],air_formation_speed_curve[32];',
 'extern const struct FlightProgram air_formation_flight_program;','extern const uint16_t air_formation_paths[22];','extern const struct BossPatternData air_formation_patterns[2];']
 out=c.c_source('air_formation_program','air_formation-programs.h');defs=[];attacks=[];sets=[];filters={}
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


 array('struct BossBindingSet','air_formation_bindings[]',','.join(sets));array('uint16_t','air_formation_entries[AFA_COUNT]',vals(entries.values()))
 for k,v in tables.items():array('uint16_t','air_formation_'+k.lower()+'[]',vals(v))
 rank_rows=[]
 for i,at in enumerate(RANKS.values()):
  mode=c.word(at)
  if mode==2:
   choices=[c.byte(at+2+j) for j in range(64)]
   array('uint8_t',f'air_formation_choices{i}[64]',vals(choices))
   array('uint16_t',f'air_formation_values{i}[{(max(choices)+1)*8}]',vals(c.word(at+66+2*j) for j in range((max(choices)+1)*8)))
   rank_rows.append('{air_formation_values'+str(i)+',air_formation_choices'+str(i)+'}')
  else:
   if mode not in (0,1):raise ValueError('rank mode')
   array('uint16_t',f'air_formation_values{i}[64]',vals(c.word(at+2+2*j) for j in range(64)))
   rank_rows.append('{air_formation_values'+str(i)+',0}')
 array('struct AirFormationRankData','air_formation_rank[AFR_COUNT]',','.join(rank_rows))
 array('uint16_t','air_formation_pilot_palettes[4]',vals([0x49,0x170,0x171][(c.word(0x5e81e+2*i)-0x16)//2] for i in range(4)))
 array('uint8_t','air_formation_scales[4][6]',','.join('{'+vals(c.byte(c.long(0x2e3ebc+8*i+4)+j) for j in range(6))+'}' for i in range(4)))
 array('int16_t','air_formation_offsets[4][6][2]',','.join('{'+','.join('{'+vals(c.signed(c.long(0x2e3ebc+8*i)+4*j+2*k) for k in range(2))+'}' for j in range(6))+'}' for i in range(4)))
 array('int16_t','air_formation_bounds[2][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x2e3c44,0x2e3c4c)))
 array('int16_t','air_formation_config[2][4][4]',','.join('{'+','.join('{'+vals(c.signed(0x2e3cf8+32*i+8*j+2*k) for k in range(4))+'}' for j in range(4))+'}' for i in range(2)))
 for name,at,n,m in [('approach',0x2beed0,6,4),('pilot_offsets',0x2e3d80,8,4)]:array('int16_t',f'air_formation_{name}[{n}][{m}]',','.join('{'+vals(c.signed(at+2*(i*m+j)) for j in range(m))+'}' for i in range(n)))
 array('int16_t','air_formation_speed_curve[32]',vals(c.signed(0x8175e+2*i) for i in range(32)))
 deaths=[0x81018,0x80b1a,0x80cdc,0x80e48,0x80bd6,0x80c08]
 array('uint8_t','air_formation_deaths[7][8]',','.join('{'+vals(deaths.index(c.long(c.long(0x2e3dc0+4*i)+4*j)) for j in range(8))+'}' for i in range(7)))
 kinds=[[0x785f6,0x78650,0x7868c,0x78740,0x7875e],[0x8141e,0x8140c,0x81430,0x81442,0x81454]]
 array('uint8_t','air_formation_pattern_choices[2][16]',','.join('{'+vals(kinds[i].index(c.long(at+4*j)) for j in range(16))+'}' for i,at in enumerate((0x2e3c78,0x2e3cb8))))
 array('struct BossPatternData','air_formation_patterns[2]',','.join('{'+vals([c.signed(at+4),c.signed(at+8),c.word(at+6),c.word(at+10),c.word(at+2),c.byte(at),c.byte(at+1),c.byte(at+13),i])+'}' for i,at in enumerate((0x2e3c54,0x2e3c66))))
 pending=[];seen={};rows=[];calls=[];fields=[]
 def entry(at):
  if at not in seen:seen[at]=len(pending);pending.append(at)
  return seen[at]
 paths=[entry(at) for at in [3025694, 3025782, 3026150, 3025970, 3026058, 3025874, 3026808, 3026830, 3026786, 3026726, 3026748, 3026868, 3026242, 3026296, 3026350, 3026392, 3026434, 3026494, 3026548, 3026608, 3026662, 3026694]]
 for at in pending:
  op=c.byte(at);value=target=length=width=increment=angle=0;nxt=65535
  if op==0:kind='ANGLE';angle=c.byte(at+1);nxt=entry(at+2)
  elif op==1:kind='DISTANCE';length=c.word(at+2);nxt=entry(at+4)
  elif op==2:kind='ARC';length=c.word(at+4);increment=struct.unpack('b',bytes([c.byte(at+1)]))[0];angle=c.byte(at+2);nxt=entry(at+6)
  elif op==3:kind='WAIT';length=c.word(at+2);nxt=entry(at+4)
  elif op==4:kind='JUMP';target=entry(c.long(at+2))
  elif op in (5,6,7):
   kind='FIELD';width={5:4,6:2,7:1}[op];value=c.byte(at+1) if width==1 else c.word(at+2) if width==2 else c.long(at+4);key=(c.word(at+(4 if width==2 else 2)),width)
   if key==(0,4):raise ValueError('unreviewed path handler')
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
 if set(fields)!={(33, 1), (58, 1), (127, 1), (32, 1)} or set(calls)!={530468, 530440, 530504, 530412, 530384, 529202, 530390, 530454, 530398}:raise ValueError('unreviewed path')
 print('path',len(rows),'fields',fields,'calls',[hex(x) for x in calls])
 if fields:h.append('enum AirFormationPathField {'+','.join('AFPF_'+format(x,'X')+'_'+str(y) for x,y in fields)+'};')
 if calls:h.append('enum AirFormationPathCallback {'+','.join('AFPC_'+format(x,'X') for x in calls)+'};')
 out.append('static const struct FlightCommand air_formation_flight_commands[]={'+','.join(rows)+'};')
 array('struct FlightProgram','air_formation_flight_program','air_formation_flight_commands,'+str(len(rows)));array('uint16_t','air_formation_paths[22]',vals(paths));h.append('#endif')
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,rank_sources=RANKS,templates=TEMPLATES,selector_bounds=SELECTORS,path_commands=[dict(source=at,native_index=i) for at,i in seen.items()],path_fields=fields,path_callbacks=calls,implementation=['src/air_formation.c','src/air_formation_parts.c','src/air_formation_death.c'],implemented_unreachable_aliases=['080DA6 shrinking death path','08163A glint','0817FA/08186C/081884/08189C/0818AA helper APIs'],validation='Source conversion only; no gameplay tests',complete_port=False,source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in [(0x800a6,0x818ec)]])
 for name,value in [('air_formation-programs.h','\n'.join(h)),('air_formation-programs.c','\n'.join(out)),('air_formation-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),ranks=len(RANKS))))
if __name__=='__main__':main()
