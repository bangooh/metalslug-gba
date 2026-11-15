#!/usr/bin/env python3
"""Template 31..49 / siege auxiliary guards: offline typed animation data."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(ENTRY0=0x2c175e,ENTRY1=0x2c17ac,ENTRY2=0x2c1886,IDLE=0x2c196e,THROW=0x2c19ce,TURN=0x2c1b86,WAIT_A=0x2c1b02,REACT_A=0x2c1b2a,WAIT_B=0x2c1d44,REACT_B=0x2c1d88,WAIT0=0x2c1d98,WAIT1=0x2c1df8,WAIT2=0x2c1e58,WAIT3=0x2c1f28,PROJECTILE=0x2c72c0,ARRIVE_A=0x2c21dc,ARRIVE_B=0x2c09ac,ARRIVE_ESCAPE=0x2c226a)
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw);entries={k:c.entry(v) for k,v in ROOTS.items()};deaths=[c.entry(c.long(0x2c20b8+4*i)) for i in range(2)];atk=c.binding('ATTACK',0x2c2168);c.compile()
 if [x['source'] for x in c.callbacks]!=[0x6034c] or {(x['source_offset'],x['width']) for x in c.fields}!={(0x59,1),(0x3a,1)} or c.parameters:raise ValueError('unreviewed guard graph')
 h=['#ifndef NATIVE_LATE_GUARD_PROGRAMS_H','#define NATIVE_LATE_GUARD_PROGRAMS_H','#include "../src/bosses.h"','enum LateGuardAnimation {'+','.join('LGA_'+k for k in entries)+',LGA_COUNT};',
 'enum LateGuardField {'+','.join('LGF_'+format(x['source_offset'],'X') for x in c.fields)+'};',
 'enum {LGB_PROJECTILE='+str(atk)+'};','extern const struct AnimationProgram late_guard_program;','extern const struct BossBindingSet late_guard_bindings['+str(len(c.bindings))+'];',
 'extern const uint16_t late_guard_entries[LGA_COUNT],late_guard_deaths[2],late_guard_wait[64];','extern const uint8_t late_guard_choices[16];','extern const int16_t late_guard_bounds[3][4];','#endif']
 out=c.c_source('late_guard_program','late-guard-programs.h');defs=[];attacks=[];sets=[];filters={}
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
 array('struct BossBindingSet','late_guard_bindings[]',','.join(sets));array('uint16_t','late_guard_entries[LGA_COUNT]',vals(entries.values()));array('uint16_t','late_guard_deaths[2]',vals(deaths))
 if c.word(0x2b79a0) not in (0,1):raise ValueError('rank mode')
 array('uint16_t','late_guard_wait[64]',vals(c.word(0x2b79a2+2*i) for i in range(64)))
 source_entries=[0x5fd78,0x5fd88,0x5fd98,0x5ff64,0x60050,0x6013c,0x6014c,0x6015c,0x6016c,0x60420,0x60440,0x60460,0x60480,0x604a0,0x60426,0x60446,0x60466,0x60486,0x604a6]
 for i,at in enumerate(source_entries):
  if c.long(0xe8000+4*(31+i))!=at:raise ValueError('template mapping')
 array('uint8_t','late_guard_choices[16]',vals(source_entries.index(c.long(0x2eb02c+4*i)) for i in range(16)))
 if [c.long(0x2c21d0+i*4) for i in range(3)]!=source_entries[:3]:raise ValueError('replacement table')
 array('int16_t','late_guard_bounds[3][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x2c21bc,0x2c21c6,0x2c2314)))
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,templates={31+i:at for i,at in enumerate(source_entries)},implementation=['src/late_guard.c'],validation='Source conversion only',complete_port=False,source_ranges=[dict(start=0x5fd78,end_exclusive=0x60576,sha256=hashlib.sha256(raw[0x5fd78:0x60576]).hexdigest())])
 for name,data in [('late-guard-programs.h','\n'.join(h)),('late-guard-programs.c','\n'.join(out)),('late-guard-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(data+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks))))
if __name__=='__main__':main()
