#include "scene_runtime.h"
#include "scene_driver.h"
#include "play_world.h"
#include "bosses.h"
_Static_assert(sizeof(struct SceneRuntime)<6144,"scene storage budget");
static int fail(struct SceneRuntime*s){s->fault=1;return 0;}
static uint16_t integer(uint32_t x){return x>>16;}
static int16_t word(int32_t x){return (int16_t)(uint16_t)x;}
void scene_runtime_publish(struct SceneRuntime*s,struct NativePlayWorld*w){
 struct SceneCamera*c=&s->camera;struct WeaponWorld*k=&w->weapons;
 w->terrain.scene=s->scene;w->terrain.camera_x=s->native_camera_x;w->terrain.camera_y=s->native_camera_y;w->terrain.delta_x=s->maps.planes[0].dx;w->terrain.delta_y=s->maps.planes[0].dy;
 k->camera_x=w->terrain.camera_x;k->camera_y=w->terrain.camera_y;k->camera_delta_x=w->terrain.delta_x;k->scene_number=s->scene;
 k->mission_progress=c->progress;k->mission_valid=1;k->scroll_seed_latch=c->seed_latch;
 /* $044200/$04420C: M2's scroll override carries actors horizontally
  * with the camera even while airborne. The terrain kernel must consume
  * the same live latch as the authored moving-deck camera. */
 w->terrain.scene1_horizontal_lock=(uint8_t)!!c->seed_latch;
 k->background_x_offset=word(integer(s->maps.planes[0].x));k->background_y_offset=word(integer(s->maps.planes[0].y));k->background_view_valid=1;
 w->terrain.cell_offset_x=word(k->background_x_offset-s->native_camera_x);w->terrain.cell_offset_y=word(k->background_y_offset-s->native_camera_y);
 /* Historical native "secondary" names refer to $0440BC/$0440D0's
  * $106F50/54 camera, not the second tile plane ($107FE8). */
 k->secondary_x_offset=word(integer(c->x));k->secondary_y_offset=word(integer(c->y));k->secondary_view_valid=1;
 k->tertiary_delta_x=s->maps.planes[2].dx;k->tertiary_delta_y=s->maps.planes[2].dy;k->tertiary_delta_valid=1;
 w->enemies.m5_scene.secondary_x=k->secondary_x_offset;w->enemies.m5_scene.secondary_y=k->secondary_y_offset;w->enemies.m5_scene.valid=1;
}
/* $0967C0/$05E2D8: X remains eligible for a non-retired dead player;
 * only Y requires lifecycle bit7. Substitute each missing axis separately. */
