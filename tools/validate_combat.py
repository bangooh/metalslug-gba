#!/usr/bin/env python3
"""ARM input-only combat fixture plus tick-by-tick native host comparison.

Does not purport to be the M1 enemy/camera/boss playthrough.
"""
import argparse,ctypes as C,json,os,struct,subprocess
from pathlib import Path
from PIL import Image
R=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--mgba-root',type=Path,required=True);a=ap.parse_args();libdir=a.mgba_root/'lib/x86_64-linux-gnu'
subprocess.run(['cc','-O2','-I'+str(a.mgba_root/'include'),'tests/combat_arm.c','-L'+str(libdir),'-Wl,-rpath-link,'+str(libdir),'-l:libmgba.so.0.10.2','-o','build/combat-arm'],cwd=R,check=True)
subprocess.run(['cc','-shared','-fPIC','-O2','-DCOMBAT_FIXTURE','src/game.c','src/combat.c','src/hitbox.c','generated/combat-assets.c','build/assets.S','-Wl,-z,noexecstack','-o','build/combat-host.so'],cwd=R,check=True)
host=C.CDLL(str(R/'build/combat-host.so'));sym=json.loads((R/'build/symbols-combat.json').read_text());version=(R/'VERSION').read_text().strip();out=[]
for mode in range(5):
 proc=subprocess.run(['build/combat-arm',f'dist/metal-slug-gba-native-v{version}-combat-fixture.gba',str(sym['game']),str(sym['combat']),str(sym['metrics']),str(sym['g_audio_debug']),str(mode)],cwd=R,env={**os.environ,'LD_LIBRARY_PATH':str(libdir)},capture_output=True,text=True,check=True)
 v=json.loads(proc.stdout);print(v,flush=True)
 assert v['kills']==v['drops']==(0 if mode==4 else 3 if mode==3 else 1),v
 assert v['strikes']==(0 if mode in (0,4) else 3 if mode==3 else 1),v
 assert v['shots']==(1 if mode in (0,4) else 0),v
 assert v['fault']==v['late_vblank']==v['audio_unknown']==v['audio_underruns']==0,v
 raw=(R/f'build/combat-trace-{mode}.bin').read_bytes();gs,cs=struct.unpack_from('<II',raw)
 game=(C.c_uint8*gs).in_dll(host,'game');combat=(C.c_uint8*cs).in_dll(host,'combat');host.game_boot();C.memset(C.addressof(combat),0,cs)
 checked=0
 for at in range(8,len(raw),8+gs+cs):
  frame,keys=struct.unpack_from('<II',raw,at);expected_game=raw[at+8:at+8+gs];expected_combat=raw[at+8+gs:at+8+gs+cs]
  host.game_step(keys)
  assert bytes(game)==expected_game,('game',mode,frame,bytes(game).hex(),expected_game.hex())
  # GBA's sound queue is drained by the audio driver after each game tick.
  host.combat_clear_sounds()
  assert bytes(combat)==expected_combat,('combat',mode,frame,[(i,x,y) for i,(x,y) in enumerate(zip(bytes(combat),expected_combat)) if x!=y][:12])
  checked+=1
 v.update(host_arm_ticks=checked,workload='Three declared test crates, no enemies; NOT an M1 playthrough',
          present_fps=(v['commits']-1)*16777216/280896/(v['last_present']-v['first_present']),
          worst_interval_ms=v['max_gap_frames']*280896/16777216*1000)
 for kind in ['attack','result']:
  path=R/f'build/combat-{mode}-{kind}.ppm'
  if path.exists():Image.open(path).save(path.with_suffix('.png'))
 out.append(v)
(R/'build/combat-arm-validation.json').write_text(json.dumps(out,indent=2)+'\n')
