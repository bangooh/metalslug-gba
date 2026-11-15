#!/usr/bin/env python3
"""Flying turret142, staged obstacles144..150 and static terrain props276..280."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS={'A_IDLE': 2892822, 'A_HURT': 2892834, 'A_HURT2': 2892856, 'A_DEAD': 2892878, 'A_DEAD2': 2893060, 'SMOKE': 3003262, 'B_IDLE': 2893492, 'B_HURT': 2893510, 'B_DEAD': 2893546, 'C_IDLE': 2894268, 'C_HURT': 2894286, 'C_DEAD_A': 2894356, 'C_DEAD_B': 2894538, 'T_DEAD': 2895064, 'T_BARREL': 2895238, 'T_SHELL': 2895352}
TABLES={'BODY': (0x2c2cbc,7)}
SELECTORS={}
RANKS=dict(HP=0x2bdd0a,WAIT=0x2bdd8c,INITIAL_WAIT=0x2bde0e,SHOT_DELAY=0x2bde90,SHOT_COUNT=0x2bdf12,GRAVITY=0x2bdf94,A_HP=0x2bf414,A_HP2=0x2bf496,B_HP=0x2bf518)
FIXED=dict(BODY=('DEFENSE',0x2c32d6),SHELL=('DEFENSE',0x2c334c),SHELL_HIT=('ATTACK',0x2c322e),CRASH=('ATTACK',0x2c3282),A_BODY=('DEFENSE',0x2c231e))
TEMPLATES={142:0x60e62,144:0x60778,145:0x6076e,146:0x60a60,147:0x60a56,148:0x60a4c,149:0x60b84,150:0x60b7a,276:0x6063a,277:0x6065c,278:0x6067e,279:0x606a0,280:0x606c2}
STAMPS=dict(A_INIT=0x2c252e,A_FIRST0=0x2c2572,A_FIRST1=0x2c2550,A_SECOND0=0x2c2594,A_SECOND1=0x2c25b6,A_SECOND2=0x2c25d8,
 B_INIT0=0x2c27a6,B_INIT1=0x2c2796,B_INIT2=0x2c27b6,B_DEAD0=0x2c27c6,B_DEAD1=0x2c27d6,B_DEAD2=0x2c27e6,
 C_INIT0=0x2c28bc,C_INIT1=0x2c28fc,C_DEAD0=0x2c293c,C_DEAD1=0x2c297c,
 PROP0=0x2c25fa,PROP1=0x2c2606,PROP2=0x2c2622,PROP3=0x2c2614,PROP4=0x2c2630)
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',required=True,type=Path);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw,SELECTORS)
 if any(c.long(0xe8000+4*i)!=at for i,at in TEMPLATES.items()):raise ValueError('template dispatch')
 entries={k:c.entry(v) for k,v in ROOTS.items()};tables={k:[65535 if c.long(at+4*i)==0xffffffff else c.entry(c.long(at+4*i)) for i in range(n)] for k,(at,n) in TABLES.items()};fixed={k:c.binding(*v) for k,v in FIXED.items()};c.compile()
 if {x['source'] for x in c.callbacks}!={0x609c2,0x60b12,0x60c9a} or c.parameters:raise ValueError('unreviewed graph')
 if {(x['source_offset'],x['width']) for x in c.fields}!={(0x13,1)}:raise ValueError('unreviewed fields')
 def drawing(at):
  parts=[dict(map=at,flags=8,dx=0,dy_up=0,priority=65535)]
  return c.intern(c.drawings,c.draw_ids,json.dumps(parts,sort_keys=True),parts)
 guns=[]
 for i in range(7):
  at=c.long(0x2c3212+4*i);row=[]
  for j in range(16):
   if c.long(at+8*j)==0xffffffff:raise ValueError('gun count')
   row.append((drawing(c.long(at+8*j)),c.byte(at+8*j+4)))
  if c.long(at+128)!=0xffffffff:raise ValueError('gun terminator')
  guns.append(row)
 props=[drawing(at) for at in (0x2507fa,0x25086e,0x250838,0x25084a,0x25085c)]
 h=['#ifndef NATIVE_TURRET142_PROGRAMS_H','#define NATIVE_TURRET142_PROGRAMS_H','#include "../src/bosses.h"',
 'enum Turret142Animation {'+','.join('T42A_'+k for k in ROOTS)+',T42A_COUNT};',
 'enum Turret142Callback {'+','.join('T42C_'+format(x['source'],'X') for x in c.callbacks)+'};',
 'enum Turret142Field {'+','.join('T42F_'+format(x['source_offset'],'X')+'_'+str(x['width']) for x in c.fields)+'};',
 'enum Turret142Binding {'+','.join('T42B_'+k+'='+str(v) for k,v in fixed.items())+'};',
 'enum Turret142Rank {'+','.join('T42R_'+k for k in RANKS)+',T42R_COUNT};',
 'extern const struct AnimationProgram turret142_program;','extern const struct BossBindingSet turret142_bindings['+str(len(c.bindings))+'];','extern const uint16_t turret142_entries[T42A_COUNT];',
 'struct Turret142RankData {const uint16_t*values;const uint8_t*choices;};','extern const struct Turret142RankData turret142_rank[T42R_COUNT];',
 *['extern const uint16_t turret142_'+k.lower()+'['+str(n)+'];' for k,(_,n) in TABLES.items()],
 'extern const int16_t turret142_bounds[3][4];','extern const uint16_t turret142_prop_drawings[5],turret142_prop_palettes[3];',
 'struct Turret142Gun {uint16_t drawing;uint8_t delay;};','extern const struct Turret142Gun turret142_guns[7][16];',
 'enum Turret142StampId {'+','.join('T42S_'+k for k in STAMPS)+',T42S_COUNT};',
 'struct Turret142Stamp {const uint8_t*cells;int16_t dx,dy;uint16_t width,height;};','extern const struct Turret142Stamp turret142_stamps[T42S_COUNT];']
 out=c.c_source('turret142_program','turret142-programs.h');defs=[];attacks=[];sets=[];filters={}
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


 array('struct BossBindingSet','turret142_bindings[]',','.join(sets));array('uint16_t','turret142_entries[T42A_COUNT]',vals(entries.values()))
 for k,v in tables.items():array('uint16_t','turret142_'+k.lower()+'[]',vals(v))
 rank_rows=[]
 for i,at in enumerate(RANKS.values()):
  mode=c.word(at)
  if mode==2:
   choices=[c.byte(at+2+j) for j in range(64)]
   array('uint8_t',f'turret142_choices{i}[64]',vals(choices))
   array('uint16_t',f'turret142_values{i}[{(max(choices)+1)*8}]',vals(c.word(at+66+2*j) for j in range((max(choices)+1)*8)))
   rank_rows.append('{turret142_values'+str(i)+',turret142_choices'+str(i)+'}')
  else:
   if mode not in (0,1):raise ValueError('rank mode')
   array('uint16_t',f'turret142_values{i}[64]',vals(c.word(at+2+2*j) for j in range(64)))
   rank_rows.append('{turret142_values'+str(i)+',0}')
 array('struct Turret142RankData','turret142_rank[T42R_COUNT]',','.join(rank_rows))
 array('int16_t','turret142_bounds[3][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x2c33a0,0x2c33aa,0x606fe)))
 array('struct Turret142Gun','turret142_guns[7][16]',','.join('{'+','.join('{'+vals(row)+'}' for row in bank)+'}' for bank in guns))
 array('uint16_t','turret142_prop_drawings[5]',vals(props));array('uint16_t','turret142_prop_palettes[3]',vals(c.word(0x60b74+2*i) for i in range(3)))
 rows=[]
 for i,at in enumerate(STAMPS.values()):
  dims=[c.signed(at+2*j) for j in range(4)]
  if min(dims[2:])<=0 or dims[2]*dims[3]>1024:raise ValueError('stamp dimensions')
  array('uint8_t',f'turret142_stamp{i}[]',vals(c.byte(at+8+j) for j in range(dims[2]*dims[3])));rows.append('{turret142_stamp'+str(i)+','+vals(dims)+'}')
 array('struct Turret142Stamp','turret142_stamps[T42S_COUNT]',','.join(rows));h.append('#endif')
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,templates=TEMPLATES,rank_sources=RANKS,stamp_sources=STAMPS,gun_source=0x2c3212,gun_counts=[16]*7,implementation=['src/turret142.c','src/turret142_parts.c','src/obstacles.c'],unreferenced_aliases=[0x60764,0x60a0e,0x60cb8,0x619ae],pending=[],complete_port=False,validation='Source conversion only; no gameplay tests',source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in [(0x6063a,0x60cd4),(0x60e62,0x619ca)]])
 for name,value in [('turret142-programs.h','\n'.join(h)),('turret142-programs.c','\n'.join(out)),('turret142-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),ranks=len(RANKS))))
if __name__=='__main__':main()
