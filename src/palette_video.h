#ifndef NATIVE_PALETTE_VIDEO_H
#define NATIVE_PALETTE_VIDEO_H
#include "scene_video.h"
#include "entities.h"
struct NativePlayWorld;
/* Source palette commands select RGB records, not hardware palette numbers.
 * Keep full precision until the existing OBJ allocator shares exact colours. */
struct NativePaletteState {
 uint32_t serial; /* Distinguish A -> B -> A rebinding within one frame. */
 uint16_t source,target,period,timer,binding,generation;
 uint16_t colors[16];
 uint8_t progress,step,flags,valid;
};
/* Draw-time ownership is independent of an entity that can be deleted or
 * recycled later in the same scheduler pass. No live actor pointer is kept. */
struct NativePaletteCapture {
 struct NativePaletteState state;
 struct EntityRef owner;
 uint16_t slot;
 uint8_t fixed;
};
struct NativePaletteWorld {
 struct NativePaletteState slots[256],actors[ENTITY_CAPACITY];
 struct NativePlayWorld*world;
 struct ScenePaletteFade fade;
 uint32_t fixed_revision,binding_revision,unfaded_revision;
 uint32_t fixed_pending[8],actor_live[8];
 uint8_t fault,native_backgrounds;
};
/* Cached ROM-bank composition. The caller owns the output colors and must
 * clear base whenever another renderer overwrites that output buffer. */
struct NativeBackgroundPaletteCache {
 const uint16_t*base,*bindings;
 uint32_t revision,shade;
};
int native_palette_attach(struct NativePaletteWorld*,struct NativePlayWorld*);
int native_palette_set(void*,uint8_t,uint16_t,uint8_t);
/* Historical service name "cycle": source2C26(high)/2C30(low) is interpolation TO target,
 * with a word period and byte progress step. It is not an animation list. */
int native_palette_blend(void*,uint16_t,uint16_t,uint16_t,uint8_t);
int native_actor_palette_blend(void*,uint16_t,uint16_t,uint16_t,uint16_t,uint8_t);
int native_palette_frame(void*,const struct ScenePaletteFade*);
const uint16_t*native_palette_slot(const struct NativePaletteWorld*,unsigned);
uint16_t native_palette_unfaded_color(const struct NativePaletteWorld*,unsigned slot,unsigned index);
int native_palette_background(struct NativeBackgroundPaletteCache*,const struct NativePaletteWorld*,const uint16_t*base,const uint16_t*bindings,uint16_t out[192]);
int native_palette_actor(struct NativePaletteWorld*,struct EntityRef,uint16_t source,uint8_t flags,uint16_t out[16]);
int native_palette_capture(struct NativePaletteWorld*,struct EntityRef,uint16_t source,uint8_t flags,unsigned fixed_slot,struct NativePaletteCapture*);
int native_palette_resolve_capture(struct NativePaletteWorld*,struct NativePaletteCapture*);
#endif
