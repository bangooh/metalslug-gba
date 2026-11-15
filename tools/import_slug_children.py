#!/usr/bin/env python3
"""Extract Slug turret, wheel and marker dependencies; no game execution."""
import argparse,hashlib,json
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS={'RESTORE':0x29d3a8,'THROWN':0x29d090,'SPARK':0x29d7d2,
 'FIRE':0x2dd594,'ICON':0x32292,'ICON_ALT':0x32286}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args()
 raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('P-ROM fingerprint')
 c=AnimationCompiler(raw);entries={k:c.entry(v) for k,v in ROOTS.items()};c.compile()
 if c.fields or c.callbacks or c.bindings or c.parameters:raise ValueError('unreviewed child animation side effect')
 if any(n['op'] in ('ATTACK','DEFENSE','CONTACT','PARAMETERS') and not(n['op']=='ATTACK' and n['value']==0xffffffff) for n in c.nodes):raise ValueError('unreviewed child binding clear')
 drawing_maps=[];drawings=[]
 for angle in range(32):
  row=[];maps=[]
  for phase in range(6):
   source=c.long(0x289204+angle*24+phase*4);c.offset(source)
   parts=[dict(map=source,flags=0,dx=0,dy_up=0,priority=65535)]
   row.append(c.intern(c.drawings,c.draw_ids,json.dumps(parts,sort_keys=True),parts));maps.append(source)
  drawings.append(row);drawing_maps.append(maps)
 aims=[[c.long(0x289504+variant*64+i*4) for i in range(16)] for variant in range(5)]
 if any(v!=0xffffffff and v>65535 for row in aims for v in row):raise ValueError('unreviewed turret angle')
 offsets=[[c.signed(0x29d752+i*4+j*2) for j in range(2)] for i in range(32)]
 wheel_offsets=[c.word(0x2fb92+i*2) for i in range(4)]
 if wheel_offsets!=[0x16,0x18,0x1a,0x1c]:raise ValueError('wheel palette slots')
 bounds={k:[c.signed(at+i*2) for i in range(4)] for k,at in [('fire',0x2df206),('icon',0x32070)]}
 h=['#ifndef SLUG_CHILD_PROGRAMS_H','#define SLUG_CHILD_PROGRAMS_H','#include "../src/animation.h"',
  'enum SlugChildAnimation {'+','.join('SCA_'+k for k in ROOTS)+',SCA_COUNT};',
  'extern const struct AnimationProgram slug_child_program;','extern const uint16_t slug_child_entries[SCA_COUNT];',
  'extern const int32_t slug_turret_aims[5][16];','extern const uint16_t slug_turret_drawings[32][6];',
  'extern const int16_t slug_turret_offsets[32][2];']
 h+=['extern const int16_t slug_child_'+k+'_bounds[4];' for k in bounds]+['#endif']
 out=c.c_source('slug_child_program','slug-child-programs.h')
 out+=['const uint16_t slug_child_entries[SCA_COUNT]={'+vals(entries.values())+'};',
  'const int32_t slug_turret_aims[5][16]={'+','.join('{'+vals(-1 if v==0xffffffff else v for v in row)+'}' for row in aims)+'};',
  'const uint16_t slug_turret_drawings[32][6]={'+','.join('{'+vals(row)+'}' for row in drawings)+'};',
  'const int16_t slug_turret_offsets[32][2]={'+','.join('{'+vals(row)+'}' for row in offsets)+'};']
 out+=['const int16_t slug_child_'+k+'_bounds[4]={'+vals(v)+'};' for k,v in bounds.items()]
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,entries=entries,
  turret_aims=dict(source=0x289504,values=aims),turret_drawings=dict(source=0x289204,maps=drawing_maps,ids=drawings),
  turret_offsets=dict(source=0x29d752,values=offsets),wheel_palette_offsets=dict(source=0x2fb92,values=wheel_offsets),
  bounds=bounds,source_helpers=[0x457a6,0x457cc,0x4580c,0x458dc,0x4593a,0x459a8,0x459c0,0x459d8,0x459f0,
   0x45a0a,0x45a38,0x45abe,0x45b02,0x45b9c,0x45c20,0x45c72,0x45c98,0x45cae,0x45cc2,0x45cea,
   0x45d3a,0x45d8c,0x45fde,0x308b0,0x30c14,0x31fca,0x2fb9a,0x2fc6a,0x77f6a,0x780de],
  observations=['$045D1C..$045D2E loads the variant table but never installs or calls it; preserve the inert load',
   '$02FB92 is four palette offsets, not the disassembler-labelled ORI instructions',
   'Turret drawing table is raw sprite maps, not animation streams; publish native drawing IDs directly'],
  adaptations=['Scene supplies resolved player input, camera delta and mount gate',
   'Palette IDs and drawing IDs await all-pose renderer; no Neo Geo hardware pool emulation'],
  validation='Source extraction and ARM compilation only; no gameplay execution',complete=False)
 for name,value in [('slug-child-programs.h','\n'.join(h)),('slug-child-programs.c','\n'.join(out)),('slug-child-source.json',json.dumps(ledger,indent=2))]:
  (R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),aims=80,turret_selections=192)))
if __name__=='__main__':main()
