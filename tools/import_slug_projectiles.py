#!/usr/bin/env python3
"""Compile ordinary Slug cannon/drop/exhaust dependencies, without execution."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS={'CANNON':0x29fa8a,'DROP':0x319d0,'EXHAUST_ANGLE':0x29e440,
       'TRAIL':0x323cc,'SMOKE':0x29e64a,'SPARK':0x29fcce}
ATTACKS={'LIGHT':0x30d7c,'CANNON':0x30e20,'CRASH_A':0x30f68,'CRASH_B':0x3100c,'CRASH_C':0x310b0}
# Ten consecutive frames, each with 32 angle rows of 18 bytes. The native
# owner field is (angle & 255) >> 3; no unbounded ROM dispatch remains.
SELECTORS={0x29fa8c+i*0x244:32 for i in range(10)}
def values(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args()
 raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('P-ROM fingerprint')
 c=AnimationCompiler(raw,SELECTORS);entries={k:c.entry(v) for k,v in ROOTS.items()}
 exhaust_pointers=[c.long(0x319dc+i*4) for i in range(5)]
 exhaust=[65535 if at==0xffffffff else c.entry(at) for at in exhaust_pointers];c.compile()
 if c.fields!=[dict(source_offset=0x30,width=2)] or c.callbacks or c.bindings or c.parameters:raise ValueError('unreviewed projectile operation')
 if set(n['op'] for n in c.nodes)-{'WAIT','DRAW','END','SELECT','JUMP'}:raise ValueError('unreviewed command')
 records=[];sets=[];attacks=[]
 for name,at in ATTACKS.items():
  first=len(records);q=at
  for count in range(256):
   if c.byte(q)==255:break
   group,kind,damage,strength,shape,offscreen=struct.unpack_from('>BBhBBB',raw,c.offset(q))
   if group>=5 or kind>34 or shape not in (0,4):raise ValueError('unreviewed attack')
   rect=[c.signed(q+10+i*2) for i in range(4)]
   records.append('{{'+values(rect)+'},'+values([damage,group,kind,strength,shape,offscreen])+'}');q+=80
  else:raise ValueError('unterminated attack')
  sets.append('{attack_records+'+str(first)+','+str(count)+'}');attacks.append(dict(name=name,source=at,count=count,first=first))
 tables=[]
 offsets=[[[c.signed(at+10*i+2*j) for j in range(5)] for i in range(5)] for at in (0x3115c,0x3118e)]
 bounds=[c.signed(0x31154+2*i) for i in range(4)]
 atan_log=[c.byte(0x5de18+i) for i in range(256)];atan_exp=[c.byte(0x5df18+i) for i in range(256)]
 h=['#ifndef SLUG_PROJECTILE_PROGRAMS_H','#define SLUG_PROJECTILE_PROGRAMS_H','#include "../src/animation.h"','#include "../src/hitbox.h"',
    'enum SlugProjectileAnimation {']+[f' SPA_{k}={i},' for i,k in enumerate(entries)]+['};','enum SlugProjectileAttack {']+[f' SPH_{k}={i},' for i,k in enumerate(ATTACKS)]+['};',
    'extern const struct AnimationProgram slug_projectile_program;',
    'extern const uint16_t slug_projectile_entries[],slug_exhaust_entries[5];',
    'extern const struct HitAttackSet slug_projectile_attacks[5];',
    'extern const int16_t slug_cannon_offsets[2][5][5],slug_cannon_bounds[4];',
    'extern const uint8_t slug_atan_log[256],slug_atan_exp[256];','#endif']
 out=c.c_source('slug_projectile_program','slug-projectile-programs.h')
 out+=['const uint16_t slug_projectile_entries[]={'+values(entries.values())+'};',
       'const uint16_t slug_exhaust_entries[5]={'+values(exhaust)+'};',
       'static const struct HitAttack attack_records[]={'+','.join(records)+'};',
       'const struct HitAttackSet slug_projectile_attacks[5]={'+','.join(sets)+'};',
       'const int16_t slug_cannon_offsets[2][5][5]={'+','.join('{'+','.join('{'+values(row)+'}' for row in table)+'}' for table in offsets)+'};',
       'const int16_t slug_cannon_bounds[4]={'+values(bounds)+'};',
       'const uint8_t slug_atan_log[256]={'+values(atan_log)+'};','const uint8_t slug_atan_exp[256]={'+values(atan_exp)+'};']
 (R/'generated/slug-projectile-programs.h').write_text('\n'.join(h)+'\n')
 (R/'generated/slug-projectile-programs.c').write_text('\n'.join(out)+'\n')
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,entries=entries,selectors=SELECTORS,
  exhaust_table=dict(source=0x319dc,pointers=exhaust_pointers,entries=exhaust),attacks=attacks,
  offsets=dict(sources=[0x3115c,0x3118e],values=offsets),bounds=dict(source=0x31154,values=bounds),
  atan=dict(function=0x5e018,log_source=0x5de18,exp_source=0x5df18),
  constructors={'IDLE':0x311c0,'LIGHT':0x3122e,'LONG':0x31240,'NORMAL':0x312b2,
   'DROP_A':0x318e6,'DROP_B':0x31922,'DROP_A_ALIAS':0x31944,'EXHAUST':0x319f0,
   'EXHAUST_AIR':0x31af6,'SPARK':0x31b52,'TRAIL':0x31b78,'SMOKE':0x31bd0},
  status='Hand-written cannon/drop/exhaust/trail/crash states; not a playable vehicle',
  adaptations=['Scene-resolved input snapshot at spawn; native owner id replaces input-context pointer',
   'HitActor.contact_resistance exposes the original defense result used by the no-explosion branch'],
  validation='Source extraction and ARM compile only; no gameplay execution',complete=False)
 (R/'generated/slug-projectile-source.json').write_text(json.dumps(ledger,indent=2)+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),attacks=len(records),selectors=len(SELECTORS))))
if __name__=='__main__':main()
