#ifndef NATIVE_SIEGE_H
#define NATIVE_SIEGE_H
#include "bosses.h"
/* Scene-list entries, selected offline from the original placement records. */
enum SiegeEntry {SIEGE_GATE,SIEGE_ARCH,SIEGE_COLUMN0,SIEGE_COLUMN1,SIEGE_COLUMN2,SIEGE_REACTIVE,SIEGE_SOLID_A,SIEGE_SOLID_B,
 SIEGE_GUARD_ROOT,SIEGE_BOX,SIEGE_TILE_A,SIEGE_TILE_B,SIEGE_BOUNCE,SIEGE_ROW8,SIEGE_SINGLE,SIEGE_SINGLE_TILE,SIEGE_ROW9,
 SIEGE_BREAKABLE,SIEGE_FINALE,SIEGE_GUARDIAN,SIEGE_REWARD,SIEGE_REWARD_DECOR};
int siege_spawn_scene(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,enum SiegeEntry,uint16_t config,uint8_t display_flags);
int siege_spawn_scene3_record(struct WeaponWorld*,struct EntityRef,unsigned index);
int siege_spawn_template(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t params[8]);
#endif
