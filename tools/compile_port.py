#!/usr/bin/env python3
"""Compile new source-port dependency groups; never execute gameplay/tests."""
import argparse,hashlib,json,os,shutil,subprocess
from build_fingerprint import build_fingerprint
from pathlib import Path
R=Path(__file__).resolve().parents[1]
UNITS=['src/animation.c','src/entities.c','src/hitbox.c','src/hit_scene.c','src/player_actions.c','src/player_arm.c','src/player_rules.c','src/player_body_pose.c','src/player_air.c','src/player_body.c','src/player_crouch.c','src/player_life.c','src/player_attachment.c','src/player_slug.c','src/slug_support.c','src/target_ring.c','src/play_world.c','src/weapon_animation.c','src/weapon_math.c','src/weapons.c','src/terrain_motion.c','generated/slug-support-data.c',
       'src/slug_children.c','src/slug_surface.c','generated/slug-child-programs.c','generated/slug-surface-programs.c','src/slug_motion.c','src/slug_animation.c','src/slug_body.c','src/slug_aux.c','src/slug_projectile.c','src/slug_death_fx.c','generated/slug-death-programs.c','generated/slug-projectile-programs.c','generated/slug-aux-programs.c','generated/slug-body-data.c','generated/slug-programs.c',
       'src/items.c','src/interactions.c','src/pow.c','src/gun.c','src/body_contact.c','src/enemy_common.c','src/human_death.c','src/enemy_ordnance.c','src/infantry.c','src/soldier.c','src/enemy_environment.c','src/mission.c','src/mission_events.c','src/mission_tables.S','generated/enemy-programs.c','generated/interaction-programs.c','generated/item-programs.c','generated/player-programs.c','generated/player-arm-select.c','generated/player-body-data.c','generated/weapon-programs.c','generated/terrain-rules.c']
def port_units():
 UNITS.extend(['src/source_strip_budget.c'])
 UNITS.extend(['src/tile_video.c','src/background_tile.c','src/video_palette.c','src/fixed_video.c','src/fixed_video_gba.c','src/tile_video_assets.S'])
 UNITS.extend(['src/native_link.c','src/native_link_gba.c','src/native_runtime_link.c','src/native_snapshot.c','src/native_snapshot_link.c'])
 UNITS.extend(['src/native_audio.c','src/native_audio_gba.c','src/native_audio_assets.S','src/gba_interrupts.c','generated/native-audio-data.c'])
 UNITS.extend(['src/palette_video.c','src/source_drawing.c','src/object_scene.c','generated/render-data.c'])
 UNITS.extend(['src/scaled_drawing.c','generated/scaled-drawing-data.c','src/scaled_drawing_assets.S','src/object_video.c','src/object_video_gba.c'])
 UNITS.extend(['src/drawing.c','generated/drawing-data.c','src/drawing_assets.S'])
 UNITS.extend(['src/source_input.c','src/native_runtime.c','src/session_system.c','generated/runtime-data.c'])
 UNITS.extend(['src/session.c','src/session_player.c','src/session_ui.c','src/session_hud.c','generated/session-data.c'])
 UNITS.extend(['src/mission_result.c','src/mission_result_roster.c','src/mission_banner.c','generated/mission-result-data.c'])
 UNITS.extend(['src/results.c','src/result_text.c','src/result_highscore.c','generated/result-data.c'])
 UNITS.extend(['src/scene_driver.c','src/scene_text.c','generated/scene-text-data.c'])
 UNITS.extend(['src/scene_rgb_fade.c','src/urban.c','src/urban_effects.c','generated/urban-programs.c'])
 UNITS.extend(['src/opening.c','src/opening_fort.c','src/opening_crew.c','generated/opening-programs.c'])
 UNITS.extend(['src/scene_placements.c','generated/scene-placements.c','src/village.c','src/village_effects.c','generated/village-programs.c'])
 UNITS.extend(['src/scene_camera.c','src/scene_maps.c','src/scene_runtime.c','src/source_audio.c','generated/scene-programs.c'])
 UNITS.extend(['src/presentation.c','src/recovery318.c','src/scripted_input.c','generated/presentation-programs.c','src/script151.c','src/script151_parts.c','src/script151_effects.c','generated/script151-programs.c','src/ring_actors.c','src/ring_guns.c','generated/ring-gun-programs.c','src/flame_trap.c','generated/flame-trap-programs.c','src/scene_props.c','generated/scene-prop-programs.c','src/banners.c','generated/banner-programs.c','src/capsule.c','src/scene_fade.c','generated/capsule-programs.c','src/nest185.c','generated/nest185-programs.c','src/grunt153.c','generated/grunt153-programs.c','src/crew23.c','generated/crew23-programs.c','src/turret142.c','src/turret142_parts.c','src/obstacles.c','generated/turret142-programs.c','src/emitter59.c','src/emitter59_effects.c','generated/emitter59-programs.c','src/assault76.c','src/assault76_parts.c','src/assault76_ordnance.c','generated/assault76-programs.c','src/air_formation.c','src/air_formation_parts.c','src/air_formation_death.c','generated/air_formation-programs.c','src/boss120.c','src/boss120_parts.c','src/boss120_effects.c','generated/boss120-programs.c','src/captor.c','generated/captor-programs.c','src/mortar.c','generated/mortar-programs.c','src/walker.c','generated/walker-programs.c','src/rocket_vehicle.c','generated/rocket-vehicle-programs.c','src/ground_nest.c','generated/ground-nest-programs.c','src/free96.c','src/gun_platform.c','generated/gun-platform-programs.c','src/light_tank.c','src/light_tank_parts.c','generated/light-tank-programs.c','src/shore.c','src/paratrooper.c','src/barrel.c','generated/shore-programs.c','src/shield.c','src/m4_lift.c','generated/shield-programs.c','src/m4.c','src/m4_scene.c','src/free104.c','generated/m4-programs.c','src/mob.c','generated/mob-programs.c','src/stationary_guard.c','src/airship.c','src/airship_structures.c','src/airship_body.c','src/airship_effects.c','generated/airship-programs.c'])
 UNITS.extend(['src/fort.c','src/fort_structures.c','src/fort_vehicles.c','src/fort_effects.c','generated/fort-programs.c'])
 UNITS.extend(['src/siege.c','src/siege_finale.c','src/siege_effects.c','src/late_guard.c','generated/siege-programs.c','generated/late-guard-programs.c'])
 UNITS.extend(['src/allen.c','src/scene_video.c'])
 UNITS.extend(['src/boss117.c','src/boss117_parts.c','src/boss117_effects.c','generated/boss117-programs.c'])
 UNITS.extend(['src/boss_common.c','src/boss_fx.c','src/fx_animation.c','src/boss2.c','src/crab.c','src/gunship.c','src/flight_path.c','generated/boss-programs.c'])
 UNITS.extend(['src/carrier.c','src/carrier_parts.c','src/carrier_crew.c','generated/carrier-programs.c'])
 UNITS.extend(['src/deploy.c','src/tank.c','src/tank_parts.c','src/tank_crew.c','src/bazooka.c','generated/tank-programs.c'])
 UNITS.extend(['src/final_boss.c','src/final_parts.c','generated/final-programs.c'])
 UNITS.extend(['src/m5.c','src/m5_children.c','src/m5_stage.c','src/m5_turret.c','generated/m5-programs.c'])
 UNITS.extend(['src/native_inflate.c','src/opening_replay_assets.S','src/native_opening.c','generated/opening-replay-data.c','src/native_frontend.c','src/native_frontend_assets.S','src/native_main.c','src/freestanding.c','src/frontend_actors.c','src/frontend_tutorial.c','generated/frontend-data.c'])
 UNITS.extend(['src/background_video.c','src/background_video_gba.c','src/fixed_object.c','src/hud_bg.c','src/native_video.c','src/result_background.c'])
 UNITS.append('generated/native-audio-tempo.c')
 UNITS.append('generated/object-masks.c')
 UNITS.extend(['generated/approved-player-data.c','src/approved_players.S'])
 UNITS.extend(['src/native_background_assets.c','generated/native-bg-assets.c','src/native_bg_assets.S'])
 UNITS.extend(['m1_native/runtime.c','m1_native/tile_stream.c','m1_native/assets.S','generated/m1-assets.c','generated/m2-assets.c','m1_native/m2_assets.S'])
 UNITS.extend([f'generated/m{m}-assets.c' for m in range(3,7)]+[f'm1_native/m{m}_assets.S' for m in range(3,7)])
 return list(dict.fromkeys(UNITS))
