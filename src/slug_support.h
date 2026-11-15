#ifndef NATIVE_SLUG_SUPPORT_H
#define NATIVE_SLUG_SUPPORT_H
#include "terrain_motion.h"
#include "player_rules.h"
#include "terrain_contact_actor.h"
/* The fixed Slug slot and three terrain followers use ordinary C fields.
 * This dependency module does not select a vehicle state or spawn a tank.
 * A scene/vehicle controller must initialize and tick the actual actor. */

struct SlugSharedState {
 struct WeaponActor actor;
 struct SlugContact contacts[3]; /* original parent $74=A, $78=B, $7C=C */
 int16_t angle,ground_y_up;
 uint16_t slope_index,damage_cursor,palette_slots[4];
 uint16_t sprite_template,scale_x_y; /* source $1C/$32: renderer metadata */
 uint8_t palette_count; /* $1E is a count, not a fifth palette binding. */
 uint32_t rider_parameter;
 uint8_t present,rider_heartbeat,previous_owner,action_flags,status_flags;
 uint8_t cooldown,cannon_count,ammo,turn_timer,lifecycle_flags;
 uint8_t damage_elapsed,damage_program,sound_state,fault;
 uint16_t mount_index;
 uint8_t mount_valid,accel_timer,frame_index;
 /* Native body controller state. No instruction address/register state. */
 int16_t impulse,body_parameter;
 uint8_t body_state,body_enter,body_phase,body_argument;
 uint8_t destroyed_timer,input_masks[2],body_contact;
};
/* $03060A/$030696; allocation is embedded, tick order remains B,A,C.
 * Initialize immediately after the parent's ground Y is established. */
void slug_chassis_create(struct SlugSharedState*,int alternate,int16_t camera_x,int16_t camera_y);
/* A separately scheduled terrain child uses the same entry/frame rules. */
int slug_contact_tick(struct SlugSharedState*,struct TerrainWorld*,unsigned index);
int slug_contacts_tick(struct SlugSharedState*,struct TerrainWorld*);
int slug_ground_contact(struct SlugSharedState*,struct TerrainWorld*,unsigned *contact_count);
int16_t slug_slope_product(const struct SlugSharedState*,int16_t *angle);
unsigned slug_slope_index(const struct SlugSharedState*);
int slug_update_chassis(struct SlugSharedState*,int preserve_index,unsigned *index);
/* $02A4F0: true only if the angle changed to a non-flat classification. */
int slug_update_angle(struct SlugSharedState*);
void slug_chassis_sync_flags(struct SlugSharedState*);
int slug_chassis_wall(const struct SlugSharedState*);
int slug_chassis_step_mismatch(const struct SlugSharedState*,int16_t camera_y);
int slug_chassis_higher_previous(const struct SlugSharedState*,int16_t camera_y,int16_t *height);
int slug_angle_sprite(const struct SlugSharedState*,uint8_t *frame);
/* Typed damage-display program, compiled offline from $02F886..$02FACA. */
int slug_damage_display(struct SlugSharedState*,unsigned program,int *finished);
void slug_damage_reset(struct SlugSharedState*);
/* Input tables have already been resolved by player/scene input policy. */
int slug_cannon_ready(const struct SlugSharedState*,struct PlayerSourceInput,unsigned layout);
void slug_ammo_consume(struct SlugSharedState*);
void slug_ammo_add(struct SlugSharedState*,uint8_t quantity);
int slug_hp_add(struct SlugSharedState*,int16_t quantity); /* 1: play $1083 */
void slug_turn_timer(struct SlugSharedState*,uint8_t input_layer2);
int slug_mount_candidate(struct SlugSharedState*,struct PlayerBodyState*);
void slug_rider_refresh(struct SlugSharedState*,const struct PlayerBodyState*);
int slug_rider_absent(struct SlugSharedState*); /* consumes heartbeat */
int slug_ridden_by(const struct SlugSharedState*,unsigned player);
int slug_rider_parameter(const struct SlugSharedState*,uint8_t owner,uint32_t*);
unsigned slug_impact_side(const struct SlugSharedState*,int16_t camera_x);
#endif
