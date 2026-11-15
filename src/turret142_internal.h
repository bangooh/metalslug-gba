#ifndef NATIVE_TURRET142_INTERNAL_H
#define NATIVE_TURRET142_INTERNAL_H
#include "tank_internal.h"
#include "m4_internal.h"
#include "items.h"
#include "../generated/turret142-programs.h"
enum Turret142State {T42_ROOT,T42_ACTIVE,T42_ORBIT,T42_DEAD,T42_DEAD_B,T42_EXIT,T42_GUN,T42_BARREL,T42_SHELL,T42_BLAST,T42_CRASH,
 T42_A_NEW,T42_A_IDLE,T42_A_HURT,T42_A_DEAD,T42_A_RESET,T42_A_IDLE2,T42_A_HURT2,T42_A_DEAD2,T42_A_WRECK,
 T42_B_NEW,T42_B_IDLE,T42_B_DEAD,T42_C_NEW,T42_C_IDLE,T42_C_DEAD,T42_PROP_NEW,T42_PROP,T42_SMOKE,T42_FLAME,T42_DELETE,T42_DELETE_TREE};
void t42_schedule(struct NativeEntity*,unsigned);
struct NativeEntity*t42_allocate(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,unsigned);
struct NativeEntity*t42_child(struct WeaponWorld*,struct NativeEntity*,unsigned,int copy);
void t42_start(struct EnemyActor*,unsigned);
void t42_entry(struct EnemyActor*,uint16_t);
void t42_defense(struct EnemyActor*,unsigned);
void t42_attack(struct EnemyActor*,unsigned);
int16_t t42_rank(struct WeaponWorld*,struct EnemyActor*,unsigned);
int t42_animate(struct WeaponWorld*,struct NativeEntity*);
int t42_outside(struct WeaponWorld*,struct WeaponActor*,unsigned,int);
int t42_stamp(struct WeaponWorld*,struct NativeEntity*,unsigned);
int t42_parts_enter(struct WeaponWorld*,struct NativeEntity*);
int t42_parts_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int obstacle_enter(struct WeaponWorld*,struct NativeEntity*);
int obstacle_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
#endif
