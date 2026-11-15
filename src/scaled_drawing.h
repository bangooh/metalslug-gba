#ifndef NATIVE_SCALED_DRAWING_H
#define NATIVE_SCALED_DRAWING_H
#include "drawing.h"
#include "display_scale.h"
/* Map-relative positions are already75%. Chunks are padded to legal GBA OBJ
 * shapes, 4bpp, 1D tile order. Never scale their pixels a second time. */
struct NativeScaledChunk {uint32_t byte_offset;int16_t x,y;uint8_t width,height,shape,size;};
struct NativeScaledMap {uint16_t first_chunk;uint16_t count;};
const struct NativeScaledMap*native_scaled_map(unsigned);
const struct NativeScaledMap*native_scaled_dense_map(unsigned);
const struct NativeScaledMap*native_scaled_zoom_map(unsigned,uint8_t zoom_x,uint8_t zoom_y);
const struct NativeScaledChunk*native_scaled_chunk(unsigned);
const uint8_t*native_scaled_pixels(const struct NativeScaledChunk*);
/* Background source tiles are12x12 packed rows, not GBA8x8 blocks. They are
 * intermediate OFFLINE inputs for the future full-background tile compiler. */
typedef int (*NativeScaledSink)(void*,const struct NativeDrawPart*,const struct NativeScaledMap*);
int native_scaled_drawing_visit(const struct Animation*,NativeScaledSink,void*);
_Static_assert(sizeof(struct NativeScaledMap)==4,"compact GBA map record");
const struct NativeScaledMap*native_scaled_bridge_map(unsigned);
#endif
