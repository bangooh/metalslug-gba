#ifndef NATIVE_TANK_INTERNAL_H
#define NATIVE_TANK_INTERNAL_H
#include "bosses.h"
#include "mission_events.h"
#include "../generated/tank-programs.h"
enum TankState {TK_ROOT,TK_DRIVE,TK_TURN,TK_RETREAT,TK_PAUSE,TK_PHASE2,TK_PHASE3,TK_DIE,TK_SINK,TK_EXPLODE,
 TK_HULL,TK_HULL_DIE,TK_CABIN,TK_LAUNCHER,TK_TURRET,TK_TURRET_FIRE,TK_AIM,TK_RECOIL,TK_TURRET_DIE,TK_WRECK,TK_WRECK_WATCH,TK_WRECK_IDLE,
 TK_GUN,TK_GUN_FIRE,TK_CASING,TK_GROUND,TK_LAUNCH,TK_FLY,TK_HOME,TK_GRAVITY,TK_ROCKET_HIT,TK_FIRE,TK_TRAIL,TK_MISSILE,
 TK_MUZZLE,TK_MUZZLE_B,TK_MUZZLE_C,TK_MUZZLE_D,TK_MUZZLE_END,TK_FRAGMENT,TK_FRAGMENT_SMOKE,TK_SMOKE,TK_SMOKE_HIT,TK_SMOKE_HIT_HIGH,
 TK_CREW,TK_CREW_ZERO,TK_ACQUIRE,TK_CHOOSE,TK_CREW_FIRE,TK_CREW_HURT,TK_CREW_DIE,TK_CREW_FADE,
 TK_WEAPON,TK_WEAPON_IDLE,TK_WEAPON_AIM,TK_WEAPON_FIRE,TK_WEAPON_CENTER,TK_WEAPON_WAIT,TK_WEAPON_FIXED,TK_WEAPON_HURT,TK_WEAPON_FLY,TK_WEAPON_FALL,TK_WEAPON_BLAST,TK_CREW_MUZZLE,TK_CREW_ROCKET,TK_CREW_ROCKET_HIT,
 TK_ALLY,TK_ALLY_ACQUIRE,TK_ALLY_CHOOSE,TK_ALLY_FIRE,TK_ALLY_HURT,TK_ALLY_DIE,TK_ALLY_FADE,
 TK_BZ_A_NEW,TK_BZ_A_IDLE,TK_BZ_A_WALK,TK_BZ_A_STOP,TK_BZ_A_TURN,TK_BZ_A_ALERT,TK_BZ_A_CHOOSE,TK_BZ_A_NEXT,TK_BZ_A_FIRE,TK_BZ_A_RECOVER,
 TK_BZ_B_NEW,TK_BZ_B_WALK,TK_BZ_B_IDLE,TK_BZ_B_ALERT,TK_BZ_B_CHOOSE_MOVE,TK_BZ_B_NEXT_MOVE,TK_BZ_B_FIRE_MOVE,TK_BZ_B_CHOOSE,TK_BZ_B_NEXT,TK_BZ_B_FIRE,TK_BZ_B_RECOVER,
 TK_BZ_DIE,TK_BZ_FADE,TK_BZ_SPAWNER,
 TK_DELETE,TK_DELETE_TREE};
static inline int16_t tk_word(int x){return (int16_t)(uint16_t)x;}
static inline int16_t tk_sx(struct WeaponWorld*w,const struct WeaponActor*a){return tk_word((a->transform.x>>8)-w->camera_x);}
static inline int16_t tk_sy(struct WeaponWorld*w,const struct WeaponActor*a){return tk_word(transform_y_up(&a->transform)+w->camera_y);}
static inline int16_t tk_faced(const struct WeaponActor*a,int x){return tk_word(a->transform.facing&1?-x:x);}
void tk_start(struct EnemyActor*,enum TankAnimation);
void tk_start_entry(struct EnemyActor*,uint16_t);
void tk_defense(struct EnemyActor*,unsigned);
void tk_attack(struct EnemyActor*,unsigned);
int tk_animate(struct WeaponWorld*,struct NativeEntity*);
int tk_sound(struct WeaponWorld*,unsigned);
int tk_hit_sound(struct WeaponWorld*,struct WeaponActor*);
uint16_t tk_random(struct WeaponWorld*);
int16_t tk_rank(struct WeaponWorld*,struct EnemyActor*,unsigned);
struct EnemyActor*tk_parent(struct WeaponWorld*,struct NativeEntity*);
struct NativeEntity*tk_child(struct WeaponWorld*,struct NativeEntity*,unsigned,enum EntityInsert);
int tk_follow(struct WeaponWorld*,struct NativeEntity*,int select);
int tk_settle(struct WeaponWorld*,struct WeaponActor*);
int tk_stamp(struct WeaponWorld*,struct NativeEntity*,unsigned);
int tk_emit(struct WeaponWorld*,struct NativeEntity*,unsigned);
int tk_parts_enter(struct WeaponWorld*,struct NativeEntity*);
int tk_parts_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
void tk_schedule_crew_death(struct NativeEntity*);
int tk_crew_enter(struct WeaponWorld*,struct NativeEntity*);
int tk_crew_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int bz_enter(struct WeaponWorld*,struct NativeEntity*);
int bz_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int tk_watch_stop(const struct NativeEntity*);
int tk_spawn_rocket(struct WeaponWorld*,struct NativeEntity*);
#endif
