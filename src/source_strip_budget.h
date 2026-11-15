#ifndef NATIVE_SOURCE_STRIP_BUDGET_H
#define NATIVE_SOURCE_STRIP_BUDGET_H
#include "object_video.h"
#include "scene_video.h"
/* Source resource accounting, separate from native OAM/VRAM. Horizontal
 * clipping counts transparent and vertically offscreen strips too. */
struct NativeSourceStripBudget {
 uint16_t strips[2],records,keys;
 uint16_t record_drops,key_drops,empty_maps;
 uint8_t fault;
};
void native_source_strips_begin(struct NativeSourceStripBudget*);
/* 1 admitted map;0 original queue/clip rejection;-1 malformed metadata.
 * Original380-record exhaustion can admit a prefix of the final map. */
int native_source_strips_map(struct NativeSourceStripBudget*,unsigned map,const struct NativeObjectPlacement*,int forced);
/* Count cached only after a complete, validated map scan; budgets remain
 * independent for original submissions and the rendered subset. */
int native_source_strips_cached(struct NativeSourceStripBudget*,unsigned strips,int forced);
int native_source_strips_free(const struct NativeSourceStripBudget*,const struct SceneSpritePools*,uint16_t out[2]);
#endif
