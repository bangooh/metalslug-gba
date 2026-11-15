#!/usr/bin/env python3
"""Compile the complete surface follower reached by Slug and POW initialization."""
import argparse,hashlib,json
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
TABLES={'ATTACH':(0x2dca1a,12),'DETACH':(0x2dca62,2),'FLASH':(0x2dca82,2),'POW_ATTACH':(0x2dc9ba,12),'POW_DETACH':(0x2dca52,2),'POW_FLASH':(0x2dca72,2)}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args()
 raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('P-ROM fingerprint')
 c=AnimationCompiler(raw);tables={};ledger={}
 for name,(at,n) in TABLES.items():
  ptrs=[c.long(at+i*4) for i in range(n)];tables[name]=[c.entry(v) for v in ptrs];ledger[name]=dict(source=at,pointers=ptrs)
 c.compile()
 if c.fields!=[dict(source_offset=0x3b,width=1),dict(source_offset=0x5c,width=4)] or c.callbacks or c.bindings or c.parameters:raise ValueError('unreviewed surface operations')
 delay_ptrs=[c.long(0x2dc946+i*4) for i in range(12)]
 if set(delay_ptrs)!={0x2dc976,0x2dc980}:raise ValueError('delay table bound')
 pow_ptrs=delay_ptrs
 pow_delays=[[c.byte(at+j) for j in range(10)] for at in pow_ptrs]
 delays=[[c.byte(at+j) for j in range(10)] for at in delay_ptrs]
 h=['#ifndef SLUG_SURFACE_PROGRAMS_H','#define SLUG_SURFACE_PROGRAMS_H','#include "../src/animation.h"',
  'extern const struct AnimationProgram slug_surface_program;','extern const uint8_t slug_surface_delays[12][10],slug_surface_pow_delays[12][10];']
 h+=['extern const uint16_t slug_surface_'+k.lower()+'['+str(len(v))+'];' for k,v in tables.items()]+['#endif']
 out=c.c_source('slug_surface_program','slug-surface-programs.h')
 out+=['const uint16_t slug_surface_'+k.lower()+'[]={'+vals(v)+'};' for k,v in tables.items()]
 out+=['const uint8_t slug_surface_delays[12][10]={'+','.join('{'+vals(v)+'}' for v in delays)+'};']
 out+=['const uint8_t slug_surface_pow_delays[12][10]={'+','.join('{'+vals(v)+'}' for v in pow_delays)+'};']
 data=c.ledger();data.update(prom_sha256=digest,tables=ledger,delay_tables=dict(source=0x2dc946,pointers=delay_ptrs,values=delays),
  source_helpers=[0x776e2,0x7773e,0x77640,0x77670,0x7776c,0x778a2,0x778fc,0x77912,0x77a16,0x77bae,0x77bc6,0x77c0c,0x77c28],
  adaptations=['World-space camera coordinates; original secondary Y offset must be explicitly supplied for special mud-strip classification',
   'Unused $74 drawing pointer is not made into runtime ROM addressing'],validation='Source extraction and ARM compilation only',complete=False)
 for name,value in [('slug-surface-programs.h','\n'.join(h)),('slug-surface-programs.c','\n'.join(out)),('slug-surface-source.json',json.dumps(data,indent=2))]:
  (R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),delay_tables=len(delays))))
if __name__=='__main__':main()
