#ifndef NATIVE_SESSION_H
#define NATIVE_SESSION_H
#include "play_world.h"
#include "scene_runtime.h"
/* Native counterpart of the boot/BIOS player controllers. Storage belongs
 * to the caller, like SceneRuntime. This does not replace released main.c. */
enum NativeSessionState {
 SESSION_MISSION_BEGIN,SESSION_PLAY,SESSION_RESULTS,SESSION_FINAL_BEGIN,
 SESSION_FINAL,SESSION_ENDING_BEGIN,SESSION_ENDING,SESSION_LOST,
 SESSION_GAMEOVER_BEGIN,SESSION_GAMEOVER,SESSION_RETURN_TITLE,SESSION_PRESENTATION
};
struct NativeSessionConfig {
 /* Initial players were accepted by the front end; no second credit charge.
  * New joins/continues consume the shared GBA credit balance (an explicit
  * adaptation, not an emulation of arcade BIOS/regional credit banks). */
 uint8_t initial_players,initial_credits,free_play,allow_continue;
 /* Reserved byte preserves the existing layout; it has no setting semantics. */
 uint8_t initial_lives,time_setting,reserved_layout,timing_variant;
 /* Original BIOS rank,106F2A layout and10FD8F blood switch. Home/arcade
  * select source HUD/pause policy only; credits remain the GBA shared bank. */
 uint8_t difficulty,control_layout,blood_enabled,arcade_mode,hud_options;
 /* Host/local game slot: 0 Marco/P1, 1 Tarma/P2. Cable role is separate. */
 uint8_t first_character;
};
struct NativeSessionInput {
 struct PlayerSourceInput player[2],raw_player[2];
 /* These original input streams are NOT interchangeable. */
 uint8_t edges[2],repeat[2],start[2];
 uint8_t join_requests,credits_added,system_edges,raw_valid;
};
struct NativePlayerControl {
 uint8_t state,entry,lives,join_blocked,saved_join_blocked;
 uint8_t saved_state,reset_timer,credit_ready;
 struct {
  uint32_t hp,hp_speed;
  uint16_t ammo,grenades;
  uint8_t hp_phase,ammo_blink,grenade_blink,grenade_mode,rescued;
 } hud;
};
struct NativeSession {
 struct NativeSessionConfig config;
 struct NativeSessionInput input;
 struct NativePlayerControl player[2];
 struct EntityRef boot,controllers[2];
 uint16_t panel_clock;
 uint8_t state,credits,transition_hold,ending,gameover_phase,ready,fault;
 uint8_t pause_allowed,paused,pause_blink,frame_hold,hud_flags;
};
/* Begin on a freshly initialized NativePlayWorld before attaching a scene.
 * The scene/video/audio services must be real supplied services; missing
 * output is an error, never a fake successful campaign. */
int native_session_begin(struct NativeSession*,struct NativePlayWorld*,
 struct SceneRuntime*,const struct NativeSessionConfig*);
/* One native input opportunity: pause may consume it without advancing the
 * world; inspect paused/ticks. Supply raw and mapped input, then execute boot,
 * timer children, scene, P2/P1 controllers, Slug and gameplay in that order.
 * RETURN_TITLE is an explicit handoff; title/attract are still separate. */
int native_session_frame(struct NativeSession*,struct NativePlayWorld*,
 const struct NativeSessionInput*);
#endif
