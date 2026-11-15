#ifndef NATIVE_LATE_GUARD_H
#define NATIVE_LATE_GUARD_H
#include "bosses.h"
struct NativeEntity*late_guard_spawn_choice(struct WeaponWorld*,struct EntityRef,const struct PlayerTransform*,unsigned choice);
int late_guard_spawn_template(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t params[8]);
#endif