void scene_runtime_follow_players(struct SceneRuntime*s,const struct NativePlayWorld*w){
 int32_t x[2]={-1,-1},y[2]={-1,-1};
 for(unsigned i=0;i<2;i++){const struct PlayerBodyState*p=w->weapons.players[i];if(!p||p->life_retired)continue;x[i]=(uint16_t)((p->actor.transform.x>>8)-w->weapons.camera_x);if(p->actor.lifecycle_flags&128)y[i]=(uint16_t)(transform_y_up(&p->actor.transform)+w->weapons.camera_y);}
 if(x[1]<0)x[1]=x[0];if(x[0]<0)x[0]=x[1];if(y[1]<0)y[1]=y[0];if(y[0]<0)y[0]=y[1];
 s->follow=(struct SceneFollow){.x={(uint16_t)x[1],(uint16_t)x[0]},.y_up={(uint16_t)y[1],(uint16_t)y[0]},.valid=x[1]>=0&&y[1]>=0};
}
int scene_runtime_spawn_hint(struct SceneRuntime*s,struct SceneSpawnHint*out){
 if(s->scene>=6){*out=(struct SceneSpawnHint){.x=80,.y_up=504,.spacing=16};return 1;}
 const struct SceneSpawnTable*t=&scene_spawn_tables[s->scene];if(s->spawn_hint>=t->count)return fail(s);const struct SceneSpawnPosition*r=&scene_spawn_positions[t->first+s->spawn_hint];
 /* $04CAC4 advances ONE record per call, including its per-frame call. */
 if(s->camera.progress>=r->next_threshold){if(++s->spawn_hint>=t->count)return fail(s);r=&scene_spawn_positions[t->first+s->spawn_hint];}
 int x=r->x,y=r->y;if(r->relative_x)x-=integer(s->camera.x);if(r->relative_y)y-=integer(s->camera.y);
 *out=(struct SceneSpawnHint){.x=word(x),.y_up=word(512-y),.mode=r->mode,.spacing=r->spacing,.item=r->item,.quantity=r->quantity,.lifetime=r->lifetime};return 1;
}
int scene_runtime_m5_relink(struct SceneRuntime*s,struct WeaponWorld*w,int16_t target,int wrap){
 uint16_t x=integer(s->camera.x)-target-320,y=integer(s->camera.y)-16;
 if(!w->video||!scene_map_bind(&s->maps,w->video,0,SCENE_M5_RELINK_IMAGE,SCENE_M5_RELINK_COLLISION,x,y)||!scene_map_probe(&s->maps,w->video,0,0))return fail(s);
 scene_map_cursor(&s->maps,w->video,0,15,0);
 if(wrap)s->camera.x=((uint32_t)(uint16_t)(integer(s->camera.x)-320)<<16)|(s->camera.x&65535);
 w->background_x_offset=word(x);w->background_y_offset=word(y);w->secondary_x_offset=word(integer(s->camera.x));w->enemies->m5_scene.secondary_x=w->secondary_x_offset;
 if(w->interactions&&w->interactions->terrain){w->interactions->terrain->cell_offset_x=word(x-s->native_camera_x);w->interactions->terrain->cell_offset_y=word(y-s->native_camera_y);}return 1;
}
/* The scene is a real scheduler slot: boot/timer children precede it and
 * player controllers follow it. Calling it before the entity chain would
 * observe the previous scene on a boot transition and break FIRST ordering. */
