#ifndef NATIVE_HIT_SCENE_H
#define NATIVE_HIT_SCENE_H
#include "entities.h"
struct HitTarget {struct HitActor*actor;const struct HitDefense*defenses;unsigned count;};
struct HitScene {
 struct EntityPool *entities;
 struct EntityRef players[2],slug,enemy_group,projectile_group;
 int16_t camera_x,camera_y;
 HitDamageSink damage;
 void *world;
 /* Resolver extends the built-in player/arm/projectile views for new actor
  * families. A successful resolution may have no active defense. */
 int (*resolve)(void*,struct NativeEntity*,struct HitTarget*);
 uint8_t last_player,fault,pad[2];
};
/* $02843C..$02856C ordered target groups. Return success even on no contact. */
int hit_scene_attack(struct HitScene*,struct HitActor*,const struct HitAttack*,struct HitActor**selected);
#endif
