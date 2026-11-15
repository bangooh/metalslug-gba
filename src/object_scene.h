#ifndef NATIVE_OBJECT_SCENE_H
#define NATIVE_OBJECT_SCENE_H
#include "source_drawing.h"
#include "palette_video.h"
#include "source_strip_budget.h"
/* Source sort queue holds348/4=210 map keys, separately from its380 strip
 * records. Use210 as a conservative native submission bound; do not pass
 * this map count to5E804, which reads free STRIPS from the previous commit. */
enum {NATIVE_DRAW_CAPTURE_LIMIT=210};
enum NativeObjectSceneFault {NOS_OK,NOS_STATE,NOS_OWNER,NOS_DRAWING,NOS_PALETTE,
 NOS_QUEUE,NOS_CAMERA,NOS_LAYER,NOS_HARDWARE};
struct NativeObjectSceneConfig {
 void*context;
 /* Resolve each source part's priority and forced pool against the real
  * background layout. Required; no invented default plane ordering. */
 int (*layer)(void*,uint16_t priority,int forced_pool,uint8_t*gba_priority);
 uint16_t tile_budget;
 int16_t pivot_x,pivot_y; /* Source screen, Y-down. */
 uint8_t projection_valid;
};
enum {NATIVE_PART_CACHE_LIMIT=64};
struct NativeCapturedPart {uint16_t map;int16_t x,y;uint16_t priority,strips;uint8_t flip,pad;};
struct NativeObjectDraw {
 const struct NativeDrawing*drawing;
 uint16_t source_drawing,last_draw_cursor;
 uint8_t drawing_flags,part_first,part_count,cosmetic;
 struct NativeSourceDrawPose pose;
 struct NativePaletteCapture palette;
 uint16_t player_owner,approved_frame;
 uint8_t forced_pool,player_layer,approved_skip,palette_protected;
};
struct NativeObjectScene {
 struct NativeObjectDraw draws[NATIVE_DRAW_CAPTURE_LIMIT];
 struct NativeCapturedPart parts[NATIVE_PART_CACHE_LIMIT];
 uint16_t part_count;
 struct NativeObjectFrame frame;
 struct NativeObjectSceneConfig config;
 struct AnimationOutput output;
 struct NativePlayWorld*world;
 struct NativePaletteWorld*palettes;
 struct NativeSourceStripBudget strips;
 struct SceneSpritePools staged_pools;
 uint16_t staged_free[2];
 uint16_t count,submitted_maps,cosmetic_omitted,budget_retries;
 uint8_t collecting,resolved,fault,budget_ready;
 /* v168: IRQ commit publishes the free-strip budget here; the main thread
  * latches it before a simulation tick, never in the middle of one. */
 uint16_t latched_budget[3];volatile uint8_t latched_valid;
};
void native_object_scene_latch(struct NativeObjectScene*);
/* Attach after native_palette_attach, before starting the session. An
 * output failure faults the World even if a manual caller ignores its return.
 * This is the OBJ component, not a substitute for the BG/FIX/HUD backend. */
int native_object_scene_attach(struct NativeObjectScene*,struct NativePlayWorld*,struct NativePaletteWorld*,const struct NativeObjectSceneConfig*);
int native_object_scene_prepare(struct NativeObjectScene*);
/* Leaves the frame open so the physical backend can append FIX objects. */
int native_object_scene_stage(struct NativeObjectScene*);
/* Append actors after an already initialized FIX prefix. */
int native_object_scene_append(struct NativeObjectScene*);
/* Retry only an exhausted hardware frame; still account for every source
 * strip and keep cosmetic actors alive in the simulation. */
int native_object_scene_append_budgeted(struct NativeObjectScene*,int skip_cosmetics);
int native_object_scene_commit(struct NativeObjectScene*);
#endif
