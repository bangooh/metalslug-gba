#include "shore.h"
int16_t shore_rank_value(struct WeaponWorld*w,struct EnemyActor*q,unsigned n){
 struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid||n>=SR_COUNT){enemy_fault(w);return 0;}const struct ShoreRankData*t=&shore_rank[n];unsigned i=(interaction_rank(w,&q->actor,runtime_owner_shore[n]))*8+(q->source_flags&3)*2;
 if(t->choices){if(!w->rng){enemy_fault(w);return 0;}i=t->choices[i+((uint8_t)(r->player_count_flag-1)&1)]*8+((source_random_word(w->rng)&14)>>1);}else i+=(r->player_count_flag&2)>>1;return (int16_t)t->values[i];
}
void shore_start(struct EnemyActor*q,enum ShoreAnimation n){animation_begin(&q->actor.animation,shore_entries[n]);q->actor.movement_flags|=32;}
void shore_defense(struct EnemyActor*q,unsigned n){q->actor.defense=n==65535?0:shore_bindings[n].defense;q->actor.defense_count=n==65535?0:shore_bindings[n].count;}
void shore_attack(struct EnemyActor*q,unsigned n){if(n==65535)q->actor.attack=0;else{q->attack_set=(struct HitAttackSet){shore_bindings[n].attack,shore_bindings[n].count};q->actor.attack=&q->attack_set;}}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;ShoreCallback callback;};
static int bind(void*ctx,unsigned kind,uint32_t v){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;if(kind==AN_PALETTE){if(v>65535)return enemy_fault(c->w);q->actor.palette=(uint16_t)v;return 1;}unsigned n=v==0xffffffffu?65535:v;if(n!=65535&&n>=sizeof(shore_bindings)/sizeof(*shore_bindings))return enemy_fault(c->w);if(kind==AN_DEFENSE)shore_defense(q,n);else if(kind==AN_ATTACK){shore_attack(q,n);hit_begin_attack(&q->actor.hit);}else return enemy_fault(c->w);return 1;}
static int sound(void*ctx,uint16_t s,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,s,stop)?1:enemy_fault(c->w);}
static int call(void*ctx,unsigned n,struct Animation*a){(void)a;struct Context*c=ctx;if(n==SC_283CA){hit_begin_attack(&c->e->payload.enemy.actor.hit);return 1;}return c->callback?c->callback(c->w,c->e,n):enemy_fault(c->w);}
int shore_animate(struct WeaponWorld*w,struct NativeEntity*e,ShoreCallback callback){struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;struct Context c={w,e,callback};struct AnimationOwner o={.context=&c,.bind=bind,.sound=sound,.call=call};int done=animation_step(&a->animation,&shore_program,&o);if(a->animation.fault)enemy_fault(w);return done;}
/* $0788C0 randomizes facing/forces scale; $0788E4 deliberately skips that. */
static void splash_tick(struct EntityPool*p,struct NativeEntity*e,void*world){struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(q->entry){q->entry=0;a->palette=12;shore_start(q,SA_SPLASH);if(!q->actor.variant){if(!w->rng){enemy_fault(w);return;}q->scale_x=q->scale_y=255;a->transform.facing|=source_random_word(w->rng)&1;a->display_flags|=64;}if(interaction_outside(w,a,shore_bounds[3],1)){entity_delete(p,e);return;}shore_defense(q,65535);}
 if(shore_animate(w,e,0)||interaction_outside(w,a,shore_bounds[3],1))entity_delete(p,e);
}
struct NativeEntity*shore_splash(struct WeaponWorld*w,struct NativeEntity*e,int fixed){struct NativeEntity*n=enemy_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,splash_tick);if(n)n->payload.enemy.actor.variant=(uint8_t)fixed;return n;}
