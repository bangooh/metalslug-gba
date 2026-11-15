#ifndef NATIVE_BACKGROUND_ASSETS_H
#define NATIVE_BACKGROUND_ASSETS_H
#include "palette_video.h"
#define NATIVE_BG_ASSET_MARK 0x0e000000u
#define NATIVE_BG_EMPTY_KEY 0x0fffffffu
unsigned native_bg_profile(const struct NativePaletteWorld*);
void native_bg_load(unsigned,uint32_t output[9]);
int native_bg_boundary(const uint32_t keys[4],const struct NativePaletteWorld*,unsigned,uint32_t output[9]);
unsigned native_bg_region_map(unsigned region,unsigned profile,unsigned x,unsigned y);
unsigned native_bg_opacity_mask(unsigned block,unsigned flip);
#endif
