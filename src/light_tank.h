#ifndef NATIVE_LIGHT_TANK_H
#define NATIVE_LIGHT_TANK_H
#include "m4_internal.h"
#include "../generated/light-tank-programs.h"
enum LightTankState {LT_NEW,LT_FALL,LT_DRIVE,LT_IDLE,LT_TURN,LT_BRAKE,LT_RESUME,LT_AIM,LT_AIM_NEAR,LT_FIRE_FIRST,LT_FIRE,LT_CLOSE,LT_TURRET_HIT,LT_HIT,LT_HIT_BIG,LT_DEAD,LT_DEAD_BIG,LT_REMOVE,
 LT_DRIVER,LT_DRIVER_OUT,LT_DRIVER_HIT,LT_DRIVER_DIE,LT_DRIVER_EXIT,LT_DRIVER_CORPSE,LT_TURRET,LT_TURRET_RELEASE,LT_SMOKE,LT_SMOKE_BACK,LT_SHELL,LT_TRAIL,LT_MISSILE,LT_HOME,LT_MISSILE_COAST,LT_FIRE_BLAST,LT_SHELL_BLAST,LT_SHELL_IMPACT,LT_DELETE,LT_DELETE_TREE};
int light_tank_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
void lt_schedule(struct NativeEntity*,unsigned);
struct NativeEntity*lt_child(struct WeaponWorld*,struct NativeEntity*,unsigned,enum EntityInsert);
void lt_start(struct EnemyActor*,enum LightTankAnimation);
void lt_start_entry(struct EnemyActor*,uint16_t);
void lt_defense(struct EnemyActor*,unsigned);
void lt_attack(struct EnemyActor*,unsigned);
int lt_animate(struct WeaponWorld*,struct NativeEntity*);
int16_t lt_rank(struct WeaponWorld*,struct EnemyActor*,unsigned);
uint16_t lt_random(struct WeaponWorld*);
int lt_parts_enter(struct WeaponWorld*,struct NativeEntity*);
int lt_parts_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int lt_callback(struct WeaponWorld*,struct NativeEntity*,unsigned);
int lt_settle(struct WeaponWorld*,struct WeaponActor*);
int lt_hurt(struct WeaponWorld*,struct WeaponActor*);
#endif
