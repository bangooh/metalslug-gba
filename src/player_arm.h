#ifndef NATIVE_PLAYER_ARM_H
#define NATIVE_PLAYER_ARM_H
#include "player_actions.h"
#include "../generated/player-arm-select.h"
/* Original body action codes are game state, not processor/program addresses. */
enum PlayerBodyAction {
 BODY_IDLE=0x10,BODY_WALK=0x11,BODY_WALK_LOOP=0x12,
 BODY_SHOOT=0x21,BODY_WALK_SHOOT=0x22,BODY_LOOP_SHOOT=0x23,
 BODY_JUMP=0x25,BODY_FALL=0x26,BODY_JUMP_SHOOT=0x27,BODY_FALL_SHOOT=0x28
};
int player_arm_init(struct PlayerActionContext*,uint16_t hit_id);
int player_arm_tick(struct PlayerActionContext*);
int player_body_motion(struct PlayerActionContext*,unsigned motion,int restart);
/* Load only the lower stream ($028CD4), retaining the arm selector. */
int player_body_stream(struct PlayerActionContext*,unsigned motion);
int player_body_animate(struct PlayerActionContext*);
#endif
