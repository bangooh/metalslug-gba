#!/usr/bin/env python3
"""ARM compiler equivalence for source-tested M1 command routines, not gameplay."""
import argparse,json,os,subprocess
from pathlib import Path
from rom_support import elf_to_rom,fix_header
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cc',required=True);p.add_argument('--mgba-root',type=Path,required=True);a=p.parse_args()
m=json.loads((R/'generated/mission-source.json').read_text());base=int(m['source_range'][0],16)
(R/'build/mission-cases.h').write_text('static const uint16_t mission_cases[]={'+','.join(str(int(n['address'],16)-base) for n in m['nodes'])+'};\n')
asm=['.section .rodata,"a",%progbits']
for symbol,file in [('mission_data','mission.bin'),('mission_next','mission.skip')]:asm+=['.balign 4',f'.global {symbol}',f'{symbol}:',f'.incbin "generated/{file}"']
(R/'build/mission-assets.S').write_text('\n'.join(asm)+'\n.section .note.GNU-stack,"",%progbits\n')
files=['tests/mission_arm.c','src/mission.c','build/mission-assets.S']
def run(args,**kw):return subprocess.check_output(args,cwd=R,text=True,**kw).strip()
run(['cc','-O2','-fno-builtin',*files,'-o','build/mission-host-test']);expected=run(['build/mission-host-test'])
run([a.cc,'-mcpu=arm7tdmi','-marm','-O2','-ffreestanding','-fno-builtin','-nostdlib','-ffunction-sections','-fdata-sections',
     'src/startup.S',*files,'-Tsrc/linker.ld','-Wl,--gc-sections','-lgcc','-o','build/mission-test.elf'])
rom=elf_to_rom((R/'build/mission-test.elf').read_bytes());fix_header(rom);(R/'build/mission-test.gba').write_bytes(rom)
symbols=run([str(Path(a.cc).with_name('arm-none-eabi-nm')),'build/mission-test.elf']);at=int(next(l.split()[0] for l in symbols.splitlines() if l.endswith(' mission_proof')),16)
lib=a.mgba_root/'lib/x86_64-linux-gnu'
run(['cc','-O2','-I'+str(a.mgba_root/'include'),'tests/read_arm.c','-L'+str(lib),'-Wl,-rpath-link,'+str(lib),'-l:libmgba.so.0.10.2','-o','build/read-arm'])
got=run(['build/read-arm','build/mission-test.gba',str(at)],env={**os.environ,'LD_LIBRARY_PATH':str(lib)})
assert got==expected,(got,expected)
h,c,f=map(int,got.split());assert f==0
out=dict(passed=True,hash=h,cases=c,faults=f,scope='M1 command module ARM/host equivalence; not campaign completion')
(R/'build/mission-arm.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out))
