#ifndef NATIVE_OPENING_INTERNAL_H
#define NATIVE_OPENING_INTERNAL_H
#include "opening.h"
#include "village_internal.h"
#include "../generated/opening-programs.h"
enum OpeningState {OP_HUT,OP_HUT_WRECK,OP_ROOF,OP_ROOF_DIE,OP_TOWER,OP_TOP,OP_TOP_WRECK,OP_BASE,OP_BASE_WRECK,OP_FLAG,OP_BUNKER,OP_BUNKER_WRECK,OP_BUNKER_END,OP_BRIDGE,OP_BRIDGE_ACTIVE,OP_PILLAR,OP_NEST,OP_NEST_WAIT,OP_NEST_ACTIVE,OP_NEST_WRECK,OP_SHED,OP_SHED2,OP_DRAIN_SYNC,OP_DRAIN,
 OP_BARRIER,OP_BARRIER_WAIT,OP_BARRIER_ACTIVE,OP_BARRIER_DAMAGED,OP_BARRIER_WRECK,OP_GATE,OP_GATE_WAIT,OP_GATE_ACTIVE,OP_GATE_DAMAGED,OP_GATE_WRECK,OP_FORT,OP_FORT_WAIT,OP_FORT_ACTIVE,OP_FORT_WRECK,OP_GATE_BLOCKER,OP_FORT_BLOCKER,OP_POST,OP_LIGHT,OP_LIGHT_WRECK,OP_GATE_ROOF,OP_GATE_ROOF_WRECK,OP_DOOR,OP_MOUNT,OP_MOUNT_FALL,OP_SIDE,OP_PART,OP_NUKE,OP_NUKE_END,
 OP_BLOCK,OP_BLOCK_WAIT,OP_BLOCK_IDLE,OP_BLOCK_HOLD,OP_BLOCK_FALL,OP_BLOCK_HIT,OP_BLOCK_LAND,OP_BLOCK_BACK,OP_PULLER,OP_PULL_RUN,OP_PULL_ACTION,OP_PULL_END,OP_PULL_ESCAPE,OP_LAND_FX,OP_PATTERN};
static inline int16_t op_word(int n){return (int16_t)(uint16_t)n;}
static inline int16_t op_sx(struct WeaponWorld*w,const struct WeaponActor*a){return op_word((a->transform.x>>8)-w->camera_x);}
static inline int op_sync(struct WeaponWorld*w,struct WeaponActor*a){return interaction_move(w,a,WMOVE_SYNC)>=0;}
int op_copy_sync(struct WeaponWorld*,struct WeaponActor*);
void op_schedule(struct NativeEntity*,unsigned);
void op_start(struct EnemyActor*,unsigned);
void op_start_entry(struct EnemyActor*,uint16_t);
void op_defense(struct EnemyActor*,unsigned);
void op_attack(struct EnemyActor*,unsigned);
int op_animate(struct WeaponWorld*,struct NativeEntity*);
int op_patch(struct WeaponWorld*,unsigned);
int op_stamp(struct WeaponWorld*,struct WeaponActor*,unsigned);
int op_outside(struct WeaponWorld*,struct NativeEntity*);
int op_cull(struct WeaponWorld*,struct NativeEntity*);
int op_hit(struct WeaponWorld*,struct WeaponActor*);
int op_watch(struct WeaponWorld*,struct NativeEntity*,uint32_t);
int op_before_sync_x(struct WeaponWorld*,struct WeaponActor*,int16_t*);
int op_map_position(struct WeaponWorld*,struct WeaponActor*,int16_t,int16_t);
struct EnemyActor*op_parent(struct WeaponWorld*,struct NativeEntity*);
struct NativeEntity*op_allocate(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,unsigned);
struct NativeEntity*op_child(struct WeaponWorld*,struct NativeEntity*,unsigned,int dx,int dy,int synchronize);
struct NativeEntity*op_pattern(struct WeaponWorld*,struct NativeEntity*,unsigned,int dx,int dy);
int op_burst(struct WeaponWorld*,struct NativeEntity*,unsigned,unsigned,unsigned,int dx,int dy);
int op_fort_enter(struct WeaponWorld*,struct NativeEntity*);
int op_fort_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int op_crew_enter(struct WeaponWorld*,struct NativeEntity*);
int op_crew_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
#endif
