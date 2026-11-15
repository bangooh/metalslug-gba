"""ARM-derived telemetry offsets for input-only extended-play diagnostics."""
from pathlib import Path
import argparse, struct, subprocess, sys
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cc',required=True);p.add_argument('--output',required=True,type=Path);p.add_argument('--elf',type=Path,default=ROOT/'build/native/native.elf');p.add_argument('--isolate',action='store_true',help='Write intermediate probe objects under the selected output, not build/native');a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
base=out/'base-probe' if a.isolate else ROOT/'build/native'
# Never inherit stale addresses after a link changes BSS layout.
subprocess.run([sys.executable,str(ROOT/'tools/native_probe.py'),'--cc',a.cc,'--elf',str(a.elf.resolve()),'--output',str(base)],cwd=ROOT,check=True)
constants={
 'scene_collision':'offsetof(struct SceneRuntime,maps.collision)',
 'terrain_platforms':'offsetof(struct NativePlayWorld,terrain.platforms)',
 'entity_stride':'sizeof(struct NativeEntity)',
 'entity_base':'offsetof(struct NativePlayWorld,entities.nodes)',
 **{f'entity_{x}':f'offsetof(struct NativeEntity,{x})' for x in ['active','kind','tick','parent']},
 **{f'enemy_{k}':f'offsetof(struct NativeEntity,payload.enemy.actor.{v})' for k,v in {
 'x':'transform.x','y':'transform.y','hp':'hit.hp','palette':'palette','state':'state','flags':'hit.flags','anim':'animation.drawing_program','velocity_x':'vx'}.items()}}
constants.update({f'weapon_{k}':f'offsetof(struct WeaponActor,{v})' for k,v in {
 'x':'transform.x','y':'transform.y','vx':'vx','vy':'vy','ay':'ay','slope':'ground_slope',
 'state':'state','kind':'kind','palette':'palette','drawing':'animation.drawing','anim':'animation.drawing_program',
 'owner':'owner','damage_owner':'damage_owner','flags':'hit.flags','damage':'damage_timer','move':'movement_flags',
 'display':'display_flags','platform':'platform_ids','hp':'hit.hp','phase':'phase'}.items()})
constants['item_kind']='offsetof(struct ItemActor,kind)'
constants['entity_cosmetic']='offsetof(struct NativeEntity,cosmetic)'
constants['mission_task_offset']='offsetof(struct NativeEntity,payload.mission.task.offset)'
constants['weapon_facing']='offsetof(struct WeaponActor,transform.facing)'
constants['enemy_defense']='offsetof(struct NativeEntity,payload.enemy.actor.defense)'
constants['enemy_defense_count']='offsetof(struct NativeEntity,payload.enemy.actor.defense_count)'
constants['defense_stride']='sizeof(struct HitDefense)'
constants['enemy_params']='offsetof(struct NativeEntity,payload.enemy.params)'
fields=[('world','entities.current','NativePlayWorld'),('video','tiles.revision','NativeVideo'),
 ('video','backgrounds.uploaded','NativeVideo'),('video','palettes.fixed_revision','NativeVideo'),
 ('video','commit_pending','NativeVideo'),('video','reveal_pending','NativeVideo'),('runtime','vblanks','NativeRuntime'),
 *[('video',f,'NativeVideo') for f in ['palettes.binding_revision','renderer_palette_revision','tiles.fixed_revision','backgrounds.native_profile','backgrounds.native_tiles','backgrounds.boundary_tiles','backgrounds.fade_control','backgrounds.fade_level','objects.frame.upload_count','backgrounds.culling','backgrounds.layer_limit','backgrounds.map_count','backgrounds.auto_planes','backgrounds.depth4','backgrounds.hud_reserved','hud_active','hud_blocked','bg4_commits','hud_commits','format_fallbacks','fixed.bytes','fixed.cached','fixed.suffix','colors.stable','objects.frame.palette_merges','objects.frame.palette_merge_error','objects.frame.palette_protected','objects.frame.palette_spills']]]
