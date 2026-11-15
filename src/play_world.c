#include "play_world.h"
#include "frontend_internal.h"
#include "session.h"
#include "bosses.h"
#include "siege.h"
#include "fort.h"
#include "airship.h"
#include "mob.h"
#include "m4.h"
#include "shield.h"
#include "shore.h"
#include "light_tank.h"
#include "late_guard.h"
#include "scene_runtime.h"
#include "scene_driver.h"
#include "scene_text.h"
#include <stddef.h>
_Static_assert(offsetof(struct NativePlayWorld,weapons)==0,"scheduler world prefix");
_Static_assert(sizeof(struct NativePlayWorld)<73728,"native simulation exceeds EWRAM allocation");
struct ActionBridge {struct NativePlayWorld *world;struct NativeEntity *entity;unsigned slot;};
static int fail(struct NativePlayWorld*w,unsigned code){if(!w->fault)w->fault=(uint8_t)code;w->entities.fault=32;return 0;}
static void body_tick(struct EntityPool*,struct NativeEntity*,void*);
static void arm_tick(struct EntityPool*,struct NativeEntity*,void*);
static int cell(void*user,int x,int y,uint8_t*raw){
 struct NativePlayWorld*w=user;
 if(w->scene_runtime){*raw=scene_map_cell(&w->scene_runtime->maps,&w->video,(uint16_t)(x-w->terrain.camera_x),(uint16_t)(512-y+w->terrain.camera_y));return 1;}
 return w->services.cell&&w->services.cell(w->services.context,x,y,raw)?1:fail(w,1);
}
static int sound(void*user,uint16_t command,int stop){
 struct NativePlayWorld*w=user;
 if(w->scene_runtime){struct SourceAudioQueue*q=&w->scene_runtime->audio;if(stop)source_audio_stop_command(q,command);else source_audio_game_command(q,command);return 1;}
 return w->services.sound&&w->services.sound(w->services.context,command,stop)?1:fail(w,2);
}
/* The capsule supplies already encoded0A,80 bytes (source2308 with60). */
static int audio_control(void*user,uint8_t command,uint8_t value){struct NativePlayWorld*w=user;if(w->scene_runtime){source_audio_byte(&w->scene_runtime->audio,command);source_audio_byte(&w->scene_runtime->audio,value);return 1;}return w->services.audio_control&&w->services.audio_control(w->services.context,command,value)?1:fail(w,2);}
static int move(void*user,struct WeaponActor*a,enum WeaponMove kind,struct WeaponContact*hit){
 return terrain_weapon_move(&((struct NativePlayWorld*)user)->terrain,a,kind,hit);
}
static int probe(void*user,const struct WeaponActor*a,struct WeaponContact*hit){
 return terrain_weapon_probe(&((struct NativePlayWorld*)user)->terrain,a,hit);
}
static int point_shape(void*user,const struct WeaponActor*a,int16_t x,int16_t y,uint8_t*shape){
 return terrain_weapon_point_shape(&((struct NativePlayWorld*)user)->terrain,a,x,y,shape);
}
static int write_cell(void*user,int x,int y,uint8_t raw){struct NativePlayWorld*w=user;if(w->scene_runtime){scene_map_write_cell(&w->scene_runtime->maps,&w->video,(uint16_t)(x-w->terrain.camera_x),(uint16_t)(512-y+w->terrain.camera_y),raw);return 1;}return w->services.write_cell&&w->services.write_cell(w->services.context,x,y,raw);}
static int spawn_template(struct NativePlayWorld*,uint16_t,const struct PlayerTransform*,const uint8_t[8],uint8_t,enum EntityInsert);
/* $029588/$029598 run as scheduled actors, preserving spawn/count ordering. */
static void slug_bounds_tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct NativePlayWorld*w=world;w->slug_narrow_bounds=e->payload.enemy.params[0];entity_delete(p,e);
}
static int mission_spawn(void*user,const struct MissionSpawn*s){
 struct NativePlayWorld*w=user;struct PlayerTransform t={.x=(int32_t)s->x*256,.y=(int32_t)s->y*256};
 if(!spawn_template(w,s->template,&t,s->params,s->subtype,ENTITY_FIRST_CHILD))return 0;
 struct NativeEntity*group=entity_get(&w->entities,w->hits.enemy_group);
 struct NativeEntity*e=group?&w->entities.nodes[group->next]:0;
 struct WeaponActor*a=entity_weapon_actor(e);
 if(!a||!e||e->parent.index!=w->hits.enemy_group.index)return fail(w,14);
 if(s->counted){a->display_flags|=2;w->entities.displayed_count++;}
 return 1;
}
static int foot_contact(void*user,struct WeaponActor*a){return terrain_weapon_foot_contact(&((struct NativePlayWorld*)user)->terrain,a);}
static int attack(void*user,struct HitActor*a,const struct HitAttack*hit,struct HitActor**target){
 return hit_scene_attack(&((struct NativePlayWorld*)user)->hits,a,hit,target);
}
static void damage(void*user,const struct HitDamage*hit){
 struct NativePlayWorld*w=user;
 if(!enemy_damage_score(&w->weapons,hit)){fail(w,3);return;}
 if(w->services.damage)w->services.damage(w->services.context,hit);
}
static int action_emit(void*user,const struct PlayerEmission*emission){
 struct ActionBridge*b=user;return weapon_emit(&b->world->weapons,entity_ref(b->entity),emission);
}
static int action_sound(void*user,uint16_t command,int stop){return sound(((struct ActionBridge*)user)->world,command,stop);}
static int action_attack(void*user,struct HitActor*a,const struct HitAttack*hit){
 struct ActionBridge*b=user;struct PlayerActionLayer*l=b->entity->kind==ENTITY_PLAYER?&b->entity->payload.player.actor:&b->entity->payload.arm;
 a->x=(int16_t)(l->transform.x>>8);a->y=transform_y_up(&l->transform);a->flip=l->transform.facing;
 return attack(b->world,a,hit,0);
}
static int ring_find(void*user,struct PlayerBodyState*p,int16_t*key){
 struct ActionBridge*b=user;struct NativePlayWorld*w=b->world;
 return target_ring_find(&w->targets,p->character,(int16_t)(p->actor.transform.x>>8),transform_y_up(&p->actor.transform),key);
}
static int ring_claim(void*user,struct PlayerBodyState*p,uint8_t key){
 struct ActionBridge*b=user;struct NativePlayWorld*w=b->world;
 return target_ring_claim(&w->targets,p->character,key)?1:fail(w,4);
}
static int ring_follow(void*user,uint8_t key,int16_t*x,int16_t*y){
 struct ActionBridge*b=user;return target_ring_follow(&b->world->targets,b->slot,key,x,y);
}
static int drop_weapon(void*user,struct PlayerBodyState*p){
 struct ActionBridge*b=user;struct NativePlayWorld*w=b->world;
 return weapon_drop_player_weapon(&w->weapons,entity_ref(b->entity),p);
}
static int visual(void*user,enum PlayerVisualEffect kind){
 struct ActionBridge*b=user;return weapon_spawn_player_effect(&b->world->weapons,entity_ref(b->entity),kind);
}
static int retire_player(void*user){
 struct ActionBridge*b=user;struct NativePlayWorld*w=b->world;struct PlayerBodyState*p=&b->entity->payload.player;
 entity_delete_children(&w->entities,b->entity);w->arms[b->slot]=(struct EntityRef){ENTITY_NONE,0};
 /* Original fixed player storage survives removal from the active list.
  * Keep its inventory address stable for surviving projectile counters. */
 p->body_contact=0;p->actor.defense=0;p->actor.attack=0;p->actor.animation.drawing=65535;p->actor.display_flags=0;
 return !w->entities.fault;
}
static struct PlayerActionContext context(struct ActionBridge*b,struct PlayerBodyState*p,struct PlayerActionLayer*l){
 return (struct PlayerActionContext){.body=p,.layer=l,.rng=&b->world->rng,.world=b,
  .emit=action_emit,.sound=action_sound,.attack=action_attack,.scene=b->world->terrain.scene};
}
/*5CFA8 checks boot44 before automatic input selection. Direct physical
 * queries through WeaponWorld.inputs intentionally bypass this gate. */
