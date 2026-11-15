#include "tank_internal.h"
#include "../generated/flame-trap-programs.h"
enum State {BASE,OVERLAY,FLASH,RELAY,RELAY_WAIT,LAUNCH,FOLLOW,PARTICLE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static struct NativeEntity*allocate(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,unsigned s){struct NativeEntity*e=enemy_allocate(w,p,o,t,tick);if(e){e->payload.enemy.flame_trap.timer=-1;e->payload.enemy.flame_trap.target=-1;enemy_schedule(&e->payload.enemy,s);}return e;}
static struct NativeEntity*child(struct WeaponWorld*w,struct NativeEntity*p,unsigned s,int copy){struct PlayerTransform t={.x=tk_word(w->camera_x-1)*256,.y=(512-tk_word(-1-w->camera_y))*256,.display_group=255};if(copy){t=p->payload.enemy.actor.transform;t.display_group=p->payload.enemy.source_flags;}return allocate(w,entity_ref(p),ENTITY_FIRST_CHILD,&t,s);}
static void start(struct EnemyActor*q,unsigned n){animation_begin(&q->actor.animation,flame_trap_entries[n]);q->actor.movement_flags|=32;}
static int animate(struct WeaponWorld*w,struct NativeEntity*e){struct Animation*a=&e->payload.enemy.actor.animation;int r=animation_step(a,&flame_trap_program,0);if(a->fault)enemy_fault(w);return r;}
static void reset(struct WeaponActor*a){a->vx=a->vy=a->ax=a->ay=0;transform_integer_copy(&a->transform,&a->transform);}
static int sync(struct WeaponWorld*w,struct WeaponActor*a){return interaction_move(w,a,WMOVE_SYNC)>=0;}
static void follow(struct WeaponActor*a,const struct PlayerTransform*t,int priority){transform_integer_offset(&a->transform,tk_word((t->x>>8)-(a->transform.x>>8)),tk_word(transform_y_up(t)-transform_y_up(&a->transform)));if(priority)a->transform.priority=t->priority;}
static int attack(struct WeaponWorld*w,struct NativeEntity*e,unsigned i){struct EnemyActor*q=&e->payload.enemy;q->attack_set=(struct HitAttackSet){&flame_trap_attacks[i],1};q->actor.attack=&q->attack_set;hit_begin_attack(&q->actor.hit);hit_begin_attack(&q->actor.hit);return enemy_attack(w,&q->actor);}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 switch(a->state){
 case BASE:case OVERLAY:case FLASH:{unsigned s=a->state;a->palette=0x1e4;reset(a);a->transform.priority=s==BASE?0x4000:s==OVERLAY?0xf000:0x8000;if(interaction_move(w,a,WMOVE_POINT)<0)return 0;start(q,s==BASE?FLTA_BASE:s==OVERLAY?FLTA_OVERLAY:FLTA_FLASH);if(s!=FLASH)q->flame_trap.timer=0;if(s==BASE){a->phase=0;if(!child(w,e,OVERLAY,0))return 0;}break;}
 case RELAY:if(!child(w,e,FLASH,1))return 0;a->state=RELAY_WAIT;return 0;
 case PARTICLE:a->palette=2;reset(a);start(q,FLTA_PARTICLE);break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static int frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(s==BASE){if(!sync(w,a))return 0;animate(w,e);if(!(tk_random(w)&7)&&!tk_sound(w,0x1098))return 0;
  if(q->flame_trap.timer>0)q->flame_trap.timer=tk_word(q->flame_trap.timer-1);
  else for(unsigned i=0;i<3;i++){if(!attack(w,e,i))return 0;if(a->hit.flags&2){a->hit.flags&=253;struct NativeEntity*n=child(w,e,RELAY,1);if(!n)return 0;n->payload.enemy.flame_trap.target=i?(int8_t)(i-1):-1;q->flame_trap.timer=12;break;}}
  if(interaction_outside(w,a,flame_trap_bounds,0)){a->phase=255;entity_delete(w->entities,e);}return !w->fault;
 }
 if(s==OVERLAY){struct EnemyActor*p=tk_parent(w,e);if(!p)return 0;follow(a,&p->actor.transform,0);animate(w,e);if(p->actor.phase==255)entity_delete(w->entities,e);return !w->fault;}
 if(s==FLASH||s==PARTICLE){if(!sync(w,a))return 0;if(animate(w,e))entity_delete(w->entities,e);return !w->fault;}
 if(s==RELAY_WAIT){a->state=LAUNCH;return 1;}
 if(s==LAUNCH||s==FOLLOW){int target=q->flame_trap.target;if(target<0){entity_delete(w->entities,e);return 0;}if(target>1||!w->players[target])return enemy_fault(w);struct PlayerBodyState*p=w->players[target];
  if(s==LAUNCH){/* Original signed-byte test; an existing positive invulnerability leaves timerFFFF. */if((int8_t)p->actor.hit.invulnerable<=0){p->vx=tk_word(p->vx*2);p->vy=4096;q->flame_trap.timer=25;}a->state=FOLLOW;}
  follow(a,&p->actor.transform,1);if(!(q->flame_trap.timer&3)){/* $05DD22 syncs the relay before copying integer coordinates. */if(!sync(w,a))return 0;struct NativeEntity*n=child(w,e,PARTICLE,1);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,0,16);n->payload.enemy.actor.transform.priority=a->transform.priority;}
  q->flame_trap.timer=tk_word(q->flame_trap.timer-1);if(q->flame_trap.timer<0)entity_delete(w->entities,e);return !w->fault;
 }
 return enemy_fault(w);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){(void)p;struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;if(q->entry){q->entry=0;if(!enter(w,e)||w->fault)return;}if(e->active&&!e->deleting)frame(w,e,q->actor.state);}
int flame_trap_spawn(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,const uint8_t params[8]){struct NativeEntity*e=allocate(w,p,o,t,BASE);if(!e)return 0;if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];return 1;}
