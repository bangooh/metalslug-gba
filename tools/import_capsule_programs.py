#!/usr/bin/env python3
"""Capsule/mission-ending actors, dropper244/item245 and scene fade constants."""
import argparse,hashlib,json
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(GLOW=0x2f377e,GLOW2=0x2f37da,FLASH=0x2f38f8,OPEN=0x2f3904,OUT=0x2f3910,MARKER=0x2f3960,MARKER2=0x2f39dc,MARKER3=0x2f39e8,ITEM=0x2f3b2c,END_FLASH=0x2f44f6,FLY=0x2f460c,DESCEND=0x2f4628,REST=0x2f46ca)
TEMPLATES={156:0x8d852,244:0x8ccea,245:0x8ce1e,246:0x8c7e2,247:0x8c7f2,248:0x8c800,320:0x8c810}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw);entries={k:c.entry(v) for k,v in ROOTS.items()};drop=[65535 if c.long(0x2f455c+4*i)==0xffffffff else c.entry(c.long(0x2f455c+4*i)) for i in range(11)]
 if any(c.long(0xe8000+4*i)!=at for i,at in TEMPLATES.items()):raise ValueError('template dispatch')
 c.compile()
 if c.callbacks or {(x['source_offset'],x['width']) for x in c.fields}!={(0x5c,1)} or c.parameters or c.bindings:raise ValueError('unreviewed graph')
 h=['#ifndef NATIVE_CAPSULE_PROGRAMS_H','#define NATIVE_CAPSULE_PROGRAMS_H','#include "../src/animation.h"','enum CapsuleAnimation {'+','.join('CAPA_'+k for k in ROOTS)+',CAPA_COUNT};','enum CapsuleField {CAPF_FADE_STEP};','extern const struct AnimationProgram capsule_program;','extern const uint16_t capsule_entries[CAPA_COUNT],capsule_droppers[11],scene_fade_rates[4][2];','#endif']
 out=c.c_source('capsule_program','capsule-programs.h');out+=['const uint16_t capsule_entries[CAPA_COUNT]={'+vals(entries.values())+'};','const uint16_t capsule_droppers[11]={'+vals(drop)+'};','const uint16_t scene_fade_rates[4][2]={'+','.join('{'+vals(c.word(0x52570+4*i+2*j) for j in range(2))+'}' for i in range(4))+'};']
 ledger=c.ledger();ledger.update(prom_sha256=digest,templates=TEMPLATES,roots=ROOTS,dropper_source=0x2f455c,implementation=['src/capsule.c','src/scene_fade.c'],unreferenced_aliases=[0x8d5cc,0x8d842],pending=['Scene B/C driver, camera velocity consumers, global fade renderer and raw audio control consumer are separate integration work'],complete_port=False,validation='Source conversion only; no gameplay tests',source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in [(0x8ccc6,0x8ce64),(0x8d3b4,0x8d994),(0x5239e,0x52580),(0x8c7e2,0x8c820)]])
 for name,value in [('capsule-programs.h','\n'.join(h)),('capsule-programs.c','\n'.join(out)),('capsule-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),templates=len(TEMPLATES))))
if __name__=='__main__':main()
