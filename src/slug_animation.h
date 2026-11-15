#ifndef NATIVE_SLUG_ANIMATION_H
#define NATIVE_SLUG_ANIMATION_H
#include "slug_support.h"
#include "../generated/slug-programs.h"
struct SlugAnimationContext {
 struct SlugSharedState *slug;
 struct WeaponWorld *weapons;
 struct TerrainWorld *terrain;
 struct EntityRef entity;
};
/* Complete drawing-owner implementation; does not dispatch vehicle states. */
int slug_animation_step(struct SlugAnimationContext*);
int slug_animation_select(struct SlugSharedState*,enum SlugAnimationTableId,unsigned choice,uint8_t frame);
int slug_animation_turn(struct SlugSharedState*,unsigned alternate,unsigned slope,unsigned phase,uint8_t frame);
int slug_set_defense(struct SlugSharedState*,unsigned binding);
int slug_set_defense_table(struct SlugSharedState*,enum SlugDefenseTableId,unsigned slope);
int slug_set_attack(struct SlugSharedState*,unsigned binding);
const struct SlugMount* slug_mount(struct SlugSharedState*);
/* $031E5E/$031E76; same ephemeral child as animation dust callbacks. */
int slug_animation_acceleration_dust(struct SlugAnimationContext*,int left);
#endif
