#!/usr/bin/env python3
"""Experimental freestanding ARM7TDMI Clang + lld builder for this source tree.
The GBA image is a compile/link artifact; it has NOT been emulator/hardware validated.
"""
from pathlib import Path
import concurrent.futures, hashlib, subprocess, sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from compile_port import port_units
from rom_support import elf_to_rom, fix_header
UNITS=['src/startup.S']+port_units()
ARM_UNITS={'src/hud_bg.c','src/native_background_assets.c','src/object_video_gba.c','src/background_video.c','src/background_tile.c','src/fixed_object.c','src/native_audio.c','src/native_audio_gba.c','src/gba_interrupts.c','src/freestanding.c'}
OUT=ROOT/'build/llvm-arm';OUT.mkdir(parents=True,exist_ok=True)
def compile_one(unit):
    src=ROOT/unit
    obj=OUT/(unit.replace('/','_')+'.o')
    arm=unit in ARM_UNITS or unit.endswith('.S')
    cmd=['clang','--target=arm-none-eabi','-mcpu=arm7tdmi', '-marm' if arm else '-mthumb','-I'+str(ROOT/'tools/llvm_arm_include'),'-O2' if arm else '-O3', '-ffreestanding','-fno-builtin','-fno-strict-aliasing','-ffunction-sections','-fdata-sections', '-Wno-unknown-attributes', '-c',str(src),'-o',str(obj)]
    if unit.endswith('.c'):cmd+=['-std=c11']
    subprocess.run(cmd,check=True,cwd=ROOT,stdout=subprocess.DEVNULL)
    return obj
if __name__=='__main__':
    with concurrent.futures.ThreadPoolExecutor(max_workers=10) as pool:
        objects=list(pool.map(compile_one,UNITS))
    objects.extend(compile_one(u) for u in ['src/llvm_aeabi.c','src/llvm_aeabi.S'])
    elf=OUT/'metal-slug.elf'
    cmd=['clang','--target=arm-none-eabi','-fuse-ld=lld','-mcpu=arm7tdmi','-nostdlib',*map(str,sorted(objects)),'-Tsrc/linker-lld-build.ld','-Wl,--no-check-sections,--gc-sections,-Map='+str(OUT/'metal-slug.map'),'-o',str(elf)]
    subprocess.run(cmd,check=True,cwd=ROOT)
    rom=elf_to_rom(elf.read_bytes());fix_header(rom)
    rom[0xa0:0xac]=b'MSLUG NATIVE';rom[0xac:0xb0]=b'MSN1'
    rom[0xbd]=(-sum(rom[0xa0:0xbd])-0x19)&255
    rom.extend(b'\xff'*(-len(rom)%4))
    path=ROOT/'dist/MetalSlug-v0.5.159-parallax-LLVM-test.gba';path.parent.mkdir(exist_ok=True);path.write_bytes(rom)
    print(path,len(rom),hashlib.sha256(rom).hexdigest())
