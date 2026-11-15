#ifndef NATIVE_ASSAULT76_INTERNAL_H
#define NATIVE_ASSAULT76_INTERNAL_H
#include "tank_internal.h"
#include "m4_internal.h"
#include "../generated/assault76-programs.h"
enum Assault76State {R76_ROOT,R76_WAIT,R76_WALK,R76_STOP,R76_KNEEL,R76_WAIT_A,R76_WAIT_B,R76_STAND,R76_CHARGE,R76_DEAD,R76_WRECK,R76_RUN_WAIT,R76_RUN,R76_LEAP,R76_THROWN,R76_SURFACE,R76_MUSIC,R76_MANAGER,R76_STOP_ALL,
 R76_PILOT,R76_PILOT_IDLE,R76_PILOT_SHOT,R76_PILOT_OPEN,R76_PILOT_REARM,R76_PILOT_NEXT,R76_PILOT_AIM,R76_PILOT_FIRE,R76_PILOT_DONE,R76_PILOT_EJECT,R76_OVERLAY,R76_OVERLAY_DEAD,R76_TRAIL,R76_ESCAPE,
 R76_GRENADE,R76_SUBMERGE,R76_FLOAT,R76_FLOAT_HIT,R76_SMOKE,R76_FLASH,R76_DEBRIS,R76_DEBRIS_FALL,R76_DEBRIS_BOUNCE,R76_DEBRIS_SLIDE,R76_DEBRIS_HOP,R76_MUZZLE,R76_MISSILE,R76_MISSILE_RESUME,R76_SPARK,R76_BOMB,R76_DUST,R76_EXPLODE,R76_FIRE,R76_DELETE,R76_DELETE_TREE};
struct NativeEntity*r76_allocate(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,unsigned);
void r76_schedule(struct NativeEntity*,unsigned);
struct NativeEntity*r76_child(struct WeaponWorld*,struct NativeEntity*,unsigned);
void r76_start(struct EnemyActor*,unsigned);
void r76_entry(struct EnemyActor*,uint16_t);
int r76_animate(struct WeaponWorld*,struct NativeEntity*);
void r76_defense(struct EnemyActor*,unsigned);
void r76_attack(struct EnemyActor*,unsigned);
int16_t r76_rank(struct WeaponWorld*,struct EnemyActor*,unsigned);
int r76_follow(struct WeaponWorld*,struct NativeEntity*);
int r76_outside(struct WeaponWorld*,struct WeaponActor*,int);
int r76_parent_done(struct WeaponWorld*,struct NativeEntity*);
int r76_parts_enter(struct WeaponWorld*,struct NativeEntity*);
int r76_parts_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int r76_ordnance_enter(struct WeaponWorld*,struct NativeEntity*);
int r76_ordnance_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int r76_emit(struct WeaponWorld*,struct NativeEntity*,unsigned);
#endif
