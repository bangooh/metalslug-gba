#ifndef SOURCE_PLAYER_AIR_H
#define SOURCE_PLAYER_AIR_H
#include "player_body_pose.h"
#include "terrain_motion.h"
enum PlayerAirPhase {PAIR_JUMP_WINDOW,PAIR_JUMP,PAIR_FALL,PAIR_FALL_DELAY,PAIR_FALL_HOLD};
enum PlayerAirTransition {PAIR_CONTINUE,PAIR_LAND,PAIR_RIDE_SLUG,PAIR_HANG_RING,PAIR_REJUMP,PAIR_PIT,PAIR_DEATH};
struct PlayerAirContext {
 struct PlayerActionContext *actions;
 struct TerrainWorld *terrain;
 struct PlayerSourceInput input;
 void *world;
 /* -1 failure, 0 no pending ring, 1 found. A negative key is rejected by
  * the original branch, even when the finder reports a pending record. */
 int (*ring_find)(void*,struct PlayerBodyState*,int16_t*key);
 int (*ring_claim)(void*,struct PlayerBodyState*,uint8_t key);
 int16_t campaign_timer;
 uint8_t pause_motion,invincible,transition,death_route;
 uint8_t control_mode;
};
/* Entry functions execute their first frame, as in the source fall-through. */
int player_air_begin_jump(struct PlayerAirContext*);
int player_air_begin_fall(struct PlayerAirContext*,enum PlayerAirPhase);
int player_air_step(struct PlayerAirContext*);
/* Shared $032FF2/$033016 and $033522 tails for the remaining body states. */
int player_body_frame_common(struct PlayerAirContext*,int align_priority,int vertical_only);
int player_body_death_publish(struct PlayerAirContext*);
int player_body_terrain_move(struct PlayerAirContext*,enum WeaponMove);
int player_body_synchronize(struct PlayerAirContext*);
int player_body_foot_contact(struct PlayerAirContext*);
int player_body_outside(struct PlayerAirContext*);
int player_body_jump_pressed(const struct PlayerAirContext*,int crouching);
#endif
