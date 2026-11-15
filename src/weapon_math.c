#include "weapon_math.h"
static int16_t word(int value){return (int16_t)(uint16_t)value;}
static int32_t coordinate(int32_t value){
 return (int16_t)(value>>8)*256+(int32_t)((uint32_t)value&255);
}
uint8_t weapon_bullet_angle(uint8_t base,int8_t spread,uint8_t mode,int special_spread){
 /* $0309AE..$030AA4, with the three distinct original spread policies. */
 unsigned value=(uint8_t)spread;int angle=base&0xf8;
 if(special_spread||mode==5){
  if(value&8)value=(~value)&7;
  angle+=((int)((value+4)&7)-4)*2;
 }else if(mode==1){angle+=(int)((value&1)*2)-1;angle+=(int)(value&3)-2;}
 else if(mode==2||mode==3)angle+=(int)(value&3)-2;
 else if(mode==0)angle+=(int)((value+4)&7)-4;
 return (uint8_t)angle;
}
void weapon_vector(uint8_t angle,int16_t speed,int16_t*x,int16_t*y_up){
 /* $013C0E: original signed Q8 trigonometric tables and arithmetic shift. */
 *x=(int16_t)(((int32_t)speed*weapon_cosine[angle])>>8);
 *y_up=(int16_t)(((int32_t)speed*weapon_sine[angle])>>8);
}
void weapon_integrate_free(struct WeaponActor*a){
 /* $09C072: direct integration, no acceleration or velocity clamping. */
 a->transform.x=coordinate(a->transform.x+a->vx);
 a->transform.y=131072-coordinate(131072-a->transform.y+a->vy);
}
void weapon_flame_velocity(struct WeaponActor*a,unsigned variant){
 /* $09C30A: vertical and horizontal fire have different drag/curvature. */
 if(variant&1){
  a->vx=word(a->vx-(a->vx>>4));
  int16_t delta=word(0x200-a->vy);a->vy=word(a->vy+(delta>>6));
 }else{
  a->vx=word(a->vx-(a->vx>>5));
  int16_t delta=word(0x200-a->vy);a->vy=word(a->vy+(delta>>3));
 }
}
void weapon_rocket_seek(struct WeaponActor*a,const struct HitActor*target,int vertical){
 /* $09C184/$09C1F4 use the selected actor's origin, not overlap center. */
 if(vertical){
  int16_t delta=word(target->x-(a->transform.x>>8));
  delta=word((uint16_t)delta<<3);delta=word(delta-a->vx);a->ax=(int16_t)(delta>>3);
 }else{
  int16_t target_y=word(target->y+30);
  int16_t delta=word(target_y-transform_y_up(&a->transform));
  delta=word((uint16_t)delta<<3);delta=word(delta-a->vy);a->ay=(int16_t)(delta>>3);
 }
}
void weapon_trail_follow(struct WeaponActor*a,const struct WeaponActor*parent,int fast){
 /* $09C410/$09C474: 3/8 or 3/4 integer following. Graphic map is an offline
  * native drawing ID; no original sprite pointer is dereferenced on GBA. */
 unsigned shift=fast?2:3;
 int16_t dx=word((parent->transform.x>>8)-(a->transform.x>>8));dx=(int16_t)(dx>>shift);
 int16_t dy=word(transform_y_up(&parent->transform)-transform_y_up(&a->transform));dy=(int16_t)(dy>>shift);
 transform_integer_offset(&a->transform,word(dx*3),word(dy*3));
 a->transform.priority=(uint16_t)(parent->transform.priority-1);
 animation_drawing(&a->animation,&weapon_animation_program,parent->trail_drawing);
 animation_output_direct(&a->animation,AN_OUTPUT_DIRECT);
}
void weapon_rocket_trail_init(struct WeaponActor*a,const struct WeaponActor*parent){
 /* $09C0A4. The source's vertical contribution reads the child's Y velocity
  * (zero after allocation), while X reads the parent's velocity twice. */
 struct PlayerTransform t=parent->transform;
 transform_integer_copy(&t,&parent->transform);t.priority=0x2010;
 *a=(struct WeaponActor){.active=1,.transform=t,.palette=0x17c,.trail_drawing=65535,
  .animation={.cursor=65535,.drawing=65535,.last_draw_cursor=65535},.platform_ids=255,.ground_slope=-1,.ground_tag=-1};
 int16_t vx,vy;weapon_vector((uint8_t)parent->trail_angle,0x400,&vx,&vy);
 if(parent->transform.facing&1)vx=word(-vx);
 a->vx=word(vx+word(parent->vx*2));a->vy=vy;
 animation_begin(&a->animation,weapon_animation_entries[WANI_BODY_FX]);
}
void weapon_rocket_trail_tick(struct WeaponAnimationContext*c){
 struct WeaponActor*a=c->actor;if(!a->active)return;
 weapon_integrate_free(a);a->vx=word(a->vx-(a->vx>>4));a->vy=word(a->vy-(a->vy>>4));
 if(weapon_animation_step(c))a->active=0;
}
