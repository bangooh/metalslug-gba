#ifndef NATIVE_ITEM_ACTOR_H
#define NATIVE_ITEM_ACTOR_H
#include "weapon_animation.h"
/* Shared payload for world pickups, score controllers and score glyphs. */
struct ItemActor {
 struct WeaponActor actor;
 uint16_t palettes[3];
 uint8_t palette_count,params[8],scale_x,scale_y;
 uint8_t entry,kind,counted,sequence,sequence_pos,motion_row,taken_variant;
 uint8_t pending,glyph,popup_kind,popup_special;
 uint16_t watchdog;
 uint32_t packed_score;
 int8_t score_spacing_x; /* Final GBA pixels; numeric popup presentation only. */
};
#endif
