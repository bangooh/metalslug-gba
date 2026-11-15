#include "tank_internal.h"
#include "slug_support.h"
#include "../generated/presentation-programs.h"
enum State {NEW,FALL,BOUNCE,BOUNCE_FRAME,FLAT,SYNC,RECOVER};
static void start(struct EnemyActor*q,unsigned n){animation_begin(&q->actor.animation,presentation_entries[n]);q->actor.movement_flags|=32;}
static int overlap(struct WeaponWorld*w,struct WeaponActor*a){struct SlugSharedState*s=w->enemies->slug;if(!s||!w->enemies->recovery318.valid)return enemy_fault(w);
 const struct HitRect*r=&w->enemies->recovery318.target;int16_t l=r->left,rr=r->right,b=r->bottom,t=r->top;
 if(s->actor.transform.facing&1){int16_t v=l;l=tk_word(-rr);rr=tk_word(-v);}if(l>=rr)return enemy_fault(w);
 int16_t x=tk_word(tk_sx(w,a)+w->enemies->recovery318.point_x);l=tk_word(l+tk_sx(w,&s->actor));rr=tk_word(rr+tk_sx(w,&s->actor));if(l>x||x>rr)return 0;
 if(s->actor.transform.facing&2){int16_t v=b;b=tk_word(-t);t=tk_word(-v);}if(b>=t)return enemy_fault(w);
 int16_t y=tk_word(tk_sy(w,a)+w->enemies->recovery318.point_y);b=tk_word(b+tk_sy(w,&s->actor));t=tk_word(t+tk_sy(w,&s->actor));return b<=y&&y<=t;
}
static void tick(struct EntityPool*pool,struct NativeEntity*e,void*world){struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct SlugSharedState*s=w->enemies->slug;
 if(q->entry){q->entry=0;switch(a->state){
 case NEW:if(!s||!s->present||!(s->action_flags&8)){entity_delete(pool,e);return;}a->palette=2;q->lifecycle_flags|=4;interaction_priority(w,a,0xc000,16);start(q,PRA_RECOVER_FALL);a->state=FALL;break;
 case BOUNCE:start(q,PRA_RECOVER_BOUNCE);a->vx=tk_faced(a,16);a->vy=1152;a->ay=-144;a->ax=0;a->state=BOUNCE_FRAME;break;
 case FLAT:start(q,PRA_RECOVER_FLAT);a->state=SYNC;break;
 case RECOVER:if(!s){enemy_fault(w);return;}s->action_flags|=16;entity_delete(pool,e);return;
 default:enemy_fault(w);return;
 }}
 unsigned state=a->state;int landed=interaction_move(w,a,state==FALL?WMOVE_ITEM:state==BOUNCE_FRAME?WMOVE_BODY:WMOVE_SYNC);if(landed<0)return;
 if(landed&&state==FALL)enemy_schedule(q,w->scene_number==0&&(a->raw_surface&192)==64?FLAT:BOUNCE);
 if(landed&&state==BOUNCE_FRAME)a->state=SYNC;
 animation_step(&a->animation,&presentation_program,0);if(a->animation.fault){enemy_fault(w);return;}
 if(interaction_outside(w,a,recovery318_bounds,0)){entity_delete(pool,e);return;}
 if(state!=FALL&&overlap(w,a))enemy_schedule(q,RECOVER);
}
int recovery318_spawn(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t){struct NativeEntity*e=enemy_allocate(w,p,o,t,tick);if(!e)return 0;enemy_schedule(&e->payload.enemy,NEW);return 1;}
