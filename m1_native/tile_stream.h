#ifndef M1_TILE_STREAM_H
#define M1_TILE_STREAM_H
#include <stdint.h>
enum { M1_ROM_TILES=18432, M1_VRAM_TILES=1024, M1_HASH_SIZE=2048, M1_MAX_REFS=4*31*21 };
struct M1TileUpload { uint16_t rom_tile, vram_tile; };
struct M1TileStream {
 uint32_t hash[M1_HASH_SIZE];
 uint16_t owner[M1_VRAM_TILES];
 uint8_t pinned[M1_VRAM_TILES], requested[M1_ROM_TILES/8];
 struct M1TileUpload uploads[M1_VRAM_TILES];
 uint16_t upload_count, capacity;
 /* v170: requested[] is all-zero between group_fits calls unless set. */
 uint8_t requested_dirty;
};
/* Tile 0 is transparent and permanently resident. All tile bytes are
 * final GBA tile bytes in ROM; uploads need no pixel conversion. */
unsigned m1_stream_find(const struct M1TileStream*, unsigned id);
int m1_stream_init(struct M1TileStream*, unsigned capacity);
/* Call only after the previous upload list has been committed.
 * Invalid/over-budget requests leave residency unchanged. */
int m1_stream_plan(struct M1TileStream*, const uint16_t*, unsigned count);
/* Unique nonzero IDs already collected while checking the layer groups,
 * stored in uploads[0..count).rom_tile. Shares the same residency policy. */
int m1_stream_plan_unique(struct M1TileStream*, unsigned count);
#endif
