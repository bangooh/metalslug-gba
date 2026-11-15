#ifndef NATIVE_SLUG_BODY_H
#define NATIVE_SLUG_BODY_H
#include "slug_animation.h"
#include "slug_motion.h"
enum SlugBodyState {
 SB_IDLE_FLAT,SB_IDLE_SLOPE,SB_ACCEL_RIGHT,SB_ACCEL_LEFT,SB_BRAKE_RIGHT,SB_BRAKE_LEFT,
 SB_CRUISE_RIGHT,SB_CRUISE_LEFT,SB_SLOPE_IDLE_ENTER,SB_SLOPE_MOUNT,
 SB_DRIVE,SB_DRIVE_B,SB_TURN_TO_DRIVE,SB_DRIVE_ALT,SB_TURN_TO_DRIVE_ALT,
 SB_CROUCH_BRAKE,SB_STALL,SB_CROUCH_ACCEL_RIGHT,SB_CROUCH_ACCEL_LEFT,
 SB_DECEL_RIGHT,SB_DECEL_LEFT,SB_CROUCH_CRUISE_RIGHT,SB_CROUCH_CRUISE_LEFT,
 SB_SET_SPEED,SB_JUMP_NEUTRAL,SB_JUMP_RIGHT,SB_JUMP_LEFT,SB_JUMP_CROUCH,
 SB_JUMP_LAUNCH,SB_JUMP_AIR,SB_JUMP_AIR_ANGLE,SB_JUMP_LAND,SB_LAND_SLOPE,
 SB_LAND_SLOPE_FRAME,SB_FALL_START,SB_FALL,SB_FALL_LOOP,SB_FALL_ANGLE,
 SB_HIT_GROUND_A,SB_HIT_GROUND_B,SB_HIT_GROUND_FRAME,SB_HIT_AIR_A,SB_HIT_AIR_B,
 SB_HIT_LAND_A,SB_HIT_LAND_B,
 SB_IDLE_ENTER_A,SB_IDLE_ENTER_B,SB_IDLE_ENTER_C,SB_UNOCCUPIED_IDLE,SB_UNOCCUPIED_ANGLE,
 SB_UNOCCUPIED_FALL,SB_UNOCCUPIED_FALL_KEEP,SB_IDLE_ANGLED_B,SB_HUNKER,SB_PLAYER_MOUNT,
 SB_FIRE_FLAT,SB_FIRE_SLOPE,SB_FIRE_JUMP_CROUCH,SB_FIRE_JUMP_AIR,
 SB_FIRE_AIR,SB_FIRE_AIR_ANGLE,SB_FIRE_IDLE,SB_FIRE_RECOIL,SB_FIRE_FLAT_RESUME,
 SB_FIRE_MOVE_RIGHT,SB_FIRE_MOVE_LEFT,SB_FIRE_AIR_LOOP,SB_FIRE_AIR_RESUME,
 SB_DESTROYED_SLIDE,SB_KNOCKED,SB_DESTROYED,
 SB_DEATH_START,SB_DEATH_EXPLODE,SB_DEATH_LAUNCH,SB_RETIRE,
 SB_DEATH_FADE,SB_RETIRED_RESET,SB_INACTIVE,
 SB_DESTROYED_SLIDE_RESUME,SB_DESTROYED_SLIDE_LOOP,SB_KNOCKED_RESUME,
 SB_DESTROYED_RESUME,SB_DESTROYED_STOP,SB_DESTROYED_FRAME,SB_SELF_DESTRUCT,
 SB_DROP_INIT,SB_DROP_FRAME,SB_DROP_DESCEND,SB_DROP_DESCEND_FRAME,
 SB_BOSS_ARENA,SB_BOSS_DROP_INIT,SB_BOSS_DROP_FRAME,SB_BOSS_DESCEND,SB_BOSS_DESCEND_FRAME,SB_BOSS_DESCEND_STEADY,
 SB_STATE_COUNT
};
/* Semantic binding of original $60. Its $028992 consumer is a no-op;
 * this records the binding, not an invented body-blocking implementation. */
enum SlugBodyContact {SBC_NONE,SBC_IDLE,SBC_CROUCH,SBC_DESTROYED};
struct SlugBodyContext {
 struct SlugAnimationContext animation;
 struct PlayerSourceInput input;
 int16_t campaign_timer;
 uint8_t layout,debug_flags,frame_phase;
 /* Original scene gates $106ED3/$106ED2 and bounds choice $106F4A. */
 uint8_t mount_enabled,player_query_enabled,narrow_bounds;
 EntityTick contact_tick; /* Native B/A/C group callback, in original child order. */
};
void slug_body_reset(struct SlugSharedState*,uint16_t entity_id);
void slug_body_schedule(struct SlugSharedState*,enum SlugBodyState);
/* Acceleration argument 1 selects the source no-dust alias. STALL argument
 * 1/2 selects the right/left sound entry; 0 is its silent entry. */
void slug_body_schedule_argument(struct SlugSharedState*,enum SlugBodyState,uint8_t);
void slug_body_schedule_speed(struct SlugSharedState*,int16_t velocity);
int slug_body_implemented(enum SlugBodyState);
/* One body handler only. Caller owns entity pre/post processing, contact
 * ordering, scene/input acquisition and frame flag reset. NativePlayWorld
 * reserves/registers the slot and supplies these services. */
int slug_body_step(struct SlugBodyContext*);
#endif
