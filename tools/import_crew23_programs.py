#!/usr/bin/env python3
"""Templates23..26, authored 33-point release and debris; source-only extraction."""
import argparse,hashlib,json
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(IDLE=0x78b90,OPEN=0x78a84,CREW=0x78b9c,DEBRIS=0x78e4c)
TEMPLATES={23:0x78c98,24:0x78c78,25:0x78ca2,26:0x78c88}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw);entries={k:c.entry(v) for k,v in ROOTS.items()}
 if any(c.long(0xe8000+4*i)!=at for i,at in TEMPLATES.items()):raise ValueError('template dispatch')
 c.compile()
 if {x['source'] for x in c.callbacks}!={0x78ede,0x78f14} or {(x['source_offset'],x['width']) for x in c.fields}!={(0x74,2),(0x78,2),(0x59,1)} or c.parameters or c.bindings:raise ValueError('unreviewed graph')
 h=['#ifndef NATIVE_CREW23_PROGRAMS_H','#define NATIVE_CREW23_PROGRAMS_H','#include "../src/animation.h"',
 'enum Crew23Animation {'+','.join('C23A_'+k for k in ROOTS)+',C23A_COUNT};',
 'enum Crew23Callback {'+','.join('C23C_'+format(x['source'],'X') for x in c.callbacks)+'};',
 'enum Crew23Field {'+','.join('C23F_'+format(x['source_offset'],'X') for x in c.fields)+'};',
 'extern const struct AnimationProgram crew23_program;','extern const uint16_t crew23_entries[C23A_COUNT];',
 'extern const int16_t crew23_path[33][2];','#endif']
 out=c.c_source('crew23_program','crew23-programs.h');out.append('const uint16_t crew23_entries[C23A_COUNT]={'+vals(entries.values())+'};');out.append('const int16_t crew23_path[33][2]={'+','.join('{'+vals(c.signed(0x78be0+4*i+2*j) for j in range(2))+'}' for i in range(33))+'};')
 ledger=c.ledger();ledger.update(prom_sha256=digest,templates=TEMPLATES,roots=ROOTS,path_source=0x78be0,path_length=33,implementation=['src/crew23.c'],unreferenced_aliases=[0x78f6e],pending=[],complete_port=False,validation='Source conversion only; no gameplay tests',source_ranges=[dict(start=0x78a84,end_exclusive=0x78f8a,sha256=hashlib.sha256(raw[0x78a84:0x78f8a]).hexdigest())])
 for name,value in [('crew23-programs.h','\n'.join(h)),('crew23-programs.c','\n'.join(out)),('crew23-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks))))
if __name__=='__main__':main()
