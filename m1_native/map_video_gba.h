#ifndef M1_MAP_VIDEO_GBA_H
#define M1_MAP_VIDEO_GBA_H
#include "map_view.h"
/* BG-only backend: three maps occupy blocks 29..31, leaving 928 8bpp
 * tiles. HUD must use OBJ or explicitly reserve more character memory.
 * Do not combine this layout with the legacy permanent BG3 HUD. */
int m1_map_commit_gba(struct M1MapFrame*,struct M1TileStream*,const uint32_t*tiles,unsigned tile_count,const uint16_t*palette,const uint8_t priorities[3]);
#endif
