#include "weapon_animation.h"
static int fail(struct WeaponAnimationContext*c){c->fault=1;return 0;}
static uint32_t read_field(void*user,unsigned field,unsigned width){
 struct WeaponAnimationContext*c=user;struct WeaponActor*a=c->actor;
 if(field==WAF_TRAIL_DRAWING&&width==4)return a->trail_drawing;
 if(field==WAF_PARACHUTE_SLOW&&width==1)return a->parachute_slow;
 if(width!=2){fail(c);return 0;}
 switch(field){
  case WAF_TRAIL_ANGLE:return a->trail_angle;
  case WAF_VELOCITY_Y:return (uint16_t)a->vy;
  case WAF_EFFECT_X:return (uint16_t)a->effect_x;
  case WAF_EFFECT_Y:return (uint16_t)a->effect_y_up;
  case WAF_ATTACHED_OFFSET:return (uint16_t)a->attached_offset;
  default:fail(c);return 0;
 }
}
static int write_field(void*user,unsigned field,unsigned width,enum AnimationFieldOperation op,uint32_t value){
 struct WeaponAnimationContext*c=user;struct WeaponActor*a=c->actor;
 uint32_t old=read_field(user,field,width);if(c->fault)return 0;
 switch(op){case AN_FIELD_SET:break;case AN_FIELD_AND:value&=old;break;case AN_FIELD_OR:value|=old;break;case AN_FIELD_XOR:value^=old;break;default:return fail(c);}
 switch(field){
  case WAF_TRAIL_DRAWING:if(value>=65535)return fail(c);a->trail_drawing=(uint16_t)value;break;
  case WAF_TRAIL_ANGLE:a->trail_angle=(uint16_t)value;break;
  case WAF_VELOCITY_Y:a->vy=(int16_t)value;break;
  case WAF_EFFECT_X:a->effect_x=(int16_t)value;break;
  case WAF_EFFECT_Y:a->effect_y_up=(int16_t)value;break;
  case WAF_PARACHUTE_SLOW:a->parachute_slow=(uint8_t)value;break;
  case WAF_ATTACHED_OFFSET:a->attached_offset=(int16_t)value;break;
  default:return fail(c);
 }
 return 1;
}
static int bind(void*user,unsigned kind,uint32_t value){
 struct WeaponAnimationContext*c=user;
 if(kind!=AN_ATTACK)return fail(c);
 if(value==0xffffffffu)c->actor->attack=0;
 else if(value<WEAPON_ATTACK_BINDINGS)c->actor->attack=&weapon_attack_sets[value];
 else return fail(c);return 1;
}
static int callback(void*user,unsigned kind,struct Animation*animation){
 (void)animation;struct WeaponAnimationContext*c=user;struct WeaponActor*a=c->actor;
 if(kind==WAC_CLEAR_ATTACK){hit_begin_attack(&a->hit);return 1;}
 if(kind==WAC_PLAYER_LIFE_SOUND)return c->sound&&c->sound(c->world,a->owner?0x1087:0x1123,0)?1:fail(c);
 if(kind==WAC_ATTACK){
  if(!a->attack)return 1;
  if(!c->attack)return fail(c);
  for(unsigned i=0;i<a->attack->count;i++)if(!c->attack(c->world,&a->hit,&a->attack->records[i]))return fail(c);
  return 1;
 }
 enum WeaponEffect effect;
 switch(kind){case WAC_BLAST_A:effect=WEFFECT_BLAST_A;break;case WAC_BLAST_B:effect=WEFFECT_BLAST_B;break;case WAC_BLAST_C:effect=WEFFECT_BLAST_C;break;default:return fail(c);}
 int dx=(a->transform.facing&1)?-a->effect_x:a->effect_x;
 int dy=(a->transform.facing&2)?-a->effect_y_up:a->effect_y_up;
 struct PlayerTransform t={0};transform_integer_copy(&t,&a->transform);transform_integer_offset(&t,dx,dy);
 return c->effect&&c->effect(c->world,effect,&t)?1:fail(c);
}
static int sound(void*user,uint16_t command,int stop){
 struct WeaponAnimationContext*c=user;
 return c->sound&&c->sound(c->world,command,stop)?1:fail(c);
}
int weapon_animation_step(struct WeaponAnimationContext*c){
 struct WeaponActor*a=c->actor;
 if(c->fault)return 0;
 a->hit.x=(int16_t)(a->transform.x>>8);a->hit.y=transform_y_up(&a->transform);a->hit.flip=a->transform.facing;
 const struct AnimationOwner owner={.context=c,.read=read_field,.write=write_field,.bind=bind,.call=callback,.sound=sound};
 return animation_step(&a->animation,&weapon_animation_program,&owner);
}
int weapon_effect_init(struct WeaponAnimationContext*c,enum WeaponEffect effect,const struct PlayerTransform*t){
 struct WeaponActor*a=c->actor;unsigned sequence;
 *a=(struct WeaponActor){.active=1,.transform=*t,.trail_drawing=65535,
  .animation={.cursor=65535,.drawing=65535,.last_draw_cursor=65535},.platform_ids=255,.ground_slope=-1,.ground_tag=-1};
 switch(effect){
  case WEFFECT_PISTOL_CASING:sequence=WANI_PISTOL_CASING;a->palette=0x184;a->transform.priority=0xd210;break;
  case WEFFECT_PISTOL_CASING_UP:sequence=WANI_PISTOL_CASING_UP;a->palette=0x184;a->transform.priority=0xd210;break;
  case WEFFECT_ROCKET_CASING:sequence=WANI_ROCKET_CASING;a->palette=0x17c;a->transform.priority=0xd210;break;
  case WEFFECT_ROCKET_CASING_UP:sequence=WANI_ROCKET_CASING_UP;a->palette=0x17c;a->transform.priority=0xd210;break;
  case WEFFECT_SCENE3_DEBRIS:sequence=WANI_SCENE3_DEBRIS;a->palette=0x1bd;break;
  case WEFFECT_BODY:sequence=WANI_BODY_FX;a->palette=0x17c;a->transform.priority=0x2010;break;
  case WEFFECT_BLOOD:sequence=WANI_DEATH_BLOOD;a->palette=0x17a;a->transform.priority=0xd210;break;
  case WEFFECT_BLAST_A:sequence=WANI_BLAST_A;a->palette=0x163;a->transform.priority=0x2010;break;
  case WEFFECT_BLAST_B:sequence=WANI_BLAST_B;a->palette=0x163;a->transform.priority=0x2010;break;
  case WEFFECT_BLAST_C:sequence=WANI_BLAST_C;a->palette=0x163;a->transform.priority=0x2010;break;
  default:return fail(c);
 }
 a->kind=(uint8_t)effect;animation_begin(&a->animation,weapon_animation_entries[sequence]);return 1;
}
void weapon_effect_tick(struct WeaponAnimationContext*c){
 if(!c->actor->active)return;
 /* $02783A is render/scroll synchronization, not ballistic integration.
  * These effects move within their original drawings; no invented gravity. */
 if(weapon_animation_step(c))c->actor->active=0;
}
