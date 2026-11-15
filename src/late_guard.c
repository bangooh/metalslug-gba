#include "late_guard.h"
#include "items.h"
#include "../generated/late-guard-programs.h"
/* Templates 31..49, source $05FD78..$060558. The source file calls these
 * breakables; their actual loops turn toward players and throw projectiles. */
enum State {WAIT,INIT,ENTRY,IDLE,THROW,TURN,NEAR,REACT,DIE,PROJECTILE,ARRIVAL,ARRIVAL_END,DELETE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static int16_t word(int n){return (int16_t)(uint16_t)n;}
static int16_t sx(struct WeaponWorld*w,struct WeaponActor*a){return word((a->transform.x>>8)-w->camera_x);}
static uint16_t random_word(struct WeaponWorld*w){if(!w->rng){enemy_fault(w);return 0;}return source_random_word(w->rng);}
static void start_entry(struct EnemyActor*q,uint16_t n){animation_begin(&q->actor.animation,n);q->actor.movement_flags|=32;}
static void start(struct EnemyActor*q,enum LateGuardAnimation n){start_entry(q,late_guard_entries[n]);}
static struct NativeEntity*spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,const struct PlayerTransform*t,unsigned variant){
 if(variant>=14){enemy_fault(w);return 0;}struct NativeEntity*n=enemy_allocate(w,parent,order,t,tick);if(!n)return 0;
 n->payload.enemy.siege=(__typeof__(n->payload.enemy.siege)){.variant=(uint8_t)variant,.timer=-1};enemy_schedule(&n->payload.enemy,variant>=9?ARRIVAL:WAIT);return n;
}
static int16_t target_dx(struct WeaponWorld*w,struct EnemyActor*q){int i=enemy_nearest_x(w,&q->actor,0);struct PlayerBodyState*p=i<0?0:w->players[i];int16_t x=p?word((p->actor.transform.x>>8)-w->camera_x):-1;return word(x-sx(w,&q->actor));}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read(void*ctx,unsigned f,unsigned width){struct Context*c=ctx;struct WeaponActor*a=&c->e->payload.enemy.actor;if(width==1){if(f==LGF_59)return a->damage_timer;if(f==LGF_3A)return a->transform.facing;}enemy_fault(c->w);return 0;}
static int write(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){struct Context*c=ctx;struct WeaponActor*a=&c->e->payload.enemy.actor;if(op==AN_FIELD_AND)v&=read(ctx,f,width);else if(op==AN_FIELD_OR)v|=read(ctx,f,width);else if(op==AN_FIELD_XOR)v^=read(ctx,f,width);else if(op!=AN_FIELD_SET)return enemy_fault(c->w);if(width!=1)return enemy_fault(c->w);if(f==LGF_59)a->damage_timer=(uint8_t)v;else if(f==LGF_3A)a->transform.facing=(uint8_t)v;else return enemy_fault(c->w);return 1;}
static int bind(void*ctx,unsigned kind,uint32_t n){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;if(n!=0xffffffffu&&n>=sizeof(late_guard_bindings)/sizeof(*late_guard_bindings))return enemy_fault(c->w);if(kind==AN_DEFENSE){q->actor.defense=n==0xffffffffu?0:late_guard_bindings[n].defense;q->actor.defense_count=n==0xffffffffu?0:late_guard_bindings[n].count;}else if(kind==AN_ATTACK){q->attack_set=n==0xffffffffu?(struct HitAttackSet){0}:(struct HitAttackSet){late_guard_bindings[n].attack,late_guard_bindings[n].count};q->actor.attack=n==0xffffffffu?0:&q->attack_set;hit_begin_attack(&q->actor.hit);}else return enemy_fault(c->w);return 1;}
static int sound(void*ctx,uint16_t s,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,s,stop)?1:enemy_fault(c->w);}
static int call(void*ctx,unsigned n,struct Animation*anim){
 (void)anim;struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;if(n)return enemy_fault(c->w);
 struct NativeEntity*v=enemy_allocate(c->w,entity_ref(c->e),ENTITY_FIRST_CHILD,&q->actor.transform,tick);if(!v)return 0;
 struct WeaponActor*a=&v->payload.enemy.actor;transform_integer_offset(&a->transform,0,24);a->vx=q->actor.vx;a->vy=1024;a->ax=0;a->ay=word(a->ay-32);enemy_schedule(&v->payload.enemy,PROJECTILE);return 1;
}
static int animate(struct WeaponWorld*w,struct NativeEntity*e){struct Context c={w,e};struct AnimationOwner o={.context=&c,.read=read,.write=write,.bind=bind,.sound=sound,.call=call};int r=animation_step(&e->payload.enemy.actor.animation,&late_guard_program,&o);if(e->payload.enemy.actor.animation.fault)enemy_fault(w);return r;}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;q->entry=0;unsigned v=q->siege.variant;
 switch(a->state){
 case ARRIVAL:
  if(!q->siege.closing)a->transform.facing|=1;start(q,v==13?LGA_ARRIVE_ESCAPE:v>=11?LGA_ARRIVE_B:LGA_ARRIVE_A);a->palette=14;a->transform.priority=0x8000;
  a->vx=a->vy=a->ax=a->ay=0;transform_integer_copy(&a->transform,&a->transform);break;
 case ARRIVAL_END:break;
 case WAIT:if(v<3)start(q,(enum LateGuardAnimation)(LGA_ENTRY0+v));else if(v>=5)start(q,(enum LateGuardAnimation)(LGA_WAIT0+v-5));break;
 case INIT:
  a->palette=14;q->scale_x=q->scale_y=255;a->transform.facing=q->params[0]&1;a->transform.priority=0x8000;a->hit.hp=1;a->vx=a->vy=a->ax=a->ay=0;transform_integer_copy(&a->transform,&a->transform);
  if(v==3||v==4)start(q,v==3?LGA_WAIT_A:LGA_WAIT_B);a->state=v<3?ENTRY:NEAR;break;
 case IDLE:{struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid)return enemy_fault(w);q->siege.timer=(int16_t)late_guard_wait[(interaction_rank(w,&q->actor,runtime_owner_late_guard[0]))*8+(q->source_flags&3)*2+((r->player_count_flag&2)>>1)];start(q,LGA_IDLE);break;}
 case THROW:a->vx=word(target_dx(w,q)*2);start(q,LGA_THROW);break;
 case TURN:start(q,LGA_TURN);break;
 case REACT:start(q,v==3?LGA_REACT_A:LGA_REACT_B);break;
 case DIE:
  /* $0519BE credits the stored owner; source $12 bit 1 suppresses it. */
  if(!(a->display_flags&2)&&(!w->items||!item_score_add(w->items,a->score_owner,enemy_hit_scores[a->hit.last_kind&31])))return enemy_fault(w);
  if(!w->rng)return enemy_fault(w);start_entry(q,late_guard_deaths[source_random(w->rng,2)]);break;
 case PROJECTILE:
  a->palette=0x15d;q->scale_x=q->scale_y=255;a->transform.priority=0xd000;a->transform.facing=0;q->siege.timer=0;start(q,LGA_PROJECTILE);
  {struct Context c={w,e};if(!bind(&c,AN_ATTACK,LGB_PROJECTILE))return 0;}break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static int replacement(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;q->params[1]--;if((int8_t)q->params[1]<0)return 1;unsigned v=random_word(w)&3;if(v==3)v=2;
 struct NativeEntity*n=spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&q->actor.transform,v);if(!n)return 0;n->payload.enemy.params[1]=q->params[1];return !w->fault;
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 for(unsigned budget=0;budget<3;budget++){
  if(a->state==DELETE){entity_delete(p,e);return;}
  if(q->entry&&!enter(w,e))return;
  if(a->state==WAIT){if(sx(w,a)<320){enemy_schedule(q,INIT);continue;}if(interaction_move(w,a,WMOVE_SYNC)<0)return;/* Re-enters the original entry stream while gated. */q->entry=1;return;}
  if(a->state==ARRIVAL||a->state==ARRIVAL_END){
   if(interaction_move(w,a,WMOVE_SYNC)<0)return;int done=animate(w,e);
   if(a->state==ARRIVAL_END){entity_delete(p,e);return;}
   if(done){
    unsigned v=q->siege.variant;
    if(v==13){if(!infantry_spawn_forced_escape(w,w->enemy_group,ENTITY_FIRST_CHILD,&a->transform,q->params))return;}
    else if(!soldier_spawn_template(w,w->enemy_group,ENTITY_FIRST_CHILD,v&1?1:11,&a->transform,q->params,q->source_flags))return;
    struct NativeEntity*g=entity_get(p,w->enemy_group);if(!g||!g->next){enemy_fault(w);return;}
    struct WeaponActor*child=entity_weapon_actor(&p->nodes[g->next]);if(!child){enemy_fault(w);return;}child->display_flags|=2;
    enemy_schedule(q,ARRIVAL_END);
   }else if(interaction_outside(w,a,late_guard_bounds[2],0))entity_delete(p,e);return;
  }
  if(a->state==PROJECTILE){
   int r=interaction_move(w,a,WMOVE_POINT);if(r<0)return;
   if(!r){static const uint16_t palettes[]={0x15d,0x164,0x165};a->palette=palettes[(uint16_t)q->siege.timer&3];if(++q->siege.timer>=3)q->siege.timer=0;a->trail_angle=enemy_angle(a->vx>>4,a->vy>>4)>>3;animate(w,e);if(!enemy_attack(w,a))return;r=(a->hit.flags&2)||interaction_outside(w,a,late_guard_bounds[1],0);}
   if(r){struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_BLAST);if(!n)return;n->payload.enemy.actor.transform.priority=0x4000;entity_delete(p,e);}return;
  }
  unsigned s=a->state;if(interaction_move(w,a,WMOVE_SYNC)<0)return;int done=animate(w,e);
  if(s==DIE){if(done&&replacement(w,e))entity_delete(p,e);return;}
  if(s==ENTRY){if(done)enemy_schedule(q,IDLE);}
  else if(s==IDLE){
   if((uint16_t)q->siege.timer!=255){q->siege.timer=word(q->siege.timer-1);if(!q->siege.timer)enemy_schedule(q,THROW);else{q->siege.timer=word(q->siege.timer-1);if(!q->siege.timer)enemy_schedule(q,THROW);}}
   if((target_dx(w,q)<0)==!!(a->transform.facing&1))enemy_schedule(q,TURN);
  }else if(s==THROW||s==TURN||s==REACT){if(done)enemy_schedule(q,IDLE);}
  else if(s==NEAR){int16_t dx=target_dx(w,q);if(dx<0)dx=word(-dx);if(dx<=32)enemy_schedule(q,q->siege.variant>=5?IDLE:REACT);}
  if(a->hit.flags&8)enemy_schedule(q,DIE);if(interaction_outside(w,a,late_guard_bounds[0],1))entity_delete(p,e);return;
 }enemy_fault(w);
}
struct NativeEntity*late_guard_spawn_choice(struct WeaponWorld*w,struct EntityRef parent,const struct PlayerTransform*t,unsigned choice){if(choice>=16){enemy_fault(w);return 0;}return spawn(w,parent,ENTITY_FIRST_CHILD,t,late_guard_choices[choice]);}
int late_guard_spawn_template(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){
 if(id<31||id>49)return enemy_fault(w);struct NativeEntity*n=spawn(w,parent,order,t,id>=45?id-36:id-31);if(!n)return 0;n->payload.enemy.siege.closing=id>=45;/* $060426 etc. skip forced facing. */if(params)for(unsigned i=0;i<8;i++)n->payload.enemy.params[i]=params[i];return 1;
}
