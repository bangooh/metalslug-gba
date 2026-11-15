#ifndef NATIVE_FIXED_VIDEO_H
#define NATIVE_FIXED_VIDEO_H
#include "tile_video.h"
#include "palette_video.h"
#include "video_palette.h"
/* 30x20 viewport tiles plus one transparent tile. FIX is packed at its
 * native6x6 scaled spacing, never padded to8x8 character spacing. */
enum {NATIVE_FIXED_TILE_LIMIT=601};
enum NativeFixedFault {NFIX_OK,NFIX_STATE,NFIX_METADATA,NFIX_PALETTE,NFIX_TILES,NFIX_LAYOUT,NFIX_HARDWARE};
struct NativeFixedLayout {uint16_t first_tile,tile_budget;uint8_t char_block,screen_block;};
struct NativeFixedFrame {
 uint32_t tiles[NATIVE_FIXED_TILE_LIMIT][16];
 uint16_t map[1024],hash[1024];
 struct NativeFixedLayout layout;
 uint16_t tile_count;
 uint8_t fault,ready;
};
/* Shared palette must be open. Other BG components may append exact colors
 * before palette_finish. This function performs bounded fragment packing of
 * dynamic text, not source resampling or full scene pixel composition. */
int native_fixed_prepare(struct NativeFixedFrame*,const struct NativeTileVideo*,const struct NativePaletteWorld*,struct NativeVideoPalette*,const struct NativeFixedLayout*);
/* BG0 only, priority0. OBJ priorities must be >=1 to preserve source FIX
 * above sprites. Caller owns all other BG regions, display/IRQ and DMA3.
 * Requires forced blank or VBlank and a closed shared BG palette. */
int native_fixed_commit(struct NativeFixedFrame*,const struct NativeVideoPalette*);
int native_fixed_layout_valid(const struct NativeFixedLayout*);
#endif
