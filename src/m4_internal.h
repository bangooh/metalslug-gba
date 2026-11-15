#ifndef NATIVE_M4_INTERNAL_H
#define NATIVE_M4_INTERNAL_H
#include "m4.h"
#include "mission_events.h"
#include "../generated/m4-programs.h"
enum M4State {M4_BODY,M4_APPROACH,M4_FIGHT,M4_WRECK,M4_TURRET,M4_ACTIVE,M4_DEAD,M4_MOUNT,M4_WHEEL,M4_DROPPER,M4_PUFF,
 M4_RAIL_ROW,M4_RAIL,M4_WATER_PAIR,M4_WATER,M4_SPLASH,M4_DEBRIS_NEW,M4_DEBRIS,M4_DEBRIS_HIT,M4_FLOOR,M4_FLOOR_STEP,M4_PATTERN,M4_DELETE};
static inline int16_t m4_word(int n){return (int16_t)(uint16_t)n;}
static inline int16_t m4_sx(struct WeaponWorld*w,const struct WeaponActor*a){return m4_word((a->transform.x>>8)-w->camera_x);}
static inline int16_t m4_sy(struct WeaponWorld*w,const struct WeaponActor*a){return m4_word(transform_y_up(&a->transform)+w->camera_y);}
void m4_reset(struct WeaponActor*);
void m4_screen(struct WeaponWorld*,struct WeaponActor*,int16_t,int16_t);
int m4_project(struct WeaponWorld*,struct WeaponActor*,int16_t,int16_t);
int m4_secondary_y(struct WeaponWorld*,struct WeaponActor*,int16_t*);
int16_t m4_rank_value(struct WeaponWorld*,struct EnemyActor*,unsigned);
void m4_start(struct EnemyActor*,enum M4Animation);
void m4_start_entry(struct EnemyActor*,uint16_t);
void m4_defense(struct EnemyActor*,unsigned);
void m4_attack(struct EnemyActor*,unsigned);
int m4_animate(struct WeaponWorld*,struct NativeEntity*,int free_actor);
int m4_patch(struct WeaponWorld*,unsigned);
int m4_palette(struct WeaponWorld*,int alternate);
int m4_hurt(struct WeaponWorld*,struct WeaponActor*);
int m4_has_children(struct WeaponWorld*,const struct NativeEntity*);
int m4_in_box(struct WeaponWorld*,const struct WeaponActor*,const struct PlayerBodyState*,const int16_t[4]);
int m4_near_box(struct WeaponWorld*,struct WeaponActor*,unsigned);
struct EnemyActor*m4_parent(struct WeaponWorld*,struct NativeEntity*);
struct NativeEntity*m4_child(struct WeaponWorld*,struct NativeEntity*,unsigned,enum EntityInsert);
struct NativeEntity*m4_pattern(struct WeaponWorld*,struct NativeEntity*,unsigned);
int m4_scene_enter(struct WeaponWorld*,struct NativeEntity*);
int m4_scene_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int free96_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
int free104_move(struct WeaponWorld*,struct WeaponActor*,int);
int free104_tail(struct WeaponWorld*,struct NativeEntity*,int);
int free104_prepare(struct WeaponWorld*,struct NativeEntity*);
int free104_shot(struct WeaponWorld*,struct NativeEntity*,unsigned,int);
struct NativeEntity*free104_hat(struct WeaponWorld*,struct NativeEntity*,int);
int free104_level_shot(struct WeaponWorld*,struct NativeEntity*,int high);
int free104_callback(struct WeaponWorld*,struct NativeEntity*,unsigned);
#endif
