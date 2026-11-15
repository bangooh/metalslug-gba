#ifndef NATIVE_OBJECT_VIDEO_H
#define NATIVE_OBJECT_VIDEO_H
#include "scaled_drawing.h"
enum {NATIVE_OBJ_LIMIT=128,NATIVE_OBJ_TILE_LIMIT=1024,NATIVE_OBJ_PALETTE_LIMIT=16,NATIVE_OBJ_AFFINE_LIMIT=32};
enum NativeObjectFault {NOBJ_OK,NOBJ_METADATA,NOBJ_CAPACITY,NOBJ_PALETTES,NOBJ_TILES,NOBJ_ZOOM,NOBJ_STATE};
struct NativeObjectPlacement {
 /* Source-screen coordinates, after world/camera/perspective calculation.
  * order is a resolved FRONT-to-BACK key, not an untranslated source priority.
  * Caller resolves drawing offsets, effective flips and palette/private fade. */
 int32_t x,y;
 uint32_t order;
 const uint16_t *palette;
 uint8_t flip,priority,zoom_x,zoom_y;
};
struct NativeObjectEntry {uint32_t order;uint16_t attr0,attr1,attr2;};
struct NativeObjectUpload {const uint8_t*pixels;uint16_t tile,count;};
struct NativeObjectPaletteCache {const uint16_t*source;uint16_t mask;uint8_t bank,valid,map[16];};
struct NativeObjectFrame {
 uint32_t mutable_generation; /* 0: mutable bytes must always be uploaded. */
 struct NativeObjectEntry objects[NATIVE_OBJ_LIMIT];
 struct NativeObjectUpload uploads[NATIVE_OBJ_LIMIT];
 uint16_t palettes[NATIVE_OBJ_PALETTE_LIMIT][16];
 uint16_t palette_used[NATIVE_OBJ_PALETTE_LIMIT];
 /* OAM is only materialized by finish(). Reuse its staging bytes while
  * building the frame, then overwrite the scratch tables in one pass. */
 union {
  uint16_t oam[NATIVE_OBJ_LIMIT][4];
  struct {uint16_t uploads[256];struct NativeObjectPaletteCache palettes[16];} scratch;
 };
 int16_t affine[NATIVE_OBJ_AFFINE_LIMIT][2]; /* diagonal pa/pd, pb=pc=0 */
 uint16_t count,upload_count,tile_count,tile_budget;
 uint8_t palette_count,affine_count,fault,ready;
 uint16_t palette_protected;uint8_t palette_merges,palette_exact,palette_tolerance,palette_merge_error;
 uint16_t palette_spills; /* Pressure-only sharing; occupies former ARM tail padding. */
 uint16_t tile_compactions;
 uint8_t dense_layout; /* bit0: dense ROM layout; bit1: tile-only row clipping */
};
void native_object_begin(struct NativeObjectFrame*,unsigned tile_budget);
/* Call after writing palettes[]/palette_used[]/palette_count directly. */
void native_object_palette_touch(void);
void native_object_bridge_scratch(void*,unsigned);
int native_object_reserve_exact_palette(struct NativeObjectFrame*,const uint16_t*);
/* Entirely clipped/transparent maps consume no OAM/palettes/VRAM. Capacity
 * and missing metadata are explicit faults, never silent sprite dropping. */
int native_object_map(struct NativeObjectFrame*,unsigned,const struct NativeObjectPlacement*);
/* Mutable packed1D4bpp FIX staging; exact RGB palette reindexing allowed. */
int native_object_raw(struct NativeObjectFrame*,uint8_t*pixels,int x,int y,unsigned width,unsigned height,const uint16_t*palette,unsigned priority,uint32_t order);
/* Exact RGB packing for a complete logical palette mask; map[0] stays0. */
int native_object_palette_mapping(struct NativeObjectFrame*,const uint16_t*,unsigned mask,uint8_t map[16]);
/* Pixels already mapped into a claimed palette; no scan or second remap. */
int native_object_raw_mapped(struct NativeObjectFrame*,uint8_t*,int,int,unsigned,unsigned,unsigned bank);
/* Dedicated FIX bank preserves source indices through palette fades. */
int native_object_raw_fixed(struct NativeObjectFrame*,uint8_t*,int,int,unsigned,unsigned,const uint16_t*,unsigned bank);
/* Approved64x64 composite with its fixed origin(32,56). Same flip/affine
 * transform as source maps; no realtime pixel resizing or recolouring. */
int native_object_approved(struct NativeObjectFrame*,const uint8_t*,const struct NativeObjectPlacement*);
typedef int (*NativeObjectPartResolver)(void*,const struct NativeDrawPart*,struct NativeObjectPlacement*);
int native_object_drawing(struct NativeObjectFrame*,const struct Animation*,NativeObjectPartResolver,void*);
int native_object_finish(struct NativeObjectFrame*);
int native_object_resident_stage(struct NativeObjectFrame*);
/* Caller supplies inactive VRAM scratch that stays untouched until commit.
 * Precopy dirty ROM uploads in main; residency records retain ROM identities. */
unsigned native_object_prestage(struct NativeObjectFrame*,uintptr_t scratch,unsigned capacity,unsigned background_bytes);
void native_object_resident_reset(void);
/* Real hardware transfer. Serialized main/VBlank caller owns DMA3. Requires
 * blanking and mode0..2; does not install IRQ handlers or initialize BGs. */
void native_object_stage_palette(struct NativeObjectFrame*);
int native_object_commit(struct NativeObjectFrame*);
#endif
