#ifndef NATIVE_WEAPON_ANIMATION_H
#define NATIVE_WEAPON_ANIMATION_H
#include "player_actions.h"
#include "../generated/weapon-programs.h"
struct WeaponActor {
 struct Animation animation;
 struct PlayerTransform transform;
 struct HitActor hit;
 const struct HitAttackSet *attack;
 const struct HitDefense *defense;
 uint16_t defense_count;
 int16_t vx,vy,ax,ay; /* Y velocity/acceleration point up. */
 int16_t effect_x,effect_y_up;
 int16_t attached_offset;
 uint16_t trail_drawing,trail_angle,palette;
 uint8_t active,owner,damage_owner,kind;
 uint8_t damage_timer,display_flags,movement_flags,collision_flags;
 /* Native projectile state, with no original function/ROM pointers. */
 uint16_t timer,bounces;
 int16_t origin_y_up;
 int16_t ground_slope,ground_tag;
 uint8_t state,variant,mirror,phase,ttl,raw_surface,surface_kind,program_kind;
 int8_t spread;
 uint8_t platform_ids;
 int8_t previous_direction;
 uint8_t parachute_slow;
 uint8_t score_owner; /* $6E: credited player; distinct from projectile owner. */
 uint8_t hit_palette; /* Fixed74..93 slot;0 means original1C=FFFF. */
 uint8_t fixed_palette; /* Nonzero: base palette is a fixed slot, e.g.wheelFF. */
};
enum WeaponEffect {WEFFECT_PISTOL_CASING,WEFFECT_PISTOL_CASING_UP,WEFFECT_ROCKET_CASING,WEFFECT_ROCKET_CASING_UP,
 WEFFECT_SCENE3_DEBRIS,WEFFECT_BODY,WEFFECT_BLOOD,WEFFECT_BLAST_A,WEFFECT_BLAST_B,WEFFECT_BLAST_C};
struct WeaponAnimationContext {
 struct WeaponActor *actor;
 void *world;
 int (*attack)(void*,struct HitActor*,const struct HitAttack*);
 int (*sound)(void*,uint16_t,int stop);
 int (*effect)(void*,enum WeaponEffect,const struct PlayerTransform*);
 uint8_t fault;
};
int weapon_animation_step(struct WeaponAnimationContext*);
int weapon_effect_init(struct WeaponAnimationContext*,enum WeaponEffect,const struct PlayerTransform*);
void weapon_effect_tick(struct WeaponAnimationContext*);
#endif
