#!/usr/bin/env python3
"""Input-only UI smoke checks; known second-demo failure remains explicit."""
import argparse,json,os,subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--candidate',required=True,type=Path);p.add_argument('--output',required=True,type=Path);p.add_argument('--mgba-prefix',required=True,type=Path);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True);lib=a.mgba_prefix/'lib/x86_64-linux-gnu';exe=a.output/'play'
subprocess.run(['cc','-O2','-I'+str(a.mgba_prefix/'include'),str(R/'tests/native_followthrough.c'),'-L'+str(lib),'-Wl,-rpath-link,'+str(lib),'-lmgba','-o',str(exe)],check=True)
results={}
for name,mode,frames in [('tutorial',2,8000),('pause',5,1800),('link-cancel',4,900),('ranking',3,1800),('demo',0,16000)]:
 d=a.output/name;d.mkdir(exist_ok=True)
 with (d/'trace.jsonl').open('w') as log,(d/'emulator.log').open('w') as err:
  run=subprocess.run([str(exe.resolve()),str((a.candidate/'rom.gba').resolve()),str((a.candidate/'probe.txt').resolve()),str(frames),str(d.resolve()),str(mode)],env=dict(os.environ,LD_LIBRARY_PATH=str(lib)),stdout=log,stderr=err)
 rows=[json.loads(x) for x in (d/'trace.jsonl').read_text().splitlines()]
 results[name]=dict(exit_code=run.returncode,last=rows[-1],execution=json.loads((d/'execution.json').read_text()),paused_observed=any(x['session.paused'] for x in rows),screens=sorted(set(x['screen'] for x in rows)),presentations=sorted(set(x['presentation'] for x in rows)))
 print(name,run.returncode,rows[-1]['app.error'],flush=True)
(a.output/'summary.json').write_text(json.dumps(results,indent=2)+'\n')
assert all(results[n]['exit_code']==0 for n in ['tutorial','pause','link-cancel','ranking'])
assert results['pause']['paused_observed'] and not results['pause']['last']['session.paused']
# This is an explicit regression record, not a passing campaign assertion.
assert results['demo']['last']['app.error']==0x0b1e0000
