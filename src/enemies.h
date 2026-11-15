#ifndef NATIVE_ENEMIES_H
#define NATIVE_ENEMIES_H
#include "interactions.h"
#include "../generated/enemy-programs.h"
/* Unlike the persistent target ring, these are one-frame SCREEN words. */
struct DeathPosition {int16_t x,y_up;};
struct DeathPositionRing {struct DeathPosition records[8];uint8_t first,end,write,count;};
struct EnemyZone {uint16_t left,top,right,bottom;};
struct EnemyZones {struct EnemyZone records[8];uint8_t first,end,write,count;};
/* Explicit secondary-camera inputs and durable scene outputs for M5.
 * Camera relink and the nose collision patch must be consumed by scene/map
 * integration; they are not approximated by the fixed M1 background. */
struct M5SceneState {
 int32_t scroll_x_q16,scroll_y_q16;
 int16_t secondary_x,secondary_y,relink_x,relink_y,nose_x,nose_y;
 uint16_t relink_generation;
 uint8_t valid,active,scroll_mode,map_linked,nose_patch,release;
};
struct AirshipSceneState {
 int32_t scroll_x_q16,scroll_y_q16;
 int16_t camera_lock_x;
 uint16_t scroll_mode;
 uint8_t camera_lock_valid,scroll_valid;
};
struct EnemyWorld {
 /* $10E39D selects $10E2E2/$10E2E8. $10E39C is shared siege_phase. */
 struct {uint8_t active,system_freeze,base_layout;struct {uint8_t layout,previous,held,actions,latch;} player[2];} scripted_input;
 /* Native corrected45F1A arguments from45DD4/45DDC, not staleA3/A4.
  * Retained as explicit geometry so direct-world callers can override it. */
 struct {struct HitRect target;int16_t point_x,point_y;uint8_t valid;} recovery318;
 /*75EFA reads the enclosing owner of global projectile group, not wall HP. */
 struct {int16_t hp,cutoff;uint8_t valid;} script151_projectile_context;
 /* Source106F60/64 velocity outputs and10E2EE/EF presentation state. */
 struct {int32_t velocity_x_q16,velocity_y_q16;uint8_t velocity_mask,variant,playback_mode;} cutscene;
 uint8_t palette_shutdown; /*Source1081B1: white-flash controller latches neon shutdown. */
 uint8_t continue_count; /* $1081B0, written by template327; scene controller consumes it. */
 int16_t mission_time; /* $106E92: timer, template250, body, Slug and HUD share one WORD. */
 /* $10E276..$10E27B, rolled over by template84 in original task order. */
 struct {uint8_t previous[2],current[2],stop[2];} assault76;
 struct DeathPositionRing deaths;
 struct EnemyZones zones;
 struct SlugSharedState *slug;
 /* Original $106ED3, $10FD8F, $10E39E and $106F2B inputs. */
 uint8_t damage_enabled,blood_enabled,clear_ordnance,grenade_mode;
 /* Original scene $10A2D0/$10A2D1 and $10E39A; consumed by scene flow.
  * M2 destruction patches persist until the scene initializes a new map. */
 uint8_t scene_flags[2],intro_gate,boss2_patch_mask;
 uint8_t siege_phase; /* $10E39C */
 uint8_t scripted_player_lock; /* $106ED2; M5 ejection releases it. */
 struct M5SceneState m5_scene;
 struct AirshipSceneState airship_scene;
 /* $10816A/$10816E receive the same M4 lower camera limit. */
 int16_t m4_camera_floor;
 uint8_t m4_camera_floor_valid,debug_visible;
};
enum EnemyOrdnance {ENEMY_GRENADE,ENEMY_BOUNCE,ENEMY_MORTAR,ENEMY_ROLLER};
void enemy_world_frame(struct EnemyWorld*);
void enemy_publish_death(struct WeaponWorld*,const struct WeaponActor*);
int enemy_near_death(struct WeaponWorld*,const struct WeaponActor*,int*behind);
int enemy_player_live(const struct PlayerBodyState*);
int enemy_nearest_player(struct WeaponWorld*,const struct WeaponActor*,uint16_t*distance);
int enemy_nearest_x(struct WeaponWorld*,const struct WeaponActor*,const struct HitRect*filter);
uint8_t enemy_angle(int16_t,int16_t);
int enemy_fault(struct WeaponWorld*);
struct NativeEntity*enemy_allocate(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,EntityTick);
void enemy_schedule(struct EnemyActor*,unsigned state);
void enemy_start(struct EnemyActor*,enum EnemyAnimation);
int enemy_animate(struct WeaponWorld*,struct NativeEntity*);
void enemy_defense(struct EnemyActor*,unsigned binding);
void enemy_attack_binding(struct EnemyActor*,unsigned binding);
int enemy_attack(struct WeaponWorld*,struct WeaponActor*);
int enemy_swapped_move(struct WeaponWorld*,struct EnemyActor*,enum WeaponMove);
int enemy_ground_status(struct WeaponWorld*,struct EnemyActor*,int point);
/* $051914: attribution and per-hit score; external service is an observer. */
int enemy_damage_score(struct WeaponWorld*,const struct HitDamage*);
/* Death replaces the current semantic controller, preserving entity/children.
 * The four source dispatch tables select by last received damage kind. */
