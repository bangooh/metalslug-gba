#include "scene_driver.h"
#include "play_world.h"
#include "bosses.h"
#include "opening.h"
#include "scene_text.h"
#include "scene_placements.h"
static int fail(struct SceneRuntime*s){s->fault=1;return 0;}
static uint16_t decimal(uint16_t n){uint16_t out=0;for(unsigned i=0;i<4;i++){out|=(n%10)<<(4*i);n/=10;}return out;}
/*96A80 debugFIX output.106E88 is the PREVIOUS counted-enemy WORD. */
static int debug_draw(struct SceneRuntime*s,struct NativePlayWorld*w){if(!(s->debug_dip&1))return 1;uint16_t x=s->camera.x>>16,y=s->camera.y>>16;return scene_text_hex(&w->video,10,6,w->weapons.mission_enemies)&&scene_text_hex(&w->video,23,3,s->camera.progress)&&scene_text_hex(&w->video,28,3,x)&&scene_text_hex(&w->video,33,3,y)&&scene_text_hex(&w->video,28,4,decimal(x))&&scene_text_hex(&w->video,33,4,decimal(y))&&scene_text_hex(&w->video,28,5,w->enemies.siege_phase);}
/*96B24 tests raw10E208 bit4 AND low-nibble==8. Do not substitute the
 * remapped player action bits or invent a trigger when debug input is absent. */
static int debug_triggers(struct SceneRuntime*s,struct NativePlayWorld*w){if(!(s->debug_dip&0x84))return 1;if(!s->debug_input_valid)return fail(s);if(!(s->debug_input&16)||(s->debug_input&15)!=8)return 1;if((s->debug_dip&4)&&!opening_clear_ordnance(&w->weapons,s->group,1))return 0;if((s->debug_dip&128)&&!opening_clear_ordnance(&w->weapons,s->group,0))return 0;return 1;}
int scene_driver_begin(struct SceneRuntime*s,struct NativePlayWorld*w,enum SceneDriverEntry id,unsigned scene,uint8_t timing){if((unsigned)id>SCENE_DRIVER_ENDING_D||scene>255||!scene_runtime_attach(s,w))return 0;s->driver.kind=(uint8_t)id+1;s->driver.entry=1;s->driver.timing_variant=timing;s->scene=(uint8_t)scene;return 1;}
static int enter(struct SceneRuntime*s,struct NativePlayWorld*w,unsigned id){struct WeaponWorld*k=&w->weapons;
 if(id<=SCENE_DRIVER_7){unsigned program=id==SCENE_DRIVER_7?7:id;if(!scene_runtime_load(s,w,program,s->scene))return 0;w->enemies.clear_ordnance=0;if(id!=SCENE_DRIVER_7)s->driver.cursor=0;return 1;}
 if(id==SCENE_DRIVER_ENDING_D&&!scene_text_clear_border(&w->video,1))return fail(s);
 source_audio_game_command(&s->audio,id==SCENE_DRIVER_ENDING_B?0x29:0x2a);s->driver.cursor=0;
 if(!scene_runtime_load(s,w,id==SCENE_DRIVER_ENDING_B?14:15,id==SCENE_DRIVER_ENDING_B?11:12))return 0;
 s->camera.seed_latch=65535;s->camera.seed_x=s->driver.timing_variant==1?0x1c000:0x18000;s->camera.seed_y=0;
 if(id==SCENE_DRIVER_ENDING_B&&!scene_text_clear_border(&w->video,1))return fail(s);s->audio.suppress_effects=255;
 /* User-requested full-height ending; show the previously masked scenery. */
 if(!scene_video_fixed_rect(&w->video,0,26,0x300,40,6,0))return fail(s);
 struct PlayerTransform t={.x=-256,.y=513*256,.priority=65535,.display_group=255};if(!capsule_spawn(k,s->group,ENTITY_FIRST_CHILD,&t,CAPSULE_ENTRY_FLY))return 0;s->driver.phase=0;return 1;
}
int scene_driver_frame(struct SceneRuntime*s,struct NativePlayWorld*w){if(!s||!s->driver.kind||s->fault)return 0;unsigned id=s->driver.kind-1;if(id>SCENE_DRIVER_ENDING_D)return fail(s);
 if(s->driver.entry){s->driver.entry=0;if(!enter(s,w,id))return fail(s);}
 if(id<=SCENE_DRIVER_7)scene_runtime_follow_players(s,w);
 if(!scene_runtime_frame(s,w))return 0;
 if(id<=SCENE_DRIVER_7){if(id!=SCENE_DRIVER_7&&!scene_placements_camera_step(&w->weapons,id,&s->driver.cursor))return fail(s);if(!debug_draw(s,w)||!debug_triggers(s,w))return fail(s);w->enemies.intro_gate=1;return 1;}
 if(!scene_placements_step(&w->weapons,id==SCENE_DRIVER_ENDING_B?6:7,&s->driver.cursor)||!debug_draw(s,w))return fail(s);
 /* Threshold handlers each return after ONE transition, even if progress
  * skips multiple thresholds. Their seed write takes effect next frame. */
 if(id==SCENE_DRIVER_ENDING_B&&s->driver.phase<6){static const uint16_t thresholds[]={0x100,0x1000,0x2b94,0x2d00,0x3000,0x4000};static const uint32_t speeds[2][6]={{0x16000,0x12000,0x12000,0x10000,0x20000,0x20000},{0x1a000,0x16000,0x16000,0x14000,0x26000,0x26000}};unsigned phase=s->driver.phase;if(s->camera.progress>=thresholds[phase]){s->camera.seed_x=speeds[s->driver.timing_variant==1][phase];s->driver.phase++;}}
 return 1;
}
