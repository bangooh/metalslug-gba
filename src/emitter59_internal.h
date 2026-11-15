#ifndef NATIVE_EMITTER59_INTERNAL_H
#define NATIVE_EMITTER59_INTERNAL_H
#include "tank_internal.h"
#include "m4_internal.h"
#include "../generated/emitter59-programs.h"
enum Emitter59State {E59_ROOT,E59_WAIT,E59_BODY,E59_INTRO,E59_OPEN,E59_IDLE,E59_TURN,E59_FIRE_NEW,E59_FIRE_REPEAT,E59_FIRE,E59_RELEASE,E59_FALL,E59_LANDED,E59_DEAD,E59_SHELL,E59_FIRE_FX,E59_BLAST,E59_MUZZLE,E59_RELEASED,E59_SELECT,E59_SOLDIER,E59_SMOKE,E59_EXHAUST,E59_FRAGMENT,E59_METAL,E59_SPARK,E59_PUFF,E59_DELETE,E59_DELETE_TREE};
void e59_schedule(struct NativeEntity*,unsigned);
struct NativeEntity*e59_child(struct WeaponWorld*,struct NativeEntity*,unsigned);
struct NativeEntity*e59_allocate(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,unsigned);
void e59_start(struct EnemyActor*,unsigned);
void e59_entry(struct EnemyActor*,uint16_t);
void e59_defense(struct EnemyActor*,unsigned);
void e59_attack(struct EnemyActor*,unsigned);
int e59_animate(struct WeaponWorld*,struct NativeEntity*);
int16_t e59_rank(struct WeaponWorld*,struct EnemyActor*,unsigned);
int e59_outside(struct WeaponWorld*,struct WeaponActor*,unsigned,int);
int e59_emit(struct WeaponWorld*,struct NativeEntity*,unsigned);
int e59_fx_enter(struct WeaponWorld*,struct NativeEntity*);
int e59_fx_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
#endif
