#ifndef NATIVE_BACKGROUND_VIDEO_H
#define NATIVE_BACKGROUND_VIDEO_H
#include "tile_video.h"
#include "palette_video.h"
#include "video_palette.h"
enum {NATIVE_BG_BASE_TILES=896,NATIVE_BG_TILES=992};
/* Collector can describe three planes; the native cartridge selects the
 * nearest two, reserving BG3 and10KiB of VRAM for its permanent HUD. */
enum {NATIVE_BG_VISIBLE_LIMIT=3};
/* Hardware H-flip of blank tile0. Remember an occluded cell so it is rebuilt
 * if exposed later; never reuse its old blank as if source pixels were empty. */
enum {NATIVE_BG_HIDDEN=1024};
enum NativeBackgroundFault {NBG_STATE=1,NBG_COLUMNS,NBG_METADATA,NBG_PALETTE,NBG_TILES,NBG_FORMAT};
struct NativeBackgroundFrame {
 uint32_t tiles[NATIVE_BG_BASE_TILES][16];
 uint16_t maps[4][1024],hash[2048];
 /* Four exact28-bit fragment keys packed into112bits; top16bits hold tile+1.
  * Direct mapping permits eviction without approximate hash equality. */
 uint16_t hash_slot[NATIVE_BG_TILES];
 uint8_t live[NATIVE_BG_TILES],generation[NATIVE_BG_TILES];
 uint32_t dirty[31],palette_revision,tile_revision,view_signature;
 uint8_t auto_frame,auto_enabled,auto_mask,culling;
 uint16_t map_ranges[4][32];
 uint16_t count,uploaded,control[4],scroll_x[4],scroll_y[4],enable;
 uint16_t victim,cache_evict;
 uint16_t previous_columns[4][32][2];
 uint8_t ready,fault,map_dirty,map_count,auto_planes,layer_limit,depth4,hud_reserved;
 uint8_t native_profile,native_cache_next;
 uint16_t fade_control,fade_level;
 uint32_t map_rows[4],native_tiles,boundary_tiles;
};
/* 4bpp tiles occupy the first32KiB of the existing57KiB staging area.
 * The rest holds palette bindings and final hardware maps, without extra
 * EWRAM. Source-cell provenance survives cache resets. */
struct NativeBackground4 {
 uint32_t hashes[NATIVE_BG_TILES],membership[16][8];
 uint8_t bank[NATIVE_BG_TILES],indices[16][16],counts[16];
 uint16_t maps[4][1024],physical_colors[256];
 uint16_t source_cells[4][32][32];
};
_Static_assert(sizeof(struct NativeBackground4)<=sizeof(((struct NativeBackgroundFrame*)0)->tiles)-32768,"4bpp staging overlay");
static inline struct NativeBackground4*native_background_4(const struct NativeBackgroundFrame*f){return (struct NativeBackground4*)((uint8_t*)f->tiles+32768);}
/* Each inactive BG frees a2KiB map block in both VRAM and staging RAM.
 * Pack active maps at the end; the preceding blocks hold32 more8bpp tiles
 * apiece. No active map or sprite VRAM is borrowed. */
static inline unsigned native_background_capacity(const struct NativeBackgroundFrame*f){return f->depth4?NATIVE_BG_TILES:(32u-!!f->hud_reserved-f->map_count)*32u-(f->hud_reserved==1?64:f->hud_reserved==2?128:f->hud_reserved?32:0);}
static inline const uint32_t*native_background_pixels(const struct NativeBackgroundFrame*f,unsigned id){
 if(f->depth4)return (const uint32_t*)f->tiles+id*8;
 return id<NATIVE_BG_BASE_TILES?f->tiles[id]:(const uint32_t*)f->maps[f->map_count]+(id-NATIVE_BG_BASE_TILES)*16;
}
static inline unsigned native_background_index(const struct NativeBackgroundFrame*f,unsigned id,unsigned pixel){
 const uint8_t*p=(const uint8_t*)native_background_pixels(f,id);if(!f->depth4)return p[pixel];
 const struct NativeBackground4*b=native_background_4(f);return b->indices[b->bank[id]][(p[pixel/2]>>((pixel&1)*4))&15];
}
/* Source planes use precompiled4bpp8x8 assets and fixed regional banks.
 * Hardware scroll aligns12px cells to the4px window grid. */
int native_background_prepare(struct NativeBackgroundFrame*,const struct NativeTileVideo*,const struct NativePaletteWorld*,struct NativeVideoPalette*,uint8_t auto_frame,int auto_enabled);
/* Update retained palette indices only if every shared index still agrees. */
int native_background_palette_update(struct NativeBackgroundFrame*,const struct NativePaletteWorld*,struct NativeVideoPalette*);
void native_background_stage_palette(struct NativeBackgroundFrame*,struct NativeVideoPalette*);
int native_background_commit(struct NativeBackgroundFrame*,struct NativeVideoPalette*);
/* Optional8KiB cache in otherwise idle staging, invalidated by its owner. */
void native_background_cache_storage(void*,unsigned generation);
/* Repack a freshly prepared canonical8bpp cache using largest color sets
 * first. Scratch aliases the now-idle16KiB FIX tail. Failure is non-mutating. */
int native_background_repack4(struct NativeBackgroundFrame*,void*scratch,unsigned hud_reservation);
#endif
