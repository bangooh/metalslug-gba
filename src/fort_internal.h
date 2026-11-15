#ifndef NATIVE_FORT_INTERNAL_H
#define NATIVE_FORT_INTERNAL_H
#include "fort.h"
#include "items.h"
#include "mission_events.h"
#include "../generated/fort-programs.h"
enum FortState {FT_ROOT,FT_ROOT_DESTROY,FT_ROOT_END,FT_PILL,FT_PILL1,FT_PILL2,FT_PILL_CRITICAL,FT_PILL_COLLAPSE,FT_PILL_COLLAPSED,FT_COLLAPSE,FT_CRATE,FT_CRATE_BURST,
 FT_HATCH,FT_HATCH_OPEN,FT_HATCH_WATCH,FT_HATCH_HOLD,FT_HATCH_CLOSE,FT_HATCH_FINAL_OPEN,FT_HATCH_FINAL,FT_PROP,FT_SIGN,FT_WALL,FT_WALL_PART,FT_QUAD,FT_QUAD_PART,
 FT_CAR,FT_CAR_ACTIVE,FT_CAR_HIT1,FT_CAR_HIT2,FT_CAR_WRECK,FT_CAR_BLAST,FT_TURRET,
 FT_HELI,FT_HELI_ENTER,FT_HELI_ACTIVE,FT_HELI_HIT1,FT_HELI_HIT2,FT_HELI_WRECK,FT_LADDER,
 FT_BARRIER,FT_BARRIER_OPEN,FT_BARRIER_ACTIVE,FT_BARRIER_HIT1,FT_BARRIER_HIT2,FT_BARRIER_HIT3,FT_BARRIER_WRECK,
 FT_PATTERN,FT_CHUNK,FT_DEBRIS,FT_BLAST,FT_DELETE};
static inline int16_t ft_word(int x){return (int16_t)(uint16_t)x;}
static inline int16_t ft_sx(struct WeaponWorld*w,const struct WeaponActor*a){return ft_word((a->transform.x>>8)-w->camera_x);}
void ft_schedule(struct NativeEntity*,unsigned);
void ft_start(struct EnemyActor*,enum FortAnimation);
void ft_start_entry(struct EnemyActor*,uint16_t);
void ft_defense(struct EnemyActor*,unsigned);
void ft_attack(struct EnemyActor*,unsigned);
int ft_animate(struct WeaponWorld*,struct NativeEntity*);
int ft_patch(struct WeaponWorld*,unsigned);
int ft_stamp(struct WeaponWorld*,struct NativeEntity*,unsigned);
int ft_hurt(struct WeaponWorld*,struct WeaponActor*,int unconditional);
int ft_score(struct WeaponWorld*,struct WeaponActor*,uint32_t);
int ft_cull(struct WeaponWorld*,struct NativeEntity*);
int ft_watch(struct WeaponWorld*,struct NativeEntity*,uint32_t);
int ft_children(struct WeaponWorld*,struct NativeEntity*);
void ft_reset(struct WeaponActor*);
uint16_t ft_random(struct WeaponWorld*);
uint16_t ft_bounded(struct WeaponWorld*,uint16_t);
uint16_t ft_masked(struct WeaponWorld*,uint16_t);
struct EnemyActor*ft_parent(struct WeaponWorld*,struct NativeEntity*);
struct NativeEntity*ft_child(struct WeaponWorld*,struct NativeEntity*,unsigned,enum EntityInsert);
struct NativeEntity*ft_pattern(struct WeaponWorld*,struct NativeEntity*,unsigned);
int ft_structures_enter(struct WeaponWorld*,struct NativeEntity*);
int ft_structures_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int ft_vehicles_enter(struct WeaponWorld*,struct NativeEntity*);
int ft_vehicles_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int ft_effects_enter(struct WeaponWorld*,struct NativeEntity*);
int ft_effects_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
#endif
