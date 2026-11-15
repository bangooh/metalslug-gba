#ifndef NATIVE_WEAPONS_H
#define NATIVE_WEAPONS_H
#include "entities.h"
#include "weapon_math.h"
#include "player_rules.h"
/* Movement policies correspond to $027CEE, $027D50 and $027BC8. The scene
 * collision service owns terrain/platform resolution, never this renderer. */
enum WeaponMove {WMOVE_POINT,WMOVE_FALL,WMOVE_BODY,WMOVE_JUMP,WMOVE_GROUND,WMOVE_SYNC,WMOVE_ITEM,WMOVE_FLAT};
enum PlayerVisualEffect {PVIS_DEATH_SMOKE,PVIS_DEATH_SMOKE_HIGH,PVIS_DEATH_ALT,PVIS_SPLASH,PVIS_RIPPLE,PVIS_BLOOD,
 PVIS_ICON,PVIS_PARACHUTE,PVIS_COUNT};
struct WeaponContact {uint8_t shape,raw,solid,pad;};
/* $05E804 compares FREE source strips after the previous VBlank commit
 * (5C9D6 copies range+0A to108080+6168=10E1E8) with a quarter of
 * last-first. It is not a current-frame map count or a GBA OAM count.
 * This renderer input must be explicitly supplied; zero is not a policy. */
struct WeaponSpriteBudget {uint16_t counter,first,last;uint8_t valid;};
struct WeaponWorld {
 struct SceneRuntime *scene_runtime;
 struct ResultWorld *results;
 struct MissionEventWorld *events;
 struct SceneVideoWorld *video;
 struct ItemWorld *items;
 struct InteractionWorld *interactions;
 struct EnemyWorld *enemies;
 struct EntityPool *entities;
 struct EntityRef projectile_group,enemy_group,prop_group;
 struct PlayerBodyState *players[2];
 struct SourceRng *rng;
 void *scene;
 /* move returns -1 on failure, otherwise the original collision condition.
  * probe supplies $027FAC terrain at the actor's feet; solid is its inverse
  * carry condition. A missed attack is successful processing, not failure. */
 int (*move)(void*,struct WeaponActor*,enum WeaponMove,struct WeaponContact*);
 int (*probe)(void*,const struct WeaponActor*,struct WeaponContact*);
 int (*foot_contact)(void*,struct WeaponActor*); /* $027EBA: -1 fault, 0 air, 1 ground. */
 int (*point_shape)(void*,const struct WeaponActor*,int16_t x,int16_t y_up,uint8_t*);
 int (*attack)(void*,struct HitActor*,const struct HitAttack*,struct HitActor**selected);
 int (*sound)(void*,uint16_t,int stop);
 int (*audio_control)(void*,uint8_t command,uint8_t value);
 int32_t camera_x,camera_y; /* Unscaled source pixels, world coordinates. */
 uint8_t scene_number,spread_mode,special_spread,grenade_body_collision;
 uint8_t shotgun_flash,fault,pad[2];
 uint32_t ticks;
 struct WeaponSpriteBudget sprite_budget;
 /* Raw10E200/206 input for direct turret/debug queries. Player/Slug active
  * snapshots, boot44 and automatic5D674 substitution are separate. */
 struct PlayerSourceInput inputs[2];
 int16_t camera_delta_x;
 uint8_t frame_phase,slug_mount_enabled;
 int8_t sound_reply; /* $106F29: decoded audio reply, NOT a second frame counter. */
 uint8_t sound_reply_valid;
 /* Original secondary camera Y used by $0440BC's special mud strip. */
 int16_t secondary_x_offset,secondary_y_offset;
 uint16_t scroll_seed_latch; /* Source106F5E: scroll velocity override, NOT deltaY. */
 int16_t background_x_offset,background_y_offset; /*106F70/74, independent background pixels. */
 uint8_t background_view_valid;
 uint8_t secondary_view_valid;
 /* $04407A consumes independent $108064/$108066 screen deltas. */
 int16_t tertiary_delta_x,tertiary_delta_y;
 uint8_t tertiary_delta_valid;
 /* Original mission progress/count inputs for source event spawners. */
 uint16_t mission_progress,mission_enemies;
 uint8_t mission_valid,mission_active;
 int (*mission_spawn)(void*,const struct MissionSpawn*);
};
/* Constructors are scheduled, not run recursively from an animation callback.
 * Use the current arm/body ref for source-parented bullets and effects. */
int weapon_emit(struct WeaponWorld*,struct EntityRef emitter,const struct PlayerEmission*);
int weapon_spawn_effect(struct WeaponWorld*,struct EntityRef parent,enum WeaponEffect,
 const struct PlayerTransform*);
int weapon_drop_player_weapon(struct WeaponWorld*,struct EntityRef parent,const struct PlayerBodyState*);
int weapon_spawn_player_effect(struct WeaponWorld*,struct EntityRef parent,enum PlayerVisualEffect);
/* $077AB6: cannon surface follower, selecting the vehicle splash table. */
int weapon_spawn_vehicle_surface(struct WeaponWorld*,struct EntityRef parent);
/* $045FDE -> $0308B0: same bullet lifetime, vehicle palette $01D6. */
int weapon_emit_slug_bullet(struct WeaponWorld*,struct EntityRef,const struct PlayerEmission*);
int weapon_emit_gun_bullet_at(struct WeaponWorld*,struct EntityRef,const struct PlayerTransform*,uint8_t angle);
int weapon_emit_gun_bullet(struct WeaponWorld*,const struct PlayerTransform*,uint8_t angle);
#endif
