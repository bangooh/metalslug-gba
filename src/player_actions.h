#ifndef NATIVE_PLAYER_ACTIONS_H
#define NATIVE_PLAYER_ACTIONS_H
#include "animation.h"
#include "hitbox.h"
#include "mission.h"

/* Original weapon-table order, established from projectile art/constructors.
 * Several decompiler labels and the previous port's names disagree with it. */
enum PlayerWeapon {WEAPON_PISTOL,WEAPON_SHOTGUN,WEAPON_FLAME,WEAPON_ROCKET,WEAPON_HMG,WEAPON_COUNT};
enum PlayerEmissionKind {
 EMIT_PISTOL,EMIT_SHOTGUN,EMIT_FLAME,EMIT_ROCKET,EMIT_HMG,
 EMIT_GRENADE_HEAVY,EMIT_ROCKET_CASING,EMIT_ROCKET_CASING_UP,
 EMIT_SCENE3_DEBRIS,EMIT_BODY_FX,
 EMIT_GRENADE_DOWN,EMIT_GRENADE_BOUNCE,EMIT_GRENADE_ARC_HIGH,EMIT_GRENADE_ARC_LOW
};
struct PlayerTransform {
 int32_t x,y; /* Q8 source pixels; world Y points down. */
 uint16_t priority;
 uint8_t facing,display_group;
};
/* An inverted Q8 axis must be inverted BEFORE extracting its source integer
 * word. 512-(world_y>>8) is off by one whenever Y has a fraction. */
static inline int16_t transform_y_up(const struct PlayerTransform*t){return (int16_t)((131072-t->y)>>8);}
static inline int16_t transform_world_y(const struct PlayerTransform*t){return (int16_t)(512-transform_y_up(t));}
static inline void transform_integer_copy(struct PlayerTransform*to,const struct PlayerTransform*from){
 to->x=(int16_t)(from->x>>8)*256;to->y=(512-(int32_t)transform_y_up(from))*256;
}
static inline void transform_integer_offset(struct PlayerTransform*t,int dx,int dy_up){
 int32_t up=131072-t->y;
 t->x=(int16_t)((t->x>>8)+dx)*256+((uint32_t)t->x&255);
 t->y=131072-((int16_t)((up>>8)+dy_up)*256+(int32_t)((uint32_t)up&255));
}
struct PlayerActionLayer {
 struct Animation animation;
 struct PlayerTransform transform;
 struct HitActor hit;
 const struct HitAttack *attack;
 const struct HitDefense *defense;
 uint8_t action_flags,shot_phase,local_weapon,owner;
 uint8_t damage_owner,damage_timer,display_flags,movement_flags;
 uint16_t aim;
 uint8_t melee_selector,lifecycle_flags; /* Source $5B; not collision flags $13. */
 uint16_t palette_base,palette_alternate,palette_impact,palette;
 uint8_t hit_palette; /* Fixed RGB slot;0 means original1C=FFFF (none). */
};
struct PlayerBodyState {
 struct PlayerActionLayer actor;
 uint16_t ammo;
 /* Original identity and control slot: 0 Marco/P1, 1 Tarma/P2. */
 uint8_t grenades,burst_remaining,character,pad;
 uint8_t aim_direction,direction_change,arm_busy,pad2;
 int16_t arm_offset_x,arm_offset_y_up;
 uint8_t action,arm_selection,animation_changed,palette_signal;
 uint8_t active_grenades,active_shots,pad3[2];
 int16_t vx,vy,ax,ay,ground_slope,ground_tag;
 uint8_t collision_flags,platform_ids,status_flags,crouch_flags;
 uint8_t jump_hold,air_phase,ring_key;
 int8_t previous_direction;
 uint8_t body_state,state_enter,ground_left,special_idle;
 uint8_t life_phase,life_timer,death_class,spawn_mode;
 uint8_t life_retired,role,body_contact,pad4; /* 0 none, 1 $32500, 2 $32598. */
};
/* Typed object-constructor arguments. No original code pointer or RAM image. */
struct PlayerEmission {
 struct PlayerTransform transform;
 enum PlayerEmissionKind kind;
 uint8_t direction,mirror,phase;
 int8_t spread;
 uint8_t owner,damage_owner,pad[2];
};
struct PlayerActionContext {
 struct PlayerBodyState *body;
 struct PlayerActionLayer *layer;
 struct SourceRng *rng;
 void *world;
 int (*emit)(void*,const struct PlayerEmission*);
 int (*sound)(void*,uint16_t,int stop);
 int (*attack)(void*,struct HitActor*,const struct HitAttack*);
 uint8_t scene,fault;
};
/* All 52 animation callbacks and all 4 fields in the complete player graph. */
int player_action_callback(struct PlayerActionContext*,unsigned,struct Animation*);
int player_action_begin(struct PlayerActionContext*,unsigned table);
int player_action_step(struct PlayerActionContext*);
/* $029404: called once by the object scheduler, not once per drawing. */
void player_action_timers(struct PlayerActionLayer*);
#endif
