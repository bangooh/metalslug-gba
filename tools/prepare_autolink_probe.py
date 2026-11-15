from pathlib import Path
import subprocess, struct, argparse, sys
p=argparse.ArgumentParser();p.add_argument('--cc',required=True);a=p.parse_args();cc=a.cc
out=Path('docs/work149/probe');subprocess.run([sys.executable,'tools/prepare_followthrough_probe.py','--cc',cc,'--output',str(out),'--isolate'],check=True)
syms={l.split()[-1]:int(l.split()[0],16) for l in subprocess.check_output([cc.replace('gcc','nm'),'-n','build/native/native.elf'],text=True).splitlines() if len(l.split())>=3}
fields=['control','peer_control','peer_running','slot','age','prepared','bad_packets']
(out/'link-offsets.c').write_text('#include "native_link.h"\n#include <stddef.h>\nconst unsigned a[]={'+','.join('offsetof(struct NativeLink,'+f+')' for f in fields)+'};')
subprocess.run([cc,'-Isrc','-c',str(out/'link-offsets.c'),'-o',str(out/'link-offsets.o')],check=True)
subprocess.run([cc.replace('gcc','objcopy'),'-O','binary','-j','.rodata',str(out/'link-offsets.o'),str(out/'link-offsets.bin')],check=True)
offsets=struct.unpack('<'+str(len(fields))+'I',(out/'link-offsets.bin').read_bytes());p=out/'probe.txt';text=p.read_text();known={l.split()[2] for l in text.splitlines()}
for f in ['linked','frozen','wire_active','vblank_count']:
 if f not in known:text+=f'{syms[f]:x} 4 {f}\n'
for f,off in zip(fields,offsets):
 if 'link.'+f not in known:text+=f'{syms["link"]+off:x} {4 if f in ("age","bad_packets") else 1} link.{f}\n'
for name,offset,width,label in [('transmit',112,2,'tx0.control'),('transmit',240,2,'tx1.control'),('published',12,4,'published.flags'),('active',0,1,'wire.active'),('position',0,1,'wire.position'),('pending_valid',0,1,'wire.pending'),('wire_errors',0,4,'wire.errors'),('in_position',0,1,'wire.inpos')]:text+=f'{syms[name]+offset:x} {width} {label}\n'
for i,f in enumerate(["raw_bytes","encoded_bytes","packets","retries","refreshes"]):text+=f'{syms["native_snapshot_stats"]+i*4:x} 4 snapshot.{f}\n'
p.write_text(text)
print('fields',len(text.splitlines()))
