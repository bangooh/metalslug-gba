#ifndef NATIVE_PLAY_WORLD_H
#define NATIVE_PLAY_WORLD_H
#include "player_body.h"
#include "hit_scene.h"
#include "target_ring.h"
#include "slug_body.h"
#include "items.h"
#include "interactions.h"
#include "enemies.h"
#include "scene_video.h"
#include "mission_events.h"
#include "results.h"
struct NativePlayWorld;
struct SceneRuntime;
struct NativeSession;
struct NativePlayServices {
 void *context;
 int (*column)(void*,unsigned plane,uint16_t column,uint16_t x,uint16_t y,int hidden);
 int (*write_fixed)(void*,uint8_t column,uint8_t row,uint16_t tile);
 int (*cell)(void*,int x,int y,uint8_t*);
 int (*write_cell)(void*,int x,int y,uint8_t);
 int (*sound)(void*,uint16_t,int stop);
 int (*audio_control)(void*,uint8_t command,uint8_t value);
 /*Source2C30 targets the private palette allocated to this entity. */
 int (*actor_palette_cycle)(void*,uint16_t entity,uint16_t generation,uint16_t first_palette,uint16_t period,uint8_t flags);
 int (*palette_cycle)(void*,uint16_t slot,uint16_t first_palette,uint16_t period,uint8_t flags);
 int (*clear_tile)(void*,unsigned plane,uint16_t column,uint8_t row);
 int (*set_palette)(void*,uint8_t slot,uint16_t source,uint8_t flags);
 int (*write_tile)(void*,unsigned plane,uint16_t column,uint8_t row,uint16_t tile,uint16_t attributes);
 HitDamageSink damage;
};
struct NativePlayWorld {
 /* Entity callbacks for projectiles already take WeaponWorld. Keep it at
  * offset zero and pass its address to the common scheduler. */
 struct WeaponWorld weapons;
 struct EntityPool entities;
 struct TerrainWorld terrain;
 struct SceneVideoWorld video;
 struct HitScene hits;
 struct TargetRing targets;
 struct SlugSharedState slug;
 struct ItemWorld items;
 struct ResultWorld results;
 struct InteractionWorld interactions;
 struct EnemyWorld enemies;
 struct MissionEventWorld events;
 struct SourceRng rng;
 struct NativePlayServices services;
 struct SceneRuntime *scene_runtime;
 struct NativeSession *session;
 const struct AnimationOutput *drawing_output;
 struct EntityRef player_groups[2],arms[2];
 struct PlayerSourceInput inputs[2],raw_inputs[2];
 uint8_t raw_inputs_valid,input_freeze,input_freeze_bypass;
 uint8_t control_mode[2],invincible[2],pause_motion,fault;
 uint8_t action_fault[2],death_routes[2],blood_enabled,slug_invulnerability;
 uint8_t slug_riders_enabled,slug_layout,slug_debug_flags,slug_player_query_enabled,slug_narrow_bounds;
 uint32_t ticks;
 uint8_t cheat_step[2],cheat_timer[2]; /* Konami code -> Slug air drop (port extra). */
};
/* This is the new simulation wiring, not the old released game.c loop.
 * The caller must supply scene/audio services; absent services
 * fail when used. Set enemies.mission_time, scene/camera and source input before
 * ticking; a zero timer has the original time-up/death semantics.
 * Retired player storage remains available until explicit respawn. The scene
 * owns lives/credits and placement. Register attachable actors in targets
 * from their entity ticks after target_ring_frame, not before play_world_tick.
 * The fixed Slug slot, hit registration and body/child/contact dispatch are
 * connected. Scene placement must enqueue an explicit Slug request.
 * Damage score dispatch is built in; services.damage is an optional observer.
 * Set enemies.damage_enabled ($106ED3), grenade_mode and clear_ordnance from
 * scene state. Ordinary and boss-specific Slug construction, item/score actors,
 * POWs, crates, guns, climb publishers, guards, chargers, skirmishers, ordinary
 * soldiers 1..22 with Slug anchors/climb/tackles, attack-exclusion zones,
 * ordnance, human death handlers and Allen 328 with sensors/MG/grenade/death
 * are implemented. Allen requires the secondary camera input. NativeSession
 * supplies rank/layout/time/lives/HUD; direct world callers must supply these
 * explicitly. Sprite budget/secondary view input belongs to weapons.
 * Physical main, startup/presentation and all-pose rendering remain separate. */
int play_world_init(struct NativePlayWorld*,const struct NativePlayServices*,const struct SourceRng*);
int play_world_add_player(struct NativePlayWorld*,unsigned slot,const struct PlayerTransform*,enum PlayerState);
int play_world_tick(struct NativePlayWorld*);
/* $02AE50 (wait while busy), $02AEA4 (discard while busy). Scene placement
 * supplies world coordinates; request is a scheduled enemy-group child.
 * The five variants clamp any byte >=5 to 4 as in the original table. */
int play_world_request_slug(struct NativePlayWorld*,const struct PlayerTransform*,uint8_t variant,int discard_busy);
/* Port extra: drop a Slug from the top of the screen above player n, as the
 * tutorial and mission air drops do. 0 when a Slug is already present. */
int play_world_cheat_slug(struct NativePlayWorld*,unsigned player);
/* Port extra: feed one frame of button edges; runs the drop on UUDDLRLR B A. */
void play_world_cheat_input(struct NativePlayWorld*,unsigned player,uint8_t edges);
enum BossSlugEntry {BOSS_SLUG_ARENA,BOSS_SLUG_DROP,BOSS_SLUG_DROP_ALTERNATE};
/* Source fixed-slot handler assignment by a scene. Returns 0 when occupied;
 * screen coordinates/palettes belong to the original constructor. */
int play_world_begin_boss_slug(struct NativePlayWorld*,enum BossSlugEntry);
int play_world_weapon_pickup(struct NativePlayWorld*,unsigned slot,unsigned weapon,unsigned quantity);
/* Original $033742 mode, with placement resolved by the scene controller.
 * Does not invent lives/credits/continue policy. Reuses a retired player slot. */
int play_world_spawn_player(struct NativePlayWorld*,unsigned slot,const struct PlayerTransform*,unsigned mode);
/* $032AC8 placement and source-selected entry mode from $04CAC4. */
int play_world_spawn_player_from_scene(struct NativePlayWorld*,unsigned slot);
/* One native scene dispatch for all implemented template families. Unknown
 * templates return 0, so the scene can stop with the original event location. */
int play_world_spawn_airship_scene_record(struct NativePlayWorld*,unsigned index);
int play_world_spawn_fort_scene_record(struct NativePlayWorld*,unsigned index);
int play_world_spawn_siege_scene_record(struct NativePlayWorld*,unsigned index);
/* Start the selected scene's original event stream (overseas ending sequence), watcher8C864 and auto-input79298. SceneRuntime must be attached
 * before these actors tick. Outer boot transitions remain separate. */
int play_world_start_events(struct NativePlayWorld*,unsigned scene);
int play_world_spawn_template(struct NativePlayWorld*,uint16_t,const struct PlayerTransform*,const uint8_t params[8],uint8_t source_flags);
#endif
