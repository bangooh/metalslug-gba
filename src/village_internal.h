#ifndef NATIVE_VILLAGE_INTERNAL_H
#define NATIVE_VILLAGE_INTERNAL_H
#include "village.h"
#include "items.h"
#include "mission_events.h"
#include "scene_runtime.h"
#include "../generated/village-programs.h"
enum VillageState {VG_STATIC,VG_GROUND_STATIC,VG_LAMP,VG_LAMP_IDLE,VG_LAMP_HIT,VG_SENSOR,VG_SENSOR_IDLE,VG_SENSOR_ACTIVE,VG_BREAK,VG_SIGN,VG_SIGN_BREAK,VG_SIGN_WRECK,VG_WALL,VG_WALL2,VG_LARGE,VG_LARGE_WRECK,VG_EXPLOSIVE,VG_EXPLOSIVE_WRECK,VG_HOUSE,VG_HUT,VG_TOWER,VG_TOWER2,VG_TOWER_WRECK,VG_GATE,VG_GATE2,VG_GATE_WRECK,VG_SWING,VG_SWING_CHILD,VG_MARKER,VG_BLINK_A,VG_BLINK_B,VG_PATTERN,VG_ONE_SHOT,VG_LAMP_SPARK,VG_CHUNK,VG_FLYING,VG_DROP,VG_DEBRIS,VG_DELETE};
static inline int16_t vg_word(int x){return (int16_t)(uint16_t)x;}
static inline int16_t vg_sx(struct WeaponWorld*w,const struct WeaponActor*a){return vg_word((a->transform.x>>8)-w->camera_x);}
void vg_schedule(struct NativeEntity*,unsigned);
void vg_start(struct EnemyActor*,unsigned);
void vg_start_entry(struct EnemyActor*,unsigned);
void vg_defense(struct EnemyActor*,unsigned);
void vg_attack(struct EnemyActor*,unsigned);
void vg_reset(struct WeaponActor*);
int vg_animate(struct WeaponWorld*,struct NativeEntity*);
int vg_patch(struct WeaponWorld*,unsigned);
int vg_cull(struct WeaponWorld*,struct NativeEntity*);
int vg_hurt(struct WeaponWorld*,struct WeaponActor*);
int vg_score(struct WeaponWorld*,struct WeaponActor*,uint32_t);
int vg_cycle(struct WeaponWorld*,uint16_t,uint16_t,uint16_t);
uint16_t vg_random(struct WeaponWorld*);
uint16_t vg_masked(struct WeaponWorld*,uint16_t);
uint16_t vg_bounded(struct WeaponWorld*,uint16_t);
struct NativeEntity*vg_child(struct WeaponWorld*,struct NativeEntity*,unsigned,enum EntityInsert);
struct EnemyActor*vg_parent(struct WeaponWorld*,struct NativeEntity*);
struct NativeEntity*vg_pattern(struct WeaponWorld*,struct NativeEntity*,unsigned);
int vg_effects_enter(struct WeaponWorld*,struct NativeEntity*);
int vg_effects_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int vg_attacker_velocity(struct WeaponWorld*,const struct WeaponActor*,int16_t*,int16_t*);
#endif
