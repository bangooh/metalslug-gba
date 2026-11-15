#ifndef NATIVE_SLUG_PROJECTILE_H
#define NATIVE_SLUG_PROJECTILE_H
#include "slug_animation.h"
enum SlugCannonKind {SCANNON_NORMAL,SCANNON_IDLE,SCANNON_LONG,SCANNON_LIGHT,SCANNON_DROP_A,SCANNON_DROP_B};
/* Source $0006FE order; initialization runs when the child is scheduled. */
int slug_spawn_cannon(struct SlugAnimationContext*,enum SlugCannonKind,struct PlayerSourceInput,uint8_t layout);
int slug_spawn_exhaust(struct SlugAnimationContext*,int always_angled);
#endif
