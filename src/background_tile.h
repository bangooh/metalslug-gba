#ifndef NATIVE_BACKGROUND_TILE_H
#define NATIVE_BACKGROUND_TILE_H
#include "tile_video.h"
/* Return an already scaled4x4 fragment. output is packed4bpp, two bytes per
 * row. Flip preserves the approved scaled tile, matching OBJ composition.
 * auto_frame and auto_enabled are the original video auto-animation state,
 * NOT World ticks. Disabling animation retains the complete authored tile. */
int native_background_block(const struct NativeTileCell*,uint8_t auto_frame,int auto_enabled,unsigned x,unsigned y,uint8_t output[8]);
/*28-bit exact block/palette/flip identity, or UINT32_MAX for bad metadata. */
uint32_t native_background_key(const struct NativeTileCell*,uint8_t,int,unsigned,unsigned);
#endif
