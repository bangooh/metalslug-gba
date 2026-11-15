#!/usr/bin/env python3
"""Compare the actual ARM memory routines with host libc at buffer boundaries."""
import argparse,json,os,subprocess
from pathlib import Path
from rom_support import elf_to_rom,fix_header
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cc',required=True,type=Path);p.add_argument('--mgba-root',required=True,type=Path);a=p.parse_args()
O=R/'build/perf113-memory-test';O.mkdir(exist_ok=True)
def run(args,**kw):return subprocess.check_output(list(map(str,args)),cwd=R,text=True,**kw).strip()
run(['cc','-O2','-fno-builtin','tests/memory_arm.c','-o',O/'host']);expected=run([O/'host'])
# A standalone fixture has no BG overlay; its IWRAM load image follows text.
ld=(R/'src/linker.ld').read_text();start=ld.index(' OVERLAY ');end=ld.index(' .iwram :',start)
ld=ld[:start]+ld[end:];ld=ld.replace('LOADADDR(.m1hot)+SIZEOF(.m1hot)','LOADADDR(.text)+SIZEOF(.text)');(O/'test.ld').write_text(ld)
run([a.cc,'-mcpu=arm7tdmi','-mthumb','-mthumb-interwork','-O2','-ffreestanding','-fno-builtin','-nostdlib',
     'src/startup.S','tests/memory_arm.c','src/freestanding.c','-T'+str(O/'test.ld'),'-Wl,--gc-sections','-lgcc','-o',O/'test.elf'])
rom=elf_to_rom((O/'test.elf').read_bytes());fix_header(rom);(O/'test.gba').write_bytes(rom)
symbols=run([a.cc.with_name('arm-none-eabi-nm'),O/'test.elf']);at=int(next(l.split()[0] for l in symbols.splitlines() if l.endswith(' memory_proof')),16)
lib=a.mgba_root/'lib/x86_64-linux-gnu'
(O/'read_extended.c').write_text((R/'tests/read_arm.c').read_text().replace('n<1200','n<12000'))
run(['cc','-O2','-I'+str(a.mgba_root/'include'),O/'read_extended.c','-L'+str(lib),'-Wl,-rpath-link,'+str(lib),'-lmgba','-o',O/'read'])
got=run([O/'read',O/'test.gba',at],env=dict(os.environ,LD_LIBRARY_PATH=str(lib)));assert expected==got,(expected,got)
h,n,e=map(int,got.split());assert not e
result=dict(passed=True,cases=n,hash=h,faults=e,scope='Actual ARM memcpy/memset vs host libc; all alignments, lengths0..260, ROM/EWRAM to EWRAM/IWRAM, return values and guard bytes')
(O/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
