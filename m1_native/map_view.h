#ifndef M1_MAP_VIEW_H
#define M1_MAP_VIEW_H
#include "tile_stream.h"
enum { M1_BG_PLANES=4 };
struct M1MapView {
 const uint16_t *map;
 uint16_t width,height;
 int32_t scroll_x,scroll_y;
};
struct M1MapFrame {
 uint16_t refs[M1_MAX_REFS], maps[M1_BG_PLANES][1024];
 uint16_t scroll_x[M1_BG_PLANES],scroll_y[M1_BG_PLANES];
 uint32_t dirty_rows[M1_BG_PLANES];
 uint16_t ref_count;
 uint8_t layers;
};
/* Maps contain final shared-palette ROM tile IDs. Source-cell layouts and
 * palettes must be resolved by the offline stage compiler. */
int m1_map_prepare(struct M1MapFrame*,struct M1TileStream*,const struct M1MapView*,unsigned layers);
#endif
