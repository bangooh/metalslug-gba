#include "bosses.h"
void boss_start_entry(struct EnemyActor*q,uint16_t entry){animation_begin(&q->actor.animation,entry);q->actor.movement_flags|=32;}
void boss_start(struct EnemyActor*q,enum BossAnimation a){boss_start_entry(q,boss_entries[a]);}
void boss_defense(struct EnemyActor*q,unsigned b){
 q->actor.defense=b==65535?0:boss_bindings[b].defense;q->actor.defense_count=b==65535?0:boss_bindings[b].count;
}
void boss_attack_binding(struct EnemyActor*q,unsigned b){
 if(b==65535){q->actor.attack=0;return;}
 q->attack_set=(struct HitAttackSet){boss_bindings[b].attack,boss_bindings[b].count};q->actor.attack=&q->attack_set;
}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;BossCallbackFn call;};
static int field(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){
 struct Context*c=ctx;struct WeaponActor*a=&c->e->payload.enemy.actor;
 if(op!=AN_FIELD_SET)return enemy_fault(c->w);
 if(f==BF_20&&width==1)a->phase=(uint8_t)v;
 else if(f==BF_2A&&width==2)a->vy=(int16_t)v;
 else if(f==BF_70&&width==2)c->e->payload.enemy.selector=(uint16_t)v;
 else if(f==BF_7D&&width==1)c->e->payload.enemy.gunship.pose=(uint8_t)v;
 else return enemy_fault(c->w);return 1;
}
static uint32_t read_field(void*ctx,unsigned f,unsigned width){
 struct Context*c=ctx;
 if(f==BF_3B&&width==1)return c->e->payload.enemy.actor.animation.pose;
 enemy_fault(c->w);return 0;
}
static int delay(void*ctx,unsigned f,unsigned pose,uint8_t*value){
 struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;
 if(f!=BF_5C||q->crab.delays>=7||pose>=256)return enemy_fault(c->w);
 *value=crab_delays[q->crab.delays][pose];return 1;
}
static int bind(void*ctx,unsigned kind,uint32_t value){
 struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;
 unsigned b=value==0xffffffffu?65535:value;if(b!=65535&&b>=sizeof(boss_bindings)/sizeof(*boss_bindings))return enemy_fault(c->w);
 if(kind==AN_DEFENSE){boss_defense(q,b);return 1;}
 if(kind==AN_ATTACK){boss_attack_binding(q,b);hit_begin_attack(&q->actor.hit);return 1;}return enemy_fault(c->w);
}
static int sound(void*ctx,uint16_t command,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,command,stop)?1:enemy_fault(c->w);}
static int call(void*ctx,unsigned callback,struct Animation*a){
 (void)a;struct Context*c=ctx;
 if(callback==BC_283CA){hit_begin_attack(&c->e->payload.enemy.actor.hit);return 1;}
 /* These exact verified source entry points are RTS, not camera shake. */
 if(callback==BC_434CE||callback==BC_434DC)return 1;
 return c->call?c->call(c->w,c->e,callback):enemy_fault(c->w);
}
int boss_animate(struct WeaponWorld*w,struct NativeEntity*e,BossCallbackFn callback){
 struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;
 struct Context ctx={w,e,callback};struct AnimationOwner owner={.context=&ctx,.read=read_field,.write=field,.bind=bind,.call=call,.sound=sound,.delay=delay};
 int done=animation_step(&a->animation,&boss_program,&owner);if(a->animation.fault)enemy_fault(w);return done;
}
int boss_sprite_admitted(struct WeaponWorld*w){
 if(!w->sprite_budget.valid){enemy_fault(w);return 0;}
 uint16_t quarter=(uint16_t)(w->sprite_budget.last-w->sprite_budget.first)>>2;
 return w->sprite_budget.counter>=quarter;
}
