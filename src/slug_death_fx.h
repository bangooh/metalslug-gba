#ifndef NATIVE_SLUG_DEATH_FX_H
#define NATIVE_SLUG_DEATH_FX_H
#include "slug_animation.h"
/* $02FF22: sound, first-child explosion, last-child debris spawner. */
int slug_spawn_death_fx(struct SlugAnimationContext*);
int slug_spawn_self_destruct_blast(struct SlugAnimationContext*);
int slug_spawn_death_smoke(struct SlugAnimationContext*);
#endif
