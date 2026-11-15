#!/usr/bin/env python3
"""Input-only play with immutable ROM/ELF snapshots and failure-safe evidence."""
import argparse,csv,hashlib,json,os,shutil,subprocess,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
HZ=16777216
LCD=280896

def main():
 p=argparse.ArgumentParser()
 p.add_argument('--output',type=Path,required=True)
 p.add_argument('--build',type=Path,default=ROOT/'generated/native-build.json')
 p.add_argument('--probe',type=Path,required=True)
 p.add_argument('--mgba-prefix',type=Path,required=True)
 p.add_argument('--frames',type=int,default=14000)
 p.add_argument('--mode',type=int,default=8)
 a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 build=json.loads(a.build.read_text())
 for name,source in [('rom.gba',build['rom']),('native.elf',build['elf']),('probe.txt',a.probe),('symbols.txt',a.probe.with_name('symbols.txt'))]:shutil.copyfile(source,out/name)
 for name,key in [('rom.gba','rom_sha256'),('native.elf','elf_sha256')]:assert hashlib.sha256((out/name).read_bytes()).hexdigest()==build[key]
 (out/'build.json').write_text(json.dumps(build,indent=2)+'\n')
 lib=a.mgba_prefix.resolve()/'lib/x86_64-linux-gnu'
 subprocess.run(['cc','-O2','-I'+str(a.mgba_prefix.resolve()/'include'),str(ROOT/'tests/native_followthrough.c'),'-L'+str(lib),'-Wl,-rpath-link,'+str(lib),'-lmgba','-o',str(out/'play')],check=True)
 with (out/'trace.jsonl').open('w') as stdout,(out/'emulator.log').open('w') as stderr:
  result=subprocess.run([str(out/'play'),str(out/'rom.gba'),str(out/'probe.txt'),str(a.frames),str(out),str(a.mode)],stdout=stdout,stderr=stderr,env=dict(os.environ,LD_LIBRARY_PATH=str(lib)))
 trace=[json.loads(s) for s in (out/'trace.jsonl').read_text().splitlines()]
 commits=list(csv.DictReader((out/'commits.csv').open()))
 active=[r for r in commits if r['playing']=='1' and r['presentation']=='0' and r['demo']=='0']
 # begin() briefly exposes presentation=0 before the logo kind is assigned.
 # Start the gameplay interval at the first committed live-player frame.
 first_player=next((i for i,r in enumerate(active) if int(r['live0'])),len(active));active=active[first_player:]
 def interval(rows):
  if len(rows)<2:return None
  first,last=rows[0],rows[-1];seconds=(int(last['lcd'])-int(first['lcd']))*LCD/HZ
  return dict(lcd_start=int(first['lcd']),lcd_end=int(last['lcd']),camera_start=int(first['camera_x']),camera_end=int(last['camera_x']),commits=int(last['commits'])-int(first['commits']),seconds=seconds,fps=(int(last['commits'])-int(first['commits']))/seconds)
 ranges={f'camera_{lo}_{hi}':interval([r for r in active if lo<=int(r['camera_x'])<hi]) for lo,hi in [(0,1536),(1552,1660),(1660,1800),(1800,2000),(2000,2400),(2400,2649),(2649,3000),(3000,4000),(4000,6000)]}
 replay=os.environ.get('INPUT_REPLAY')
 invalid=[]
 if any(int(r['paused']) for r in active):invalid.append('Paused commits cannot represent simulation FPS')
 if any(int(y['commits'])<int(x['commits']) for x,y in zip(active,active[1:])):invalid.append('Game restart reset the commit counter')
 summary=dict(reference_verified=False,reference_status='GBA execution only; matched Neo Geo ROM comparison is required separately',rom_sha256=build['rom_sha256'],mode=a.mode,input_only=True,input_policy='recorded LCD-frame keys' if replay else 'state-aware automatic controller',replay_source_sha256=hashlib.sha256(Path(replay).read_bytes()).hexdigest() if replay else None,requested_lcd_frames=a.frames,exit_code=result.returncode,last={k:trace[-1][k] for k in ['lcd_frame','world.ticks','world.weapons.camera_x','app.error','video.late_transfers']},execution=json.loads((out/'execution.json').read_text()),invalid_benchmark_reasons=invalid,active_play=None if invalid else interval(active),ranges={} if invalid else ranges)
 (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
 with zipfile.ZipFile(out.with_suffix('.zip'),'w',zipfile.ZIP_DEFLATED) as z:
  for path in out.iterdir():
   if path.is_file() and path.suffix not in ('.gba','.elf') and path.name!='play':z.write(path,path.name)
 print(json.dumps(summary))

if __name__=='__main__':main()
