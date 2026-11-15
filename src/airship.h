#ifndef NATIVE_AIRSHIP_H
#define NATIVE_AIRSHIP_H
#include "bosses.h"
enum AirshipEntry {AIRSHIP_PROP0,AIRSHIP_PROP1,AIRSHIP_PROP2,AIRSHIP_CAMP,AIRSHIP_BARRELS,AIRSHIP_ROCKRAIN,AIRSHIP_BODY,AIRSHIP_TOWER_A,AIRSHIP_TOWER_B,AIRSHIP_CRATE0,AIRSHIP_CRATE1,AIRSHIP_CRATE2,AIRSHIP_CRATE3,AIRSHIP_CRATE4,AIRSHIP_SOLID0,AIRSHIP_SOLID1,AIRSHIP_GUN};
int airship_spawn_scene(struct WeaponWorld*,struct EntityRef,enum EntityInsert,const struct PlayerTransform*,enum AirshipEntry,uint8_t);
int airship_spawn_scene5_record(struct WeaponWorld*,struct EntityRef,unsigned);
int airship_spawn_template(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
#endif
