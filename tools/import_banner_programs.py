#!/usr/bin/env python3
"""Static banner/decorative templates230..243, thrown animation and scene switches."""
import argparse,hashlib,json
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
TEMPLATES={230:0x46322,231:0x463ee,232:0x46404,233:0x4641a,234:0x46430,235:0x46446,236:0x4645c,237:0x46474,238:0x4647c,239:0x46484,240:0x464a6,241:0x464b2,242:0x463da,243:0x46534,321:0x28db5a,322:0x28db6a,327:0x469e2}
FIXED=[0x24d800,0x23603a,0x23603a,0x23ac84,0x23c778,0x239a4e,0x23d4a0,0x24c3e2,0x23a068,0x249bd4,0x249c0c,0x233c16]
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw)
 if any(c.long(0xe8000+4*i)!=at for i,at in TEMPLATES.items()):raise ValueError('template dispatch')
 thrown=c.entry(0x28dc6e);c.compile()
 if c.fields or c.callbacks or c.parameters or c.bindings:raise ValueError('unreviewed graph')
 def drawing(at):
  parts=[dict(map=at,flags=8,dx=0,dy_up=0,priority=65535)]
  return c.intern(c.drawings,c.draw_ids,json.dumps(parts,sort_keys=True),parts)
 random=[drawing(c.long(0x28db86+4*i)) for i in range(16)];fixed=[drawing(at) for at in FIXED]
 h=['#ifndef NATIVE_BANNER_PROGRAMS_H','#define NATIVE_BANNER_PROGRAMS_H','#include "../src/animation.h"','extern const struct AnimationProgram banner_program;',f'enum {{BANNER_THROWN={thrown}}};','extern const uint16_t banner_random_drawings[16],banner_fixed_drawings[12],banner_random_palettes[4][16],banner_table_palettes[16];','extern const int16_t banner_bounds[2][4];','#endif']
 out=c.c_source('banner_program','banner-programs.h');out+=['const uint16_t banner_random_drawings[16]={'+vals(random)+'};','const uint16_t banner_fixed_drawings[12]={'+vals(fixed)+'};','const uint16_t banner_random_palettes[4][16]={'+','.join('{'+vals(c.word(c.long(0x28dbc6+4*j)+2*i) for i in range(16))+'}' for j in range(4))+'};','const uint16_t banner_table_palettes[16]={'+vals(c.word(0x28dc4e+2*i) for i in range(16))+'};','const int16_t banner_bounds[2][4]={'+','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in [0x28db7e,0x28dc46])+'};']
 ledger=c.ledger();ledger.update(prom_sha256=digest,templates=TEMPLATES,static_map_source=0x28db86,random_palette_source=0x28dbc6,table_palette_source=0x28dc4e,implementation=['src/banners.c'],shared_dependencies=['0723D2 body damage-smoke with zero Y offset; src/boss_fx.c','09A7AA item drop at parent plus Y4; src/items.c'],unreferenced_aliases=[0x463c2,0x46518,0x465ec],pending=['Drawing renderer and original continue controller consuming1081B0 remain separate'],complete_port=False,validation='Source extraction and compile only; no gameplay tests',source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[c.offset(lo):c.offset(hi-1)+1]).hexdigest()) for lo,hi in [(0x46322,0x46608),(0x469e2,0x469fe),(0x28db5a,0x28dcf4)]])
 for name,value in [('banner-programs.h','\n'.join(h)),('banner-programs.c','\n'.join(out)),('banner-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),templates=len(TEMPLATES))))
if __name__=='__main__':main()
