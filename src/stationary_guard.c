#include "enemies.h"
#include "slug_surface.h"
/* $08B9A2..$08BB84. The two four-way hit tables all select the same
 * lifetime, but the original random-word consumption remains observable. */
enum State {NEW,IDLE_A,IDLE_B,TAUNT,HIT,HIT_FRAME,DELETE};
static void start(struct EnemyActor*q,unsigned state){q->actor.vx=0;if(state==IDLE_B)enemy_start(q,EAN_RUN);if(state==TAUNT)q->actor.transform.facing|=1;enemy_start(q,state==TAUNT?EAN_TAUNT_END:state==IDLE_B?EAN_STATIONARY_B:EAN_STATIONARY_A);q->actor.state=(uint8_t)state;}
static void tick(struct EntityPool*pool,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(w->fault)return;
 if(a->state==DELETE){entity_delete(pool,e);return;}
 if(q->entry){q->entry=0;
  if(a->state==NEW){
   a->palette=14;enemy_defense(q,ENB_STATIONARY);
   if(q->family!=0){unsigned budget;for(budget=0;budget<2048;budget++){int r=interaction_move(w,a,WMOVE_ITEM);if(r<0)return;if(r)break;}if(budget==2048){enemy_fault(w);return;}}
   if(!pow_spawn_surface(w,entity_ref(e),&a->transform))return;start(q,q->family==0?IDLE_A:q->family==1?IDLE_B:TAUNT);
  }else if(a->state==HIT){if(!w->rng){enemy_fault(w);return;}source_random_word(w->rng);a->palette=0x2f;enemy_start(q,EAN_DEATH_AIR);a->state=HIT_FRAME;}
  else start(q,a->state);
 }
 if(interaction_move(w,a,WMOVE_SYNC)<0)return;int done=enemy_animate(w,e);
 if(a->state==HIT_FRAME){if(done)enemy_schedule(q,DELETE);return;}
 if(done)enemy_schedule(q,a->state==IDLE_B?IDLE_B:IDLE_A);
 if(a->hit.flags&8)enemy_schedule(q,HIT);
}
int stationary_guard_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){
 if(id<168||id>170)return enemy_fault(w);struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;e->payload.enemy.family=(uint8_t)(id-168);enemy_schedule(&e->payload.enemy,NEW);if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];return 1;
}
