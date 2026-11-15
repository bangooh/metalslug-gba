#include "interactions.h"
#include "items.h"
#include "slug_surface.h"
/* $03FC38..$0403C8. Entry and frame scheduling deliberately stay separate. */
enum State {NEW,BOUND,FREED,CHEER,STACK,STRUGGLE,BREAK,STACK_CHILD,GREET,
 IDLE,IDLE_WAVE,FREE_INIT,PICK_WALK,WALK,TURN,STOP,JUMP,CLIMB,CROUCH,HOP,FALL,DESCEND,LAND,
 RESCUED,BOW,WAVE,RUN,EXIT};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static int16_t word(int x){return (int16_t)(uint16_t)x;}
static void schedule(struct InteractionActor*q,enum State state){q->actor.state=(uint8_t)state;q->entry=1;}
static int swapped(struct WeaponWorld*w,struct InteractionActor*q,enum WeaponMove policy){
 int16_t saved=q->actor.ground_slope;q->actor.ground_slope=q->support_slope;
 int r=interaction_move(w,&q->actor,policy);q->support_slope=q->actor.ground_slope;q->actor.ground_slope=saved;return r;
}
static int physics(struct WeaponWorld*w,struct InteractionActor*q){
 struct WeaponActor*a=&q->actor;if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;
 int r=terrain_weapon_foot_contact(w->interactions->terrain,a);if(r<0)return interaction_fault(w);
 q->grounded=r?255:0;
 if(q->grounded)return swapped(w,q,WMOVE_GROUND)>=0;
 r=swapped(w,q,WMOVE_ITEM);if(r<0)return 0;q->grounded=r?255:0;return 1;
}
static int live(const struct PlayerBodyState*p){return p&&!p->life_retired&&(p->actor.lifecycle_flags&128);}
static int rescue_overlap(struct WeaponWorld*w,const struct WeaponActor*a){
 if(!w->items||!w->items->player_query_enabled)return -1;int slot=-1;
 for(unsigned i=0;i<2;i++){
  struct PlayerBodyState*p=w->players[i];if(!live(p)||(p->actor.hit.flags&9))continue;
  if((uint16_t)word((p->actor.transform.x>>8)-(a->transform.x>>8)+16)>=32||
     (uint16_t)word(transform_y_up(&p->actor.transform)-transform_y_up(&a->transform)+16)>=56)continue;
  if(slot<0||(uint16_t)word((p->actor.transform.x>>8)-w->camera_x)>=(uint16_t)word((w->players[slot]->actor.transform.x>>8)-w->camera_x))slot=(int)i;
 }return slot;
}
static struct NativeEntity*child(struct WeaponWorld*w,struct NativeEntity*e,enum EntityInsert order,enum State state){
 struct PlayerTransform empty={0};
 struct NativeEntity*n=interaction_allocate(w,entity_ref(e),order,state==STACK_CHILD?&empty:&e->payload.interaction.actor.transform,tick);
 if(n)schedule(&n->payload.interaction,state);return n;
}
static int initialize(struct WeaponWorld*w,struct NativeEntity*e){
 struct InteractionActor*q=&e->payload.interaction;struct WeaponActor*a=&q->actor;
 if(q->params[2])a->transform.facing|=1;a->palette=0x182;interaction_priority(w,a,0x8000,0x18);
 a->vx=a->vy=a->ax=a->ay=0;
 int r=terrain_weapon_ground_test(w->interactions->terrain,a);if(r<0)return interaction_fault(w);q->grounded=r?255:0;
 if(!pow_spawn_surface(w,entity_ref(e),&a->transform))return 0;
 if(a->variant==POW_STACKED){
  interaction_defense(q,IND_STACK0+(q->params[1]<4?q->params[1]:3));
  do{struct NativeEntity*n=child(w,e,ENTITY_FIRST_CHILD,STACK_CHILD);if(!n)return 0;n->payload.interaction.rank=q->params[1];}while(q->params[1]--);
  schedule(q,STACK);
 }else schedule(q,a->variant==POW_BOUND?BOUND:a->variant==POW_IDLE?IDLE:FREE_INIT);return 1;
}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct InteractionActor*q=&e->payload.interaction;struct WeaponActor*a=&q->actor;
 for(unsigned budget=0;budget<5;budget++){
  enum State state=(enum State)a->state;q->entry=0;
  switch(state){
  case NEW:if(!initialize(w,e))return 0;continue;
  case BOUND:interaction_start(q,INAN_POW_BOUND);break;
  case FREED:interaction_start(q,INAN_POW_FREED);break;
  case CHEER:a->palette=0x182;interaction_start(q,INAN_POW_CHEER);break;
  case STACK:interaction_start(q,INAN_POW_STACK);break;
  case STRUGGLE:interaction_start(q,INAN_POW_STRUGGLE);break;
  case BREAK:interaction_start(q,INAN_POW_BREAK);break;
  case STACK_CHILD:{
   struct NativeEntity*parent=entity_get(w->entities,e->parent);if(!parent||parent->kind!=ENTITY_INTERACTION)return interaction_fault(w);
   struct InteractionActor*r=&parent->payload.interaction;a->palette=0x182;q->scale_x=r->scale_x;q->scale_y=r->scale_y;
   transform_integer_copy(&a->transform,&r->actor.transform);transform_integer_offset(&a->transform,0,q->rank*48);break;
  }
  case GREET:interaction_start(q,q->rank?INAN_POW_GREET_HIGH:INAN_POW_GREET);break;
  case IDLE:interaction_start(q,INAN_POW_IDLE);a->vx=a->vy=0;break;
  case IDLE_WAVE:interaction_start(q,INAN_POW_IDLE_WAVE);break;
  case FREE_INIT:a->palette=0x181;schedule(q,PICK_WALK);continue;
  case PICK_WALK:
   q->grounded=0;interaction_defense(q,IND_FREE);a->hit.flags&=(uint8_t)~9u;
   if(q->params[0]){if(q->params[0]==1)a->transform.facing&=254;else a->transform.facing|=1;}
   q->walk_timer=15;schedule(q,WALK);continue;
  case WALK:interaction_start(q,INAN_POW_WALK);a->vx=a->transform.facing&1?0x180:-0x180;break;
  case TURN:interaction_defense(q,IND_FREE);interaction_start(q,INAN_POW_TURN);a->vx=a->vy=0;break;
  case STOP:interaction_start(q,INAN_POW_STOP);a->vx=0;break;
  case JUMP:interaction_start(q,INAN_POW_JUMP);break;
  case CLIMB:interaction_start(q,INAN_POW_CLIMB);a->ay=0;a->vy=0x154;break;
  case CROUCH:interaction_start(q,INAN_POW_CROUCH);break;
  case HOP:interaction_start(q,INAN_POW_HOP);a->vx=a->transform.facing&1?0x333:-0x333;a->vy=0x4c9;a->ay=-245;a->ax=0;break;
  case FALL:interaction_start(q,INAN_POW_FALL);a->vy=a->ay=0;break;
  case DESCEND:interaction_start(q,INAN_POW_DESCEND);a->ay=0;a->vy=-0x154;break;
  case LAND:interaction_start(q,INAN_POW_LAND);a->vx=a->vy=0;q->grounded=0;break;
  case RESCUED:
   if(!interaction_sound(w,0x113a))return 0;
   if(live(w->players[q->rescuer]))w->interactions->rescued[q->rescuer]++;
   a->defense=0;a->defense_count=0;interaction_start(q,INAN_POW_WALK);
   if(word(192-word((a->transform.x>>8)-w->camera_x))>=0){a->transform.facing|=1;a->vx=0x700;}
   else {a->transform.facing&=254;a->vx=-0x700;}a->timer=4;break;
  case BOW:interaction_start(q,INAN_POW_BOW);a->vx=0;break;
  case WAVE:interaction_start(q,INAN_POW_WAVE);a->vx=0;break;
  case RUN:interaction_start(q,INAN_POW_RUN);a->transform.facing&=254;a->vx=-0x400;break;
  case EXIT:a->ttl=30;break;
  default:return interaction_fault(w);
  }return 1;
 }return interaction_fault(w);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;struct InteractionActor*q=&e->payload.interaction;struct WeaponActor*a=&q->actor;
 if(q->entry&&!enter(w,e))return;enum State state=(enum State)a->state;int done=0,r;int16_t height;
 switch(state){
 case STACK_CHILD:{
  if(interaction_move(w,a,WMOVE_SYNC)<0)return;struct NativeEntity*parent=entity_get(p,e->parent);
  if(!parent||parent->deleting){entity_delete(p,e);return;}
  struct InteractionActor*rq=&parent->payload.interaction;if(rq->actor.hit.flags&8)schedule(q,GREET);
  animation_drawing(&a->animation,&interaction_program,rq->sibling_drawing);
  if(!animation_output_direct(&a->animation,AN_OUTPUT_DIRECT)){interaction_fault(w);return;}
  if(word((a->transform.x>>8)-w->camera_x+16)<0)entity_delete(p,e);return;
 }
 case CHEER:case GREET:
  if(interaction_move(w,a,WMOVE_SYNC)<0)return;if(interaction_animate(w,e)){entity_delete(p,e);return;}
  if(state==CHEER)return;break;
 case BOUND:case IDLE:
  if(!physics(w,q))return;interaction_animate(w,e);if(a->hit.flags&8)schedule(q,state==BOUND?FREED:IDLE_WAVE);break;
 case STACK:
  if(interaction_move(w,a,WMOVE_POINT)<0)return;interaction_animate(w,e);if(a->hit.flags&8)schedule(q,STRUGGLE);break;
 case STRUGGLE:
  if(!physics(w,q))return;if(q->grounded)schedule(q,BREAK);interaction_animate(w,e);break;
 case FREED:case BREAK:
  if(interaction_move(w,a,WMOVE_SYNC)<0)return;
  if(interaction_animate(w,e)){schedule(q,FREE_INIT);if(state==FREED&&!child(w,e,ENTITY_LAST_CHILD,CHEER))return;}break;
 case IDLE_WAVE:
  if(swapped(w,q,WMOVE_ITEM)<0)return;if(interaction_animate(w,e))schedule(q,FREE_INIT);break;
 case WALK:{
  if(!physics(w,q))return;
  if(!q->grounded)schedule(q,TURN);else if((a->hit.events&32)||(!q->params[0]&&!q->walk_timer--))schedule(q,STOP);
  int ascending;if(climb_target_crossing(&w->interactions->climb,a,&q->key,&ascending))schedule(q,ascending?JUMP:FALL);
  interaction_animate(w,e);break;
 }
 case TURN:
  r=swapped(w,q,WMOVE_ITEM);if(r<0)return;q->grounded=r?255:0;if(q->grounded)schedule(q,WALK);interaction_animate(w,e);break;
 case STOP:
  if(!physics(w,q))return;if(interaction_animate(w,e)){a->transform.facing^=1;q->walk_timer=30;schedule(q,WALK);}break;
 case JUMP:a->ay=a->vy=0;
  if(!climb_target_follow(w,a,q->key,&height))return;if(interaction_animate(w,e))schedule(q,CLIMB);break;
 case CLIMB:
  if(!climb_target_follow(w,a,q->key,&height))return;
  if(height<0)schedule(q,PICK_WALK);else if((uint16_t)word(height-36)<(uint16_t)word(transform_y_up(&a->transform)+w->camera_y))schedule(q,CROUCH);
  if(interaction_animate(w,e))interaction_start(q,INAN_POW_CLIMB);break;
 case CROUCH:
  if(interaction_move(w,a,WMOVE_SYNC)<0)return;if(interaction_animate(w,e))schedule(q,HOP);break;
 case HOP:
  a->vx=word(a->vx-(a->vx>>4));if(!physics(w,q))return;if(interaction_animate(w,e))schedule(q,PICK_WALK);break;
 case FALL:
  if(!climb_target_follow(w,a,q->key,&height))return;if(interaction_animate(w,e))schedule(q,DESCEND);break;
 case DESCEND:
  if(!climb_target_follow(w,a,q->key,&height))return;if(interaction_animate(w,e))schedule(q,DESCEND);
  if(height<0)schedule(q,TURN);r=terrain_weapon_foot_contact(w->interactions->terrain,a);if(r<0){interaction_fault(w);return;}if(!r)schedule(q,LAND);break;
 case LAND:if(!physics(w,q))return;if(interaction_animate(w,e))schedule(q,PICK_WALK);break;
 case RESCUED:
  if(!physics(w,q))return;interaction_animate(w,e);if(!--a->timer){a->transform.facing^=1;schedule(q,q->params[3]?BOW:WAVE);}break;
 case BOW:case WAVE:
  if(!physics(w,q))return;done=interaction_animate(w,e);if(done)schedule(q,state==BOW?WAVE:RUN);break;
 case RUN:if(!physics(w,q))return;interaction_animate(w,e);if(a->hit.events&32)schedule(q,EXIT);break;
 case EXIT:
  if(!physics(w,q))return;a->ttl--;if(a->ttl&1)interaction_animate(w,e);if(!a->ttl){entity_delete(p,e);return;}break;
 default:interaction_fault(w);return;
 }
 if(state>=WALK&&state<=LAND){int slot=rescue_overlap(w,a);if(slot>=0){q->rescuer=(uint8_t)slot;schedule(q,RESCUED);}}
 if(state>=WALK&&(a->hit.flags&8)){entity_delete(p,e);return;}
 if(interaction_outside(w,a,pow_bounds,1))entity_delete(p,e);
}
struct NativeEntity*pow_spawn_entity(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,enum PowEntry entry,const struct PlayerTransform*t,const uint8_t params[8]){
 if((unsigned)entry>POW_FREE){interaction_fault(w);return 0;}
 struct NativeEntity*e=interaction_allocate(w,parent,order,t,tick);if(!e)return 0;struct InteractionActor*q=&e->payload.interaction;
 q->actor.variant=(uint8_t)entry;if(params)for(unsigned i=0;i<8;i++)q->params[i]=params[i];return e;
}

int pow_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,enum PowEntry entry,const struct PlayerTransform*t,const uint8_t params[8]){return pow_spawn_entity(w,parent,order,entry,t,params)!=0;}
