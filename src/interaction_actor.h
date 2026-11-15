#ifndef NATIVE_INTERACTION_ACTOR_H
#define NATIVE_INTERACTION_ACTOR_H
#include "weapon_animation.h"
#include "mission_actor.h"
struct InteractionActor {
 struct WeaponActor actor;
 uint8_t params[8],entry,kind,grounded,rescuer,rank,key,scale_x,scale_y;
 int16_t support_slope;
 uint16_t sibling_drawing,walk_timer;
 MissionWatchFn climb_watch;
 struct {uint16_t angle,cooldown,acceleration,sprite_template;int16_t offset_x,offset_y;int8_t owner;uint8_t attached,burst,muzzle,row,pose;} gun;
};
#endif
