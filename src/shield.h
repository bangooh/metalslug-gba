#ifndef NATIVE_SHIELD_H
#define NATIVE_SHIELD_H
#include "m4.h"
#include "../generated/shield-programs.h"
struct NativeEntity*shield_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
int m4_lift_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
void shield_start(struct EnemyActor*,enum ShieldAnimation);
void shield_start_entry(struct EnemyActor*,uint16_t);
void shield_defense(struct EnemyActor*,unsigned);
int shield_animate(struct WeaponWorld*,struct NativeEntity*);
int shield_hurt(struct WeaponWorld*,struct WeaponActor*);
#endif
