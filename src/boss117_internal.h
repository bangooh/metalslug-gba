#ifndef NATIVE_BOSS117_INTERNAL_H
#define NATIVE_BOSS117_INTERNAL_H
#include "bosses.h"
#include "../generated/boss117-programs.h"
enum Boss117State {
 B117_ROOT,B117_SELECT,B117_PREPARE,B117_ACCEL,B117_CRUISE,B117_BRAKE,B117_STOP,B117_DIE,B117_GUARD,
 B117_PLATE,B117_PLATE_BROKEN,B117_CENTRAL,B117_CENTRAL_FIRE,B117_CENTRAL_LOCK,B117_CENTRAL_RECOIL,B117_CENTRAL_RESET,B117_CENTRAL_RETIRE,B117_CENTRAL_WRECK,
 B117_SIDE_A,B117_SIDE_B,B117_SIDE_IDLE,B117_SIDE_OPEN,B117_SIDE_WAIT,B117_SIDE_FIRE,B117_SIDE_COUNTDOWN,B117_SIDE_PREPARE,B117_SIDE_BURST,B117_SIDE_CLOSE,B117_SIDE_RESET,B117_SIDE_DIE,
 B117_TROOP,B117_CENTRAL_ATTACHED,B117_SCATTER,B117_BEAM,B117_PAIRED_BLAST,B117_PROJECTILE,B117_PROJECTILE_FALL,B117_PROJECTILE_IMPACT,B117_BOUNCE_BLAST,B117_SIDE_ATTACHED,B117_PELLET,B117_GROUND_IMPACT,B117_BALLISTIC,B117_GROUND_BLAST,B117_PARACHUTE,B117_DEATH_OVERLAY,B117_DEBRIS,B117_PATTERN,B117_FIRE_SOUND,B117_FIRE,B117_DELETE,B117_DELETE_TREE
};
static inline int16_t b117_word(int n){return (int16_t)(uint16_t)n;}
static inline int16_t b117_sx(struct WeaponWorld*w,const struct WeaponActor*a){return b117_word((a->transform.x>>8)-w->camera_x);}
static inline int16_t b117_sy(struct WeaponWorld*w,const struct WeaponActor*a){return b117_word(transform_y_up(&a->transform)+w->camera_y);}
void b117_screen(struct WeaponWorld*,struct WeaponActor*,int,int);
uint16_t b117_random(struct WeaponWorld*);
int16_t b117_rank(struct WeaponWorld*,struct EnemyActor*,unsigned);
void b117_schedule(struct NativeEntity*,unsigned);
void b117_start(struct EnemyActor*,enum Boss117Animation);
void b117_start_entry(struct EnemyActor*,uint16_t);
void b117_defense(struct EnemyActor*,unsigned);
int b117_animate(struct WeaponWorld*,struct NativeEntity*);
struct EnemyActor*b117_parent(struct WeaponWorld*,struct NativeEntity*);
struct NativeEntity*b117_child(struct WeaponWorld*,struct NativeEntity*,unsigned);
struct NativeEntity*b117_pattern(struct WeaponWorld*,struct NativeEntity*,unsigned);
int b117_patterns(struct WeaponWorld*,struct NativeEntity*,unsigned,unsigned);
int b117_follow(struct WeaponWorld*,struct NativeEntity*,int);
int b117_full_follow(struct WeaponWorld*,struct NativeEntity*);
int b117_relay(struct WeaponWorld*,struct NativeEntity*);
int b117_parent_dead(struct WeaponWorld*,struct NativeEntity*);
void b117_target(struct WeaponWorld*,struct NativeEntity*);
int b117_special_gate(struct WeaponWorld*,struct EnemyActor*);
int b117_aim(struct WeaponWorld*,struct NativeEntity*,int*);
int b117_side_projectiles(struct WeaponWorld*,struct NativeEntity*);
int b117_side_burst(struct WeaponWorld*,struct NativeEntity*);
int b117_troops(struct WeaponWorld*,struct NativeEntity*);
int b117_parts_enter(struct WeaponWorld*,struct NativeEntity*);
int b117_parts_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int b117_effects_enter(struct WeaponWorld*,struct NativeEntity*);
int b117_effects_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int b117_callback(struct WeaponWorld*,struct NativeEntity*,unsigned);
#endif
