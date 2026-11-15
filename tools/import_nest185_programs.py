#!/usr/bin/env python3
"""Nested185/187, swarm186, camera props188..191 and grandparent-bound shards."""
import argparse,hashlib,json
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(SHARD0=0x2f40c6,LAND0=0x2f40d2,SHARD1=0x2f40f2,LAND1=0x2f40fe,SHARD2=0x2f411e,LAND2=0x2f412a,NEST=0x2f41f2,HIT0=0x2f4202,HIT1=0x2f4284,HIT2=0x2f4306,DEAD=0x2f4388,SWARM=0x2f440a,STATIC=0x2f4426,CAMERA0=0x2f4442,CAMERA1=0x2f44ae,LOB=0x2f44ca,LOB_LAND=0x2f44d6)
TEMPLATES={185:0x8e746,186:0x8ecda,187:0x8ea74,188:0x8ed82,189:0x8edc6,190:0x8edd6,191:0x8ee16}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw);entries={k:c.entry(v) for k,v in ROOTS.items()};fixed=[c.binding('DEFENSE',at) for at in (0x2f414a,0x2f419e)]
 if any(c.long(0xe8000+4*i)!=at for i,at in TEMPLATES.items()):raise ValueError('template dispatch')
 c.compile()
 if c.callbacks or {(x['source_offset'],x['width']) for x in c.fields}!={(0x20,1)} or c.parameters or len(c.bindings)!=2:raise ValueError('unreviewed graph')
 h=['#ifndef NATIVE_NEST185_PROGRAMS_H','#define NATIVE_NEST185_PROGRAMS_H','#include "../src/bosses.h"','enum Nest185Animation {'+','.join('N185A_'+k for k in ROOTS)+',N185A_COUNT};','enum Nest185Field {N185F_PHASE};','extern const struct AnimationProgram nest185_program;','extern const uint16_t nest185_entries[N185A_COUNT];','extern const struct BossBindingSet nest185_bindings[2];','extern const uint8_t nest185_shard_choices[7];','#endif']
 out=c.c_source('nest185_program','nest185-programs.h');out.append('const uint16_t nest185_entries[N185A_COUNT]={'+vals(entries.values())+'};');records=[];sets=[]
 for i,b in enumerate(c.bindings):
  at=b['source'];first=len(records)
  for n in range(32):
   if c.byte(at)==255:break
   f=c.long(at+2);name='0'
   if f!=0xffffffff:
    name='filter_'+str(i)+'_'+str(n);out.append('static const uint8_t '+name+'[36]={'+vals(c.byte(f+j) for j in range(36))+'};')
   records.append('{{'+vals(c.signed(at+10+2*j) for j in range(4))+'},'+name+','+str(c.byte(at+1))+'}');at+=80
  else:raise ValueError('defense terminator')
  sets.append('{defs+'+str(first)+',0,'+str(n)+'}')
 out.append('static const struct HitDefense defs[]={'+','.join(records)+'};');out.append('const struct BossBindingSet nest185_bindings[2]={'+','.join(sets)+'};');out.append('const uint8_t nest185_shard_choices[7]={'+vals([0x8ee96,0x8eeac].index(c.long(0x2e9a42+4*i)) for i in range(7))+'};')
 ledger=c.ledger();ledger.update(prom_sha256=digest,templates=TEMPLATES,roots=ROOTS,shard_choices_source=0x2e9a42,implementation=['src/nest185.c'],unreferenced_aliases=[0x8e738,0x8e9fc,0x8ee96,0x8eeac,0x8eec2,0x8ef5a,0x8ef74],pending=['Original tertiary-camera delta inputs must be supplied by the full scene camera integration'],complete_port=False,validation='Source conversion only; no gameplay tests',source_ranges=[dict(start=0x8e738,end_exclusive=0x8efb0,sha256=hashlib.sha256(raw[0x8e738:0x8efb0]).hexdigest())])
 for name,value in [('nest185-programs.h','\n'.join(h)),('nest185-programs.c','\n'.join(out)),('nest185-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),templates=len(TEMPLATES))))
if __name__=='__main__':main()
