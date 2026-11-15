#ifndef NATIVE_CARRIER_INTERNAL_H
#define NATIVE_CARRIER_INTERNAL_H
#include "bosses.h"
#include "items.h"
#include "../generated/carrier-programs.h"
enum CarrierState {
 CR_ROOT,CR_BODY,CR_PHASE4,CR_PHASE5,CR_WAIT,CR_INTRO_GATE,
 CR_HULL,CR_HULL_RANK,CR_ROW_SMOKE,CR_ROW,CR_ROW_DEPLOYED,CR_COCKPIT,CR_EJECT,
 CR_MARK_LEFT,CR_MARK_RIGHT,CR_MARK_COCKPIT,CR_HATCH,CR_OPEN,CR_OPEN2,CR_DROP,CR_HATCH_FALL,CR_CANNON,CR_CANNON_FALL,
 CR_SPAWNER,CR_SPAWNER_WAIT,CR_TROOPER,CR_TROOPER_JUMP,CR_TROOPER_LAND,CR_RUN_SELECT,CR_RUN_RIGHT,CR_RUN,CR_STAND,CR_IDLE,CR_AIM,CR_WALK_TURN,CR_WALK,CR_STOP,CR_TURN,CR_FIRE,
 CR_RIDER,CR_RIDE,CR_LEAP,CR_LEAP_B,CR_FALL,CR_LAND,CR_FLEE,CR_DIVE,CR_DIVE_LOOP,CR_DIVE_LAND,CR_FLEE_B,
 CR_GUNNERS,CR_RELOAD,CR_GUN_HOLD,CR_GUN_POSE,CR_GUNNER_FIRE,CR_GUNNER_SPECIAL,CR_GUNNER_LAND,CR_GUNNER_STAND,CR_GUNNER_RIGHT,CR_GUNNER_WALK,CR_GUNNER_STOP,CR_GUNNER_RIDE,CR_GUNNER_IDLE,CR_GUNNER_DECIDE,CR_GUNNER_AIM,CR_GUNNER_THROW,
 CR_GUARD,CR_GUARD_LAND,CR_GUARD_FADE,CR_MARK_DELETE,CR_DELETE
};
static inline int16_t cr_word(int x){return (int16_t)(uint16_t)x;}
static inline int16_t cr_sx(struct WeaponWorld*w,const struct WeaponActor*a){return cr_word((a->transform.x>>8)-w->camera_x);}
static inline int16_t cr_sy(struct WeaponWorld*w,const struct WeaponActor*a){return cr_word(transform_y_up(&a->transform)+w->camera_y);}
void cr_screen(struct WeaponWorld*,struct WeaponActor*,int,int);
void cr_reset(struct WeaponActor*);
void cr_schedule(struct NativeEntity*,unsigned);
void cr_start(struct EnemyActor*,enum CarrierAnimation);
void cr_start_entry(struct EnemyActor*,uint16_t);
void cr_defense(struct EnemyActor*,unsigned);
int cr_animate(struct WeaponWorld*,struct NativeEntity*);
uint16_t cr_random(struct WeaponWorld*);
int16_t cr_rank(struct WeaponWorld*,struct EnemyActor*,unsigned);
int cr_hit_sound(struct WeaponWorld*,struct WeaponActor*);
struct EnemyActor*cr_parent(struct WeaponWorld*,struct NativeEntity*);
struct EnemyActor*cr_grandparent(struct WeaponWorld*,struct NativeEntity*);
struct NativeEntity*cr_child(struct WeaponWorld*,struct NativeEntity*,unsigned);
int cr_follow(struct WeaponWorld*,struct NativeEntity*);
int cr_parent_dead(struct WeaponWorld*,struct NativeEntity*);
void cr_parent_tail(struct WeaponWorld*,struct NativeEntity*,int mark);
int cr_platforms(struct WeaponWorld*,struct WeaponActor*,int cockpit);
void cr_bob(struct EnemyActor*);
int cr_arc(struct WeaponWorld*,struct NativeEntity*);
int cr_gunner_follow(struct WeaponWorld*,struct NativeEntity*,int offset);
int cr_parts_enter(struct WeaponWorld*,struct NativeEntity*);
int cr_parts_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int cr_crew_enter(struct WeaponWorld*,struct NativeEntity*);
int cr_crew_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
#endif
