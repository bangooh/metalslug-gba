#!/usr/bin/env python3
"""Extract the complete rider mount overlay, both characters and weapons."""
import argparse,hashlib,json
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args()
 raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('P-ROM fingerprint')
 c=AnimationCompiler(raw);entries=[];pointers=[]
 for character in range(2):
  row=[]
  for weapon in range(5):
   at=c.long(0x3da02+4*(character*5+weapon));pointers.append(at);row.append(c.entry(at,character))
  entries.append(row)
 c.compile()
 if c.fields or c.callbacks or c.bindings or c.parameters:raise ValueError('unreviewed overlay operation')
 if set(v['op'] for v in c.nodes)-{'DRAW','WAIT','END'}:raise ValueError('unreviewed overlay command')
 offsets=[[c.signed(0x3daa8+4*i),c.signed(0x3daaa+4*i)] for i in range(65)]
 rect=[c.word(0x3ef9a+2*i) for i in range(4)]
 h=['#ifndef SLUG_AUX_PROGRAMS_H','#define SLUG_AUX_PROGRAMS_H','#include "../src/animation.h"',
  'extern const struct AnimationProgram slug_overlay_program;',
  'extern const uint16_t slug_overlay_entries[2][5];','extern const int16_t slug_overlay_offsets[65][2];',
  'extern const uint16_t slug_mount_rect[4];','#endif']
 out=c.c_source('slug_overlay_program','slug-aux-programs.h')
 out+=['const uint16_t slug_overlay_entries[2][5]={'+','.join('{'+','.join(map(str,row))+'}' for row in entries)+'};',
  'const int16_t slug_overlay_offsets[65][2]={'+','.join('{'+','.join(map(str,row))+'}' for row in offsets)+'};',
  'const uint16_t slug_mount_rect[4]={'+','.join(map(str,rect))+'};']
 (R/'generated/slug-aux-programs.h').write_text('\n'.join(h)+'\n')
 (R/'generated/slug-aux-programs.c').write_text('\n'.join(out)+'\n')
 ledger=c.ledger();ledger.update(prom_sha256=digest,overlay=dict(source=0x3d944,table=0x3da02,pointers=pointers,entries=entries,
  offsets_source=0x3daa8,offsets=offsets,mount_rect_source=0x3ef9a,mount_rect=rect),
  complete=False,status='Mount overlay native constructor/follow/completion; no renderer or world vehicle creation')
 (R/'generated/slug-aux-source.json').write_text(json.dumps(ledger,indent=2)+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),selections=10,offsets=len(offsets))))
if __name__=='__main__':main()
