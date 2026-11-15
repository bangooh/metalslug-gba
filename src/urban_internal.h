#ifndef NATIVE_URBAN_INTERNAL_H
#define NATIVE_URBAN_INTERNAL_H
#include "urban.h"
#include "opening_internal.h"
#include "../generated/urban-programs.h"
enum UrbanState {UR_BUILDING,UR_BUILDING2,UR_BUILDING_WRECK,UR_COLUMN,UR_WALL_CHILD,UR_WALL,UR_WALL2,UR_WALL_WRECK,UR_COMPOUND,UR_COMPOUND_IDLE,UR_COMPOUND_WAIT,UR_COMPOUND_DRAIN,UR_CAPTOR_TRIGGER,UR_NEON_WAIT,UR_NEON,UR_NEON_WRECK,UR_STALL,UR_STALL_WRECK,UR_FRAGILE,UR_FRAGILE_BREAK,UR_FRAGILE_WRECK,UR_BREAKABLE,UR_HIT_STAGE,UR_SMALL,UR_SMALL_WRECK,UR_RANKED,UR_MULTI,UR_MULTI2,UR_MULTI3,UR_MULTI_WRECK,UR_BLOCKER,UR_BLINK_A,UR_BLINK_B,UR_PATTERN,UR_DEBRIS};
void ur_schedule(struct NativeEntity*,unsigned);
void ur_start(struct EnemyActor*,unsigned);
void ur_start_entry(struct EnemyActor*,uint16_t);
void ur_defense(struct EnemyActor*,unsigned);
int ur_animate(struct WeaponWorld*,struct NativeEntity*);
struct NativeEntity*ur_allocate(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,unsigned);
struct NativeEntity*ur_pattern(struct WeaponWorld*,struct NativeEntity*,unsigned);
int ur_fx_enter(struct WeaponWorld*,struct NativeEntity*);
int ur_fx_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
#endif
