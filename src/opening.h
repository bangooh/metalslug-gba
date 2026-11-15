#ifndef NATIVE_OPENING_H
#define NATIVE_OPENING_H
#include "bosses.h"
enum OpeningEntry {OPENING_HUT,OPENING_TOWER,OPENING_BUNKER,OPENING_BRIDGE,OPENING_NEST,OPENING_SHED,OPENING_BARRIER,OPENING_ENTRY_COUNT};
int opening_spawn_scene(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,enum OpeningEntry,uint16_t config,uint8_t flags);
/* Unreferenced original aliases remain explicit, separate from authored records. */
struct NativeEntity*opening_flag(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*);
struct NativeEntity*opening_side(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*);
/*86586 damage+clear and86582 clear-only entry, blank FIRST child. */
int opening_clear_ordnance(struct WeaponWorld*,struct EntityRef,int damage);
#endif
