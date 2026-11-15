#ifndef NATIVE_SHORE_H
#define NATIVE_SHORE_H
#include "m4_internal.h"
#include "../generated/shore-programs.h"
/* $065F40..$0670D2: aquatic barrel, reinforcement controller and its troops. */
typedef int (*ShoreCallback)(struct WeaponWorld*,struct NativeEntity*,unsigned);
int16_t shore_rank_value(struct WeaponWorld*,struct EnemyActor*,unsigned);
void shore_start(struct EnemyActor*,enum ShoreAnimation);
void shore_defense(struct EnemyActor*,unsigned);
void shore_attack(struct EnemyActor*,unsigned);
int shore_animate(struct WeaponWorld*,struct NativeEntity*,ShoreCallback);
struct NativeEntity*shore_splash(struct WeaponWorld*,struct NativeEntity*,int fixed);
int paratrooper_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
int barrel_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,const uint8_t[8]);
#endif
