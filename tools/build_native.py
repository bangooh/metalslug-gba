#!/usr/bin/env python3
"""Link the actual NativePlayWorld cartridge. No execution or gameplay tests."""
import argparse,hashlib,json,os,shutil,subprocess,runpy
from pathlib import Path
from compile_port import port_units
from build_fingerprint import build_fingerprint
from rom_support import elf_to_rom,fix_header
R=Path(__file__).resolve().parents[1]
def main():
 from apply_final_fonts import apply as apply_final_fonts
 apply_final_fonts()
 # The menu is a baked copy of the final opening frame. Rebuild it before
 # fingerprinting/compiling so edited opening glyphs cannot leave a stale menu.
 runpy.run_path(str(R/'tools/build_title_from_opening.py'),run_name='__main__')
 from build_background_animation_row_index import build as compile_animation_rows
 compile_animation_rows(R)
 from compile_fx_steps import generate as compile_fx
 compile_fx()
 from compile_fixed_palette import generate as compile_fixed_palette
 compile_fixed_palette()
 p=argparse.ArgumentParser();p.add_argument('--cc',default=os.environ.get('ARM_CC','arm-none-eabi-gcc'));p.add_argument('--elf-only',action='store_true');p.add_argument('--incremental',action='store_true');a=p.parse_args()
 cc=shutil.which(a.cc) or a.cc;units=port_units();b=R/'build/native';b.mkdir(parents=True,exist_ok=True)
 # Regenerate the wire fingerprint from every linked source/header/asset.
 fingerprint=build_fingerprint(R,units)
 flags=['-Itools/llvm_arm_include','-mcpu=arm7tdmi','-mthumb-interwork','-O2','-std=c11','-ffreestanding','-fno-builtin','-fno-strict-aliasing','-ffunction-sections','-fdata-sections','-fstack-usage','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',*os.environ.get('NATIVE_EXTRA_CFLAGS','').split()]
 # Cartridge bus is16-bit; Thumb avoids fetching32-bit ARM instructions
 # from ROM. Hot loops retain ARM in32-bit IWRAM.
 arm_units={'src/native_opening.c','src/native_inflate.c','src/hud_bg.c','src/native_background_assets.c','src/object_video_gba.c','src/background_video.c','src/background_tile.c','src/fixed_object.c','src/native_audio.c','src/native_audio_gba.c','src/gba_interrupts.c','src/freestanding.c'}
 if os.environ.get('NATIVE_PROFILE'):flags.append('-DNATIVE_PROFILE')
 objects=[];compiled=0
 for unit in ['src/startup.S']+units:
  obj=b/(unit.replace('/','_')+'.o');dep=obj.with_suffix('.d');stamp=obj.with_suffix('.command.json')
  arm=unit in arm_units or unit.endswith('.S')
  command=[cc,*flags,'-Os' if unit=='src/native_inflate.c' else '-O2' if arm else '-O3','-marm' if arm else '-mthumb','-MMD','-MF',str(dep),'-c',unit,'-o',str(obj)]
  inputs=[R/unit]
  if dep.exists():inputs.extend(R/v for v in dep.read_text().replace('\\\n',' ').split(':',1)[1].split())
  if unit.endswith('.S'):
   import re
   inputs.extend(R/v for v in re.findall(r'\.incbin\s+"([^"]+)"',(R/unit).read_text()))
  fresh=a.incremental and obj.exists() and dep.exists() and stamp.exists() and json.loads(stamp.read_text())==command and all(v.exists() and v.stat().st_mtime_ns<=obj.stat().st_mtime_ns for v in inputs)
  if not fresh:subprocess.run(command,cwd=R,check=True);stamp.write_text(json.dumps(command));compiled+=1
  objects.append(str(obj))
 elf=b/'native.elf';mapfile=b/'native.map'
 subprocess.run([cc,'-mcpu=arm7tdmi','-marm','-nostdlib',*objects,'-Tsrc/linker.ld',f'-Wl,--gc-sections,-Map={mapfile}','-lgcc','-o',str(elf)],cwd=R,check=True)
 size=subprocess.check_output([str(Path(cc).with_name('arm-none-eabi-size')),'-A',str(elf)],text=True)
 report=dict(version=(R/'VERSION').read_text().strip(),target='native_main -> NativeRuntime -> NativeSession -> NativePlayWorld',elf=str(elf),elf_bytes=elf.stat().st_size,elf_sha256=hashlib.sha256(elf.read_bytes()).hexdigest(),fingerprint=fingerprint.hex(),units=len(units)+1,compiled_units=compiled,section_sizes=size,validation='ARM/Thumb compilation and static cartridge link; execution evidence is recorded separately',complete_port=False)
 if not a.elf_only:
  rom=elf_to_rom(elf.read_bytes());fix_header(rom);rom[0xa0:0xac]=b'MSLUG NATIVE'.ljust(12,b' ');rom[0xac:0xb0]=b'MSN1';rom[0xbd]=(-sum(rom[0xa0:0xbd])-0x19)&255;rom.extend(b'\xff'*((-len(rom))%4))
  out=R/'dist'/('metal-slug-gba-native-v'+report['version']+'.gba');out.parent.mkdir(exist_ok=True);out.write_bytes(rom);report.update(rom=str(out),rom_bytes=len(rom),rom_sha256=hashlib.sha256(rom).hexdigest())
 (R/'generated/native-build.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
if __name__=='__main__':main()
