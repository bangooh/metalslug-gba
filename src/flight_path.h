#ifndef NATIVE_FLIGHT_PATH_H
#define NATIVE_FLIGHT_PATH_H
#include "player_actions.h"
/* Authored flight-path operations, compiled offline to bounded indices.
 * These are not CPU opcodes, original pointers, or a source memory image. */
enum FlightOp {FP_ANGLE,FP_DISTANCE,FP_ARC,FP_WAIT,FP_JUMP,FP_FIELD,FP_CALL,FP_SOUND,FP_FINISH};
struct FlightCommand {uint32_t value;uint16_t next,target,length;uint8_t op,width;int8_t increment;uint8_t angle;};
struct FlightState {uint32_t distance;uint16_t cursor,angle;};
struct FlightProgram {const struct FlightCommand*commands;uint16_t count;};
struct FlightOwner {
 void*context;
 int (*field)(void*,unsigned,unsigned,uint32_t);
 int (*call)(void*,unsigned);
 int (*sound)(void*,uint16_t);
};
/* Returns source carry (terminal movement) or -1 on a rejected program. */
int flight_step(struct PlayerTransform*,struct FlightState*,int16_t speed,const struct FlightProgram*,const struct FlightOwner*);
#endif
