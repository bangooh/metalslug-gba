#ifndef NATIVE_PLAYER_BODY_H
#define NATIVE_PLAYER_BODY_H
#include "player_air.h"
struct SlugSharedState;
/* Native scheduler states. None is an original CPU address. A scheduled
 * transition enters on the next actor tick; direct source branches are
 * resolved in entry before that tick's single physics/animation update. */
enum PlayerState {
 PST_STAND,PST_STAND_FROM_WALK,PST_REENTER,PST_WALK_RIGHT,PST_WALK_LEFT,
 PST_LOOP_RIGHT,PST_LOOP_LEFT,PST_TURN_RIGHT,PST_TURN_LEFT,
 PST_SHOOT_STAND,PST_SHOOT_UP,PST_SHOOT_STAND_ALT,PST_SHOOT_UP_ALT,
 PST_WALK_SHOOT_RIGHT,PST_WALK_SHOOT_LEFT,PST_WALK_UP_RIGHT,PST_WALK_UP_LEFT,
 PST_LOOP_SHOOT_RIGHT,PST_LOOP_SHOOT_LEFT,PST_LOOP_UP_RIGHT,PST_LOOP_UP_LEFT,
 PST_MELEE,PST_MELEE_ALT,PST_MELEE_ALT_WALK,
 PST_GRENADE,PST_GRENADE_WALK,PST_GRENADE_LOOP,
 PST_IDLE_SPECIAL,PST_IDLE_LOW_AMMO,PST_RELOAD,PST_EMPTY,
 PST_LAND,PST_STOP,
 PST_CROUCH_ENTER,PST_CROUCH_ENTER_B,PST_CROUCH_EXIT,PST_CROUCH_REENTER,
 PST_CROUCH_IDLE,PST_CROUCH_AFTER_SHOT,PST_CROUCH_AFTER_THROW,
 PST_CRAWL_RIGHT,PST_CRAWL_LEFT,PST_CRAWL_TURN_RIGHT,PST_CRAWL_TURN_LEFT,
 PST_CROUCH_SHOOT,PST_CROUCH_GRENADE,PST_CROUCH_MELEE,PST_CROUCH_RELOAD,PST_CROUCH_EMPTY,
 PST_JUMP,PST_FALL,PST_FALL_DELAY,PST_FALL_HOLD,
 PST_RIDE_SLUG,PST_SLUG_POSE,PST_HANG_RING,PST_ATTACH_JUMP,PST_ATTACH_LAND,PST_JUMP_DROP,PST_PIT,PST_DEATH,
 PST_SPAWN_FALL,PST_SPAWN_LAND,PST_SPAWN_HOLD,PST_SPAWN_PARACHUTE,PST_SPAWN_FREEFALL,PST_SPAWN_DONE,
 PST_STATE_COUNT
};
struct PlayerBodyContext {
 struct PlayerAirContext air;
 void *effects;
 int (*drop_weapon)(void*,struct PlayerBodyState*);
 int (*visual)(void*,enum PlayerVisualEffect);
 int (*retire)(void*);
 int (*ring_follow)(void*,uint8_t key,int16_t*x,int16_t*y_up);
 uint8_t blood_enabled,slug_invulnerability;
 struct SlugSharedState *slug;
 uint8_t slug_riders_enabled; /* original global $106ED3, supplied by scene */
};
void player_body_schedule(struct PlayerBodyState*,enum PlayerState);
int player_body_tick(struct PlayerBodyContext*);
/* Shared native entry/frame helpers, also used by the crouch family. */
int player_body_set_pose(struct PlayerBodyContext*,unsigned action,unsigned motion,int x,int y,int restart);
int player_body_frame(struct PlayerBodyContext*,int *complete,int announce);
int player_body_tail(struct PlayerBodyContext*,enum PlayerState fall,int crouch_jump,int jump_first,int bounds,int allow_jump);
int player_body_actions(struct PlayerBodyContext*,unsigned gait,int crouching);
void player_body_reenter(struct PlayerBodyContext*,int crouching,int immediate);
int player_body_idle_checks(struct PlayerBodyContext*,int low_ammo,int force_empty);
int player_crouch_enter(struct PlayerBodyContext*,enum PlayerState);
int player_crouch_frame(struct PlayerBodyContext*,enum PlayerState);
int player_life_tick(struct PlayerBodyContext*);
int player_attachment_tick(struct PlayerBodyContext*);
int player_slug_tick(struct PlayerBodyContext*);
#endif
