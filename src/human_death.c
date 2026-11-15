#include "enemies.h"
/* $049FD0..$04BB8E. Entry functions and scheduled frame functions are kept
 * distinct: a later source check can replace an earlier next-tick choice. */
enum State {DISPATCH,TUMBLE_CHOOSE,BACK_START,FORWARD_START,BACK_CYCLE,FORWARD_CYCLE,
 BACK_LAND,COLLAPSE_PICK,KNOCK,SIDE,COLLAPSE,AIR,GROUND,RANDOM_GROUND,
 LOW_FALL,HIGH_FALL,KNOCK_LAND,RANDOM_BURST,BURST_INIT,BURST,BURST_LAND,
 CHARRED,GROUND_CHARRED,CHARRED_LAND,LAUNCH,HARD_HIT,VERTICAL,BURN,BURN_LAND,
 BURN_RUN,BURN_DOWN,FLAME,BLOOD_A,BLOOD_B,SMOKE_BODY,SMOKE,SMOKE_FRAME,SIMPLE,FADE,BLINK,DELETE,CORPSE,DIRECT_COLLAPSE,DIRECT_TUMBLE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static int16_t word(int v){return (int16_t)(uint16_t)v;}
static uint16_t random_word(struct WeaponWorld*w){if(!w->rng){enemy_fault(w);return 0;}return source_random_word(w->rng);}
static void velocity(struct WeaponActor*a,int16_t v){a->vx=a->transform.facing&1?v:word(-v);}
static int ground_test(struct WeaponWorld*w,struct EnemyActor*q){
 int r=terrain_weapon_ground_test(w->interactions->terrain,&q->actor);if(r<0)return enemy_fault(w);q->grounded=r?255:0;return 1;
}
static int reset(struct WeaponWorld*w,struct EnemyActor*q){
 struct WeaponActor*a=&q->actor;if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;
 a->palette=w->enemies->blood_enabled?0xe:0x2f;a->display_flags&=253;a->ground_slope=-1;
 if(!ground_test(w,q))return 0;a->ax=a->ay=0;a->transform.priority=0xc000;return 1;
}
static void damp(struct WeaponActor*a){
 int16_t*x[2]={&a->vx,&a->vy};for(unsigned i=0;i<2;i++)if(*x[i]){int16_t d=*x[i]>>5;if(!d)d=1;*x[i]=word(*x[i]-d);}
}
/* $04A218, $04A268 and $04A2BE deliberately differ in gravity, drag and
 * the final probe. Do not merge them into one generic corpse integrator. */
static int physics(struct WeaponWorld*w,struct EnemyActor*q,unsigned mode){
 struct WeaponActor*a=&q->actor;int r;
 if(!enemy_ground_status(w,q,mode==1))return 0;
 if(mode!=2)damp(a);
 if(!q->grounded){
  if(mode!=1)a->ay=-192;
  r=interaction_move(w,a,mode==1?WMOVE_FALL:WMOVE_BODY);if(r<0)return 0;q->grounded=r?255:0;
  if(mode!=2)return 1;
 }else if(mode!=2)damp(a);
 if(interaction_move(w,a,WMOVE_GROUND)<0)return 0;
 if(mode==1){struct WeaponContact hit;if(!w->probe||!w->probe(w->scene,a,&hit))return enemy_fault(w);q->grounded=hit.solid?255:0;}
 if(mode==2){r=terrain_weapon_foot_contact(w->interactions->terrain,a);if(r<0)return enemy_fault(w);q->grounded=r?255:0;}
 return 1;
}
static int bounds(struct WeaponWorld*w,struct NativeEntity*e){
 struct WeaponActor*a=&e->payload.enemy.actor;
 if((uint16_t)word((a->transform.x>>8)-w->camera_x+32)>=384||(uint16_t)word(transform_y_up(&a->transform)+w->camera_y)<224){entity_delete(w->entities,e);return 1;}return 0;
}
static struct NativeEntity*child(struct WeaponWorld*w,struct NativeEntity*e,unsigned state,const struct PlayerTransform*t){
 struct NativeEntity*n=enemy_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,t,tick);if(n)enemy_schedule(&n->payload.enemy,state);return n;
}
int human_spawn_blood(struct WeaponWorld*w,struct NativeEntity*e,int variant){
 const struct WeaponActor*a=&e->payload.enemy.actor;struct PlayerTransform t={.x=a->transform.x,.y=a->transform.y};
 if(!(a->transform.facing&1))t.facing=1;transform_integer_offset(&t,(a->transform.facing&1)?8:-8,0);
 return child(w,e,variant?BLOOD_B:BLOOD_A,&t)!=0;
}
#define blood human_spawn_blood
static void burst_velocity(struct WeaponWorld*w,struct EnemyActor*q){
 struct WeaponActor*a=&q->actor;a->vy=0xcc6;a->ay=-109;a->ax=0;
 uint16_t v=random_word(w)&1023;if(word((a->transform.x>>8)-a->hit.impact_x)<0)v^=65535;
 a->vx=(int16_t)v;a->vy=word(a->vy+(random_word(w)&1023));q->grounded=0;
}
static int select_route(struct WeaponWorld*w,struct EnemyActor*q){
 static const uint8_t routes[]={TUMBLE_CHOOSE,AIR,KNOCK,RANDOM_BURST,CHARRED,BURST_INIT,GROUND,GROUND_CHARRED,HARD_HIT,VERTICAL,BURN,COLLAPSE,SIDE,RANDOM_GROUND};
 if(q->family>3)return enemy_fault(w);if(!ground_test(w,q))return 0;
 struct WeaponActor*a=&q->actor;
 uint16_t sx=(uint16_t)word((a->transform.x>>8)-w->camera_x),ix=(uint16_t)word(a->hit.impact_x-w->camera_x);
 q->back_hit=((sx>=ix)^!!(a->transform.facing&1))?255:0;
 unsigned kind=a->hit.last_kind<34?a->hit.last_kind:15;
 if(!reset(w,q))return 0;enemy_publish_death(w,a);enemy_schedule(q,routes[human_death_routes[q->family][kind]]);return 1;
}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 for(unsigned budget=0;budget<12;budget++){
  unsigned state=a->state;q->entry=0;
  switch(state){
  case DIRECT_COLLAPSE:case DIRECT_TUMBLE:
   if(!ground_test(w,q))return 0;if(state==DIRECT_COLLAPSE)a->palette=w->enemies->blood_enabled?14:47;
   enemy_start(q,state==DIRECT_COLLAPSE?EAN_COLLAPSE:EAN_TUMBLE_LAND);a->state=SIMPLE;break;
  case CORPSE:if(!reset(w,q))return 0;enemy_schedule(q,q->family?LAUNCH:BURST_INIT);continue;
  case DISPATCH:if(!select_route(w,q))return 0;continue;
  case TUMBLE_CHOOSE:if(!blood(w,e,1))return 0;enemy_schedule(q,q->back_hit?BACK_START:FORWARD_START);continue;
  case BACK_START:case FORWARD_START:
   if((uint8_t)random_word(w)<(uint8_t)word(transform_y_up(&a->transform)+w->camera_y)){enemy_schedule(q,LOW_FALL);continue;}
   if((int16_t)random_word(w)<0){enemy_schedule(q,KNOCK);continue;}q->sequence=0;enemy_schedule(q,state==BACK_START?BACK_CYCLE:FORWARD_CYCLE);continue;
  case BACK_CYCLE:case FORWARD_CYCLE:
   if(q->sequence>=5){enemy_schedule(q,state==BACK_CYCLE?BACK_LAND:COLLAPSE_PICK);continue;}
   animation_begin(&a->animation,human_tumble_entries[state==BACK_CYCLE?0:1][q->sequence++]);a->movement_flags|=32;
   a->hit.flags&=247;velocity(a,state==BACK_CYCLE?-128:128);a->vy=word(a->vy+128);transform_integer_offset(&a->transform,0,2);break;
  case BACK_LAND:enemy_start(q,EAN_TUMBLE_LAND);a->state=SIMPLE;break;
  case COLLAPSE_PICK:enemy_start(q,(int16_t)random_word(w)<0?EAN_COLLAPSE_B:EAN_COLLAPSE_A);a->state=SIMPLE;break;
  case KNOCK:
   enemy_start(q,q->back_hit?EAN_KNOCK_BACK:EAN_KNOCK_FORWARD);velocity(a,q->back_hit?-1024:1024);
   a->vy=256;transform_integer_offset(&a->transform,0,2);a->state=SIMPLE;break;
  case SIDE:enemy_start(q,q->back_hit?EAN_DEATH_BACK:EAN_DEATH_SIDE);a->state=SIMPLE;break;
  case COLLAPSE:enemy_start(q,EAN_COLLAPSE);a->state=SIMPLE;break;
  case AIR:enemy_start(q,EAN_DEATH_AIR);a->state=SIMPLE;break;
  case RANDOM_GROUND:enemy_schedule(q,(int16_t)random_word(w)<0?HIGH_FALL:GROUND);continue;
  case GROUND:enemy_start(q,EAN_DEATH_GROUND);break;
  case LOW_FALL:case HIGH_FALL:
   enemy_start(q,state==LOW_FALL?EAN_LOW_FALL:EAN_HIGH_FALL);if(!q->back_hit)a->transform.facing^=1;
   velocity(a,-200);a->ay=-192;a->vy=0;break;
  case KNOCK_LAND:enemy_start(q,EAN_KNOCK_LAND);a->state=SIMPLE;break;
  case RANDOM_BURST:
   if(!(random_word(w)&127)){enemy_schedule(q,LAUNCH);continue;}
   if((int16_t)random_word(w)<0){enemy_schedule(q,TUMBLE_CHOOSE);continue;}
   enemy_schedule(q,(random_word(w)&3)?BURST:CHARRED);continue;
  case BURST_INIT:transform_integer_offset(&a->transform,0,4);enemy_schedule(q,BURST);continue;
  case BURST:
   enemy_start(q,EAN_BURST);burst_velocity(w,q);enemy_defense(q,65535);a->hit.flags&=247;q->sequence=0;break;
  case BURST_LAND:enemy_start(q,EAN_BURST_LAND);a->state=SIMPLE;break;
  case GROUND_CHARRED:if(!ground_test(w,q))return 0;enemy_schedule(q,CHARRED);continue;
  case CHARRED:
   transform_integer_offset(&a->transform,0,4);enemy_start(q,EAN_CHARRED_AIR);burst_velocity(w,q);a->palette=0x19;break;
  case CHARRED_LAND:enemy_start(q,EAN_CHARRED_LAND);a->state=SIMPLE;break;
  case LAUNCH:
   if(!ground_test(w,q))return 0;a->palette=0x8a;a->vx=a->transform.facing&1?4:-4;a->vy=0x870;a->ay=-72;a->ax=0;
   enemy_start(q,EAN_LAUNCH);q->scale=0x2800;a->transform.priority=65535;a->display_flags|=64;break;
  case HARD_HIT:{
   enemy_start(q,EAN_KNOCK_BACK);
   if(a->hit.last_attacker>=ENTITY_CAPACITY)return enemy_fault(w);
   struct NativeEntity*attacker=&w->entities->nodes[a->hit.last_attacker];struct WeaponActor*source=entity_weapon_actor(attacker);
   /* The reachable kind-18 producer is the horizontal shotgun. Its source
    * $5C direction is retained by the existing typed projectile hit field. */
   if(!source||attacker->kind!=ENTITY_PROJECTILE||source->kind!=EMIT_SHOTGUN)return enemy_fault(w);
   if(source->hit.impact_x)a->transform.facing|=1;else a->transform.facing&=254;
   velocity(a,-2048);a->vy=256;transform_integer_offset(&a->transform,0,2);if(!blood(w,e,0))return 0;break;
  }
  case VERTICAL:
   a->vx=0;enemy_start(q,EAN_BURST);a->vy=0x800;transform_integer_offset(&a->transform,0,2);if(!blood(w,e,0))return 0;break;
  case BURN:
   interaction_priority(w,a,0x8000,0);if(!(random_word(w)&3)){enemy_schedule(q,BURN_RUN);continue;}
   enemy_start(q,EAN_BURN);a->timer=30;break;
  case BURN_LAND:enemy_start(q,EAN_BURN_LAND);a->state=SIMPLE;break;
  case BURN_RUN:{
   enemy_start(q,EAN_BURN_RUN);if(!interaction_sound(w,0x1042))return 0;q->dead=0;
   struct PlayerTransform empty={0};if(!child(w,e,FLAME,&empty))return 0;
   velocity(a,512);a->timer=40;q->turn_interval=(int16_t)random_word(w)<0?10:255;q->turn_timer=q->turn_interval;a->ay=-64;break;
  }
  case BURN_DOWN:enemy_start(q,EAN_CHARRED_LAND);q->dead=255;a->state=SIMPLE;break;
  case FLAME:a->palette=4;enemy_start(q,EAN_FLAME);break;
  case BLOOD_A:case BLOOD_B:
   a->palette=w->enemies->blood_enabled?0x17a:0x1ba;interaction_priority(w,a,0xc000,16);enemy_start(q,state==BLOOD_A?EAN_BLOOD_A:EAN_BLOOD_B);break;
  case SMOKE_BODY:{
   struct PlayerTransform t=a->transform;transform_integer_offset(&t,0,24);if(!child(w,e,SMOKE,&t))return 0;
   a->palette=w->enemies->blood_enabled?0x19f:0x1a3;enemy_start(q,EAN_SMOKE_BODY);a->state=SMOKE_FRAME;return 1;
  }
  case SMOKE:a->palette=13;enemy_start(q,EAN_SMOKE);a->state=SMOKE_FRAME;break;
  case FADE:case BLINK:a->ttl=10;break;
  case SIMPLE:case SMOKE_FRAME:case DELETE:break;
  default:return enemy_fault(w);
  }return !w->fault;
 }return enemy_fault(w);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 int smoke_entry=q->entry&&a->state==SMOKE_BODY;if(q->entry&&!enter(w,e))return;if(smoke_entry)return;
 unsigned state=a->state;int done;
 switch(state){
 case DELETE:entity_delete(p,e);return;
 case SIMPLE:
  if(!physics(w,q,0))return;if(enemy_animate(w,e))enemy_schedule(q,FADE);break;
 case FADE:
  if(!physics(w,q,0))return;enemy_animate(w,e);if(!--a->ttl)enemy_schedule(q,BLINK);break;
 case BLINK:
  if(!physics(w,q,0))return;if(!--a->ttl){entity_delete(p,e);return;}if(!(a->ttl&1))enemy_animate(w,e);break;
 case BACK_CYCLE:case FORWARD_CYCLE:
  if(!physics(w,q,0))return;if(enemy_animate(w,e))enemy_schedule(q,(a->hit.flags&8)?state:state==BACK_CYCLE?BACK_LAND:COLLAPSE_PICK);break;
 case GROUND:
  if(!physics(w,q,1))return;if(enemy_animate(w,e)){entity_delete(p,e);return;}break;
 case LOW_FALL:case HIGH_FALL:
  if(!physics(w,q,1))return;done=enemy_animate(w,e);if(done&&q->grounded)enemy_schedule(q,KNOCK_LAND);break;
 case BURST:
  if(!q->sequence&&a->vy<0){enemy_defense(q,ENB_FALLING_BODY);q->sequence=1;}
  if(!physics(w,q,1))return;q->gib_delays=random_word(w)&7;enemy_animate(w,e);
  if(a->hit.flags&8){
   if(q->dispatch_depth>=4){enemy_fault(w);return;}q->dispatch_depth++;q->family=3;enemy_schedule(q,DISPATCH);tick(p,e,world);q->dispatch_depth--;
   if(!e->active||e->deleting||w->fault)return;
  }
  if(q->grounded)enemy_schedule(q,BURST_LAND);break;
 case CHARRED:
  if(!physics(w,q,1))return;enemy_animate(w,e);if(q->grounded)enemy_schedule(q,CHARRED_LAND);break;
 case LAUNCH:{
  unsigned scale=q->scale+(q->scale>>5);q->scale=(uint16_t)(scale>65535?65535:scale);q->scale_x=q->scale_y=q->scale>>8;
  if(interaction_move(w,a,WMOVE_POINT)<0)return;enemy_animate(w,e);break;
 }
 case HARD_HIT:
  if(!physics(w,q,0)||!physics(w,q,2))return;if(enemy_animate(w,e))enemy_schedule(q,FADE);break;
 case VERTICAL:
  if(!physics(w,q,0))return;a->ay=0;if(!physics(w,q,2))return;q->gib_delays=random_word(w)&7;enemy_animate(w,e);
  if(q->grounded)enemy_schedule(q,BURST_LAND);break;
 case BURN:
  if(!physics(w,q,0))return;enemy_animate(w,e);if(a->timer)a->timer--;else if(q->grounded)enemy_schedule(q,BURN_LAND);break;
 case BURN_RUN:
  if(!physics(w,q,1))return;enemy_animate(w,e);if(a->hit.events&32)enemy_schedule(q,BURN_DOWN);
  if(!--a->timer)enemy_schedule(q,BURN_DOWN);if(!--q->turn_timer){a->transform.facing^=1;q->turn_timer=q->turn_interval;a->vx=word(-a->vx);}break;
 case FLAME:{
  struct NativeEntity*parent=entity_get(p,e->parent);
  if(!parent||parent->deleting){entity_delete(p,e);return;}
  if(parent->kind!=ENTITY_ENEMY){enemy_fault(w);return;}struct EnemyActor*r=&parent->payload.enemy;
  if(r->dead){entity_delete(p,e);return;}transform_integer_offset(&a->transform,word((r->actor.transform.x>>8)-(a->transform.x>>8)),word(transform_y_up(&r->actor.transform)+30-transform_y_up(&a->transform)));
  a->transform.priority=r->actor.transform.priority;enemy_animate(w,e);return;
 }
 case BLOOD_A:case BLOOD_B:
  if(interaction_move(w,a,WMOVE_SYNC)<0)return;if(enemy_animate(w,e))entity_delete(p,e);return;
 case SMOKE_FRAME:{
  static const int16_t b[4]={0,1,0,1};if(interaction_move(w,a,WMOVE_SYNC)<0)return;
  if(enemy_animate(w,e)||interaction_outside(w,a,b,1))enemy_schedule(q,DELETE);return;
 }
 default:enemy_fault(w);return;
 }if(!w->fault)bounds(w,e);
}
static int adopt(struct WeaponWorld*w,struct NativeEntity*e){
 if(e->kind==ENTITY_ENEMY){e->tick=tick;return 1;}
 struct WeaponActor*a=entity_weapon_actor(e);if(!a||e->kind==ENTITY_SLUG)return enemy_fault(w);
 struct WeaponActor saved=*a;
 struct HitAttackSet attack=saved.attack?*saved.attack:(struct HitAttackSet){0};
 e->payload.enemy=(struct EnemyActor){.actor=saved,.attack_set=attack,.scale_x=255,.scale_y=255,.gib_delays=255,.dead=255,.support_slope=65535};
 if(saved.attack)e->payload.enemy.actor.attack=&e->payload.enemy.attack_set;
 e->kind=ENTITY_ENEMY;e->tick=tick;return 1;
}
int human_death_schedule(struct WeaponWorld*w,struct NativeEntity*e,unsigned table){
 if(!w->enemies||table>3||!adopt(w,e))return enemy_fault(w);
 e->payload.enemy.family=(uint8_t)table;enemy_schedule(&e->payload.enemy,DISPATCH);return 1;
}
static int hit_tail(struct WeaponWorld*w,struct NativeEntity*e,unsigned ground_table){
 struct WeaponActor*a=entity_weapon_actor(e);if(!w->enemies||!a)return enemy_fault(w);
 if(w->enemies->damage_enabled&&!(a->hit.flags&8))return 1;
 int r=terrain_weapon_foot_contact(w->interactions->terrain,a);if(r<0)return enemy_fault(w);
 return human_death_schedule(w,e,r?ground_table:2);
}
int human_death_hit_tail(struct WeaponWorld*w,struct NativeEntity*e){return hit_tail(w,e,0);}
int human_death_hit_tail_alternate(struct WeaponWorld*w,struct NativeEntity*e){return hit_tail(w,e,1);}
struct NativeEntity*human_corpse_entity(struct WeaponWorld*w,struct EntityRef parent,const struct WeaponActor*a,int launched){
 struct NativeEntity*e=enemy_allocate(w,parent,ENTITY_FIRST_CHILD,&a->transform,tick);if(!e)return 0;
 /* Both source writes target $54; source Y overwrites source X. */
 e->payload.enemy.actor.hit.impact_x=word(transform_y_up(&a->transform)+w->camera_y+w->camera_x);
 /* Reset is scheduled, just like $04A18C/$04A1B2. */
 e->payload.enemy.family=(uint8_t)(launched?1:0);enemy_schedule(&e->payload.enemy,CORPSE);return e;
}
int human_spawn_corpse(struct WeaponWorld*w,struct EntityRef parent,const struct WeaponActor*a,int launched){return human_corpse_entity(w,parent,a,launched)!=0;}
int human_smoke_transform(struct WeaponWorld*w,struct NativeEntity*e){
 if(!w->enemies||!adopt(w,e))return enemy_fault(w);enemy_schedule(&e->payload.enemy,SMOKE_BODY);return 1;
}

