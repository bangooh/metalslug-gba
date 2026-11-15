#ifndef NATIVE_TARGET_RING_H
#define NATIVE_TARGET_RING_H
#include <stdint.h>
/* $08F138/$08F148/$08F3A6..$08F696: eight records, at most four writes
 * advanced per frame. Coordinates here are world X and source-up Y. */
struct TargetRingRecord {int16_t x,y_up;uint8_t key,owner;};
struct TargetRing {struct TargetRingRecord records[8];uint8_t first,write,end,count,last_key;};
void target_ring_reset(struct TargetRing*);
void target_ring_frame(struct TargetRing*);
uint8_t target_ring_new_key(struct TargetRing*);
/* owner is an in/out result, initialized by the registering actor. */
void target_ring_register(struct TargetRing*,int16_t screen_x,int16_t x,int16_t y_up,uint8_t key,int8_t*owner);
int target_ring_find(const struct TargetRing*,unsigned player,int16_t x,int16_t y_up,int16_t*key);
int target_ring_claim(struct TargetRing*,unsigned player,uint8_t key);
int target_ring_follow(struct TargetRing*,unsigned player,uint8_t key,int16_t*x,int16_t*y_up);
#endif
