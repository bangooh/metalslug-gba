#!/usr/bin/env python3
"""Input-only title/movement paths; separate explicit renderer camera fixture."""
import argparse,json,os,subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--mgba-root',type=Path,required=True);a=ap.parse_args()
lib=a.mgba_root/'lib/x86_64-linux-gnu'
subprocess.run(['cc','-O2','-I'+str(a.mgba_root/'include'),'tests/arm_run.c','build/assets.S','-L'+str(lib),'-Wl,-rpath-link,'+str(lib),'-Wl,-z,noexecstack','-l:libmgba.so.0.10.2','-o','build/arm-run'],cwd=R,check=True)
sym=json.loads((R/'build/symbols-movement.json').read_text());out=[]
version=(R/'VERSION').read_text().strip()
for mode,n in [(0,300),(1,1600),(2,1800),(3,1200),(4,1000),(5,1200),(6,900),(7,3100)]:
 proc=subprocess.run(['build/arm-run',f'dist/metal-slug-gba-native-v{version}-movement-fixture.gba',str(sym['game']),str(sym['metrics']),str(mode),str(n),str(sym['g_audio_debug'])],cwd=R,env={**os.environ,'LD_LIBRARY_PATH':str(lib)},capture_output=True,text=True)
 if proc.returncode:raise RuntimeError(proc.stdout+'\n'+proc.stderr)
 v=json.loads(proc.stdout);print(v,flush=True)
 assert v['pixel_errors']==v['late_vblank']==0,v
 assert v['audio_irq_count']>0 and v['audio_unknown']==v['audio_underruns']==0,v
 if mode in (1,2):assert v['ended']==1 and v['reloads']==1,v
 if mode==5:assert v['reloads']==2,v
 if mode==6:assert v['reloads']==3,v
 v['workload']='renderer camera injection, no gameplay claim' if mode==7 else 'title and empty first-map movement, with M1 music; no combat'
 v['approx_present_fps']=(v['commits']-1)*16777216/280896/(v['last_present']-v['first_present'])
 v['worst_interval_ms']=v['max_gap_frames']*280896/16777216*1000
 v['worst_work_ms']=v['max_work_cycles']/16777216*1000
 v['worst_normal_work_ms']=v['normal_max_cycles']/16777216*1000
 v['mean_work_cycles_per_lcd_frame']=v['total_work_cycles']/(v['frames']-v['ready_frame'])
 out.append(v)
(R/'build/arm-validation.json').write_text(json.dumps(out,indent=2)+'\n')
