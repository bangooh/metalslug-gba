#ifndef NATIVE_SCRIPT151_INTERNAL_H
#define NATIVE_SCRIPT151_INTERNAL_H
#include "tank_internal.h"
#include "../generated/script151-programs.h"
enum Script151State {S151_ROOT,S151_WAIT,S151_ACTIVE,S151_DEAD,
 S151_WALL_NEW,S151_WALL,S151_WALL_WAIT,S151_WALL_OPEN,S151_LOW_WAIT,
 S151_TOWER_A,S151_AIM_A,S151_FIRE_A,S151_TOWER_B,S151_AIM_B,S151_FIRE_B,
 S151_DOOR_A_OPEN,S151_DOOR_A_LOOP,S151_DOOR_A_CLOSE,S151_DOOR_B_OPEN,S151_DOOR_B_LOOP,S151_DOOR_B_CLOSE,S151_COLLAPSE,
 S151_FLAG,S151_FLAME,S151_SMOKE,S151_WINDOW,S151_WINDOW_FALL,S151_ROOF_A,S151_ROOF_A_WRECK,S151_ROOF_B,S151_ROOF_B_WRECK,S151_ROOF_B_DRAIN,
 S151_GATE,S151_BASE,S151_CREW,S151_NUKE,S151_NUKE_END,S151_FRAGMENT,S151_TRAIL};
enum Script151Signal {S151_SIG_OPEN,S151_SIG_WATCH,S151_SIG_ROOF_A,S151_SIG_ROOF_B,S151_SIG_ROOF_A_END,S151_SIG_ROOF_B_END,S151_SIG_BASE_END,S151_SIG_HIT,S151_SIG_DEAD};
void s151_schedule(struct NativeEntity*,unsigned);
struct NativeEntity*s151_allocate(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,unsigned);
struct NativeEntity*s151_child(struct WeaponWorld*,struct NativeEntity*,unsigned);
void s151_start(struct EnemyActor*,unsigned);
int s151_animate(struct WeaponWorld*,struct NativeEntity*);
int16_t s151_rank(struct WeaponWorld*,struct EnemyActor*,unsigned);
int s151_anchor(struct WeaponWorld*,struct WeaponActor*,int16_t x,int16_t y);
int s151_patch(struct WeaponWorld*,unsigned);
int s151_effect_callback(struct WeaponWorld*,struct NativeEntity*,unsigned);
int s151_parts_enter(struct WeaponWorld*,struct NativeEntity*);
int s151_parts_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int s151_effects_enter(struct WeaponWorld*,struct NativeEntity*);
int s151_effects_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
void s151_attack(struct EnemyActor*,unsigned);
#endif
