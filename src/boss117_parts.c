#include "boss117_internal.h"
/* $081CD6..$082454: six destructible plates and three separately scheduled
 * weapons. The shared timer is intentionally decremented twice in attacks. */
static int16_t rank_pair(struct WeaponWorld*w,struct EnemyActor*q,unsigned first){return b117_rank(w,q,first+(q->boss117.upgraded?1:0));}
static int side_idle(struct EnemyActor*q){q->boss117.selector=0;b117_start(q,B117A_SIDE_IDLE);q->actor.state=B117_SIDE_IDLE;return 1;}
int b117_parts_enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned s=a->state;q->entry=0;struct EnemyActor*p;struct NativeEntity*n;
 switch(s){
 case B117_PLATE:
  if(q->boss117.slot>=6)return enemy_fault(w);a->palette=0x71;a->hit_palette=0x81;a->hit.hp=b117_rank(w,q,B117R_PLATE_HP);
  b117_start_entry(q,boss117_plate[q->boss117.slot]);return !w->fault;
 case B117_PLATE_BROKEN:
  if(q->boss117.slot>=6)return enemy_fault(w);
  if(!b117_patterns(w,e,B117P_PLATE_FIRE0+q->boss117.slot,B117P_PLATE_DEBRIS0+q->boss117.slot))return 0;
  a->hit.flags&=254;b117_defense(q,65535);b117_start_entry(q,boss117_plate_broken[q->boss117.slot]);return 1;
 case B117_CENTRAL:
  a->palette=0x71;a->hit_palette=0x81;q->boss117.slot=a->phase=0;q->boss117.anchor_x=0;q->boss117.anchor_y=56;q->boss117.selector=3;
  q->boss117.timer=rank_pair(w,q,B117R_CENTRAL_WAIT);b117_start(q,B117A_CENTRAL);return !w->fault;
 case B117_CENTRAL_FIRE:
  q->boss117.timer=rank_pair(w,q,B117R_CENTRAL_FIRE);if(!b117_troops(w,e)||!b117_child(w,e,B117_CENTRAL_ATTACHED))return 0;return interaction_sound(w,0x1099);
 case B117_CENTRAL_LOCK:return 1;
 case B117_CENTRAL_RECOIL:
  p=b117_parent(w,e);if(!p)return 0;a->phase=0;p->boss117.aimed=1;p->boss117.flags|=32;q->dead=0;
  n=b117_child(w,e,B117_BEAM);if(!n)return 0;n->payload.enemy.boss117.selector=q->boss117.selector;return 1;
 case B117_CENTRAL_RESET:
  p=b117_parent(w,e);if(!p)return 0;p->boss117.flags&=(uint8_t)~52u;q->boss117.timer=rank_pair(w,q,B117R_CENTRAL_WAIT);b117_target(w,e);
  b117_start(q,B117A_CENTRAL);a->state=B117_CENTRAL;return !w->fault;
 case B117_CENTRAL_RETIRE:
  p=b117_parent(w,e);if(!p)return 0;p->boss117.flags&=(uint8_t)~20u;a->phase=1;return 1;
 case B117_CENTRAL_WRECK:
  if(!b117_pattern(w,e,B117P_CENTRAL_FIRE)||!b117_pattern(w,e,B117P_CENTRAL_DEBRIS))return 0;b117_start(q,B117A_CENTRAL_WRECK);return 1;
 case B117_SIDE_A:case B117_SIDE_B:
  q->boss117.anchor_x=s==B117_SIDE_A?77:-64;q->boss117.anchor_y=s==B117_SIDE_A?46:42;if(s==B117_SIDE_B)a->transform.facing=1;
  a->palette=0x71;a->hit_palette=0x82;a->hit.hp=b117_rank(w,q,B117R_SIDE_HP);q->boss117.slot=0;b117_defense(q,B117B_SIDE);
  q->boss117.timer=rank_pair(w,q,B117R_SIDE_WAIT);return side_idle(q);
 case B117_SIDE_OPEN:
  if(q->boss117.upgraded){b117_schedule(e,B117_SIDE_FIRE);return 2;}
  q->boss117.timer=30;a->state=B117_SIDE_WAIT;
  /* $08210C restarts only animation; it must not reset the wait counter. */
  /* fall through */
 case B117_SIDE_WAIT:b117_start(q,B117A_SIDE_IDLE);return 1;
 case B117_SIDE_FIRE:
  if((uint16_t)q->boss117.selector>=5)return enemy_fault(w);if(!b117_side_projectiles(w,e))return 0;b117_start_entry(q,boss117_side[q->boss117.selector]);return 1;
 case B117_SIDE_COUNTDOWN:b117_start(q,B117A_SIDE_IDLE);return 1;
 case B117_SIDE_PREPARE:b117_start(q,B117A_SIDE_PREPARE);return 1;
 case B117_SIDE_BURST:
  q->boss117.count=(uint8_t)rank_pair(w,q,B117R_BURST_COUNT);q->boss117.timer=rank_pair(w,q,B117R_BURST_WAIT);q->boss117.shot=0;a->phase=0;
  if(!b117_child(w,e,B117_SIDE_ATTACHED))return 0;b117_start(q,B117A_SIDE_BURST);return !w->fault;
 case B117_SIDE_CLOSE:a->phase=1;b117_start(q,B117A_SIDE_CLOSE);return 1;
 case B117_SIDE_RESET:
  p=b117_parent(w,e);if(!p)return 0;p->boss117.flags&=(uint8_t)~11u;q->boss117.timer=rank_pair(w,q,B117R_SIDE_WAIT);b117_target(w,e);return side_idle(q);
 case B117_SIDE_DIE:
  p=b117_parent(w,e);if(!p)return 0;p->boss117.flags&=(uint8_t)~11u;p->boss117.sides&=(uint8_t)~(1u<<(a->transform.facing&1));a->phase=1;b117_defense(q,65535);
  n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMOKE);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,0,8);
  b117_start(q,B117A_SIDE_WRECK);return 1;
 default:return enemy_fault(w);
 }
}
static int central_tail(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy,*p=b117_parent(w,e);if(!p)return 0;q->boss117.timer=b117_word(q->boss117.timer-1);
 if(q->boss117.timer<=0)p->boss117.flags|=16;if(p->actor.phase)b117_schedule(e,B117_CENTRAL_RETIRE);return !w->fault;
}
static int side_tail(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy,*p=b117_parent(w,e);if(!p)return 0;struct WeaponActor*a=&q->actor;
 q->boss117.timer=b117_word(q->boss117.timer-1);if(q->boss117.timer<=0)p->boss117.flags|=8;
 if(a->hit.flags&8){a->hit.flags&=247;if(!(a->hit.flags&1)&&(a->hit.events&1)&&a->hit.last_kind==1&&!interaction_sound(w,0x108d))return 0;}
 if(p->actor.phase||(a->hit.flags&1))b117_schedule(e,B117_SIDE_DIE);return !w->fault;
}
int b117_parts_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;int delta,changed,done;
 if(state==B117_PLATE||state==B117_PLATE_BROKEN){
  if(!b117_relay(w,e)||!b117_follow(w,e,0))return 0;b117_animate(w,e);
  if(state==B117_PLATE){a->hit.flags&=247;if((a->hit.flags&1)||b117_parent_dead(w,e))b117_schedule(e,B117_PLATE_BROKEN);}return !w->fault;
 }
 p=b117_parent(w,e);if(!p)return 0;
 if(state<=B117_CENTRAL_WRECK){
  if(state==B117_CENTRAL_FIRE||state==B117_CENTRAL_LOCK){
   changed=b117_aim(w,e,&delta);
   if(state==B117_CENTRAL_FIRE&&changed&&q->boss117.upgraded&&!(w->frame_phase&7)){
    q->boss117.selector=b117_word(q->boss117.selector+delta);if(b117_special_gate(w,q))b117_schedule(e,B117_CENTRAL_LOCK);
   }
  }
  if(state==B117_CENTRAL_RECOIL)q->boss117.timer=2;
  if(!b117_follow(w,e,1)||!b117_relay(w,e))return 0;b117_animate(w,e);
  switch(state){
  case B117_CENTRAL:
   if(b117_aim(w,e,&delta)&&!(w->frame_phase&7))q->boss117.selector=b117_word(q->boss117.selector+delta);
   if(p->boss117.flags&4)b117_schedule(e,B117_CENTRAL_FIRE);break;
  case B117_CENTRAL_FIRE:case B117_CENTRAL_LOCK:
   q->boss117.timer=b117_word(q->boss117.timer-1);if(q->boss117.timer<=0){a->phase=1;b117_schedule(e,B117_CENTRAL_RECOIL);}
   if(q->boss117.timer<=15)p->boss117.flags|=32;break;
  case B117_CENTRAL_RECOIL:if(q->dead)b117_schedule(e,B117_CENTRAL_RESET);break;
  case B117_CENTRAL_RETIRE:if(p->actor.phase==0x77)b117_schedule(e,B117_CENTRAL_WRECK);return !w->fault;
  case B117_CENTRAL_WRECK:return !w->fault;
  default:return enemy_fault(w);
  }
  return central_tail(w,e);
 }
 if(state==B117_SIDE_WAIT||state==B117_SIDE_FIRE)p->boss117.flags|=1;
 else if(state==B117_SIDE_BURST)p->boss117.flags|=2;
 if(!b117_follow(w,e,1))return 0;done=b117_animate(w,e);
 switch(state){
 case B117_SIDE_IDLE:
  if(p->boss117.flags&1)b117_schedule(e,B117_SIDE_OPEN);else if(p->boss117.flags&2)b117_schedule(e,B117_SIDE_PREPARE);break;
 case B117_SIDE_WAIT:
  q->boss117.timer=b117_word(q->boss117.timer-1);if(q->boss117.timer<=0){q->boss117.selector=b117_word(q->boss117.selector+1);q->boss117.timer=30;if(!(q->boss117.selector&1))b117_schedule(e,B117_SIDE_FIRE);}break;
 case B117_SIDE_FIRE:if(done)b117_schedule(e,q->boss117.selector>=4?B117_SIDE_COUNTDOWN:B117_SIDE_WAIT);break;
 case B117_SIDE_COUNTDOWN:
  if(!(w->frame_phase&15)){q->boss117.selector=b117_word(q->boss117.selector-1);if(!q->boss117.selector)b117_schedule(e,B117_SIDE_RESET);}break;
 case B117_SIDE_PREPARE:if(done)b117_schedule(e,B117_SIDE_BURST);break;
 case B117_SIDE_BURST:
  q->boss117.timer=b117_word(q->boss117.timer-1);if(q->boss117.timer<=0){
   if(!b117_side_burst(w,e))return 0;q->boss117.timer=rank_pair(w,q,B117R_BURST_WAIT);
   if((int8_t)--q->boss117.count<=0){q->boss117.count=(uint8_t)rank_pair(w,q,B117R_BURST_COUNT);q->boss117.shot=b117_word(q->boss117.shot+1);if(q->boss117.shot==5)b117_schedule(e,B117_SIDE_CLOSE);}
  }break;
 case B117_SIDE_CLOSE:if(done)b117_schedule(e,B117_SIDE_RESET);break;
 case B117_SIDE_DIE:return !w->fault;
 default:return enemy_fault(w);
 }
 return side_tail(w,e);
}