fields += [('video',f,'NativeVideo') for f in ('objects.cosmetic_omitted','objects.budget_retries','objects.frame.tile_compactions','objects.frame.dense_layout')]
fields += [('world',f,'NativePlayWorld') for f in ('enemies.cutscene.playback_mode','enemies.cutscene.variant','weapons.mission_enemies','entities.displayed_count')]
fields += [('world','results.progress','NativePlayWorld')]
fields += [('world',f,'NativePlayWorld') for f in ('slug.present','slug.actor.owner','slug.actor.damage_owner','slug.actor.transform.x','slug.actor.transform.y','slug.body_state')]
fields += [('video',f,'NativeVideo') for f in ('objects.count','objects.strips.records','objects.strips.record_drops','objects.strips.key_drops','objects.frame.count')]
fields += [('scene',f'camera.{f}','SceneRuntime') for f in ('x','y','raw_y','smoothing')]
fields += [('scene',f'maps.planes[{i}].{f}','SceneRuntime') for i in range(4) for f in ('image','x','y','flags','controls','rect_x','rect_y','width','height')]
fields += [('world',f,'NativePlayWorld') for f in ('slug.actor.hit.hp','slug.previous_owner','slug.rider_parameter','interactions.rescued[0]','interactions.rescued[1]')]
fields += [('scene',f,'SceneRuntime') for f in ('command','native_camera_x','native_camera_y','camera.progress','camera.min_x','camera.max_x')]
fields += [('world',f,'NativePlayWorld') for f in ('weapons.secondary_x_offset','weapons.secondary_y_offset','weapons.mission_progress','enemies.intro_gate','terrain.scene1_horizontal_lock')]
fields += [('world',f'video.views[0].{f}','NativePlayWorld') for f in ('base_x','base_y')]
fields += [('scene',f,'SceneRuntime') for f in ('camera.seed_latch','camera.seed_x','camera.seed_y','camera.center_x','camera.center_y','camera.edge_off','camera.axis','follow.x[0]','follow.x[1]','follow.y_up[0]','follow.y_up[1]','follow.valid')]
addresses=['hash','live','dirty','palette_revision','fault','map_dirty','enable']
expr=list(constants.values())
for _,f,t in fields:expr += [f'offsetof(struct {t},{f})',f'sizeof(((struct {t}*)0)->{f})']
expr += [f'offsetof(struct NativeVideo,backgrounds.{f})' for f in addresses]
expr += ['offsetof(struct NativeVideo,tiles.fixed)']
src='#include "play_world.h"\n#include "native_video.h"\n#include "native_runtime.h"\n#include "scene_runtime.h"\n#include <stddef.h>\nconst unsigned a[]={'+','.join(expr)+'};'
(out/'offsets.c').write_text(src)
subprocess.run([a.cc,'-mcpu=arm7tdmi','-marm','-Isrc','-c',str(out/'offsets.c'),'-o',str(out/'offsets.o')],cwd=ROOT,check=True)
subprocess.run([a.cc.replace('gcc','objcopy'),'-O','binary','-j','.rodata',str(out/'offsets.o'),str(out/'offsets.bin')],check=True)
nm=subprocess.check_output([a.cc.replace('gcc','nm'),'-S','-n',str(a.elf.resolve())],text=True)
syms={l.split()[-1]:int(l.split()[0],16) for l in nm.splitlines() if len(l.split())>=3}
vals=iter(struct.unpack('<'+str(len(expr))+'I',(out/'offsets.bin').read_bytes()))
probe=(base/'probe.txt').read_text();known={l.split()[2] for l in probe.splitlines()}
probe+=f'{syms["scene"]:x} 0 base.scene\n'
for name in constants:probe+=f'{next(vals):x} 0 offset.{name}\n'
probe+=f'{syms["world"]:x} 0 base.world\n{syms["boss_program"]:x} 0 program.boss\n{syms["turret142_program"]:x} 0 program.helicopter\n'
probe+=f'{syms["script151_program"]:x} 0 program.m1_boss\n'
probe+=f'{syms["mission_result_program"]:x} 0 program.mission_results\n'
for s,f,_ in fields:
 address=syms[s]+next(vals);width=next(vals)
 if s+'.'+f not in known:probe+=f'{address:x} {width} {s}.{f}\n'
for f in addresses:probe+=f'{syms["video"]+next(vals):x} 0 address.backgrounds.{f}\n'
probe+=f'{syms["video"]+next(vals):x} 0 address.fixed\n'
(out/'probe.txt').write_text(probe)
(out/'symbols.txt').write_text('\n'.join(l for l in nm.splitlines() if len(l.split())==4 and l.split()[2] in 'tT')+'\n')
print('Extended ARM telemetry fields:',len(probe.splitlines()))
