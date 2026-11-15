#!/usr/bin/env python3
"""Input-only mGBA opening integration; explicitly not a full M1 playthrough."""
import argparse,ctypes as C,json,os,struct,subprocess
from pathlib import Path
from PIL import Image
R=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--mgba-root',type=Path,required=True);ap.add_argument('--modes',default='0,1,2,3,4,5');a=ap.parse_args();lib=a.mgba_root/'lib/x86_64-linux-gnu'
subprocess.run(['cc','-O2','-I'+str(a.mgba_root/'include'),'tests/campaign_arm.c','-L'+str(lib),'-Wl,-rpath-link,'+str(lib),'-l:libmgba.so.0.10.2','-o','build/campaign-arm'],cwd=R,check=True)
subprocess.run(['cc','-shared','-fPIC','-O2','-DNATIVE_MISSION','src/game.c','src/combat.c','src/hitbox.c','src/mission.c','src/actors.c','src/campaign.c','src/terrain_edits.c','generated/combat-assets.c','generated/actor-assets.c','build/assets.S','-Wl,-z,noexecstack','-o','build/campaign-host.so'],cwd=R,check=True)
host=C.CDLL(str(R/'build/campaign-host.so'));sym=json.loads((R/'build/symbols.json').read_text());version=(R/'VERSION').read_text().strip();results=[]
for mode in map(int,a.modes.split(',')):
 proc=subprocess.run(['build/campaign-arm',f'dist/metal-slug-gba-native-v{version}.gba',*[str(sym[k])for k in ['game','combat','actors','campaign','metrics','g_audio_debug','terrain_edits']],str(mode)],cwd=R,env={**os.environ,'LD_LIBRARY_PATH':str(lib)},capture_output=True,text=True,check=True)
 v=json.loads(proc.stdout);print(v,flush=True)
 assert v['fault']==v['late_vblank']==v['audio_unknown']==v['audio_underruns']==v['sprite_overflow']==0,v
 if mode==0:assert v['player_hits']>0 and v['respawns']>0,v
 if mode==1:assert v['actor_kills']>0 and v['player_hits']==0,v
 if mode==2:assert v['food_pickups']>0 and v['terrain_edits']>0 and v['failed_template']==0x8e and v['fault_source']==0xe8aa8,v
 if mode==4:assert v['grenades']>0,v
 if mode==5:assert v['up_shot_seen'] and v['rescued']==1 and v['pow_gift_requests']==1 and v['player_hits']==0,v
 raw=(R/f'build/campaign-trace-{mode}.bin').read_bytes();sizes=struct.unpack_from('<5I',raw);names=['game','combat','actors','campaign','terrain_edits']
 states=[(C.c_uint8*size).in_dll(host,name)for size,name in zip(sizes,names)]
 for state,size in zip(states,sizes):C.memset(C.addressof(state),0,size)
 host.game_boot();checked=0
 for at in range(20,len(raw),8+sum(sizes)):
  frame,keys=struct.unpack_from('<II',raw,at);host.game_step(keys);host.combat_clear_sounds();offset=at+8
  for name,state,size in zip(names,states,sizes):
   expected=raw[offset:offset+size];offset+=size
   assert bytes(state)==expected,(name,mode,frame,[(i,x,y)for i,(x,y)in enumerate(zip(bytes(state),expected))if x!=y][:16])
  checked+=1
 v.update(host_arm_ticks=checked,workload='Original M1 event integration with native reduced soldier AI. Stops explicitly at unsupported event; not full M1.',
   present_fps=(v['live_commits']-1)*16777216/280896/(v['live_last']-v['live_first']),worst_game_interval_ms=v['live_gap_frames']*280896/16777216*1000)
 for path in (R/'build').glob(f'campaign-{mode}-*.ppm'):Image.open(path).save(path.with_suffix('.png'))
 results.append(v)
(R/'build/campaign-arm-validation.json').write_text(json.dumps(results,indent=2)+'\n')
