#!/usr/bin/env python3
"""Check actual committed OBJ DMA/OAM/palette bytes from read-only captures."""
import argparse,json,struct,subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--cc',required=True);p.add_argument('--elf',required=True,type=Path);p.add_argument('--rom',required=True,type=Path);p.add_argument('directories',type=Path,nargs='+');a=p.parse_args()
 out=a.directories[0];fields=['objects.frame.objects','objects.frame.uploads','objects.frame.count','objects.frame.upload_count','objects.frame.palettes','objects.frame.palette_count','objects.frame.oam']
 expr=[f'offsetof(struct NativeVideo,{f})' for f in fields]+['sizeof(struct NativeObjectEntry)','sizeof(struct NativeObjectUpload)']
 source='#include "native_video.h"\n#include <stddef.h>\nconst unsigned a[]={'+','.join(expr)+'};\n'
 (out/'object-offsets.c').write_text(source)
 subprocess.run([a.cc,'-mcpu=arm7tdmi','-marm','-Isrc','-c',str(out/'object-offsets.c'),'-o',str(out/'object-offsets.o')],cwd=R,check=True)
 subprocess.run([a.cc.replace('gcc','objcopy'),'-O','binary','-j','.rodata',str(out/'object-offsets.o'),str(out/'object-offsets.bin')],check=True)
 nm=subprocess.check_output([a.cc.replace('gcc','nm'),'-n',str(a.elf)],text=True)
 syms={l.split()[-1]:int(l.split()[0],16) for l in nm.splitlines() if len(l.split())>=3}
 offsets=struct.unpack('<9I',(out/'object-offsets.bin').read_bytes());base=syms['video']-0x02000000;rom=a.rom.read_bytes()
 for d in a.directories:
  ram=(d/'ewram.bin').read_bytes();vram=(d/'vram.bin').read_bytes();pal=(d/'pal.bin').read_bytes();oam=(d/'oam.bin').read_bytes()
  count=struct.unpack_from('<H',ram,base+offsets[2])[0];uploads=struct.unpack_from('<H',ram,base+offsets[3])[0];banks=ram[base+offsets[5]]
  assert count<=128 and uploads<=128 and banks<=16
  hud=sum(struct.unpack_from('<I',ram,base+offsets[0]+i*offsets[7])[0]==0 for i in range(count))
  (d/'hud-count.bin').write_bytes(struct.pack('<I',hud))
  bad=0;total=0;prestaged=0
  for i in range(uploads):
   pointer,tile,n=struct.unpack_from('<IHH',ram,base+offsets[1]+i*offsets[8]);n*=32
   if 0x08000000<=pointer<0x0a000000:data=rom[pointer-0x08000000:pointer-0x08000000+n]
   elif 0x06000000<=pointer<0x06018000:
    data=vram[pointer-0x06000000:pointer-0x06000000+n];prestaged+=n
    assert rom.find(data)>=0, 'Prestaged immutable bytes must exist in the cartridge'
   else:
    assert 0x02000000<=pointer<0x02040000;data=ram[pointer-0x02000000:pointer-0x02000000+n]
   assert len(data)==n;total+=n;bad+=sum(x!=y for x,y in zip(data,vram[0x10000+tile*32:0x10000+tile*32+n]))
  oam_bad=sum(x!=y for x,y in zip(oam,ram[base+offsets[6]:base+offsets[6]+1024]))
  pal_bad=sum(x!=y for x,y in zip(pal[512:512+banks*32],ram[base+offsets[4]:base+offsets[4]+banks*32]))
  report=dict(objects=count,hud_objects=hud,uploads=uploads,verified_obj_tile_bytes=total,prestaged_obj_bytes=prestaged,obj_tile_mismatched_bytes=bad,oam_mismatched_bytes=oam_bad,palette_mismatched_bytes=pal_bad)
  (d/'object-hardware.json').write_text(json.dumps(report,indent=2)+'\n');print(d.name,report);assert not(bad or oam_bad or pal_bad)
if __name__=='__main__':main()