def main():
 units=port_units()
 # Wire compatibility derives from executable inputs, not just a version
 # label. Exclude this generated header itself to avoid a hash cycle.
 build_fingerprint(R,units)
 p=argparse.ArgumentParser();p.add_argument('--cc',default=os.environ.get('ARM_CC','arm-none-eabi-gcc'));a=p.parse_args()
 cc=shutil.which(a.cc) or a.cc;b=R/'build/port-objects';b.mkdir(parents=True,exist_ok=True)
 flags=['-mcpu=arm7tdmi','-marm','-O2','-std=c11','-ffreestanding','-fno-builtin','-fno-strict-aliasing',
        '-Wall','-Wextra','-Werror','-Wno-misleading-indentation']
 rows=[];objects=[]
 for unit in units:
  obj=b/(Path(unit).stem+'.o')
  subprocess.run([cc,*flags,'-c',unit,'-o',str(obj)],cwd=R,check=True)
  objects.append(str(obj))
  rows.append(dict(source=unit,sha256=hashlib.sha256((R/unit).read_bytes()).hexdigest(),object_bytes=obj.stat().st_size))
 # Relocatable link catches duplicate definitions and connects this group.
 # This includes the native runtime, but only build_native.py performs
 # cartridge memory placement, startup linkage and libgcc resolution.
 linked=b/'port-group.o'
 subprocess.run([cc,'-nostdlib','-r',*objects,'-o',str(linked)],cwd=R,check=True)
 nm=subprocess.check_output([cc,'-print-prog-name=nm'],text=True).strip()
 undefined=subprocess.check_output([nm,'-u',str(linked)],text=True).splitlines()
 report=dict(version=(R/'VERSION').read_text().strip(),scope='ARM compilation and relocatable group link only; no ROM, game execution, functional tests or performance measurement',
             compiler=subprocess.check_output([cc,'--version'],text=True).splitlines()[0],flags=flags,units=rows,
             headers={str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for folder in ['src','generated'] for p in sorted((R/folder).glob('*.h'))},
             group_link=dict(object_bytes=linked.stat().st_size,undefined_symbols=[line.split()[-1] for line in undefined if line.strip()]),complete_port=False)
 (R/'generated/port-compile.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
if __name__=='__main__':main()
