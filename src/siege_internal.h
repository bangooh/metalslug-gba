#ifndef NATIVE_SIEGE_INTERNAL_H
#define NATIVE_SIEGE_INTERNAL_H
#include "siege.h"
#include "items.h"
#include "../generated/siege-programs.h"
enum SiegeState {SG_DRIFT,SG_FLICKER,SG_SINE,SG_GATE,SG_GATE_BROKEN,SG_ARCH,SG_COLUMN,SG_COLUMN_HIT1,SG_COLUMN_HIT2,SG_COLUMN_WRECK,SG_COLUMN_BLAST,
 SG_REACTIVE,SG_REACTIVE_ACTIVE,SG_REACTIVE_CLEAR,SG_SOLID_A,SG_SOLID_B,SG_GUARD_ROOT,SG_GUARD_FOLLOW,SG_GUARD,SG_BOX,SG_BOX_WRECK,
 SG_TILE_A,SG_TILE_B,SG_BOUNCE,SG_BOUNCE_ACTIVE,SG_BOUNCE_HIT,SG_BOUNCE_WRECK,SG_ROW8_ROOT,SG_ROW8_WAIT,SG_ROW8,SG_ROW8_BROKEN,
 SG_SINGLE,SG_SINGLE_HIT,SG_SINGLE_TILE,SG_SINGLE_TILE_BREAK,SG_ROW9_ROOT,SG_ROW9,SG_BREAKABLE,SG_REWARD,SG_REWARD_DECOR,
 SG_FINALE,SG_FINALE_BREAK,SG_FINALE_CLOSE,SG_FINALE_END,SG_GUARDIAN,SG_FLASH,SG_PANEL,SG_PANEL_HIT,SG_PANEL_EMPTY,SG_AUX,
 SG_STEP,SG_STEP_BREAK,SG_STEP_FALL,SG_STEP_SINK,SG_BODY_WAIT,SG_BODY,SG_CREW,SG_CREW_FALL,SG_CREW_LAND,SG_CREW_RISE,SG_CREW_RUN,SG_CREW_EXIT,SG_PARACHUTE,
 SG_PATTERN,SG_CHUNK,SG_FLYING,SG_SHARD,SG_SHARD_LAND,SG_NUKE,SG_NUKE_END,SG_DELETE,SG_DELETE_TREE};
static inline int16_t sg_word(int v){return (int16_t)(uint16_t)v;}
static inline int16_t sg_sx(struct WeaponWorld*w,const struct WeaponActor*a){return sg_word((a->transform.x>>8)-w->camera_x);}
static inline int16_t sg_sy(struct WeaponWorld*w,const struct WeaponActor*a){return sg_word(transform_y_up(&a->transform)+w->camera_y);}
void sg_schedule(struct NativeEntity*,unsigned);
void sg_start(struct EnemyActor*,enum SiegeAnimation);
void sg_start_entry(struct EnemyActor*,uint16_t);
int sg_animate(struct WeaponWorld*,struct NativeEntity*);
void sg_defense(struct EnemyActor*,unsigned);
void sg_attack(struct EnemyActor*,unsigned);
void sg_reset(struct WeaponActor*);
void sg_screen(struct WeaponWorld*,struct WeaponActor*,int,int);
int sg_score(struct WeaponWorld*,struct WeaponActor*,uint32_t);
int sg_hurt(struct WeaponWorld*,struct WeaponActor*,int unconditionally);
int sg_cull(struct WeaponWorld*,struct NativeEntity*,int tree);
uint16_t sg_random(struct WeaponWorld*);
uint16_t sg_bounded(struct WeaponWorld*,uint16_t);
uint16_t sg_masked(struct WeaponWorld*,uint16_t);
struct EnemyActor*sg_parent(struct WeaponWorld*,struct NativeEntity*);
struct NativeEntity*sg_child(struct WeaponWorld*,struct NativeEntity*,unsigned,enum EntityInsert);
struct NativeEntity*sg_pattern(struct WeaponWorld*,struct NativeEntity*,unsigned);
int sg_patch(struct WeaponWorld*,unsigned);
int sg_stamp(struct WeaponWorld*,struct NativeEntity*,unsigned);
int sg_finale_enter(struct WeaponWorld*,struct NativeEntity*);
int sg_finale_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int sg_effects_enter(struct WeaponWorld*,struct NativeEntity*);
int sg_effects_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int sg_spawn_steps(struct WeaponWorld*,struct NativeEntity*);
int sg_spawn_guards(struct WeaponWorld*,struct NativeEntity*);
int sg_spawn_pow(struct WeaponWorld*,struct NativeEntity*);
#endif
