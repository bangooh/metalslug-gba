#ifndef NATIVE_AIRSHIP_INTERNAL_H
#define NATIVE_AIRSHIP_INTERNAL_H
#include "airship.h"
#include "items.h"
#include "mission_events.h"
#include "../generated/airship-programs.h"
enum AirshipState {AS_PROP,AS_CAMP,AS_TENT,AS_TENT_WRECK,AS_BUNKER,AS_DEPOT,AS_DEPOT_BLAST,AS_DEPOT_END,AS_CRATE,AS_CRATE_WRECK,AS_SOLID,AS_BARRELS,AS_BARREL_WAIT,AS_BARREL,AS_ROCKRAIN,AS_ROCKRAIN_ACTIVE,AS_ROCK,
 AS_TOWER,AS_TOWER_PATCH0,AS_TOWER_WAIT,AS_TOWER_ACTIVE,AS_TOWER_PATCH1,AS_TOWER_MID,AS_TOWER_PATCH2,AS_TOWER_FINAL,AS_TOWER_DESTROY,AS_PORT_WAIT,AS_PORT,AS_PORT_SECOND,AS_PORT_WRECK,AS_TOWER_BLAST,AS_TOWER_BLAST_END,AS_SMOKE,AS_SMOKE_BURST,AS_SMOKE_FALL,
 AS_BODY,AS_BODY_INIT,AS_BODY_ENTER,AS_BODY_LANDED,AS_BODY_HOVER,AS_BODY_LOWER,AS_BODY_DEPART,AS_BODY_CHILD,AS_GROUND,AS_GROUND_CHILD,AS_PLAYER_TRACK,AS_GUN,AS_GUN_IDLE,AS_GUN_ACTIVE,AS_GUN_COOLDOWN,AS_OVERLAY,AS_CASING,AS_CASING_REST,AS_CASING_FREEZE,AS_CROUCH_PAIR,AS_CROUCH,
 AS_PATTERN,AS_DEBRIS,AS_IMPACT,AS_SPLASH,AS_SPARK_WAIT,AS_SPARK,AS_THROWN,AS_DROP,AS_RICOCHET,AS_RICOCHET_IMPACT,AS_DELETE_TREE,AS_DELETE};
static inline int16_t as_word(int x){return (int16_t)(uint16_t)x;}
static inline int16_t as_sx(struct WeaponWorld*w,const struct WeaponActor*a){return as_word((a->transform.x>>8)-w->camera_x);}
static inline int16_t as_sy(struct WeaponWorld*w,const struct WeaponActor*a){return as_word(transform_y_up(&a->transform)+w->camera_y);}
void as_screen(struct WeaponWorld*,struct WeaponActor*,int16_t,int16_t);
void as_schedule(struct NativeEntity*,unsigned);
void as_start(struct EnemyActor*,enum AirshipAnimation);
void as_start_entry(struct EnemyActor*,uint16_t);
void as_defense(struct EnemyActor*,unsigned);
void as_attack(struct EnemyActor*,unsigned);
int as_animate(struct WeaponWorld*,struct NativeEntity*);
int as_lamp(struct WeaponWorld*,struct EnemyActor*,int three,int on);
int as_patch(struct WeaponWorld*,unsigned);
int as_stamp(struct WeaponWorld*,struct NativeEntity*,unsigned);
int as_hurt(struct WeaponWorld*,struct WeaponActor*);
int as_score(struct WeaponWorld*,struct WeaponActor*,uint32_t);
int as_cull(struct WeaponWorld*,struct NativeEntity*);
int as_watch(struct WeaponWorld*,struct NativeEntity*,uint32_t);
void as_reset(struct WeaponActor*);
uint16_t as_random(struct WeaponWorld*);
uint16_t as_bounded(struct WeaponWorld*,uint16_t);
struct EnemyActor*as_parent(struct WeaponWorld*,struct NativeEntity*);
struct NativeEntity*as_child(struct WeaponWorld*,struct NativeEntity*,unsigned,enum EntityInsert,int,int);
struct NativeEntity*as_pattern(struct WeaponWorld*,struct NativeEntity*,unsigned);
int as_blast(struct WeaponWorld*,struct NativeEntity*,int,int);
int as_structures_enter(struct WeaponWorld*,struct NativeEntity*);
int as_structures_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int as_body_enter(struct WeaponWorld*,struct NativeEntity*);
int as_body_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int as_effects_enter(struct WeaponWorld*,struct NativeEntity*);
int as_effects_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
#endif
