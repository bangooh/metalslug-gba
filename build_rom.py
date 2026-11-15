#!/usr/bin/env python3
"""Build the GBA cartridge locally from user-supplied original game and BIOS ZIPs."""
import argparse,os,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent
PIN='574a517547f05bc8c1539fab1b7d566035543496'
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--rom',type=Path,required=True);p.add_argument('--bios',type=Path,required=True);p.add_argument('--cc',default='arm-none-eabi-gcc');p.add_argument('--source',type=Path,default=ROOT/'.work/upstream');a=p.parse_args()
 sys.path.insert(0,str(ROOT/'tools'));from check_original_inputs import check
 check(a.rom,a.bios)
 if not shutil.which(a.cc):p.error('Install GNU Arm Embedded GCC, or supply --cc /path/to/arm-none-eabi-gcc')
 source=a.source.resolve()
 if not source.exists():
  source.parent.mkdir(parents=True,exist_ok=True)
  subprocess.run(['git','clone','--no-checkout','https://github.com/correo415415/mslug-decompiled.git',str(source)],check=True)
  subprocess.run(['git','-C',str(source),'checkout','--detach',PIN],check=True)
 head=subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()
 if head!=PIN:p.error('Existing upstream checkout is not at '+PIN+'; use a separate checkout at that revision')
 env=os.environ.copy();env.setdefault('NATIVE_EXTRA_CFLAGS','-Wno-error=maybe-uninitialized')
 subprocess.run([sys.executable,str(ROOT/'tools/rebuild_verified_assets.py'),'--source',str(source),'--rom',str(a.rom.resolve()),'--bios',str(a.bios.resolve()),'--cc',a.cc],cwd=ROOT,env=env,check=True)
if __name__=='__main__':main()
