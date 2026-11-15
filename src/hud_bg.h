#ifndef NATIVE_HUD_BG_H
#define NATIVE_HUD_BG_H
#include "fixed_object.h"
/* Alias the existing FIX scratch arena. Upper8KiB remain available to the
 * background cache. No new persistent EWRAM allocation. */
enum {NATIVE_HUD_TILES=320,NATIVE_HUD_DIRTY_WORDS=NATIVE_HUD_TILES/32};
struct NativeHudFrame {
 uint32_t revision,palette_revision,dirty[NATIVE_HUD_DIRTY_WORDS],rows[20];
 uint32_t pixels[NATIVE_HUD_TILES][8];
 uint16_t map[1024],source_colors[16][16],palettes[4][16];
 uint32_t source_targets[16][16][2];
 uint16_t source_mask,tile_span,tile_base,tile_limit,source_used[16];
 uint8_t occupied[NATIVE_HUD_TILES],counts[4],valid,map_dirty,palette_dirty;
 uint16_t cache_keys[NATIVE_HUD_TILES][4],references[NATIVE_HUD_TILES],hash[512],hash_slot[NATIVE_HUD_TILES],victim;
 uint8_t cache_phase[NATIVE_HUD_TILES],cache_bank[NATIVE_HUD_TILES];
};
_Static_assert(sizeof(struct NativeHudFrame)<=24576,"HUD/BG scratch partition");
int native_hud_prepare(struct NativeHudFrame*,const struct NativeTileVideo*,const struct NativePaletteWorld*,int retained);
int native_hud_prepare_limit(struct NativeHudFrame*,const struct NativeTileVideo*,const struct NativePaletteWorld*,int,unsigned);
void native_hud_layout(struct NativeHudFrame*,unsigned background_maps);
void native_hud_stage_palette(struct NativeHudFrame*);
void native_hud_commit(struct NativeHudFrame*);
#endif
