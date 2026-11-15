#ifndef NATIVE_MISSION_RESULT_INTERNAL_H
#define NATIVE_MISSION_RESULT_INTERNAL_H
#include "result_internal.h"
#include "bosses.h"
#include "items.h"
#include "slug_support.h"
#include "../generated/mission-result-data.h"
enum MissionResultState {MR_NEW,MR_COUNTDOWN,MR_FADE,MR_FADE_WAIT,MR_SETUP,MR_WAIT,MR_FADEOUT,MR_TEARDOWN,MR_BANNER_WAIT,MR_IDLE,MR_COLUMN,MR_SCORE,MR_COUNT,MR_WAITOTHER,MR_BONUS,MR_TOTAL,MR_PAUSE,MR_WAITALL,MR_VEHICLE,MR_VEHICLE_WAIT,MR_AWARD,MR_FINISH,MR_ANIMATE,MR_ROSTER,MR_FRAME,MR_PRIZE,MR_PRIZE_FLY,MR_PRIZE_DONE,MR_LABEL,MR_LABEL_FINISH,MR_VEHICLE_ICON};
struct NativeEntity*mr_allocate(struct WeaponWorld*,struct EntityRef,unsigned);
struct NativeEntity*mr_child(struct WeaponWorld*,struct NativeEntity*,unsigned);
struct EnemyActor*mr_parent(struct WeaponWorld*,struct NativeEntity*);
int mr_animate(struct WeaponWorld*,struct NativeEntity*);
void mr_start(struct EnemyActor*,uint16_t);
void mr_screen(struct WeaponWorld*,struct WeaponActor*,int16_t,int16_t);
int mr_text(struct WeaponWorld*,uint16_t,unsigned,uint8_t,int);
int mr_two_digits(struct WeaponWorld*,uint16_t,uint16_t,int);
int mr_roster(struct WeaponWorld*,struct NativeEntity*);
int mr_frame(struct WeaponWorld*,struct NativeEntity*);
#endif
