"""Prepare read-only profiling symbols for the current cartridge build."""
from pathlib import Path
import argparse, subprocess, struct
p=argparse.ArgumentParser();p.add_argument('--cc',required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1];out=root/'build/perf100'
fields=[('video','backgrounds.uploaded','NativeVideo'),('video','tiles.revision','NativeVideo'),('video','commit_pending','NativeVideo'),('video','reveal_pending','NativeVideo'),('runtime','vblanks','NativeRuntime'),('runtime','ready','NativeRuntime')]
(out/'offsets.c').write_text('#include "native_video.h"\n#include "native_runtime.h"\n#include <stddef.h>\nconst unsigned a[]={'+','.join(f'offsetof(struct {t},{f}),sizeof(((struct {t}*)0)->{f})' for s,f,t in fields)+'};')
subprocess.run([a.cc,'-mcpu=arm7tdmi','-marm','-Isrc','-c',str(out/'offsets.c'),'-o',str(out/'offsets.o')],cwd=root,check=True)
subprocess.run([a.cc.replace('gcc','objcopy'),'-O','binary','-j','.rodata',str(out/'offsets.o'),str(out/'offsets.bin')],check=True)
nm=subprocess.check_output([a.cc.replace('gcc','nm'),'-S','-n',str(root/'build/native/native.elf')],text=True)
syms={l.split()[-1]:int(l.split()[0],16) for l in nm.splitlines() if len(l.split())>=3}
values=struct.unpack('<'+str(len(fields)*2)+'I',(out/'offsets.bin').read_bytes());probe=(root/'build/native/probe.txt').read_text()
for i,(s,f,t) in enumerate(fields):probe+=f'{syms[s]+values[2*i]:x} {values[2*i+1]} {s}.{f}\n'
(out/'probe.txt').write_text(probe);(out/'symbols.txt').write_text('\n'.join(l for l in nm.splitlines() if len(l.split())==4 and l.split()[2] in 'tT')+'\n')
s=(root/'tests/native_diagnosis.c').read_text().replace('"native_fixed_objects"','"native_fixed_prefix"').replace('"native_object_scene_stage"','"native_object_scene_append"')
s=s.replace('{600,3000,15000}','{600,1800,3600}').replace('{1200,3600,15600}','{900,2100,3900}').replace('switch(pc){case','switch(0){case')
# Halfword granularity also resolves adjacent Thumb functions.
s=s.replace('0x100000/4','0x100000/2').replace('0x8000/4','0x8000/2').replace('(pc-0x08000000)/4','(pc-0x08000000)/2').replace('(pc-0x03000000)/4','(pc-0x03000000)/2').replace('p+=4','p+=2').replace('(p-0x08000000)/4','(p-0x08000000)/2').replace('(p-0x03000000)/4','(p-0x03000000)/2')
(out/'native_diagnosis.c').write_text(s.replace('#include "native_play.c"','#include "../../tests/native_play.c"'))
