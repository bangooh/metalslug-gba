#ifndef PLAYER_BODY_POSE_H
#define PLAYER_BODY_POSE_H
#include "player_arm.h"
#include "player_rules.h"
#include "../generated/player-body-data.h"
/* $03705A/$037168 select the upper pose while preserving the lower stream.
 * horizontal is source input field 2: 0 none, 1 left, 2 right, 3 both. */
int player_air_neutral_pose(struct PlayerActionContext*);
int player_air_action_pose(struct PlayerActionContext*,enum PlayerIntent,uint8_t horizontal);
enum PlayerDeathRoute player_death_route(uint8_t hit_kind,int airborne);
#endif
