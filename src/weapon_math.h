#ifndef NATIVE_WEAPON_MATH_H
#define NATIVE_WEAPON_MATH_H
#include "weapon_animation.h"
uint8_t weapon_bullet_angle(uint8_t base,int8_t spread,uint8_t mode,int special_spread);
void weapon_vector(uint8_t angle,int16_t speed,int16_t*x,int16_t*y_up);
void weapon_integrate_free(struct WeaponActor*);
void weapon_flame_velocity(struct WeaponActor*,unsigned variant);
void weapon_rocket_seek(struct WeaponActor*,const struct HitActor*target,int vertical);
void weapon_trail_follow(struct WeaponActor*,const struct WeaponActor*,int fast);
void weapon_rocket_trail_init(struct WeaponActor*,const struct WeaponActor*);
void weapon_rocket_trail_tick(struct WeaponAnimationContext*);
#endif
