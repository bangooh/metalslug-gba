#!/usr/bin/env python3
"""Source271 flame trap with ordered target groups and delayed player launch."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(BASE=0x2977e0,OVERLAY=0x297856,FLASH=0x2978cc,PARTICLE=0x297922)
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw)
 if c.long(0xe8000+271*4)!=0x53a42:raise ValueError('template dispatch')
 entries=[c.entry(v) for v in ROOTS.values()];c.compile()
 if c.fields or c.callbacks or c.bindings or c.parameters:raise ValueError('unreviewed graph')
 h=['#ifndef NATIVE_FLAME_TRAP_PROGRAMS_H','#define NATIVE_FLAME_TRAP_PROGRAMS_H','#include "../src/bosses.h"','enum FlameTrapAnimation {'+','.join('FLTA_'+k for k in ROOTS)+',FLTA_COUNT};','extern const struct AnimationProgram flame_trap_program;','extern const uint16_t flame_trap_entries[FLTA_COUNT];','extern const struct HitAttack flame_trap_attacks[3];','extern const int16_t flame_trap_bounds[4];','#endif']
 out=c.c_source('flame_trap_program','flame-trap-programs.h');out+=['const uint16_t flame_trap_entries[FLTA_COUNT]={'+vals(entries)+'};'];records=[]
 for at,expected_group in [(0x298896,3),(0x2987ee,1),(0x298842,2)]:
  g,k,damage,strength,shape,offscreen=struct.unpack_from('>BBhBBB',raw,c.offset(at))
  if g!=expected_group or c.byte(at+80)!=255 or k!=9 or shape!=4:raise ValueError('attack format')
  records.append('{{'+vals(c.signed(at+10+2*i) for i in range(4))+'},'+vals([damage,g,k,strength,shape,offscreen])+'}')
 out+=['const struct HitAttack flame_trap_attacks[3]={'+','.join(records)+'};','const int16_t flame_trap_bounds[4]={'+vals(c.signed(0x298740+2*i) for i in range(4))+'};']
 ledger=c.ledger();ledger.update(prom_sha256=digest,templates={271:0x53a42},roots=ROOTS,implementation=['src/flame_trap.c'],attack_sources=[0x298896,0x2987ee,0x298842],source_ranges=[dict(start=0x53a42,end_exclusive=0x53d2c,sha256=hashlib.sha256(raw[0x53a42:0x53d2c]).hexdigest())],complete_port=False,validation='Source extraction and compilation only; no gameplay tests')
 for name,value in [('flame-trap-programs.h','\n'.join(h)),('flame-trap-programs.c','\n'.join(out)),('flame-trap-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks))))
if __name__=='__main__':main()
