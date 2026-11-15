#ifndef NATIVE_SCENE_VIDEO_H
#define NATIVE_SCENE_VIDEO_H
#include <stdint.h>
/* Source background/auxiliary tile planes, with no Neo Geo MMIO addresses.
 * The renderer binds each plane's logical columns to its own tile storage. */
enum SceneTilePlane {SCENE_TILE_MAIN,SCENE_TILE_AUX,SCENE_TILE_THIRD,SCENE_TILE_FOURTH,SCENE_TILE_PLANE_COUNT};
struct SceneTileView {
 int16_t base_x,base_y;
 uint16_t first_column;
 uint8_t column_y[32],valid;
};
struct SceneTilePatch {
 const uint16_t*cells; /* cropped column-major pairs: tile, attributes */
 int16_t x,y;
 uint16_t width,height;
 uint8_t plane;
};
struct ScenePaletteFade {uint8_t mode,red,green,blue,complete;};
/* Source1390E partitions380 strips: lower BG, ordinary objects, upper BG,
 * forced objects. Bounds are exclusive source indices, not GBA resources. */
struct SceneSpritePools {uint16_t lower_end,upper_begin,forced_begin;uint8_t valid;};
/* Imported catalog: low bank14E00=0..1023, high bank1CE00=1024..1535.
 * Keeping bank selection explicit prevents fixed/HUD palettes being shifted. */
static inline uint16_t scene_palette_high(uint16_t n){return n==65535?n:(uint16_t)(1024+n);}
struct SceneVideoWorld {
 void*native_context;
 void (*native_reset)(void*,unsigned);
 void (*native_clear_plane)(void*,unsigned);
 int (*native_patch)(void*,const struct SceneTilePatch*);
 int (*native_clear)(void*,unsigned,int16_t,int16_t,uint16_t,uint16_t);
 /* Source10A2C8/CA/CB/CC/CF; consumed by the future palette renderer. */
 struct ScenePaletteFade fade;
 struct SceneTileView views[SCENE_TILE_PLANE_COUNT];
 struct SceneSpritePools pools;
 void*sprite_context;
 int (*sprite_reset)(void*,const struct SceneSpritePools*);
 void*context;
 void*palette_context;
 int (*palette_frame)(void*,const struct ScenePaletteFade*);
 /* Original column position/size words, or hide without changing X. */
 int (*column)(void*,unsigned plane,uint16_t column,uint16_t x,uint16_t y,int hidden);
 /* Batch equivalent of23 column calls; preserves logical column state. */
 int (*commit_columns)(void*,unsigned,uint16_t,uint16_t,unsigned,const uint8_t*);
 int (*write_fixed)(void*,uint8_t column,uint8_t row,uint16_t tile);
 /* Source52328 / BIOS start eligibility, supplied by the scene's credits
  * policy. Values0..4 select original fixed-layer prompt data. */
 uint8_t start_prompt,start_prompt_valid;
 /* Presentation adaptation only:0 keeps source row29. Native75% uses28
  * so bottom credits/LEVEL remain fully visible after the4+4 pixel crop. */
 uint8_t hud_bottom_row;
 /* Native ROM maps own visual cells; retain source columns and collision. */
 uint8_t native_maps,native_visible;
 /* Source2C26/2C30 interpolate to high/low banks; last byte is progress step. */
 int (*actor_palette_cycle)(void*,uint16_t entity,uint16_t generation,uint16_t first_palette,uint16_t period,uint8_t flags);
 int (*palette_cycle)(void*,uint16_t slot,uint16_t first_palette,uint16_t period,uint8_t flags);
 int (*clear_tile)(void*,unsigned plane,uint16_t column,uint8_t row);
 int (*set_palette)(void*,uint8_t slot,uint16_t source,uint8_t flags); /* canonical catalog ID */
 int (*write_tile)(void*,unsigned plane,uint16_t column,uint8_t row,uint16_t tile,uint16_t attributes);
 int (*write_source_tile)(void*,unsigned,uint16_t,uint8_t,const uint16_t*);
 uint8_t fault;
};
/* $05022A/$051DE2: offline-cropped source rectangles, wrapped columns/rows,
 * byte-only per-column Y adjustment, and ordered two-word cell writes. */
int scene_video_clear(struct SceneVideoWorld*,unsigned plane,int16_t x,int16_t y,uint16_t width,uint16_t height);
int scene_video_patch(struct SceneVideoWorld*,const struct SceneTilePatch*);
int scene_video_fixed_rect(struct SceneVideoWorld*,int column,int row,uint16_t tile,unsigned width,unsigned height,int atlas);
#endif
