#ifndef NATIVE_RESULT_INTERNAL_H
#define NATIVE_RESULT_INTERNAL_H
#include "results.h"
#include "enemies.h"
#include "scene_video.h"
#include "../generated/result-data.h"
enum ResultState {R_ENDING,R_ENDING_TITLE,R_ENDING_WAIT30,R_ENDING_WIPE,R_ENDING_WAIT15,R_ENDING_P1,R_ENDING_P2,R_ENDING_FADE_A,R_ENDING_FADE_B,R_ENDING_DONE,R_PANEL,R_CONTINUE_LABEL,R_CONTINUE_ROLL,R_CONTINUE_WAIT,R_PRISONER_LABEL,R_PRISONER_ROLL,R_PRISONER_WAIT,R_SCORE,R_HISCORE,R_TITLE_MISSION,R_TITLE_ALL,R_TITLE_OVER,R_TITLE_DONE,R_WIPE,R_PEACE_WAIT,R_PEACE_TEXT,R_PEACE_DONE,R_TYPEWRITER,R_ORBIT,R_ORBIT_CHILD};
int result_rect(struct WeaponWorld*,uint16_t,uint16_t,unsigned,unsigned,int);
int result_string(struct WeaponWorld*,uint16_t,unsigned,uint8_t,int);
int result_character(struct WeaponWorld*,uint16_t,uint8_t,uint8_t,int);
int result_stream(struct WeaponWorld*,uint16_t,const struct ResultBytes*,uint16_t);
int result_digits(struct WeaponWorld*,uint16_t,uint32_t,unsigned);
int result_rank(struct WeaponWorld*,uint16_t,uint16_t);
int result_name(struct WeaponWorld*,uint16_t,const uint16_t*,unsigned);
int result_packed_digits(struct WeaponWorld*,uint16_t,uint32_t);
int result_has_children(struct WeaponWorld*,const struct NativeEntity*);
int result_parent_signal(struct WeaponWorld*,struct NativeEntity*,int phase,int value,int add);
struct NativeEntity*result_allocate(struct WeaponWorld*,struct EntityRef,enum EntityInsert,unsigned);
void result_highscore_tick(struct EntityPool*,struct NativeEntity*,void*);
#endif
