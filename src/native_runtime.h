#ifndef NATIVE_RUNTIME_H
#define NATIVE_RUNTIME_H
#include "session.h"
#include "source_input.h"
enum NativeFrameResult {NATIVE_FRAME_ERROR=-1,NATIVE_FRAME_WAIT,NATIVE_FRAME_PAUSED,NATIVE_FRAME_ADVANCED};
struct NativeRuntimeServices {
 /* Optional platform hook after state advances, before rendering. */
 void (*after_simulate)(void);
 void (*before_simulate)(void);
 /* v168: nonzero once the previously committed display list was transferred
  * (consumes that completion). Optional; without it prepare waits for ready. */
 int (*settled)(void*);
 struct SourceAudioTransport audio;
 void *video_context;
 /* Must perform real output work. commit: staged VBlank tile/palette/OAM
  * transfer request (0 fault,1 complete,2 pending); prepare: build the next
  * display from the current native world.
  * A missing backend prevents startup; no no-op success fallback. */
 int (*commit)(void*,struct NativePlayWorld*);
 int (*prepare)(void*,struct NativePlayWorld*);
};
struct NativeRuntime {
 struct NativePlayWorld *world;
 struct NativeSession *session;
 struct SourceInput input;
 struct NativeRuntimeServices services;
 struct NativeLink *link;
 uint16_t waited;
 uint8_t vblanks,half_rate,ready,fault,link_consuming,join_pending;
 /* Port extra (v168): simulation may run while the previous display list
  * still waits for its VBlank transfer; prepare/commit still wait. */
 uint8_t simulated,simulated_result;
 uint32_t refreshes;
};
/* Attach to a begun session. No interrupt handler or released main.c change.
 * Serialized caller: invoke vblank once per observed hardware refresh, pump
 * when ready. Concurrent IRQ queue access/critical sections belong to the
 * future hardware integration; these callbacks are not interrupt-safe APIs.
 * credits_added is consumed only on PAUSED/ADVANCED, never on WAIT. */
int native_runtime_init(struct NativeRuntime*,struct NativePlayWorld*,struct NativeSession*,const struct NativeRuntimeServices*);
int native_runtime_vblank(struct NativeRuntime*);
/* A delayed main loop still services audio/cadence for elapsed refreshes,
 * but may commit only in the currently observed hardware blanking window. */
int native_runtime_refresh(struct NativeRuntime*,int can_present);
enum NativeFrameResult native_runtime_pump(struct NativeRuntime*,const uint16_t gba_keys[2],uint8_t credits_added);
int native_runtime_attach_link(struct NativeRuntime*,struct NativeLink*);
int native_runtime_resume_link(struct NativeRuntime*,struct NativeLink*);
/* Once attached, direct pump is blocked. Freeze local input/credit at offer,
 * poll the cable while WAIT, and do not apply the credit again to next frame. */
enum NativeFrameResult native_runtime_link_pump(struct NativeRuntime*,uint16_t local_keys,uint8_t local_credits);
/* Active-high GBA keys: A fire, B jump, R grenade, D-pad direction; START
 * joins/continues idle slots or pauses active slots. Select/L are unassigned.
 * Player2 keys must come from a real producer; this is not a link protocol. */
uint8_t native_gba_player_buttons(uint16_t);
uint16_t native_gba_read_keys(void);
#endif
