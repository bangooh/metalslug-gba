#ifndef NATIVE_FIXED_OBJECT_H
#define NATIVE_FIXED_OBJECT_H
#include "object_video.h"
#include "tile_video.h"
#include "palette_video.h"
struct NativeFixedObjects {uint32_t pixels[8192];uint32_t revision;uint16_t bytes,banks;uint8_t entry_banks[128];uint8_t fault,cached,upload_pending,suffix,storage_generation,bottom_clean;};
/* FIX stays above all four source backgrounds without requiring a fifth
 * GBA BG. It shares the real OBJ tile/palette/OAM limits with actors. */
int native_fixed_objects(struct NativeFixedObjects*,struct NativeObjectFrame*,const struct NativeTileVideo*,const struct NativePaletteWorld*);
int native_fixed_fallback(struct NativeFixedObjects*,struct NativeObjectFrame*,const struct NativeTileVideo*,const struct NativePaletteWorld*);
int native_fixed_refresh(struct NativeFixedObjects*,struct NativeObjectFrame*,const struct NativeTileVideo*,const struct NativePaletteWorld*);
int native_fixed_prefix(struct NativeFixedObjects*,struct NativeObjectFrame*,const struct NativeTileVideo*,const struct NativePaletteWorld*);
int native_fixed_suffix(struct NativeFixedObjects*,struct NativeObjectFrame*,const struct NativeTileVideo*,const struct NativePaletteWorld*);
int native_fixed_compact(struct NativeFixedObjects*,struct NativeObjectFrame*);
#endif
