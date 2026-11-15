#!/usr/bin/env python3
"""Crew/captor actors125..128 and their hut30 dependency: source conversion only."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS={'IDLE_A': 2736302, 'HIT_A': 2736596, 'IDLE_B': 2736708, 'HIT_B': 2736780, 'GUARD': 3011826, 'LEFT_IDLE': 2737002, 'LEFT_ACTION': 2737054, 'OVERLAY': 2737370, 'CENTER_IDLE': 2737092, 'CENTER_A': 2737144, 'CENTER_B': 2737292, 'CENTER_TURN': 2735424, 'HUT_WAIT': 2887456, 'HUT_OPEN': 2887580, 'HUT_ACTIVE': 2887896, 'DOOR_WAIT': 2888908, 'DOOR_ACTIVE': 2888930, 'DOOR_HIT': 2889030}
TEMPLATES={30:0x5fa00,125:0x79a6c,126:0x79c8c,127:0x79eb8,128:0x79ec2}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',required=True,type=Path);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw)
 if any(c.long(0xe8000+4*i)!=at for i,at in TEMPLATES.items()):raise ValueError('template dispatch')
 entries={k:c.entry(v) for k,v in ROOTS.items()};c.compile()
 if {x['source'] for x in c.callbacks}!={0x7a41c} or c.fields or c.parameters:raise ValueError('unreviewed graph')
 h=['#ifndef NATIVE_CAPTOR_PROGRAMS_H','#define NATIVE_CAPTOR_PROGRAMS_H','#include "../src/bosses.h"',
 'enum CaptorAnimation {'+','.join('CPA_'+k for k in ROOTS)+',CPA_COUNT};',
 'enum CaptorCallback {'+','.join('CPC_'+format(x['source'],'X') for x in c.callbacks)+'};',
 'extern const struct AnimationProgram captor_program;','extern const struct BossBindingSet captor_bindings['+str(len(c.bindings))+'];','extern const uint16_t captor_entries[CPA_COUNT],captor_commands[4];','extern const int16_t captor_bounds[5][4];','#endif']
 out=c.c_source('captor_program','captor-programs.h');defs=[];attacks=[];sets=[];filters={}
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


 array('struct BossBindingSet','captor_bindings[]',','.join(sets));array('uint16_t','captor_entries[CPA_COUNT]',vals(entries.values()))
 array('uint16_t','captor_commands[4]',vals(c.word(0x2df4ea+2*i) for i in range(4)))
 array('int16_t','captor_bounds[5][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x79b64,0x2df4e2,0x2df594,0x2df59c,0x2c168e)))
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,templates=TEMPLATES,implementation=['src/captor.c'],validation='Source conversion only; no gameplay tests',complete_port=False,source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in [(0x79a6c,0x79b6e),(0x79c8c,0x79d9a),(0x79eb8,0x7a456),(0x5fa00,0x5fd78),(0x58fc2,0x58fe2)]])
 for name,value in [('captor-programs.h','\n'.join(h)),('captor-programs.c','\n'.join(out)),('captor-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks))))
if __name__=='__main__':main()
