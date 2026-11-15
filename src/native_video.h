#ifndef NATIVE_VIDEO_H
#define NATIVE_VIDEO_H
#include "background_video.h"
#include "fixed_object.h"
#include "object_scene.h"
struct NativeVideo {
 struct NativeTileVideo tiles;
 struct NativePaletteWorld palettes;
 struct NativeObjectScene objects;
 struct NativeBackgroundFrame backgrounds;
 struct NativeVideoPalette colors;
 struct NativeBackgroundPaletteCache background_palette;
 struct NativeFixedObjects fixed;
 struct NativePlayWorld*world;
 uint32_t commits,late_transfers,renderer_palette_revision;
 uint8_t auto_frame,auto_wait,auto_disabled,fault;
 volatile uint8_t reveal_pending,reveal_hold,commit_pending,commit_complete;
 uint8_t hud_active,hud_blocked,renderer_scene,bg4_replanned;
 uint32_t bg4_commits,hud_commits,format_fallbacks;
 int32_t result_x,result_y;int16_t result_pa,result_pd;
 uint8_t result_mode,result_loaded,result_visible;
 uint8_t flash_pending,flash_active;
};
int native_video_attach(struct NativeVideo*,struct NativePlayWorld*);
int native_video_prepare(void*,struct NativePlayWorld*);
/*0 fault,1 committed by IRQ,2 queued: retain staging and retry. */
int native_video_commit(void*,struct NativePlayWorld*);
int native_video_settled(void*);
/* Called once per real VBlank, even during pause/link waits. */
void native_video_vblank(struct NativeVideo*);
/* IRQ transfers a queued immutable frame at the start of VBlank. */
void native_video_irq_vblank(struct NativeVideo*);
#endif
