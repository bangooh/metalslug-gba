#!/usr/bin/env python3
"""Source-driven ambient/scene actors153..167(except156),180..184."""
import argparse,hashlib,json
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(A_WAIT=0x2f3ab4,A_ACTIVE=0x2f3ac0,B_WAIT=0x2f3af0,B_ACTIVE=0x2f3afc,CARRIER=0x2f3b2c,CARRIER_RELEASE=0x2f3b70,DROPPED=0x2f3c04,WALK=0x2f3c10,STAND=0x2f3c58,STAND2=0x2f3c64,HOP_UP=0x2f3c84,HOP_DOWN=0x2f3cb4,RUN_FIXED=0x2f3cc0,WALK2=0x2f3e0c,STAND_B=0x2f3e4a,STAND_B2=0x2f3e56,WIDE_WAIT=0x2f3e86,WIDE_ACTIVE=0x2f3e92,SENTRY=0x2f3e9e,ONE_SHOT=0x2f3f4c,SWING=0x2f3fc6,BACK=0x2f3fe2,TURN=0x2f3fee,BOB=0x2f4036,LEAP1=0x2f4072,LEAP2=0x2f407e,LEAP3=0x2f408a,RUN184=0x2f4096)
TEMPLATES={153:0x8df4a,154:0x8df94,155:0x8e0dc,157:0x8d994,158:0x8d9e2,159:0x8da4c,160:0x8e19c,161:0x8dc5a,162:0x8db50,163:0x8dc5a,164:0x8de70,165:0x8defc,166:0x8df0c,167:0x8e21c,180:0x8e2ac,181:0x8e30c,182:0x8e4e6,183:0x8e5ce,184:0x8e6e0}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw);entries={k:c.entry(v) for k,v in ROOTS.items()}
 if any(c.long(0xe8000+4*i)!=at for i,at in TEMPLATES.items()):raise ValueError('template dispatch')
 directions=[65535 if c.long(0x2f3cdc+4*i)==0xffffffff else c.entry(c.long(0x2f3cdc+4*i)) for i in range(8)];c.compile()
 if c.callbacks or {(x['source_offset'],x['width']) for x in c.fields}!={(0x5c,2)} or c.parameters or c.bindings:raise ValueError('unreviewed graph')
 choices=[]
 for at,states in [(0x2f3712,[0x8dc94,0x8dcfa,0x8dd26]),(0x2f3722,[0x8dfb8,0x8e02a,0x8e060])]:choices.append([states.index(c.long(at+4*i)) for i in range(4)])
 h=['#ifndef NATIVE_GRUNT153_PROGRAMS_H','#define NATIVE_GRUNT153_PROGRAMS_H','#include "../src/animation.h"','enum Grunt153Animation {'+','.join('G153A_'+k for k in ROOTS)+',G153A_COUNT};','enum Grunt153Field {G153F_SIGNAL};','extern const struct AnimationProgram grunt153_program;','extern const uint16_t grunt153_entries[G153A_COUNT],grunt153_directions[8];','extern const uint8_t grunt153_choices[2][4];','#endif']
 out=c.c_source('grunt153_program','grunt153-programs.h');out+=['const uint16_t grunt153_entries[G153A_COUNT]={'+vals(entries.values())+'};','const uint16_t grunt153_directions[8]={'+vals(directions)+'};','const uint8_t grunt153_choices[2][4]={'+','.join('{'+vals(x)+'}' for x in choices)+'};']
 ledger=c.ledger();ledger.update(prom_sha256=digest,templates=TEMPLATES,roots=ROOTS,direction_table=0x2f3cdc,choice_tables=[0x2f3712,0x2f3722],implementation=['src/grunt153.c'],unreferenced_aliases=[0x8da30,0x8dc4a,0x8de64,0x8dee0,0x8e0ba,0x8e1fa,0x8f11c],pending=['Template156 and capsule/mission-end scene flow are a separate remaining dependency group'],complete_port=False,validation='Source conversion only; no gameplay tests',source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in [(0x8d184,0x8d3b4),(0x8d994,0x8e738),(0x8efb0,0x8f138)]])
 for name,value in [('grunt153-programs.h','\n'.join(h)),('grunt153-programs.c','\n'.join(out)),('grunt153-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),templates=len(TEMPLATES))))
if __name__=='__main__':main()
