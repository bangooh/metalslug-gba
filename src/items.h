#ifndef NATIVE_ITEMS_H
#define NATIVE_ITEMS_H
#include "weapons.h"
struct SlugSharedState;
enum ItemKind {
 ITEM_HMG,ITEM_ROCKET,ITEM_SHOTGUN,ITEM_FLAME,ITEM_CYCLE,
 ITEM_AMMO,ITEM_GRENADES,ITEM_REPAIR,
 ITEM_FOOD_FIRST,ITEM_FOOD_LAST=ITEM_FOOD_FIRST+19,
 ITEM_TIMED_FOOD,ITEM_ROT_FOOD,ITEM_THROWN_FOOD,ITEM_KIND_COUNT
};
struct ItemWorld {
 struct SlugSharedState *slug;
 uint32_t scores[2]; /* Decimal adaptation of source BCD, saturating at 99,999,999. */
 uint8_t player_query_enabled,combo_timer,combo_level,count_previous,count_current,fault;
};
/* Typed scene/drop entry points. Params are source $98..$9F, default FF.
 * Constructors and score changes run when their scheduled entity executes.
 * These are simulation APIs; scene callers and rendering are separate. */
int item_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,enum ItemKind,
 const struct PlayerTransform*,const uint8_t params[8],int counted);
int item_spawn_drop(struct WeaponWorld*,const struct PlayerTransform*,uint8_t type,
 uint8_t quantity,uint8_t lifetime,int at_parent,int counted);
/* Original DECIMAL template IDs 284..315 (0x11C..0x13B) and 316.
 * 292 is the separate Slug request; 293 now dispatches the breakable item
 * crate through the interaction group. Unknown/unimplemented templates return 0, without a substitute. */
int item_spawn_template(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t template_id,
 const struct PlayerTransform*,const uint8_t params[8]);
int item_spawn_combo_controller(struct WeaponWorld*,struct EntityRef,enum EntityInsert);
/* $09BA78 copies both attribution bytes; $09B90A spawns at parent Y+4. */
int item_spawn_thrown_food(struct WeaponWorld*,struct EntityRef,const struct PlayerTransform*,uint8_t owner,uint8_t damage_owner);
int item_respawn_below(struct WeaponWorld*,const struct PlayerTransform*,uint8_t type,uint8_t quantity,uint8_t lifetime);
int item_score_popup(struct WeaponWorld*,struct EntityRef,const struct PlayerTransform*,uint8_t owner,
 int table_notice,uint8_t kind,uint8_t special);
/* $051A44's unsigned packed decimal addition; negative owner means no score. */
int item_score_add(struct ItemWorld*,uint8_t owner,uint32_t packed_bcd);
#endif
