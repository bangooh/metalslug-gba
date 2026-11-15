#!/usr/bin/env python3
"""Original ambient actors 174..179 and 195..226; offline animation assets."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(WALK_A=0x2f55be,HIT_A=0x2f5642,INTRO_A=0x2f5780,HIT_B=0x2f5b50,STAGGER=0x2f5c80,DROP=0x2f5c9c,SIT=0x2f5ed8,HOP=0x2f5ef8,STATIC_A=0x2f5f18,WALK_D=0x2f5f38,WALK_E=0x2f5f80,WALK_F=0x2f5fc8,WAIT_A=0x2f6010,NEAR_A=0x2f6022,WAIT_B=0x2f6048,NEAR_B=0x2f6068,STATIC_B=0x2f607a,STATIC_C=0x2f608c,STATIC_D=0x2f609e)
TABLES=dict(WALK_B=(0x2f558e,2),INTRO_B=(0x2f5596,2),WALK_C=(0x2f55ae,2),INTRO_C=(0x2f55b6,2))
TEMPLATES={174:0x989bc,175:0x98ad4,176:0x98bc6,177:0x98bd0,178:0x98d8c,179:0x98d96,195:0x98f40,196:0x98fa2,197:0x98f4e,198:0x98fb0,199:0x98f5c,200:0x98fbe,201:0x990bc,202:0x9911e,203:0x990ca,204:0x9912c,205:0x990d8,206:0x9913a,207:0x991da,208:0x992d8,209:0x99346,210:0x99446,211:0x994a2,212:0x992e6,213:0x99354,214:0x99454,215:0x994b0,216:0x992f4,217:0x99362,218:0x99462,219:0x994be,220:0x994fe,221:0x995cc,222:0x996b6,223:0x996d0,224:0x996ea,225:0x99704,226:0x9971e}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',required=True,type=Path);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw)
 if any(c.long(0xe8000+4*i)!=at for i,at in {**TEMPLATES,325:0x29598,326:0x29588}.items()):raise ValueError('template dispatch')
 if [c.long(0x2f55a6+4*i) for i in range(2)]!=[0x98a08,0x98c32]:raise ValueError('ground resume dispatch')
 entries={k:c.entry(v) for k,v in ROOTS.items()};tables={k:[c.entry(c.long(at+4*i)) for i in range(n)] for k,(at,n) in TABLES.items()};c.compile()
 if c.fields or c.callbacks or c.parameters or any(b['kind']!='DEFENSE' for b in c.bindings):raise ValueError('unreviewed animation operation')
 h=['#ifndef NATIVE_MOB_PROGRAMS_H','#define NATIVE_MOB_PROGRAMS_H','#include "../src/bosses.h"','enum MobAnimation {'+','.join('MOA_'+k for k in ROOTS)+',MOA_COUNT};','extern const struct AnimationProgram mob_program;','extern const uint16_t mob_entries[MOA_COUNT];','extern const struct BossBindingSet mob_bindings['+str(len(c.bindings))+'];','extern const int16_t mob_bounds[4];']
 out=c.c_source('mob_program','mob-programs.h')
 def array(ty,name,value,static=False):out.append(('static ' if static else '')+'const '+ty+' '+name+'={'+value+'};')
 defs=[];sets=[];filters={}
 for b in c.bindings:
  at=b['source'];first=len(defs)
  for count in range(256):
   if c.byte(at)==255:break
   ptr=c.long(at+2);filter_name='0'
   if ptr!=0xffffffff:
    if ptr not in filters:
     filters[ptr]='filter_'+str(len(filters));array('uint8_t',filters[ptr]+'[36]',vals(c.byte(ptr+i) for i in range(36)),True)
    filter_name=filters[ptr]
   defs.append('{{'+vals(c.signed(at+10+2*i) for i in range(4))+'},'+filter_name+','+str(c.byte(at+1))+'}');at+=80
  else:raise ValueError('unterminated defense')
  sets.append('{defs+'+str(first)+',0,'+str(count)+'}')
 array('struct HitDefense','defs[]',','.join(defs),True);array('struct BossBindingSet','mob_bindings[]',','.join(sets));array('uint16_t','mob_entries[MOA_COUNT]',vals(entries.values()))
 for k,v in tables.items():h.append('extern const uint16_t mob_'+k.lower()+'['+str(len(v))+'];');array('uint16_t','mob_'+k.lower()+'[]',vals(v))
 array('int16_t','mob_bounds[4]',vals(c.signed(0x2f61ac+2*i) for i in range(4)));h.append('#endif')
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,tables=TABLES,templates=TEMPLATES,implementation=['src/mob.c'],scheduled_slug_bounds={325:0x29598,326:0x29588},scheduled_slug_bounds_implementation='src/play_world.c',validation='Source conversion only; no gameplay tests',complete_port=False,source_ranges=[dict(start=0x989bc,end_exclusive=0x9979c,sha256=hashlib.sha256(raw[0x989bc:0x9979c]).hexdigest())])
 for name,value in [('mob-programs.h','\n'.join(h)),('mob-programs.c','\n'.join(out)),('mob-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(templates=len(TEMPLATES),commands=len(c.nodes),drawings=len(c.drawings),bindings=len(c.bindings))))
if __name__=='__main__':main()
