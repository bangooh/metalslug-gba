#ifndef NATIVE_URBAN_H
#define NATIVE_URBAN_H
#include "bosses.h"
enum UrbanEntry {URBAN_BUILDING,URBAN_COLUMN,URBAN_COMPOUND,URBAN_WALL,URBAN_FRAGILE,URBAN_STALL,URBAN_HIT_STAGE,URBAN_BREAKABLE,URBAN_RANKED,URBAN_SMALL,URBAN_BLOCKER,URBAN_MULTISTAGE0,URBAN_MULTISTAGE1,URBAN_MULTISTAGE2,URBAN_MULTISTAGE3,URBAN_MULTISTAGE4,URBAN_ENTRY_COUNT};
int urban_spawn_scene(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,enum UrbanEntry,uint16_t config,uint8_t flags);
int urban_blink(struct WeaponWorld*,struct EntityRef);
struct NativeEntity*urban_debris_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,unsigned variant);
#endif
