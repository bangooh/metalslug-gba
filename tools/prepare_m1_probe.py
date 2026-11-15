#!/usr/bin/env python3
"""Read-only ARM telemetry for the private M1 renderer layout."""
import argparse,struct,subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cc',required=True);p.add_argument('--directory',required=True,type=Path);a=p.parse_args();d=a.directory.resolve()
fields=['groups','culling','frame.layers','stream[0].upload_count','stream[1].upload_count','pool1_scratch','committed_groups','staged_frames','staged_bytes']
source='#include "m1_native/runtime.c"\n#include <stddef.h>\n__attribute__((section(".layout"))) const unsigned layout[]={offsetof(struct NativeVideo,backgrounds.tiles),'+','.join('offsetof(struct State,'+f+'),sizeof(((struct State*)0)->'+f+')' for f in fields)+'};\n'
(d/'m1-layout.c').write_text(source)
subprocess.run([a.cc,'-mcpu=arm7tdmi','-marm','-I.','-c',str(d/'m1-layout.c'),'-o',str(d/'m1-layout.o')],cwd=R,check=True)
subprocess.run([a.cc.replace('gcc','objcopy'),'-O','binary','-j','.layout',str(d/'m1-layout.o'),str(d/'m1-layout.bin')],check=True)
values=struct.unpack('<'+str(1+2*len(fields))+'I',(d/'m1-layout.bin').read_bytes())
nm=subprocess.check_output([a.cc.replace('gcc','nm'),'-n',str(d/'native.elf')],text=True)
video=next(int(x.split()[0],16) for x in nm.splitlines() if x.split()[-1]=='video')
s=(d/'probe.txt').read_text()
for i,f in enumerate(fields):s+=f'{video+values[0]+values[1+2*i]:x} {values[2+2*i]} m1.{f}\n'
(d/'m1-probe.txt').write_text(s);print('M1 probe fields:',len(s.splitlines()))
