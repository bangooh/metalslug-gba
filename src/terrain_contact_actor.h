#ifndef NATIVE_TERRAIN_CONTACT_ACTOR_H
#define NATIVE_TERRAIN_CONTACT_ACTOR_H
#include "player_actions.h"
/* Shared $030068/$0300BA/$03010C terrain follower storage. */
struct SlugContact {
 struct PlayerTransform transform;
 struct HitActor hit;
 int16_t vx,vy,ax,ay,slope,ground_tag;
 int16_t offset_x,old_x,old_y,surface_y,correction;
 uint16_t direction_mask;
 uint8_t grounded,drop_through,lifecycle_flags,platform_ids,entered;
 int8_t previous_direction;
};
#endif
