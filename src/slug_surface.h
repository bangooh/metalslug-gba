#ifndef NATIVE_SLUG_SURFACE_H
#define NATIVE_SLUG_SURFACE_H
#include "slug_animation.h"
int vehicle_spawn_surface(struct WeaponWorld*,struct EntityRef,const struct PlayerTransform*);
int slug_spawn_surface(struct SlugAnimationContext*);
int pow_spawn_surface(struct WeaponWorld*,struct EntityRef,const struct PlayerTransform*);
#endif