static struct PlayerSourceInput actor_input(struct NativePlayWorld*w,unsigned slot){
 if(w->input_freeze&&!w->input_freeze_bypass)return (struct PlayerSourceInput){0};
 return scripted_input_select(&w->enemies,slot,w->inputs[slot]);
}
static void body_tick(struct EntityPool*pool,struct NativeEntity*e,void*user){
 struct NativePlayWorld*w=user;struct PlayerBodyState*p=&e->payload.player;
 if(w->fault||p->life_retired)return;if(entity_parent_deleting(pool,e)){entity_delete(pool,e);return;}
 unsigned slot=p->character;if(slot>1){fail(w,7);return;}
 struct ActionBridge bridge={w,e,slot};struct PlayerActionContext a=context(&bridge,p,&p->actor);
 struct PlayerBodyContext c={.air={.actions=&a,.terrain=&w->terrain,.input=actor_input(w,slot),.world=&bridge,
  .ring_find=ring_find,.ring_claim=ring_claim,.campaign_timer=w->enemies.mission_time,.pause_motion=w->pause_motion,
  .invincible=w->invincible[slot],.control_mode=scripted_input_layout(&w->enemies,slot,w->control_mode[slot]),.death_route=w->death_routes[slot]},
  .effects=&bridge,.drop_weapon=drop_weapon,.visual=visual,.retire=retire_player,.ring_follow=ring_follow,
  .blood_enabled=w->blood_enabled,.slug_invulnerability=w->slug_invulnerability,
  .slug=&w->slug,.slug_riders_enabled=w->slug_riders_enabled};
 int ok=player_body_tick(&c);w->death_routes[slot]=c.air.death_route;
 if(!ok||a.fault||p->actor.animation.fault){w->action_fault[slot]=a.fault?a.fault:p->actor.animation.fault;fail(w,8);}
}
static void arm_tick(struct EntityPool*pool,struct NativeEntity*e,void*user){
 struct NativePlayWorld*w=user;if(w->fault)return;
 struct NativeEntity*parent=entity_get(pool,e->parent);
 if(!parent||parent->deleting){entity_delete(pool,e);return;}
 if(parent->kind!=ENTITY_PLAYER){fail(w,7);return;}
 struct PlayerBodyState*p=&parent->payload.player;unsigned slot=p->character;
 struct ActionBridge bridge={w,e,slot};struct PlayerActionContext a=context(&bridge,p,&e->payload.arm);
 if(e->payload.arm.animation.cursor==65535&&!player_arm_init(&a,e->index)){fail(w,8);return;}
 if(!player_arm_tick(&a)){w->action_fault[slot]=a.fault?a.fault:e->payload.arm.animation.fault;fail(w,8);}
}
static void contacts_tick(struct EntityPool*pool,struct NativeEntity*e,void*user){
 struct NativePlayWorld*w=user;if(w->fault)return;
 if(entity_parent_deleting(pool,e)){entity_delete(pool,e);return;}
 if(e->payload.slug!=&w->slug||!slug_contacts_tick(&w->slug,&w->terrain))fail(w,13);
}
static void slug_tick(struct EntityPool*pool,struct NativeEntity*e,void*user){
 (void)pool;struct NativePlayWorld*w=user;if(w->fault)return;struct SlugSharedState*s=e->payload.slug;
 if(s!=&w->slug){fail(w,13);return;}
 s->status_flags=0; /* $029582 -> $02A46C, immediately before fixed Slug. */
 struct PlayerSourceInput in={0};
 if((int8_t)s->actor.damage_owner>=0)in=actor_input(w,s->actor.damage_owner==1?0:1);
 struct SlugBodyContext c={.animation={.slug=s,.weapons=&w->weapons,.terrain=&w->terrain,.entity=entity_ref(e)},
  .input=in,.campaign_timer=w->enemies.mission_time,.layout=scripted_input_layout(&w->enemies,s->actor.damage_owner==1?0:1,w->slug_layout),.debug_flags=w->slug_debug_flags,
  .frame_phase=w->weapons.frame_phase,.mount_enabled=w->slug_riders_enabled,
  .player_query_enabled=w->slug_player_query_enabled,.narrow_bounds=w->slug_narrow_bounds,.contact_tick=contacts_tick};
 if(!slug_body_step(&c))fail(w,13);
}
static void clear_player_status(struct EntityPool*pool,struct NativeEntity*e,void*user){
 (void)pool;(void)e;struct NativePlayWorld*w=user;
 for(unsigned n=0;n<2;n++)if(w->weapons.players[n])w->weapons.players[n]->status_flags=0; /* $032EA4 */
}
static void clear_player_crouch(struct EntityPool*pool,struct NativeEntity*e,void*user){
 (void)pool;(void)e;struct NativePlayWorld*w=user;
 for(unsigned n=0;n<2;n++)if(w->weapons.players[n])w->weapons.players[n]->crouch_flags=0; /* $032EBA: $88, NOT arm completion. */
}
static void slug_request_tick(struct EntityPool*pool,struct NativeEntity*e,void*user){
 struct NativePlayWorld*w=user;if(w->fault)return;struct SlugAuxActor*q=&e->payload.slug_aux;
 if(w->slug.present){if(q->actor.variant)entity_delete(pool,e);return;}
 struct SlugSharedState*s=&w->slug;
 /* $05DD02 copies integer position, priority and facing; $26/$27 remain. */
 transform_integer_offset(&s->actor.transform,(int16_t)((q->actor.transform.x>>8)-(s->actor.transform.x>>8)),
  (int16_t)(transform_y_up(&q->actor.transform)-transform_y_up(&s->actor.transform)));
 s->actor.transform.priority=q->actor.transform.priority;s->actor.transform.facing=q->actor.transform.facing;
 s->ground_y_up=transform_y_up(&s->actor.transform);s->present=s->actor.active=1;
 slug_body_schedule_argument(s,SB_DROP_INIT,q->turret.variant);entity_delete(pool,e);
}
static int request_slug(struct NativePlayWorld*w,const struct PlayerTransform*t,uint8_t variant,int discard_busy,enum EntityInsert order){
 if(w->fault||!t)return 0;
 struct NativeEntity*e=entity_spawn(&w->entities,w->hits.enemy_group,ENTITY_SLUG_AUX,order,slug_request_tick);
 if(!e)return fail(w,9);struct SlugAuxActor*q=&e->payload.slug_aux;
 q->actor=(struct WeaponActor){.active=1,.owner=255,.damage_owner=255,.score_owner=255,.variant=(uint8_t)!!discard_busy,
  .hit={.id=e->index,.hp=-1,.last_attacker=65535,.last_kind=255,.flags=64},
  .animation={.drawing=65535,.cursor=65535,.last_draw_cursor=65535}};
 q->actor.transform=*t;q->turret.variant=variant<5?variant:4;return 1;
}
int play_world_request_slug(struct NativePlayWorld*w,const struct PlayerTransform*t,uint8_t variant,int discard_busy){
 return request_slug(w,t,variant,discard_busy,ENTITY_LAST_CHILD);
}
int play_world_cheat_slug(struct NativePlayWorld*w,unsigned player){
 if(!w||w->fault||player>1||w->slug.present)return 0;
 struct PlayerBodyState*p=w->weapons.players[player];if(!p)return 0;
 int sx=(int)(p->actor.transform.x>>8)-(int)w->weapons.camera_x;
 if(sx<48)sx=48;if(sx>272)sx=272;
 /* Same request the tutorial uses ($160,$511 there): top of the screen,
  * falling through SB_DROP_INIT -> SB_DROP_DESCEND -> landing. */
 struct PlayerTransform t=frontend_transform(&w->weapons,sx,511,0);
 return play_world_request_slug(w,&t,3,0);
}
void play_world_cheat_input(struct NativePlayWorld*w,unsigned player,uint8_t edges){
 static const uint8_t code[10]={PB_UP,PB_UP,PB_DOWN,PB_DOWN,PB_LEFT,PB_RIGHT,PB_LEFT,PB_RIGHT,PB_JUMP,PB_FIRE};
 if(!w||player>1)return;
 uint8_t*step=&w->cheat_step[player],*timer=&w->cheat_timer[player];
 if(*step&&*timer&&!--*timer)*step=0;          /* about 1.5 s between presses */
 edges&=PB_UP|PB_DOWN|PB_LEFT|PB_RIGHT|PB_FIRE|PB_JUMP;if(!edges)return;
 if(edges&code[*step])(*step)++;
 else *step=(edges&code[0])?1:0;
 *timer=90;
 if(*step==10){*step=0;play_world_cheat_slug(w,player);}
}
int play_world_begin_boss_slug(struct NativePlayWorld*w,enum BossSlugEntry entry){
 if(w->fault||w->slug.present||(unsigned)entry>BOSS_SLUG_DROP_ALTERNATE)return 0;
 w->slug.present=w->slug.actor.active=1;
 slug_body_schedule_argument(&w->slug,entry==BOSS_SLUG_ARENA?SB_BOSS_ARENA:SB_BOSS_DROP_INIT,(uint8_t)(entry==BOSS_SLUG_DROP_ALTERNATE));return 1;
}
int play_world_init(struct NativePlayWorld*w,const struct NativePlayServices*services,const struct SourceRng*rng){
 if(!services||!rng)return 0;
 *w=(struct NativePlayWorld){.services=*services,.rng=*rng};entities_init(&w->entities);results_reset(&w->results);
 struct EntityRef none={ENTITY_NONE,0},root=entity_ref(&w->entities.nodes[0]);
 w->hits=(struct HitScene){.entities=&w->entities,.players={none,none},.slug=none,.world=w,.damage=damage};
 w->arms[0]=w->arms[1]=none;
 /* $000A7C installs with prepend. Runtime order is the reverse: Slug,
  * player-status reset, P2, P1, crouch-trigger reset, shots, enemies. */
 struct NativeEntity*slug=entity_spawn(&w->entities,root,ENTITY_SLUG,ENTITY_LAST_CHILD,slug_tick);
 if(!slug)return fail(w,9);entity_reserve(slug);slug->payload.slug=&w->slug;
 slug_body_reset(&w->slug,slug->index);w->hits.slug=entity_ref(slug);
 struct NativeEntity*reset=entity_spawn(&w->entities,root,ENTITY_GROUP,ENTITY_LAST_CHILD,clear_player_status);
 if(!reset)return fail(w,9);entity_reserve(reset);
 for(int n=1;n>=0;n--){
  struct NativeEntity*g=entity_spawn(&w->entities,root,ENTITY_GROUP,ENTITY_LAST_CHILD,0);if(!g)return fail(w,9);
  entity_reserve(g);w->player_groups[n]=entity_ref(g);
 }
 reset=entity_spawn(&w->entities,root,ENTITY_GROUP,ENTITY_LAST_CHILD,clear_player_crouch);
 if(!reset)return fail(w,9);entity_reserve(reset);
 struct NativeEntity*shots=entity_spawn(&w->entities,root,ENTITY_GROUP,ENTITY_LAST_CHILD,0);
 if(!shots)return fail(w,9);entity_reserve(shots);
 struct NativeEntity*enemies=entity_spawn(&w->entities,root,ENTITY_GROUP,ENTITY_LAST_CHILD,0);
 if(!enemies)return fail(w,9);entity_reserve(enemies);w->hits.enemy_group=entity_ref(enemies);
 /* Source1008A0 is a separate destructible-prop group, not100120 shots. */
 struct NativeEntity*props=entity_spawn(&w->entities,root,ENTITY_GROUP,ENTITY_LAST_CHILD,0);
 if(!props)return fail(w,9);entity_reserve(props);w->hits.projectile_group=entity_ref(props);
 /* Boot table278000 installs100120 -> A7C; the projectile group parent
  * is that idle owner, whose66/7E words retain allocatorFFFF. */
 w->enemies.script151_projectile_context.hp=-1;w->enemies.script151_projectile_context.cutoff=-1;w->enemies.script151_projectile_context.valid=1;
 /* Native correction:45F0C prepares these source45DD4/45DDC tables in
  * A2/A0;28C20 erroneously readsA3/A4. Pass the prepared geometry directly. */
 w->enemies.recovery318.target=(struct HitRect){-24,24,-4,8};
 w->enemies.recovery318.point_x=w->enemies.recovery318.point_y=0;w->enemies.recovery318.valid=1;
 w->enemies.slug=&w->slug;w->items.slug=&w->slug;w->interactions.terrain=&w->terrain;w->interactions.targets=&w->targets;
 w->weapons=(struct WeaponWorld){.results=&w->results,.events=&w->events,.items=&w->items,.interactions=&w->interactions,.enemies=&w->enemies,.entities=&w->entities,.projectile_group=entity_ref(shots),.enemy_group=entity_ref(enemies),.prop_group=entity_ref(props),.rng=&w->rng,
  .scene=w,.move=move,.probe=probe,.point_shape=point_shape,.foot_contact=foot_contact,.attack=attack,.sound=sound,.audio_control=audio_control,.mission_spawn=mission_spawn};
 w->video.context=w->video.palette_context=w->services.context;w->video.column=w->services.column;w->video.write_fixed=w->services.write_fixed;w->video.write_tile=w->services.write_tile;w->video.clear_tile=w->services.clear_tile;w->video.set_palette=w->services.set_palette;w->video.palette_cycle=w->services.palette_cycle;w->video.actor_palette_cycle=w->services.actor_palette_cycle;w->weapons.video=&w->video;
 w->terrain.map=w;w->terrain.cell=cell;w->terrain.write_cell=write_cell;terrain_platform_reset(&w->terrain.platforms);return 1;
}
int play_world_add_player(struct NativePlayWorld*w,unsigned slot,const struct PlayerTransform*t,enum PlayerState state){
 if(w->fault||slot>1||!t||(unsigned)state>=PST_STATE_COUNT)return 0;
 if(entity_get(&w->entities,w->hits.players[slot]))return fail(w,10);
 struct NativeEntity*e=entity_spawn(&w->entities,w->player_groups[slot],ENTITY_PLAYER,ENTITY_LAST_CHILD,body_tick);
 if(!e)return fail(w,9);struct PlayerBodyState*p=&e->payload.player;p->character=(uint8_t)slot;p->actor.transform=*t;
 unsigned visual=p->character;
 player_inventory_init(p);p->actor.hit.hp=1;p->actor.palette_base=(uint16_t)(visual?0x177:0x176);
 p->actor.hit_palette=visual?0x90:0x8f;
 p->actor.palette=p->actor.palette_base;p->actor.palette_alternate=0x190;p->actor.palette_impact=(uint16_t)(visual?0x192:0x191);
 player_body_schedule(p,state);
 w->hits.players[slot]=entity_ref(e);w->weapons.players[slot]=p;
 struct NativeEntity*arm=entity_spawn(&w->entities,entity_ref(e),ENTITY_PLAYER_ARM,ENTITY_FIRST_CHILD,arm_tick);
 if(!arm)return fail(w,9);w->arms[slot]=entity_ref(arm);return 1;
}
int play_world_tick(struct NativePlayWorld*w){
 if(w->fault)return 0;
 w->weapons.camera_x=w->hits.camera_x=w->terrain.camera_x;
 w->weapons.camera_y=w->hits.camera_y=w->terrain.camera_y;w->weapons.scene_number=w->terrain.scene;
 w->weapons.ticks=w->ticks;w->weapons.frame_phase++; /*203C precedes actors.*/
 w->enemies.scripted_input.base_layout=w->control_mode[0];
 w->interactions.player_count_flag=0;
 for(unsigned i=0;i<2;i++){
  w->weapons.inputs[i]=w->raw_inputs_valid?w->raw_inputs[i]:w->inputs[i];
  struct NativeEntity*e=entity_get(&w->entities,w->hits.players[i]);
  if(e&&!e->deleting&&enemy_player_live(w->weapons.players[i]))w->interactions.player_count_flag++;
 }
 w->events.player_count=w->interactions.player_count_flag;
 w->weapons.camera_delta_x=w->terrain.delta_x;w->weapons.slug_mount_enabled=w->slug_riders_enabled;
 w->items.player_query_enabled=w->slug_player_query_enabled;
 w->enemies.blood_enabled=w->blood_enabled;
 /* $001F84 frame boundary, before the entity chain. */
 w->items.count_previous=w->items.count_current;w->items.count_current=0;
 /* $0437DA calls $08F8E2 before gameplay slots. Upper nibbles inhibit
  * claims; external anchor actors write LOW bits for the next frame. */
 w->slug.input_masks[(~w->weapons.frame_phase)&1]=0;
 terrain_platform_frame(&w->terrain.platforms);
 target_ring_frame(&w->targets);climb_targets_frame(&w->interactions.climb,w->weapons.frame_phase);
 enemy_world_frame(&w->enemies);
 w->weapons.mission_enemies=w->entities.displayed_count;
 if(w->scene_runtime){struct SceneRuntime*s=w->scene_runtime;w->weapons.sound_reply=source_audio_reply_frame(&s->audio);w->weapons.sound_reply_valid=1;}
 w->hits.camera_x=w->terrain.camera_x;w->hits.camera_y=w->terrain.camera_y;
 if(w->drawing_output&&(!w->drawing_output->begin||!w->drawing_output->end||
  !w->drawing_output->begin(w->drawing_output->context)))return fail(w,16);
 const struct AnimationOutput*previous=animation_output_scope(w->drawing_output);
 entities_tick(&w->entities,&w->weapons);
 animation_output_scope(previous);
 if(w->video.fault||w->entities.fault||w->weapons.fault||w->hits.fault||w->terrain.fault||w->slug.fault||w->items.fault)return fail(w,11);
 if(w->video.palette_frame&&!w->video.palette_frame(w->video.palette_context,&w->video.fade))return fail(w,15);
 if(w->drawing_output&&!w->drawing_output->end(w->drawing_output->context))return fail(w,16);
 w->ticks++;return 1;
}
int play_world_weapon_pickup(struct NativePlayWorld*w,unsigned slot,unsigned weapon,unsigned quantity){
 if(w->fault||slot>1)return 0;
 struct NativeEntity*e=entity_get(&w->entities,w->hits.players[slot]);if(!e||e->kind!=ENTITY_PLAYER)return fail(w,10);
 if(!player_set_weapon_ammo(&e->payload.player,weapon,quantity,1))return fail(w,12);
 player_begin_weapon_flash(&e->payload.player);return 1;
}
static int spawn_template(struct NativePlayWorld*w,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8],uint8_t flags,enum EntityInsert order){
 if(w->fault||!t)return 0;
 struct PlayerTransform placed=*t;placed.display_group=flags;t=&placed;
 if(id>=1&&id<=22)return soldier_spawn_template(&w->weapons,w->hits.enemy_group,order,id,t,params,flags);
 if(id==228||id==229)return deploy_spawn(&w->weapons,w->hits.enemy_group,order,t,params);
 if((id>=174&&id<=179)||(id>=195&&id<=226))return mob_spawn_template(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id>=168&&id<=170)return stationary_guard_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id==227)return presentation_spawn(&w->weapons,w->hits.enemy_group,order,t,0);
 if(id==251)return scripted_input_spawn(&w->weapons,w->hits.enemy_group,order,t);
 if(id==318)return recovery318_spawn(&w->weapons,w->hits.enemy_group,order,t);
 if(id==151)return script151_spawn(&w->weapons,w->hits.enemy_group,order,t,params);
 if(id==152)return enemy_zone_spawn(&w->weapons,w->hits.enemy_group,order,t,params);
 if(id==328)return allen_spawn(&w->weapons,w->hits.enemy_group,order,t,params);
 if(id==132)return boss2_spawn(&w->weapons,w->hits.enemy_group,order,t,params);
 if((id>=230&&id<=243)||id==321||id==322||id==327)return banner_spawn_template(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id==250||id==252||(id>=259&&id<=262)||(id>=272&&id<=274)||id==317||id==319)return scene_prop_spawn_template(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id==271)return flame_trap_spawn(&w->weapons,w->hits.enemy_group,order,t,params);
 if(id==29)return turret29_spawn(&w->weapons,w->hits.enemy_group,order,t,params);
 if(id==171||id==193)return ring_gun_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id==194)return ring_walker_spawn(&w->weapons,w->hits.enemy_group,order,t,params);
 if(id==156||(id>=244&&id<=248)||id==320)return capsule_spawn_template(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id>=185&&id<=191)return nest185_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if((id>=153&&id<=155)||(id>=157&&id<=167)||(id>=180&&id<=184))return grunt153_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id>=23&&id<=26)return crew23_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id==142||(id>=144&&id<=150)||(id>=276&&id<=280))return turret142_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id==59)return emitter59_spawn(&w->weapons,w->hits.enemy_group,order,t,params);
 if(id>=76&&id<=85)return assault76_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id==75)return gunship_spawn(&w->weapons,w->hits.enemy_group,order,t,params);
 if(id>=111&&id<=113)return m5_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id>=53&&id<=55)return light_tank_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id==94||id==95)return paratrooper_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id==141)return barrel_spawn(&w->weapons,w->hits.enemy_group,order,t,params);
 if(id==108||id==109)return shield_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params)!=0;
 if(id==267||id==275)return m4_lift_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if((id>=96&&id<=103)||id==106||id==107||id==110)return free96_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id==104||id==105)return free104_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params)!=0;
 if((id>=263&&id<=266)||id==268)return m4_spawn_template(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id>=31&&id<=49)return late_guard_spawn_template(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if((id>=56&&id<=58)||id==253)return siege_spawn_template(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if((id>=254&&id<=258)||id==270||id==323||id==324)return airship_spawn_template(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id==325||id==326){struct NativeEntity*e=enemy_allocate(&w->weapons,w->hits.enemy_group,order,t,slug_bounds_tick);if(!e)return 0;e->payload.enemy.params[0]=id==326?255:0;return 1;}
 if(id==269)return fort_spawn_template(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id==117)return boss117_spawn(&w->weapons,w->hits.enemy_group,order,t,params);
 if(id>=129&&id<=131)return carrier_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id==64||id==65||id==86||id==87||(id>=90&&id<=93))return bazooka_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params,flags);
 if(id==118||id==119)return tank_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id>=114&&id<=116)return final_boss_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id==137)return air_formation_spawn(&w->weapons,w->hits.enemy_group,order,t,params);
 if(id==120||id==121)return boss120_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id==30||(id>=125&&id<=128))return captor_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if((id>=60&&id<=63)||(id>=122&&id<=124))return mortar_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id>=66&&id<=74)return walker_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id==88||id==89)return rocket_vehicle_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id>=50&&id<=52)return ground_nest_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id>=133&&id<=136)return gun_platform_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params);
 if(id>=138&&id<=140)return crab_spawn(&w->weapons,w->hits.enemy_group,order,id,t,params);
 switch(id){
 case 0:case 27:case 28:case 249:case 281:case 282:
  return infantry_spawn_template(&w->weapons,w->hits.enemy_group,order,id,t,params,flags);
 case 143:case 172:case 173:case 192:case 283:case 293:
  return interaction_spawn_template(&w->weapons,w->hits.enemy_group,order,id,t,params);
 case 292:return request_slug(w,t,params?params[0]:255,1,order);
 default:return item_spawn_template(&w->weapons,w->hits.enemy_group,order,id,t,params);
 }
}
int play_world_spawn_template(struct NativePlayWorld*w,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8],uint8_t flags){
 return spawn_template(w,id,t,params,flags,ENTITY_LAST_CHILD);
}
int play_world_start_events(struct NativePlayWorld*w,unsigned scene){
 uint32_t source=mission_scene_source(scene);
 if(w->fault||!source)return 0;
 w->enemies.cutscene.variant=0;
 if(!scene_text_spawn(&w->weapons,w->hits.enemy_group,SCENE_TEXT_ENTRY_WATCHER))return 0;
 struct PlayerTransform t={.x=-256,.y=513*256,.priority=65535,.display_group=255};
 if(!scripted_input_spawn(&w->weapons,w->hits.enemy_group,ENTITY_FIRST_CHILD,&t))return 0;
 return mission_event_begin(&w->weapons,w->hits.enemy_group,source);
}
int play_world_spawn_player(struct NativePlayWorld*w,unsigned slot,const struct PlayerTransform*t,unsigned mode){
 if(w->fault||slot>1||!t||mode>2)return 0;
 struct NativeEntity*old=entity_get(&w->entities,w->hits.players[slot]);
 if(old){
  if(old->kind!=ENTITY_PLAYER||!old->payload.player.life_retired)return fail(w,10);
  entity_delete(&w->entities,old);w->hits.players[slot]=(struct EntityRef){ENTITY_NONE,0};w->weapons.players[slot]=0;
 }
 enum PlayerState state=mode==2?PST_SPAWN_FALL:mode==1?PST_SPAWN_FREEFALL:PST_SPAWN_PARACHUTE;
 if(!play_world_add_player(w,slot,t,state))return 0;
 struct PlayerBodyState*p=w->weapons.players[slot];p->spawn_mode=(uint8_t)mode;
 p->actor.transform.priority=(uint16_t)((p->actor.transform.priority&0xffe3)|0x9ff0);
 if(mode!=2)p->actor.damage_timer=p->actor.hit.invulnerable=127;
 return 1;
}
int play_world_spawn_player_from_scene(struct NativePlayWorld*w,unsigned slot){
 if(!w->scene_runtime||slot>1)return fail(w,13);struct SceneSpawnHint hint;if(!scene_runtime_spawn_hint(w->scene_runtime,&hint)||hint.mode>2)return fail(w,13);
 int16_t x=(int16_t)(uint16_t)(hint.x+(slot?-(int)hint.spacing:hint.spacing));
 struct PlayerTransform t={.x=(int32_t)(int16_t)(uint16_t)(x+w->weapons.camera_x)*256,.y=(int32_t)(int16_t)(uint16_t)(512-hint.y_up+w->weapons.camera_y)*256};
 return play_world_spawn_player(w,slot,&t,hint.mode);
}

int play_world_spawn_siege_scene_record(struct NativePlayWorld*w,unsigned index){
 return siege_spawn_scene3_record(&w->weapons,w->hits.enemy_group,index);
}

int play_world_spawn_fort_scene_record(struct NativePlayWorld*w,unsigned index){return fort_spawn_scene4_record(&w->weapons,w->hits.enemy_group,index);}

int play_world_spawn_airship_scene_record(struct NativePlayWorld*w,unsigned index){return airship_spawn_scene5_record(&w->weapons,w->hits.enemy_group,index);}
