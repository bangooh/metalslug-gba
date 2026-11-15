#ifndef NATIVE_M4_H
#define NATIVE_M4_H
#include "bosses.h"
int m4_spawn_template(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
struct NativeEntity*free104_spawn(struct WeaponWorld*,struct EntityRef,enum EntityInsert,uint16_t,const struct PlayerTransform*,const uint8_t[8]);
#endif
