#ifndef NATIVE_AIR_FORMATION_INTERNAL_H
#define NATIVE_AIR_FORMATION_INTERNAL_H
#include "tank_internal.h"
#include "m4_internal.h"
#include "../generated/air_formation-programs.h"
enum AirFormationState {AF_ROOT,AF_INTRO_WAIT,AF_MANAGER,AF_SPAWN,AF_IDLE,AF_INTRO,AF_TURN,AF_ZOOM,AF_NOTIFY,AF_APPROACH,AF_APPROACH_START,AF_APPROACH_RUN,AF_PLANE,AF_PLANE_WAIT,AF_PLANE_START,AF_PLANE_ACTIVE,AF_RETIRED,AF_OUTSIDE,
 AF_OVERLAY_INTRO,AF_OVERLAY,AF_OVERLAY_DEAD,AF_PILOT,AF_PILOT_LEAP,AF_PILOT_FALL,AF_PILOT_EXPLODE,AF_PILOT_FIRE,
 AF_HOP,AF_HOP_WAIT,AF_HOP_FALL,AF_TUMBLE,AF_TUMBLE_ALT,AF_TUMBLE_RUN,AF_SKID,AF_SKID_RUN,AF_SKID_FALL,AF_SHRINK,AF_SHRINK_RUN,AF_ARC,AF_CRASH,AF_WATER_CRASH,AF_SPLIT,AF_SHARD,AF_SHARD_FORWARD,AF_DEBRIS,AF_SHARD_RUN,AF_GLINT,AF_PATTERN,AF_SOUND_REMOVE,AF_REMOVE,AF_DELETE,AF_DELETE_TREE};
void af_schedule(struct NativeEntity*,unsigned);
struct NativeEntity*af_child(struct WeaponWorld*,struct NativeEntity*,unsigned,enum EntityInsert);
void af_start(struct EnemyActor*,unsigned);
void af_defense(struct EnemyActor*,unsigned);
int af_animate(struct WeaponWorld*,struct NativeEntity*);
int16_t af_rank(struct WeaponWorld*,struct EnemyActor*,unsigned);
int af_path(struct WeaponWorld*,struct NativeEntity*);
void af_path_begin(struct EnemyActor*,unsigned);
void af_speed(struct EnemyActor*);
int af_follow(struct WeaponWorld*,struct NativeEntity*);
int af_outside(struct WeaponWorld*,struct WeaponActor*,int);
int af_front_edge(struct WeaponWorld*,struct WeaponActor*);
int af_water(struct WeaponWorld*,struct WeaponActor*);
int af_surface(struct WeaponWorld*,struct NativeEntity*,int);
int af_parts_enter(struct WeaponWorld*,struct NativeEntity*);
int af_parts_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int af_death_enter(struct WeaponWorld*,struct NativeEntity*);
int af_death_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int af_pattern(struct WeaponWorld*,struct NativeEntity*,unsigned);
#endif