static void runtime_tick(struct EntityPool*p,struct NativeEntity*e,void*user){
 (void)p;(void)e;struct NativePlayWorld*w=user;struct SceneRuntime*s=w->scene_runtime;
 if(w->fault||!s)return;
 if(s->driver.kind||s->active){
  if(!(s->driver.kind?scene_driver_frame(s,w):scene_runtime_frame(s,w))){w->fault=13;return;}
  w->hits.camera_x=w->terrain.camera_x;w->hits.camera_y=w->terrain.camera_y;
 }
}
int scene_runtime_attach(struct SceneRuntime*s,struct NativePlayWorld*w){
 if(!s||!w||s->fault||w->fault)return 0;
 struct NativeEntity*g=entity_get(&w->entities,s->group);
 if(!s->group.index||!g){g=entity_spawn(&w->entities,entity_ref(&w->entities.nodes[0]),ENTITY_GROUP,ENTITY_FIRST_CHILD,runtime_tick);if(!g)return fail(s);entity_reserve(g);s->group=entity_ref(g);}
 if(g->kind!=ENTITY_GROUP)return fail(s);
 g->tick=runtime_tick;
 if(!s->audio.ready)source_audio_reset(&s->audio);
 w->scene_runtime=s;w->weapons.scene_runtime=s;return 1;
}
int scene_runtime_load(struct SceneRuntime*s,struct NativePlayWorld*w,unsigned program,unsigned scene){
 if(!s||!w||program>=16||scene>255||w->fault)return 0;
 const struct SceneDescriptor*d=&scene_descriptors[program];s->scene=scene;s->program=program;s->command=d->command;s->spawn_hint=0;
 if(!scene_runtime_attach(s,w))return 0;
 if(w->video.native_reset)w->video.native_reset(w->video.native_context,program);
 if(!scene_maps_load(&s->maps,&w->video,d))return fail(s);scene_camera_reset(&s->camera);
 w->enemies.siege_phase=w->enemies.scripted_input.active=0;w->enemies.damage_enabled=1;
 w->interactions.climb.count[0]=w->interactions.climb.count[1]=0;
 target_ring_reset(&w->targets);w->enemies.deaths.first=w->enemies.deaths.end=w->enemies.deaths.write=w->enemies.deaths.count=0;
 w->enemies.zones.first=w->enemies.zones.end=w->enemies.zones.write=w->enemies.zones.count=0;
 w->interactions.rescued[0]=w->interactions.rescued[1]=0;terrain_platform_reset(&w->terrain.platforms);
 /* The scene system owns fades independently of the two groups erased by
  * $052780. Its children run before Slug/players/projectiles/enemies. */
 s->active=1;w->scene_runtime=s;w->weapons.scene_runtime=s;scene_runtime_publish(s,w);return 1;
}
static uint32_t field(struct SceneRuntime*s,struct NativePlayWorld*w,unsigned id){
 switch(id){case SCF_X:return integer(s->camera.x);case SCF_Y:return integer(s->camera.y);case SCF_INTRO:return w->enemies.intro_gate;case SCF_FADE_DONE:return w->video.fade.complete;case SCF_ENEMIES:return w->weapons.mission_enemies;case SCF_SIGNAL:return s->signal;case SCF_SIEGE:return w->enemies.siege_phase;case SCF_SEED_Y:return s->camera.seed_y;default:fail(s);return 0;}
}
static int call(struct SceneRuntime*s,struct NativePlayWorld*w,const uint32_t*v){
 switch(v[0]){
 case SCC_PALETTE_CYCLE:return w->video.palette_cycle&&w->video.palette_cycle(w->video.palette_context,v[1],scene_palette_high(v[2]),v[3],1);
 case SCC_COLLISION_ROW:{
  /* $05026C converts map(0,240) through $044022 before $043F5E. */
  uint16_t x=-integer(s->maps.planes[0].x),y=512-240+integer(s->maps.planes[0].y);
  for(unsigned i=0;i<48;i++,x+=8)scene_map_write_cell(&s->maps,&w->video,x,y,1);return 1;
 }
 case SCC_FADE_OUT:case SCC_FADE_IN:return scene_fade_spawn(&w->weapons,s->group,v[1],v[0]==SCC_FADE_OUT,0);
 case SCC_DELETE_ACTORS:{
  struct NativeEntity*g=entity_get(&w->entities,w->weapons.projectile_group);if(!g)return 0;entity_delete_children(&w->entities,g);g=entity_get(&w->entities,w->weapons.enemy_group);if(!g)return 0;entity_delete_children(&w->entities,g);return 1;
 }
 case SCC_AUDIO_STOP:source_audio_byte(&s->audio,6);return 1;
 default:return 0;
 }
}
static int commands(struct SceneRuntime*s,struct NativePlayWorld*w){
 for(unsigned budget=0;budget<855;budget++){
  if(s->command>=855)return fail(s);const struct SceneCommand*c=&scene_commands[s->command];const uint32_t*v=c->value;unsigned id=c->plane;struct SceneMapPlane*p=&s->maps.planes[id];struct SceneCamera*k=&s->camera;
  switch(c->op){
  case SCOP_FRAME:return 1;
  case SCOP_BIND:if(!scene_map_bind(&s->maps,&w->video,id,v[0],v[1],integer(k->x)-v[2],integer(k->raw_y)+integer(k->smoothing)-v[3]))return fail(s);break;
  case SCOP_FIELDS:p->flags=v[0];p->scale_x=word(v[1]);p->scale_y=word(v[2]);break;
  case SCOP_WARP:case SCOP_POSITION:scene_camera_position(k,v[0],v[1],c->op==SCOP_WARP);break;
  case SCOP_CURSOR:scene_map_cursor(&s->maps,&w->video,id,v[0],v[1]);break;
  case SCOP_DETACH:p->image=SCENE_NO_IMAGE;if(!scene_map_hide(&s->maps,&w->video,id)||!scene_map_commit(&s->maps,&w->video,id))return fail(s);break;
  case SCOP_WAIT:{uint32_t a=field(s,w,v[0]),b=v[2];int wait=v[1]==SCCOND_LT?a<b:v[1]==SCCOND_GT?a>b:v[1]==SCCOND_NE?a!=b:v[1]==SCCOND_EQ?a==b:0;if(s->fault)return 0;if(wait)return 1;break;}
  case SCOP_DIRTY:p->dirty=1;break;
  case SCOP_HIDE:if(!scene_map_hide(&s->maps,&w->video,id))return fail(s);break;
  case SCOP_DRAW:if(!scene_map_probe(&s->maps,&w->video,id,0))return fail(s);p->dirty=1;break;
  case SCOP_PALETTES:
   if(!w->video.set_palette||2*(v[0]+v[1])>4284)return fail(s);
   for(unsigned i=0;i<v[1];i++){unsigned n=2*(v[0]+i);uint16_t slot=scene_palette_pairs[n],palette=scene_palette_pairs[n+1];uint8_t flags=palette==65535?64:(slot>>8)|1;if(!w->video.set_palette(w->video.palette_context,(uint8_t)slot,scene_palette_high(palette),flags))return fail(s);}break;
  case SCOP_CLEAR:if(!scene_map_clear(&s->maps,&w->video,id))return fail(s);break;
  case SCOP_MODE:s->mode=v[0];break;
  case SCOP_CALL:if(!call(s,w,v))return fail(s);break;
  case SCOP_LIMITS:k->min_x=word(v[0]);k->min_y=word(v[1]);k->max_x=word(v[2]);/* M1 boss: stop 8 source pixels (6 GBA pixels) before overscan. */if(s->program==0&&v[2]==3848)k->max_x=3840;k->max_y=word(v[3]);k->slope=word(v[4]);break;
  case SCOP_CENTER:k->center_x=word(v[0]);k->center_y=word(v[1]);break;
  case SCOP_TRIGGER:if(v[0]>=23)return fail(s);p->controls=v[0];p->control_index=0;break;
  case SCOP_AXIS:scene_camera_axis(k,v[0]);break;
  case SCOP_SOUND:source_audio_command(&s->audio,v[0]);break;
  case SCOP_WRITE:
   if(v[0]==SCF_SIGNAL)s->signal=v[1];else if(v[0]==SCF_SIEGE)w->enemies.siege_phase=v[1];else if(v[0]==SCF_SEED_Y)k->seed_y=v[1];else return fail(s);break;
  case SCOP_PROGRESS:scene_camera_progress(k,v[0]);break;
  default:return fail(s);
  }
  s->command=c->next;
 }return fail(s);
}
int scene_runtime_frame(struct SceneRuntime*s,struct NativePlayWorld*w){
 if(!s||s->fault||!s->active)return 0;uint32_t vx,vy;
 scene_camera_follow(&s->camera,s->follow.x[0],s->follow.y_up[0],s->follow.x[1],s->follow.y_up[1],s->follow.valid,&vx,&vy);
 /* $0437DA snapshots seeds before script-side writes, for this frame only. */
 if(s->camera.seed_latch){vx=s->camera.seed_x;vy=s->camera.seed_y;}
 if(!commands(s,w))return 0;scene_camera_integrate(&s->camera,vx,vy);
 struct SceneSpawnHint hint;if(!scene_runtime_spawn_hint(s,&hint))return 0;
 if(!scene_maps_step(&s->maps,&w->video,s->camera.dx,s->camera.dy))return fail(s);
 s->native_camera_x=word(s->native_camera_x+s->maps.planes[0].dx);s->native_camera_y=word(s->native_camera_y+s->maps.planes[0].dy);
 scene_runtime_publish(s,w);return 1;
}
