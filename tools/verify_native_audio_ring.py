#!/usr/bin/env python3
"""Verify real FIFO DMA continuity under an input-only replay, no game writes."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
p.add_argument('--cc',required=True);p.add_argument('--elf',type=Path,required=True)
p.add_argument('--rom',type=Path,required=True);p.add_argument('--inputs',type=Path,required=True)
p.add_argument('--mgba-prefix',type=Path,required=True);p.add_argument('--frames',type=int,default=36000)
p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
nm=subprocess.check_output([a.cc.replace('gcc','nm'),str(a.elf)],text=True)
ring=next(l.split()[0] for l in nm.splitlines() if l.split()[-1]=='ring')
lib=a.mgba_prefix/'lib/x86_64-linux-gnu';exe=a.output/'observe'
subprocess.run(['cc','-O2','-I'+str(a.mgba_prefix/'include'),str(R/'tests/native_audio_ring.c'),'-L'+str(lib),'-Wl,-rpath-link,'+str(lib),'-lmgba','-o',str(exe)],check=True)
with (a.output/'observer.log').open('w') as err:
 run=subprocess.run([str(exe.resolve()),str(a.rom.resolve()),str(a.inputs.resolve()),ring,str(a.frames)],stdout=subprocess.PIPE,stderr=err,text=True,env=dict(os.environ,LD_LIBRARY_PATH=str(lib)),check=True)
r=json.loads(run.stdout);r.update(rom_sha256=hashlib.sha256(a.rom.read_bytes()).hexdigest(),elf_sha256=hashlib.sha256(a.elf.read_bytes()).hexdigest(),inputs_sha256=hashlib.sha256(a.inputs.read_bytes()).hexdigest(),method='Read-only mGBA0.10.2 store wrappers compare every DMA1 ring restart with actual nextSource; original stores unchanged; key replay only; not physical hardware evidence')
(a.output/'verification.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r))
