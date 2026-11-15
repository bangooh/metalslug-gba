#ifndef NATIVE_M5_INTERNAL_H
#define NATIVE_M5_INTERNAL_H
#include "bosses.h"
#include "../generated/m5-programs.h"
enum M5State {M5_ROOT,M5_INTRO_LEFT,M5_INTRO_RIGHT,M5_ENTRANCE,M5_WAIT,M5_MUSIC,
 M5_IDLE,M5_PICK,M5_PICK_SPECIAL,M5_HOVER,M5_ATTACK_A,M5_HOLD_A,M5_END_A,
 M5_CHARGE,M5_BARRAGE,M5_BARRAGE_END,M5_ATTACK_B,M5_LOOP_B,M5_END_B,M5_ATTACK_C,M5_LOOP_C,
 M5_DIE,M5_EXPLODE,M5_PART,M5_ROTOR,M5_ROTOR_A,M5_ROTOR_B,
 M5_BOMB,M5_BOMB_FALL,M5_BOMB_BOUNCE,M5_MISSILE,M5_HOME,M5_SHELL,M5_SHELL_FALL,M5_PROJ_EXPLODE,
 M5_FX_A,M5_FX_B,M5_SPARK_BURST,M5_SPARK,M5_SPARK_END,M5_MUZZLE,M5_MUZZLE_END,
 M5_SOLDIER,M5_SOLDIER_WALK,M5_SOLDIER_ACQUIRE,M5_SOLDIER_TURN,M5_SOLDIER_AIM,M5_SOLDIER_WAIT,M5_CREW_ENTER,M5_CREW_RAISE,
 M5_GRENADIER,M5_GRENADE_ACQUIRE,M5_GRENADE_IDLE,M5_GRENADE_COUNT,M5_GRENADE_THROW,M5_GRENADE_HURT,
 M5_EJECT,M5_EJECT_BOUNCE,M5_EJECT_SETTLED,M5_EJECT_REST,M5_FIRE,
 M5_FX_8770,M5_FX_87AE,M5_WIND,M5_SMOKE,M5_SMOKE_SYNC,M5_ROCKET,M5_ROCKET_JITTER,M5_ROCKET_GROUND,
 M5_SHOT_FLASH,M5_SHOT,M5_SHOT_HIT,M5_SHOT_SMOKE,M5_EJECT_PIECE,M5_PIECE_FIRE,M5_DELETE,M5_DELETE_TREE};
static inline int16_t m5_word(int x){return (int16_t)(uint16_t)x;}
static inline int16_t m5_sx(struct WeaponWorld*w,const struct WeaponActor*a){return m5_word((a->transform.x>>8)-w->camera_x);}
static inline int16_t m5_sy(struct WeaponWorld*w,const struct WeaponActor*a){return m5_word(transform_y_up(&a->transform)+w->camera_y);}
static inline void m5_screen(struct WeaponWorld*w,struct WeaponActor*a,int16_t x,int16_t y){transform_integer_offset(&a->transform,m5_word(x-m5_sx(w,a)),m5_word(y-m5_sy(w,a)));}
uint16_t m5_random(struct WeaponWorld*);
int16_t m5_rank_value(struct WeaponWorld*,struct EnemyActor*,unsigned);
void m5_start(struct EnemyActor*,enum M5Animation);
void m5_start_entry(struct EnemyActor*,uint16_t);
int m5_animate(struct WeaponWorld*,struct NativeEntity*);
void m5_defense(struct EnemyActor*,unsigned);
struct EnemyActor*m5_parent(struct WeaponWorld*,struct NativeEntity*);
struct NativeEntity*m5_child(struct WeaponWorld*,struct NativeEntity*,unsigned,enum EntityInsert);
void m5_follow(struct EnemyActor*,const struct EnemyActor*,int full,int facing,int dx,int dy,int priority);
void m5_reset(struct WeaponActor*);
int m5_sound(struct WeaponWorld*,unsigned);
int m5_random_target(struct WeaponWorld*);
int16_t m5_player_x(struct WeaponWorld*,unsigned);
int16_t m5_player_y(struct WeaponWorld*,unsigned);
int m5_hit_sound(struct WeaponWorld*,struct WeaponActor*);
int m5_children_enter(struct WeaponWorld*,struct NativeEntity*);
void m5_children_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int m5_emit(struct WeaponWorld*,struct NativeEntity*,unsigned);
int m5_stage_spawn(struct WeaponWorld*,struct NativeEntity*);
struct NativeEntity*m5_turret_spawn(struct WeaponWorld*,struct NativeEntity*);
void m5_turret_release(struct NativeEntity*);
#endif