int human_death_schedule(struct WeaponWorld*,struct NativeEntity*,unsigned table);
int human_death_hit_tail(struct WeaponWorld*,struct NativeEntity*);
int human_death_hit_tail_alternate(struct WeaponWorld*,struct NativeEntity*);
int human_spawn_corpse(struct WeaponWorld*,struct EntityRef,const struct WeaponActor*,int launched);
struct NativeEntity*human_corpse_entity(struct WeaponWorld*,struct EntityRef,const struct WeaponActor*,int launched);
int human_platform_death(struct WeaponWorld*,struct NativeEntity*,int collapse);
int human_launch_direct(struct WeaponWorld*,struct NativeEntity*);
int script151_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,const uint8_t[8]);
int scripted_input_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*);
struct PlayerSourceInput scripted_input_select(const struct EnemyWorld*,unsigned,struct PlayerSourceInput);
unsigned scripted_input_layout(const struct EnemyWorld*,unsigned,unsigned);
int presentation_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,int logo);
int recovery318_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*);
int ring_gun_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
int ring_walker_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,const uint8_t[8]);
int human_corpse_transform(struct WeaponWorld*,struct NativeEntity*,int launched);
int human_charred_transform(struct WeaponWorld*,struct NativeEntity*);
int human_smoke_transform(struct WeaponWorld*,struct NativeEntity*);
int human_spawn_blood(struct WeaponWorld*,struct NativeEntity*,int variant);
/* Template 328 / $0508D6, including its four sensors and attack children. */
int allen_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,const uint8_t params[8]);
enum AllenEmission {ALLEN_FLASH_LEVEL,ALLEN_FLASH_DIAGONAL,ALLEN_BULLET_LEVEL,ALLEN_BULLET_DIAGONAL,ALLEN_GRENADE};
int allen_emit(struct WeaponWorld*,struct NativeEntity*,enum AllenEmission);
/* $055BCE/$056066/$056438: first child of the global projectile group.
 * Roller uses the complete $056844 wrapper, also in that global group. */
int enemy_spawn_ordnance(struct WeaponWorld*,struct EntityRef,const struct PlayerTransform*,enum EnemyOrdnance,uint8_t trajectory,uint8_t flags);
/* Actual template IDs, not labels inferred from sprite names. */
int turret29_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,const uint8_t[8]);
int infantry_spawn_template(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t params[8],uint8_t flags);
int soldier_spawn_template(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t params[8],uint8_t flags);
int soldier_mortar_eject(struct WeaponWorld*,struct NativeEntity*);
int soldier_tank_eject(struct WeaponWorld*,struct NativeEntity*);
struct NativeEntity*soldier_spawn_jump_entity(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,int grab);
int soldier_spawn_jump(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,int grab);
int soldier_think(struct WeaponWorld*,struct NativeEntity*);
enum InfantryExit {INFANTRY_EXIT_FACE,INFANTRY_EXIT_LOWER};
void infantry_escape(struct WeaponWorld*,struct NativeEntity*,enum InfantryExit);
void infantry_stand_escape(struct NativeEntity*,uint8_t source_flags72);
void infantry_free_escape(struct NativeEntity*,uint8_t source_flags72);
void infantry_force_escape(struct NativeEntity*,uint8_t source_flags72);
struct NativeEntity*infantry_spawn_forced_escape(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,const uint8_t[8]);
int enemy_zone_contains(struct WeaponWorld*,const struct WeaponActor*);
void enemy_zone_publish(struct WeaponWorld*,const struct EnemyZone*);
int enemy_zone_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,const uint8_t params[8]);
/* $08F7CE/$08F82A/$08F85C/$08F884: availability, approach, claim, refresh. */
uint8_t enemy_anchor_mask(struct WeaponWorld*,struct EnemyActor*);
int enemy_anchor(struct WeaponWorld*,struct EnemyActor*,unsigned index,unsigned mode,int16_t*x,int16_t*y_up);
void enemy_anchor_detach_cannon(struct WeaponWorld*,struct EnemyActor*);
int stationary_guard_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
int deploy_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,const uint8_t params[8]);
int enemy_bounce_explode(struct WeaponWorld*,struct NativeEntity*);
#endif
