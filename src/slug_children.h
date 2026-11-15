#ifndef NATIVE_SLUG_CHILDREN_H
#define NATIVE_SLUG_CHILDREN_H
#include "slug_animation.h"
/* Scheduled constructors; each retains its original first/last child order. */
int slug_spawn_turret(struct SlugAnimationContext*,unsigned variant);
int slug_spawn_wheel(struct SlugAnimationContext*);
int slug_spawn_marker(struct SlugAnimationContext*);
#endif
