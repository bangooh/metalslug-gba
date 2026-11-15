#ifndef NATIVE_SLUG_AUX_ACTOR_H
#define NATIVE_SLUG_AUX_ACTOR_H
#include "weapon_animation.h"
/* Typed vehicle child storage, not a source CPU actor image. */
struct SlugAuxActor {
 struct WeaponActor actor;
 union {
  struct {
   int16_t offset_x,offset_y_up,speed;
   uint16_t angle,distance,range,palette_alternate,palette_impact,sprite_template;
   uint8_t layout,frame,counted,input_held;
  };
  struct {
   uint16_t direction,angular_speed,target_direction,travel;
   uint8_t variant,input,burst,cooldown,sequence,shot_phase;
  } turret;
  struct {uint16_t palettes[4],phase;uint8_t dead;} wheel;
  struct {int16_t old_x,old_y;uint16_t selector;uint8_t raw,material,air,delay_table;} surface;
 };
 uint8_t kind;
};
#endif
