#ifndef NATIVE_FORT_H
#define NATIVE_FORT_H
#include "bosses.h"
enum FortEntry {FORT_ROOT0,FORT_ROOT1,FORT_ROOT2,FORT_ROOT3,FORT_ROOT4,FORT_PROP0,FORT_PROP1,FORT_PROP2,FORT_SIGN,FORT_WALL,FORT_HATCH5,FORT_HATCH6,FORT_QUAD,FORT_HELI,FORT_BARRIER,FORT_CAR};
int fort_spawn_scene(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,enum FortEntry,uint16_t config,uint8_t flags);
int fort_spawn_scene4_record(struct WeaponWorld*,struct EntityRef,unsigned index);
int fort_spawn_template(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
#endif
