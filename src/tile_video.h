#ifndef NATIVE_TILE_VIDEO_H
#define NATIVE_TILE_VIDEO_H
#include "scene_video.h"
struct NativePlayWorld;
/* Exact source writes, independently of the eventual GBA layer composition.
 * The 380 source strips are NOT GBA OAM entries or a free-strip estimate. */
struct NativeTileCell {uint16_t tile,attributes;};
struct NativeTileColumn {uint16_t x,y;uint8_t hidden;};
struct NativeTilePlane {
 struct NativeTileCell cells[32][32];
 uint32_t dirty[32];
};
struct NativeTileVideo {
 struct NativeTilePlane planes[SCENE_TILE_PLANE_COUNT];
 struct NativeTileColumn columns[380];
 uint16_t fixed[40][32];
 uint32_t fixed_dirty[40];
 struct NativePlayWorld*world;
 uint16_t (*source_cells)[32][32];
 uint32_t revision,fixed_revision;
 uint8_t fault;
};
/* Install after play_world_init, before the session loads any scene. Palette
 * attachment uses its independent context and may precede/follow this call. */
int native_tile_video_attach(struct NativeTileVideo*,struct NativePlayWorld*);
int native_tile_video_columns(void*,unsigned,uint16_t,uint16_t,unsigned,const uint8_t*);
int native_tile_video_column(void*,unsigned,uint16_t,uint16_t,uint16_t,int);
int native_tile_video_write(void*,unsigned,uint16_t,uint8_t,uint16_t,uint16_t);
int native_tile_video_clear(void*,unsigned,uint16_t,uint8_t);
int native_tile_video_fixed(void*,uint8_t,uint8_t,uint16_t);
/* Resolve against CURRENT view ownership: scene reload can reuse a physical
 * strip for another plane. Returning NULL is a fault, never a blank tile. */
const struct NativeTileCell*native_tile_video_cell(const struct NativeTileVideo*,unsigned,uint16_t,uint8_t);
/* Source pools map to FIX=0, upper backgrounds=1, ordinary OBJ=2, lower
 * backgrounds=3. Forced OBJ=1 wins its tie with the upper backgrounds.
 * Layers within a background pool must still be composed in strip order. */
int native_tile_video_object_layer(void*,uint16_t,int,uint8_t*);
#endif
