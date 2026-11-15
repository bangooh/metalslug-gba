#!/usr/bin/env python3
"""Check committed GBA BG maps and all valid native campaign tile-cache VRAM bytes."""
import argparse,json,struct,subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cc',required=True);p.add_argument('--elf',required=True,type=Path);p.add_argument('--rom',required=True,type=Path);p.add_argument('--mission',type=int,choices=[1,2,3,4,5,6],default=1);p.add_argument('directories',nargs='+',type=Path);a=p.parse_args();out=a.directories[0]
fields=['frame.maps','frame.layers','stream[0].owner','stream[1].owner','groups','pool1_scratch','staged_frames','staged_bytes']
s='#include "m1_native/runtime.c"\n#include <stddef.h>\n__attribute__((section(".layout"))) const unsigned layout[]={offsetof(struct NativeVideo,backgrounds.tiles),'+','.join('offsetof(struct State,'+f+')' for f in fields)+'};'
(out/'bg-layout.c').write_text(s)
subprocess.run([a.cc,'-mcpu=arm7tdmi','-marm','-I.','-c',str(out/'bg-layout.c'),'-o',str(out/'bg-layout.o')],cwd=R,check=True)
subprocess.run([a.cc.replace('gcc','objcopy'),'-O','binary','-j','.layout',str(out/'bg-layout.o'),str(out/'bg-layout.bin')],check=True)
offsets=struct.unpack('<9I',(out/'bg-layout.bin').read_bytes())
nm=subprocess.check_output([a.cc.replace('gcc','nm'),'-n',str(a.elf)],text=True);syms={x.split()[-1]:int(x.split()[0],16) for x in nm.splitlines() if len(x.split())>=3}
rom=a.rom.read_bytes();tilebase=syms[f'm{a.mission}_tiles4']-0x08000000
for d in a.directories:
 ram=(d/'ewram.bin').read_bytes();vram=(d/'vram.bin').read_bytes();base=syms['video']-0x02000000+offsets[0]
 at={k:base+off for k,off in zip(fields,offsets[1:])};layers=ram[at['frame.layers']];groups=ram[at['groups']];scratch=ram[at['pool1_scratch']]
 assert layers<=4 and not (scratch and groups&((1<<layers)-1))
 maps=ram[at['frame.maps']:at['frame.maps']+layers*2048]
 bad=sum(x!=y for x,y in zip(maps,vram[0xe000:0xe000+len(maps)]))
 checked=tilebad=0
 for g,capacity in enumerate([1024,768]):
  if g==1 and scratch:continue
  owners=struct.unpack_from('<1024H',ram,at[f'stream[{g}].owner'])
  for slot,ident in enumerate(owners[:capacity]):
   if ident==65535:continue
   expected=rom[tilebase+ident*32:tilebase+(ident+1)*32];actual=vram[g*0x8000+slot*32:g*0x8000+(slot+1)*32]
   assert len(expected)==len(actual)==32;checked+=32;tilebad+=sum(x!=y for x,y in zip(expected,actual))
 report=dict(layers=layers,groups=groups,verified_map_bytes=len(maps),map_mismatched_bytes=bad,verified_cached_tile_bytes=checked,tile_mismatched_bytes=tilebad,pool1_used_as_inactive_scratch=bool(scratch),prestage_frames=struct.unpack_from('<H',ram,at['staged_frames'])[0],prestage_bytes=struct.unpack_from('<I',ram,at['staged_bytes'])[0])
 (d/f'm{a.mission}-hardware.json').write_text(json.dumps(report,indent=2)+'\n');print(d.name,report);assert not(bad or tilebad)
