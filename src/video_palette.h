#ifndef NATIVE_VIDEO_PALETTE_H
#define NATIVE_VIDEO_PALETTE_H
#include <stdint.h>
/* Shared exact GBA BG palette. Index0 remains transparent in every BG;
 * an opaque pixel equal to backdrop RGB still receives a NONZERO index. */
struct NativeVideoPalette {
 uint16_t colors[256],hash[512],keys[256],count,limit;
 uint8_t fault,ready,stable;
};
void native_video_palette_begin(struct NativeVideoPalette*,uint16_t backdrop);
/* Returns0xffff on >255 distinct opaque colors. No quantizer/fallback. */
uint16_t native_video_palette_color(struct NativeVideoPalette*,uint16_t rgb555);
uint16_t native_video_palette_stable_color(struct NativeVideoPalette*,uint16_t rgb555,uint16_t identity);
unsigned native_palette_dirty(const uint16_t*,uintptr_t destination,unsigned banks);
int native_video_palette_finish(struct NativeVideoPalette*);
#endif
