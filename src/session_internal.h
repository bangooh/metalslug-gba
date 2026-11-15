#ifndef NATIVE_SESSION_INTERNAL_H
#define NATIVE_SESSION_INTERNAL_H
#include "session.h"
#include "scene_driver.h"
#include "scene_text.h"
#include "bosses.h"
#include "../generated/session-data.h"
enum NativePlayerControlState {
 PC_IDLE,PC_READY,PC_SUSPEND,PC_NEW,PC_LIVE,PC_WAIT_TIME,PC_WAIT_BODY,
 PC_FINISH,PC_CONTINUE_BEGIN,PC_CONTINUE,PC_HIGHSCORE_BEGIN,PC_HIGHSCORE,
 PC_RESET_BEGIN,PC_RESET,PC_STOP,PC_INIT,PC_CONTINUE_RESTART
};
int session_fail(struct NativePlayWorld*);
int session_input_allowed(const struct NativePlayWorld*);
int session_credit_ready(const struct NativeSession*);
void session_player_tick(struct EntityPool*,struct NativeEntity*,void*);
int session_text(struct WeaponWorld*,uint16_t,unsigned,uint8_t,int);
int session_hud_clear(struct NativePlayWorld*,unsigned);
int session_hud_player(struct NativePlayWorld*,unsigned,int);
int session_hud_global(struct NativePlayWorld*);
int session_pause(struct NativePlayWorld*,uint8_t);
int session_ui_timer(struct NativePlayWorld*);
int session_ui_start(struct NativePlayWorld*);
int session_ui_continue(struct NativePlayWorld*,unsigned);
int session_ui_continue_drop(struct NativePlayWorld*,unsigned);
#endif
