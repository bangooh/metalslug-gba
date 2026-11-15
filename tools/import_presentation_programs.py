#!/usr/bin/env python3
"""Prompt tiles, logo and Slug cannon recovery; offline source conversion."""
import argparse,hashlib,json
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(RECOVER_FALL=0x29d090,RECOVER_BOUNCE=0x29d178,RECOVER_FLAT=0x29d234,LOGO=0x2f53a6)
def vals(v):return ','.join(str(int(x)) for x in v)
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw);entries=[c.entry(v) for v in ROOTS.values()];c.compile()
 if c.fields or c.callbacks or c.parameters or c.bindings:raise ValueError('unreviewed presentation graph')
 templates={227:0x91630,251:0x79298,318:0x45de4}
 if any(c.long(0xe8000+4*i)!=v for i,v in templates.items()):raise ValueError('template entry')
 if [c.long(0x2df442+4*i) for i in range(5)]!=[0xffffffff,0x794aa,0x7957a,0x79444,0x7933a]:raise ValueError('script entries')
 h=['#ifndef NATIVE_PRESENTATION_PROGRAMS_H','#define NATIVE_PRESENTATION_PROGRAMS_H','#include "../src/animation.h"','enum PresentationAnimation {'+','.join('PRA_'+k for k in ROOTS)+',PRA_COUNT};','extern const struct AnimationProgram presentation_program;','extern const uint16_t presentation_entries[PRA_COUNT],presentation_prompt[5][20];','extern const int16_t recovery318_bounds[4];','#endif']
 out=c.c_source('presentation_program','presentation-programs.h');out.append('const uint16_t presentation_entries[PRA_COUNT]={'+vals(entries)+'};')
 prompts=[]
 for i in range(5):
  at=c.long(0x2f4cb8+4*i)
  if c.word(at+40)!=65535:raise ValueError('prompt length')
  prompts.append('{'+vals(c.word(at+2*j) for j in range(20))+'}')
 out.append('const uint16_t presentation_prompt[5][20]={'+','.join(prompts)+'};');out.append('const int16_t recovery318_bounds[4]={'+vals(c.signed(0x5dd4c+2*i) for i in range(4))+'};')
 ledger=c.ledger();ledger.update(prom_sha256=digest,templates=templates,roots=ROOTS,implementation=['src/presentation.c','src/recovery318.c','src/scripted_input.c'],pending=['318 source45F1A inherited A3/A4 shape provenance; explicit semantic input required','Original platform BIOS start/credit eligibility as presentation input','1001C0 input gate and complete scene scheduling'],complete_port=False,validation='Source extraction and compilation only',source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[c.offset(lo):c.offset(hi-1)+1]).hexdigest()) for lo,hi in [(0x45dd4,0x45f3a),(0x79298,0x797ce),(0x91514,0x916c8),(0x5da56,0x5dad8),(0x5d674,0x5d6a0)]])
 for name,value in [('presentation-programs.h','\n'.join(h)),('presentation-programs.c','\n'.join(out)),('presentation-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),prompt_tiles=100)))
if __name__=='__main__':main()
