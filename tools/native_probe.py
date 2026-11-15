"""Derive ARM offsets for read-only emulator telemetry, no ROM code changes."""
import argparse,json,struct,subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cc',required=True);p.add_argument('--elf',type=Path,required=True);p.add_argument('--output',type=Path,default=R/'build/native');a=p.parse_args()
types={'world':'NativePlayWorld','video':'NativeVideo','scene':'SceneRuntime','session':'NativeSession','runtime':'NativeRuntime','audio':'NativeAudio','link':'NativeLink'}
members={
 'world':['ticks','fault','weapons.fault','weapons.players[0]','weapons.camera_x','weapons.camera_y','weapons.scene_number','entities.fault','entities.active_count','entities.peak_count','hits.fault','results.progress','results.live_active[0]','results.live_active[1]','enemies.mission_time','events.fault','video.fault','action_fault[0]','action_fault[1]'],
 'video':['fault','commits','late_transfers','objects.fault','objects.count','objects.strips.records','objects.frame.fault','objects.frame.count','objects.frame.tile_count','objects.frame.palette_count','backgrounds.fault','backgrounds.count','colors.fault','colors.count','fixed.fault','palettes.fault'],
 'scene':['fault','scene','program','camera.x','camera.y'],
 'session':['fault','state','paused'],
 'runtime':['fault','ready','refreshes'],
 'audio':['fault','music.command','music.position','music.asset','volume','mixed_samples','played_samples','clipped','preempted','rejected',*[f'voices[{i}].asset' for i in range(4)]],
 'link':['state','age'],
}
rows=[(key,field) for key,fields in members.items() for field in fields]
text='#include "native_runtime.h"\n#include "native_video.h"\n#include "native_audio.h"\n#include "native_link.h"\n#include <stddef.h>\nconst unsigned native_offsets[]={'+',\n'.join(f'offsetof(struct {types[k]},{f}),sizeof(((struct {types[k]}*)0)->{f})' for k,f in rows)+'};\n'
b=a.output.resolve();b.mkdir(parents=True,exist_ok=True);source=b/'probe.c';source.write_text(text)
subprocess.run([a.cc,'-mcpu=arm7tdmi','-marm','-Isrc','-c',str(source),'-o',str(b/'probe.o')],cwd=R,check=True)
subprocess.run([str(Path(a.cc).with_name('arm-none-eabi-objcopy')),'-O','binary','-j','.rodata',str(b/'probe.o'),str(b/'probe.bin')],check=True)
values=struct.unpack('<'+str(len(rows)*2)+'I',(b/'probe.bin').read_bytes())
symbols={line.split()[-1]:int(line.split()[0],16) for line in subprocess.check_output([str(Path(a.cc).with_name('arm-none-eabi-nm')),str(a.elf)],text=True).splitlines() if len(line.split())>=3}
out=[]
for i,(symbol,member) in enumerate(rows):out.append(dict(name=symbol+'.'+member,address=symbols[symbol]+values[i*2],width=values[i*2+1]))
for key in ('screen','playing','presentation','demo','vblank_count'):out.append(dict(name=key,address=symbols[key],width=4))
for field,offset in [('app.refreshes',0),('app.frames',4),('app.error',8)]:out.append(dict(name=field,address=symbols['native_app_status']+offset,width=4))
extra={'player_x': 'actor.transform.x','player_y':'actor.transform.y','player_hp':'actor.hit.hp','player_state':'body_state'}
source.write_text('#include "player_actions.h"\n#include <stddef.h>\nconst unsigned extra[]={'+','.join('offsetof(struct PlayerBodyState,'+f+')' for f in extra.values())+'};\n')
subprocess.run([a.cc,'-mcpu=arm7tdmi','-marm','-Isrc','-c',str(source),'-o',str(b/'probe.o')],cwd=R,check=True)
subprocess.run([str(Path(a.cc).with_name('arm-none-eabi-objcopy')),'-O','binary','-j','.rodata',str(b/'probe.o'),str(b/'probe.bin')],check=True)
for k,v in zip(extra,struct.unpack('<4I',(b/'probe.bin').read_bytes())):out.append(dict(name='offset.'+k,address=v,width=0))
(b/'probe.json').write_text(json.dumps(out,indent=2)+'\n');(b/'probe.txt').write_text(''.join(f"{r['address']:x} {r['width']} {r['name']}\n" for r in out));print('ARM telemetry fields:',len(out))
