#ifndef NATIVE_INTERACTIONS_H
#define NATIVE_INTERACTIONS_H
#include "terrain_motion.h"
#include "target_ring.h"
#include "../generated/interaction-programs.h"
#include "../generated/runtime-data.h"
/* $076FEA: separate from the terrain platform ring and the gun target ring. */
struct ClimbRecord {int16_t x,y;uint8_t height,key;};
struct ClimbTargets {struct ClimbRecord records[2][8];uint8_t count[2],phase,last_key;};
struct InteractionWorld {
 struct TerrainWorld *terrain;
 struct TargetRing *targets;
 struct ClimbTargets climb;
 uint16_t rescued[2];
 /* BIOS difficulty0..7 BEFORE799A4's demo/Slug adjustment. The Slug
  * predicate is sampled at each use, not cached at the frame boundary. */
 uint8_t rank,rank_valid,player_count_flag,demo_mode;
};
enum PowEntry {POW_BOUND,POW_STACKED,POW_IDLE,POW_FREE};
void climb_targets_frame(struct ClimbTargets*,unsigned phase);
int climb_targets_publish(struct ClimbTargets*,int16_t x,int16_t world_y,uint8_t height,uint8_t key);
int climb_target_crossing(const struct ClimbTargets*,const struct WeaponActor*,uint8_t*key,int*ascending);
int climb_target_nearest(const struct ClimbTargets*,const struct WeaponActor*,uint8_t*key,int16_t*x);
int climb_target_follow(struct WeaponWorld*,struct WeaponActor*,uint8_t key,int16_t*height);
/* $077224 template 143: static climb edge, height from source $98. */
int climb_actor_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,uint8_t height);
/* $077228: the parent-selected flag can retire an attached climb publisher. */
int climb_actor_spawn_watched(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,uint8_t height,MissionWatchFn);
struct NativeEntity*pow_spawn_entity(struct WeaponWorld*,struct EntityRef,enum EntityInsert,enum PowEntry,const struct PlayerTransform*,const uint8_t params[8]);
int pow_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,enum PowEntry,const struct PlayerTransform*,const uint8_t params[8]);
int crate_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,const uint8_t params[8]);
/* Attached form interprets t integer X and source Y-up as parent-local offsets. */
int gun_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,int attached);
int gun_destroy(struct WeaponWorld*,struct EntityRef);
int interaction_spawn_template(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t params[8]);
/* Common typed owner for the compiled POW, crate and gun shell streams. */
struct NativeEntity* interaction_allocate(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,EntityTick);
int interaction_fault(struct WeaponWorld*);
/*799A4/2AC4C also copies table+6A into actor68 when a Slug is present. */
unsigned interaction_rank(struct WeaponWorld*,struct WeaponActor*,uint8_t table_owner);
int interaction_move(struct WeaponWorld*,struct WeaponActor*,enum WeaponMove);
int interaction_animate(struct WeaponWorld*,struct NativeEntity*);
void interaction_start(struct InteractionActor*,unsigned animation);
void interaction_defense(struct InteractionActor*,unsigned binding);
int interaction_outside(struct WeaponWorld*,struct WeaponActor*,const int16_t*,int force);
void interaction_priority(struct WeaponWorld*,struct WeaponActor*,uint16_t,unsigned bits);
int interaction_sound(struct WeaponWorld*,uint16_t);
void crate_body_contact(struct WeaponWorld*,struct WeaponActor*);
void interaction_body_contact(struct WeaponWorld*,struct WeaponActor*,const struct InteractionContact*);
#endif
