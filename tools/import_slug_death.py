#!/usr/bin/env python3
"""Extract the complete dependencies reached by Slug death/self-destruct FX."""
import argparse,hashlib,json
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS={'EXPLOSION':0x2f6c0,'SMOKE':0x2dd6a6}
PIECES=[0x785f6,0x78650,0x7868c,0x78740,0x7875e]
def values(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args()
 raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('P-ROM fingerprint')
 c=AnimationCompiler(raw);entries={k:c.entry(v) for k,v in ROOTS.items()};pieces=[]
 for source in PIECES:
  if c.word(source)!=0x41f9 or c.word(source+12)!=0x41f9 or c.word(source+22)!=0x323c:raise ValueError('debris constructor changed')
  animation=c.long(source+2);bounds=c.long(source+14);palette=c.word(source+24)
  pieces.append(dict(source=source,animation=animation,entry=c.entry(animation),bounds_source=bounds,
   bounds=[c.signed(bounds+i*2) for i in range(4)],palette=palette))
 c.compile()
 if c.fields or c.callbacks or c.bindings or c.parameters or set(n['op'] for n in c.nodes)-{'WAIT','DRAW','END','JUMP'}:raise ValueError('unreviewed death animation operation')
 at=0x2e3c54;choices_at=c.long(at+14)
 spec=dict(source=at,follow=c.byte(at),interval=c.byte(at+1),count=c.word(at+2),dx=c.signed(at+4),rx=c.word(at+6),dy=c.signed(at+8),ry=c.word(at+10),flags=c.byte(at+12),random=c.byte(at+13),choices_source=choices_at)
 if spec['follow']!=1 or spec['interval']!=1 or spec['count']!=4 or spec['flags'] or spec['random']!=1:raise ValueError('unreviewed spawner policy')
 pointers=[c.long(choices_at+i*4) for i in range(16)];choices=[PIECES.index(v) for v in pointers]
 default_bounds=[c.signed(0x5dd4c+i*2) for i in range(4)]
 if default_bounds!=[0,1,0,1]:raise ValueError('unreviewed default bounds')
 h=['#ifndef SLUG_DEATH_PROGRAMS_H','#define SLUG_DEATH_PROGRAMS_H','#include "../src/animation.h"',
    'enum SlugDeathAnimation {SDA_EXPLOSION,SDA_SMOKE};',
    'struct SlugDebrisSpec {uint16_t entry,palette;int16_t bounds[4];};',
    'struct SlugDebrisSpawner {int16_t dx,dy;uint16_t rx,ry,count;uint8_t interval;};',
    'extern const struct AnimationProgram slug_death_program;',
    'extern const uint16_t slug_death_entries[2];','extern const struct SlugDebrisSpec slug_debris_specs[5];',
    'extern const struct SlugDebrisSpawner slug_debris_spawner;','extern const uint8_t slug_debris_choices[16];','#endif']
 out=c.c_source('slug_death_program','slug-death-programs.h')
 out+=['const uint16_t slug_death_entries[2]={'+values(entries.values())+'};',
       'const struct SlugDebrisSpec slug_debris_specs[5]={'+','.join('{'+values([v['entry'],v['palette']])+',{'+values(v['bounds'])+'}}' for v in pieces)+'};',
       'const struct SlugDebrisSpawner slug_debris_spawner={'+values([spec[k] for k in ['dx','dy','rx','ry','count','interval']])+'};',
       'const uint8_t slug_debris_choices[16]={'+values(choices)+'};']
 (R/'generated/slug-death-programs.h').write_text('\n'.join(h)+'\n')
 (R/'generated/slug-death-programs.c').write_text('\n'.join(out)+'\n')
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,entries=entries,pieces=pieces,spawner=spec,
  choices=pointers,native_choices=choices,default_bounds=dict(source=0x5dd4c,values=default_bounds),
  source_helpers=[0x77c7e,0x77c98,0x77da6,0x77de4,0x77df2,0x77e00,0x78778,0x8189c,0x5e804,0x517fe,0x5ccc8],
  status='Native explosion/smoke, four-tick debris spawner, all five reachable debris constructors and blast child',
  adaptations=['World-space camera delta is already represented by camera coordinates',
   'Debris source sprite-range admission values must be supplied by the renderer; absent input faults',
   'Spawner scale copies are dead stores here: all five reachable constructors overwrite both scales with 255'],
  validation='Source extraction and ARM compile only; no gameplay execution',complete=False)
 (R/'generated/slug-death-source.json').write_text(json.dumps(ledger,indent=2)+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),debris_types=len(pieces),choices=len(choices))))
if __name__=='__main__':main()
