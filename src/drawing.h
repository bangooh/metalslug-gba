#ifndef NATIVE_DRAWING_H
#define NATIVE_DRAWING_H
#include "animation.h"
/* Offline-decoded drawing metadata. IDs are native array indices, never
 * emulated addresses. Coordinates remain signed source pixels/Y-up. */
struct NativeDrawPiece {uint32_t first_tile;int16_t x,y_up;uint8_t width,height;};
struct NativeDrawMap {uint32_t first_piece;uint8_t count,attributes;};
struct NativeDrawPart {int16_t x,y_up;uint16_t map,priority;uint8_t flags;};
struct NativeDrawing {uint32_t first_part;uint16_t count;};
struct NativeDrawingSet {
 const struct AnimationProgram *program;
 const struct NativeDrawing *drawings;
 uint16_t count;
};
/* Success for an intentional blank drawing; failure for unknown programs,
 * invalid IDs or malformed metadata. The physical backend still owns sorting,
 * clipping, zoom, per-part priority/palette, water reflection and OAM/VRAM. */
int native_drawing_resolve(const struct Animation*,const struct NativeDrawing**);
typedef int (*NativeDrawSink)(void*,const struct NativeDrawPart*,const struct NativeDrawMap*,const struct NativeDrawPiece*,const uint16_t*);
int native_drawing_visit(const struct Animation*,NativeDrawSink,void*);
/* C-ROM source tile IDs (bank<<16|tile) map to four 8x8 GBA4bpp tiles,
 * TL/TR/BL/BR. FIX tiles are already one8x8 GBA4bpp tile. No scaling,
 * recoloring, quantization or runtime pixel composition is performed. */
const uint8_t*native_sprite_tile(uint32_t);
const uint8_t*native_fixed_tile(unsigned);
const uint16_t*native_palette(unsigned);
#endif
