#include "flight_path.h"
#include "../generated/boss-programs.h"
static int32_t coordinate(int32_t v){return (int16_t)(v>>8)*256+(int32_t)((uint32_t)v&255);}
static void integrate(struct PlayerTransform*t,uint8_t angle,int16_t speed){
 /* $0791E0 rounds signed products with +127/+128 before dropping eight
  * bits. This differs from the ordinary projectile polar helper. */
 int32_t y=(int32_t)boss_trig[0][angle]*speed,x=(int32_t)boss_trig[1][angle]*speed;
 y=(y+(y<0?128:127))>>8;x=(x+(x<0?128:127))>>8;
 t->x=coordinate(t->x+x);t->y=131072-coordinate(131072-t->y+y);
}
int flight_step(struct PlayerTransform*t,struct FlightState*s,int16_t speed,const struct FlightProgram*p,const struct FlightOwner*o){
 uint16_t remaining=(uint16_t)speed;
 for(unsigned budget=0;budget<1024;budget++){
  if(s->cursor>=p->count)return -1;const struct FlightCommand*c=&p->commands[s->cursor];
  switch(c->op){
  case FP_ANGLE:s->angle=(uint16_t)(c->angle<<8);s->cursor=c->next;break;
  case FP_JUMP:s->cursor=c->target;break;
  case FP_FIELD:if(!o->field||!o->field(o->context,c->target,c->width,c->value))return -1;s->cursor=c->next;break;
  case FP_CALL:if(!o->call||!o->call(o->context,c->target))return -1;s->cursor=c->next;break;
  case FP_SOUND:if(!o->sound||!o->sound(o->context,(uint16_t)c->value))return -1;s->cursor=c->next;break;
  case FP_WAIT:{
   /* The word counter overlaps the high word of the distance accumulator. */
   uint16_t count=(uint16_t)((s->distance>>16)+1);s->distance=(s->distance&65535)|((uint32_t)count<<16);
   if(c->length<=count){s->distance&=65535;s->cursor=c->next;}return 0;
  }
  case FP_FINISH:integrate(t,(uint8_t)(s->angle>>8),(int16_t)remaining);return 1;
  case FP_DISTANCE:case FP_ARC:{
   if(!remaining)return 0;
   uint32_t length=(uint32_t)c->length<<(c->op==FP_DISTANCE?16:8),delta=length-(s->distance&0xffffff00u);
   if(delta<0x1000000u&&remaining>=(uint16_t)(delta>>8)){
    uint16_t consumed=(uint16_t)(delta>>8);remaining=(uint16_t)(remaining-consumed);s->distance&=255;
    integrate(t,(uint8_t)(s->angle>>8),(int16_t)consumed);
    if(c->op==FP_DISTANCE)s->cursor=c->next;
    else{uint8_t angle=(uint8_t)((s->angle>>8)+c->increment);s->angle=(uint16_t)((s->angle&255)|(angle<<8));if(angle==c->angle)s->cursor=c->next;}
   }else{s->distance+=(uint32_t)remaining<<8;integrate(t,(uint8_t)(s->angle>>8),(int16_t)remaining);return 0;}break;
  }
  default:return -1;
  }
 }return -1;
}
