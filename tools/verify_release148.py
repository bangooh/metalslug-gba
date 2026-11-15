"""Reproduce the selected checksum-10 M1 smoke test with a local mGBA harness."""
import os,subprocess,json
from pathlib import Path
p=Path(__file__).resolve().parents[1];b=p/'build/work148';d=p/'docs/work148'
cc=os.environ['ARM_CC'];env=os.environ.copy()
subprocess.run(['python3',str(p/'tools/prepare_followthrough_probe.py'),'--cc',cc,'--elf',str(p/'build/native/native.elf'),'--output',str(b/'probe'),'--isolate'],cwd=p,env=env,check=True,stdout=open(d/'probe.log','w'),stderr=subprocess.STDOUT)
children=[]
for n in [1,2]:
 o=open(d/f'm1-{n}p.csv','w');e=open(d/f'm1-{n}p.log','w')
 c=subprocess.Popen([str(b/'cable'),str(p/'dist/metal-slug-gba-native-v0.5.148-wip.gba'),str(b/'probe/probe.txt'),'100000',str(n==2 and 1 or 0)],cwd=p,env=env,stdout=o,stderr=e)
 children.append((n,c,o,e));print('RUN',n,flush=True)
for n,c,o,e in children:
 r=c.wait();o.close();e.close();print('DONE',n,r,flush=True)
 if r:raise SystemExit(r)