/* $04A166 changes the current handler; it does not allocate a new corpse. */
int human_corpse_transform(struct WeaponWorld*w,struct NativeEntity*e,int launched){
 if(!w->enemies||!adopt(w,e))return enemy_fault(w);e->payload.enemy.family=(uint8_t)!!launched;enemy_schedule(&e->payload.enemy,CORPSE);return 1;
}

/* $04A802 entered directly by the siege emitter, without $04A09C reset. */
int human_launch_direct(struct WeaponWorld*w,struct NativeEntity*e){if(!adopt(w,e))return 0;enemy_schedule(&e->payload.enemy,LAUNCH);return 1;}

/* $04A146/$04A154 preserve incoming velocities/defense and skip the normal
 * damage-kind dispatch/reset. The caller schedules this for the next tick. */
int human_platform_death(struct WeaponWorld*w,struct NativeEntity*e,int collapse){if(!w->enemies||!adopt(w,e))return enemy_fault(w);enemy_schedule(&e->payload.enemy,collapse?DIRECT_COLLAPSE:DIRECT_TUMBLE);return 1;}

/* $04A7A6 starts with its own ground probe, without the $04A09C reset. */
int human_charred_transform(struct WeaponWorld*w,struct NativeEntity*e){if(!w->enemies||!adopt(w,e))return enemy_fault(w);enemy_schedule(&e->payload.enemy,GROUND_CHARRED);return 1;}
