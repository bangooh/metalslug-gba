#ifndef NATIVE_BOSSES_H
#define NATIVE_BOSSES_H
#include "enemies.h"
#include "../generated/boss-programs.h"
typedef int (*BossCallbackFn)(struct WeaponWorld*,struct NativeEntity*,unsigned);
void boss_start(struct EnemyActor*,enum BossAnimation);
void boss_start_entry(struct EnemyActor*,uint16_t);
void boss_defense(struct EnemyActor*,unsigned);
void boss_attack_binding(struct EnemyActor*,unsigned);
int boss_animate(struct WeaponWorld*,struct NativeEntity*,BossCallbackFn);
int boss_fx_animate(struct WeaponWorld*,struct NativeEntity*);
int boss_sprite_admitted(struct WeaponWorld*);
enum BossFxKind {BFX_SMALL,BFX_FIRE,BFX_SMOKE,BFX_BLAST,BFX_M2_DEBRIS,BFX_RANDOM_DEBRIS,
 BFX_DEBRIS0,BFX_DEBRIS1,BFX_DEBRIS2,BFX_DEBRIS3,BFX_DEBRIS4,BFX_DEBRIS5,BFX_DEBRIS6,BFX_DEBRIS7,
 BFX_DEBRIS8,BFX_DEBRIS9,BFX_DEBRIS10,BFX_METAL,BFX_SURFACE,BFX_SURFACE20,BFX_SURFACE40,
 BFX_WRECK0,BFX_WRECK1,BFX_WRECK2,BFX_DAMAGE_SMOKE,BFX_PUFF_SMOKE,BFX_PUFF_FIRE,BFX_METAL_HEAVY,BFX_DAMAGE_SMOKE_BODY,BFX_CRASH_FRAGMENT,BFX_BLAST_B,BFX_FIRE_BLUE,BFX_SMOKE_BLUE,BFX_DEBRIS_AD_A,BFX_DEBRIS_AD_B,BFX_DEBRIS77_A,BFX_DEBRIS77_B,BFX_SPARK_DRIFT,BFX_SPARK_SCATTER,BFX_CRASH_FRAGMENT_B,BFX_FLASH,BFX_TRAIL77,BFX_DEBRIS28};
struct NativeEntity*boss_fx_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,enum BossFxKind);
struct NativeEntity*boss_fx_pattern(struct WeaponWorld*,struct EntityRef,const struct PlayerTransform*,enum BossPattern);
int boss_fx_transform(struct WeaponWorld*,struct NativeEntity*,enum BossFxKind);
int boss_fx_schedule(struct WeaponWorld*,struct NativeEntity*,enum BossFxKind);
int boss2_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,const uint8_t params[8]);
int crab_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t params[8]);
struct NativeEntity*crab_shard(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,int small);
int gunship_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,const uint8_t params[8]);
struct NativeEntity*gunship_arc(struct WeaponWorld*,struct EntityRef,const struct PlayerTransform*);
int m5_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t params[8]);
int final_boss_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t params[8]);
struct NativeEntity*tank_ally_spawn(struct WeaponWorld*,struct EntityRef,const struct PlayerTransform*);
void gun_platform_projectile(struct WeaponWorld*,struct NativeEntity*,int16_t,int);
void rocket_vehicle_adopt(struct NativeEntity*);
int rocket_vehicle_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
int captor_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
int mortar_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
int walker_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
int ground_nest_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
int gun_platform_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
int bazooka_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8],uint8_t flags);
int tank_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t params[8]);
int carrier_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t params[8]);
struct NativeEntity*boss_radial_blast(struct WeaponWorld*,struct NativeEntity*,unsigned);
enum AirFormationAux {AF_AUX_POSE_C,AF_AUX_ACCEL_SLOW,AF_AUX_ACCEL_FAST,AF_AUX_DEBRIS,AF_AUX_RADIAL};
int air_formation_aux(struct WeaponWorld*,struct NativeEntity*,enum AirFormationAux);
int air_formation_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,const uint8_t[8]);
int boss120_plain_shot(struct WeaponWorld*,struct NativeEntity*);
int boss120_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
int boss117_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,const uint8_t params[8]);
int boss_fx_scatter_selected(struct WeaponWorld*,struct NativeEntity*,unsigned,uint16_t,uint16_t);
int assault76_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
int assault76_spark(struct WeaponWorld*,struct NativeEntity*);
int assault76_spawn_corpses(struct WeaponWorld*,struct NativeEntity*);
int assault76_missile_resume(struct WeaponWorld*,struct NativeEntity*,struct NativeEntity*timer_owner);
struct NativeEntity*assault76_thrown_soldier(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct WeaponActor*);
int assault76_pilot_done(struct WeaponWorld*,struct NativeEntity*);
enum Grunt153Alias {GRUNT_CARRIER_FIXED,GRUNT_RANDOM_FIXED,GRUNT_STANDING_FIXED,GRUNT_RUNNER_FIXED,GRUNT_WALKER_FIXED,GRUNT_SENTRY_FIXED};
enum CapsuleEntry {CAPSULE_ENTRY_FLY,CAPSULE_ENTRY_OPEN,CAPSULE_ENTRY_MARKER_FIXED};
struct NativeEntity*capsule_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,enum CapsuleEntry);
int capsule_spawn_template(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
int capsule_rise(struct WeaponWorld*,struct NativeEntity*);
int banner_spawn_template(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
int banner_drop_item(struct WeaponWorld*,const struct NativeEntity*);
int banner_has_children(struct WeaponWorld*,const struct NativeEntity*);
int scene_prop_spawn_template(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
struct NativeEntity*scene_prop_frog(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*);
int flame_trap_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,const uint8_t[8]);
int scene_rgb_fade_spawn(struct WeaponWorld*,struct EntityRef,int16_t red,int16_t green,int16_t blue,int16_t ticks);
int banner_white_flash(struct WeaponWorld*,struct EntityRef);
int scene_fade_spawn(struct WeaponWorld*,struct EntityRef,uint16_t period,int down,int alternate);
int scene_preserve_screen(struct WeaponWorld*,struct WeaponActor*);
int scene_scroll_tertiary(struct WeaponWorld*,struct WeaponActor*);
int nest185_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
struct NativeEntity*nest185_attached(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,const uint8_t[8]);
struct NativeEntity*nest185_shard(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,unsigned,const uint8_t[8]);
int nest185_fall(struct WeaponWorld*,struct NativeEntity*);
int nest185_shard_land_alias(struct WeaponWorld*,struct NativeEntity*,unsigned);
int grunt153_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
struct NativeEntity*grunt153_spawn_alias(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,enum Grunt153Alias);
int grunt153_has_children(struct WeaponWorld*,const struct NativeEntity*);
int crew23_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
int crew23_has_children(struct WeaponWorld*,const struct NativeEntity*);
int turret142_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
struct NativeEntity*obstacle_a_variant2(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*);
struct NativeEntity*obstacle_flame(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*);
int turret142_has_children(struct WeaponWorld*,const struct NativeEntity*);
int obstacle_has_children(struct WeaponWorld*,const struct NativeEntity*);
int emitter59_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,const uint8_t[8]);
int emitter59_set_release_choice(struct WeaponWorld*,struct NativeEntity*,unsigned);
int assault76_has_children(struct WeaponWorld*,const struct NativeEntity*);
int boss_fx_scatter_graph(struct WeaponWorld*,struct NativeEntity*,enum BossAnimation,uint16_t,uint16_t);
#endif
