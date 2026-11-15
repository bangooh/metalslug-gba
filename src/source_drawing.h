#ifndef NATIVE_SOURCE_DRAWING_H
#define NATIVE_SOURCE_DRAWING_H
#include "object_video.h"
#include "player_actions.h"
/* Owner supplies a normalized SOURCE-screen anchor and the actual palette.
 * This resolver handles the shared028DFE/0294B0 drawing-part semantics. The
 * camera origin, owner visibility/hit palette, and inter-plane placement are
 * intentionally explicit; this is not a guessed whole-World renderer. */
struct NativeSourceDrawPose {
 int16_t x,y; /* source pixels, screen Y-down */
 uint16_t priority,sequence;
 const uint16_t*palette;
 uint8_t flip,zoom_x,zoom_y,display_flags,gba_priority;
 int16_t reflection_y; /* Visible source-screen water surface, Y-down. */
 uint8_t reflection,direct;
};
/* Source video visible line0 is hardware line16. Animation DRAW subtracts
 * one source Y pixel;05CA2A direct maps do not. Camera/world stay unscaled. */
void native_source_anchor(struct NativeSourceDrawPose*,const struct PlayerTransform*,int32_t camera_x,int32_t camera_y,int direct);
/* Source5B1B2 owner-foot test, with RAW43F02 terrain byte (not shape ID).
 * Reflection is emitted before EACH main part, after that part's offsets. */
void native_source_reflection(struct NativeSourceDrawPose*,unsigned scene,int16_t secondary_x,int16_t secondary_y,uint8_t terrain_raw);
/*028E4E: returns0 when the whole ordinary DRAW is blink-suppressed; updates
 * source5A.bit7. Alternate1E bypasses this ordinary drawing condition. */
int native_source_blink(const struct Animation*,uint8_t damage_timer,uint8_t hit_flags,uint8_t frame_phase,uint8_t*movement_flags);
int native_source_drawing(struct NativeObjectFrame*,const struct Animation*,const struct NativeSourceDrawPose*);
typedef int (*NativeSourcePartSink)(void*,unsigned map,const struct NativeObjectPlacement*);
/* The same ordered stream is used to count source queue records and build
 * GBA objects. Only admitted parts consume sequence numbers. */
int native_source_visit(const struct Animation*,const struct NativeSourceDrawPose*,NativeSourcePartSink,void*,uint16_t*count);
/* Resolved immutable metadata may be retained through actor deletion. */
int native_source_visit_resolved(const struct NativeDrawing*,unsigned flags,const struct NativeSourceDrawPose*,NativeSourcePartSink,void*,uint16_t*);
/*05CA60 perspective is used only by the appropriate direct drawing caller;
 * ordinary animation DRAW does not automatically enable it. */
void native_source_project(struct NativeSourceDrawPose*,int16_t pivot_x,int16_t pivot_y);
#endif
