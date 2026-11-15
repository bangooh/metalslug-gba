#include "tank_internal.h"
#include "m4_internal.h"
#include "../generated/walker-programs.h"
/* $06D654..$06DCE0 and $06E176..$06E50C. The nine constructor table
 * entries interleave attack modes; source code order is not template order. */
enum State {NEW,IDLE_A,IDLE_B,WAKE,PAUSE,FIRST,RECOUNT,FIRE,DEAD,EXPLODE,
 SHELL,TRAIL,MUZZLE,SHELL_IMPACT,IMPACT,SMOKE_BLAST,SMOKE_LOW,DELETE_TREE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static void schedule(struct NativeEntity*e,unsigned s){e->tick=tick;enemy_schedule(&e->payload.enemy,s);}
static void init(struct NativeEntity*e,unsigned s){uint8_t*p=(uint8_t*)&e->payload.enemy.walker;for(unsigned i=0;i<sizeof(e->payload.enemy.walker);i++)p[i]=255;schedule(e,s);}
static struct NativeEntity*child(struct WeaponWorld*w,struct NativeEntity*e,unsigned s,enum EntityInsert order){struct NativeEntity*n=enemy_allocate(w,entity_ref(e),order,&e->payload.enemy.actor.transform,tick);if(n)init(n,s);return n;}
static void start(struct EnemyActor*q,unsigned n){animation_begin(&q->actor.animation,walker_entries[n]);q->actor.movement_flags|=32;}
static void defense(struct EnemyActor*q,unsigned n){q->actor.defense=n==65535?0:walker_bindings[n].defense;q->actor.defense_count=n==65535?0:walker_bindings[n].count;}
static void attack(struct EnemyActor*q,unsigned n){if(n==65535)q->actor.attack=0;else{q->attack_set=(struct HitAttackSet){walker_bindings[n].attack,walker_bindings[n].count};q->actor.attack=&q->attack_set;}}
static int16_t rank(struct WeaponWorld*w,struct EnemyActor*q,unsigned n){struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid||n>=WKR_COUNT){enemy_fault(w);return 0;}return (int16_t)walker_rank[n][(interaction_rank(w,&q->actor,runtime_owner_walker[n]))*8+(q->source_flags&3)*2+((r->player_count_flag&2)>>1)];}
static int stamp(struct WeaponWorld*w,struct EnemyActor*q){if(!q->walker.stamp)return 1;const struct WalkerStamp*s=&walker_stamps[q->walker.variant!=0];return terrain_stamp(w->interactions->terrain,&q->actor.transform,s->dx,s->dy,s->width,s->height,s->cells)?1:enemy_fault(w);}
static int fragments(struct WeaponWorld*w,struct NativeEntity*e){for(unsigned i=0;i<16;i++){struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,BFX_CRASH_FRAGMENT);if(!n)return 0;n->payload.enemy.params[3]=e->payload.enemy.params[3];}return 1;}
static int smoke(struct WeaponWorld*w,struct NativeEntity*e,int16_t y){struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,BFX_SMOKE);if(!n)return 0;n->payload.enemy.actor.transform.priority=0x8000;transform_integer_offset(&n->payload.enemy.actor.transform,0,y);return 1;}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read(void*ctx,unsigned f,unsigned width){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;if(width==1&&f==WKF_20)return q->actor.phase;if(width==2){if(f==WKF_16)return q->walker.palettes[0];if(f==WKF_18)return q->walker.palettes[1];}enemy_fault(c->w);return 0;}
static int write(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){struct Context*c=ctx;if(f!=WKF_20||width!=1||op!=AN_FIELD_SET)return enemy_fault(c->w);c->e->payload.enemy.actor.phase=(uint8_t)v;return 1;}
static int bind(void*ctx,unsigned kind,uint32_t v){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;if(kind==AN_PALETTE){q->actor.palette=(uint16_t)v;return 1;}unsigned n=v==0xffffffffu?65535:v;if(n!=65535&&n>=sizeof(walker_bindings)/sizeof(*walker_bindings))return enemy_fault(c->w);if(kind==AN_DEFENSE)defense(q,n);else if(kind==AN_ATTACK){attack(q,n);hit_begin_attack(&q->actor.hit);}else return enemy_fault(c->w);return 1;}
static int sound(void*ctx,uint16_t s,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,s,stop)?1:enemy_fault(c->w);}
static int callback(void*ctx,unsigned cb,struct Animation*an){(void)an;struct Context*c=ctx;struct WeaponWorld*w=c->w;struct NativeEntity*e=c->e,*n;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 switch(cb){
 case WKC_283CA:hit_begin_attack(&a->hit);return 1;
 case WKC_6E3A4:
  if(!tk_sound(w,0x1065))return 0;n=child(w,e,MUZZLE,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.actor.transform.priority=0xd000;transform_integer_offset(&n->payload.enemy.actor.transform,tk_faced(a,-20),12);
  n=child(w,e,SHELL,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.walker.smoke_only=q->params[7];n->payload.enemy.walker.mode=q->walker.mode;n->payload.enemy.params[6]=q->params[6];n->payload.enemy.actor.transform.priority=0xd000;transform_integer_offset(&n->payload.enemy.actor.transform,tk_faced(a,-48),44);return 1;
 case WKC_6E412:return fragments(w,e);
 case WKC_6E436:return smoke(w,e,16);
 case WKC_6E456:n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_FLASH);if(!n)return 0;n->payload.enemy.actor.transform.priority=0x2000;return 1;
 case WKC_6E4CC:
  n=child(w,e,TRAIL,ENTITY_LAST_CHILD);if(!n)return 0;n->payload.enemy.params[6]=q->params[6];n->payload.enemy.actor.transform.priority--;q->walker.cycle=(q->walker.cycle+1)&3;if(q->walker.cycle>=3)q->walker.cycle=0;a->palette=q->walker.palettes[q->walker.cycle];return 1;
 default:return enemy_fault(w);
 }}
static int animate(struct WeaponWorld*w,struct NativeEntity*e){struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;struct Context c={w,e};struct AnimationOwner o={.context=&c,.read=read,.write=write,.bind=bind,.sound=sound,.call=callback};int done=animation_step(&a->animation,&walker_program,&o);if(a->animation.fault)enemy_fault(w);return done;}
static int trajectory(struct WeaponWorld*w,struct EnemyActor*q){unsigned row=rank(w,q,WKR_TRAJECTORY)&3;if(q->walker.mode==2)row=4;int target=enemy_nearest_x(w,&q->actor,&walker_filter);if(target<0)target=enemy_nearest_x(w,&q->actor,0);struct PlayerBodyState*p=w->players[target<0?1:target];int16_t px=p?tk_word((p->actor.transform.x>>8)-w->camera_x):-1,dx=tk_word(tk_sx(w,&q->actor)-px);if(dx<=96)dx=96;unsigned column=(uint16_t)tk_word(dx-96)>>4;if(column>=3)column=3;const int16_t*v=walker_trajectories[row][column];q->actor.vx=tk_faced(&q->actor,v[0]);q->actor.ay=v[1];q->actor.vy=v[2];return !w->fault;}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*n;unsigned s=a->state;
 switch(s){
 case NEW:
  if(!tk_settle(w,a))return 0;m4_reset(a);q->walker.palettes[0]=walker_palettes[q->params[3]&3];q->walker.palettes[1]=1;a->palette=q->walker.palettes[0];a->hit_palette=0x7c;a->hit.hp=q->walker.max_hp=rank(w,q,WKR_HP);defense(q,WKB_BODY);a->phase=0;q->walker.trigger=0;q->walker.limit=(q->params[2]?q->params[2]:21)<<4;interaction_priority(w,a,q->params[5]?0x8000:0x4000,20);if(!stamp(w,q))return 0;
  n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_DAMAGE_SMOKE_BODY);if(!n)return 0;n->payload.enemy.actor.attached_offset=0;schedule(e,q->walker.mode?IDLE_B:IDLE_A);return 2;
 case IDLE_A:q->walker.trigger=0;start(q,WKA_IDLE_A);break;
 case IDLE_B:start(q,WKA_IDLE_B);break;
 case WAKE:start(q,WKA_WAKE);break;
 case PAUSE:q->walker.timer=rank(w,q,WKR_WAIT);start(q,WKA_PAUSE);break;
 case FIRST:q->walker.repeats=1;start(q,WKA_FIRST);q->walker.timer=rank(w,q,WKR_PERIOD);a->state=FIRE;break;
 case RECOUNT:q->walker.repeats=rank(w,q,WKR_REPEATS);/* FALLTHROUGH */
 case FIRE:start(q,WKA_FIRE);q->walker.timer=rank(w,q,WKR_PERIOD);a->state=FIRE;break;
 case DEAD:a->display_flags&=253;m4_reset(a);start(q,WKA_DEAD);break;
 case EXPLODE:if(!tk_sound(w,0x1033)||!smoke(w,e,0)||!fragments(w,e))return 0;entity_delete(w->entities,e);return 0;
 case SHELL:if(!trajectory(w,q))return 0;a->hit.type_flags|=16;for(unsigned i=0;i<3;i++)q->walker.palettes[i]=0x157+i;a->palette=q->walker.palettes[0];interaction_priority(w,a,0xd000,20);start(q,WKA_SHELL);break;
 case TRAIL:for(unsigned i=0;i<3;i++)q->walker.palettes[i]=0x157+i;a->palette=q->walker.palettes[0];start(q,WKA_TRAIL);break;
 case MUZZLE:a->transform.facing^=1;a->palette=8;interaction_priority(w,a,0x4000,0);start(q,WKA_MUZZLE);break;
 case SHELL_IMPACT:
  if(q->walker.smoke_only){schedule(e,SMOKE_LOW);return 2;}interaction_priority(w,a,0x4000,28);if(!tk_sound(w,0x1021))return 0;n=boss_fx_pattern(w,entity_ref(e),&a->transform,BP_WALKER_IMPACT);if(!n)return 0;n->payload.enemy.actor.transform.priority=0xc000;a->palette=4;q->walker.timer=2;attack(q,WKB_IMPACT);hit_begin_attack(&a->hit);start(q,WKA_IMPACT);a->state=IMPACT;break;
 case SMOKE_BLAST:case SMOKE_LOW:interaction_priority(w,a,0x4000,s==SMOKE_LOW?0:28);defense(q,65535);boss_fx_transform(w,e,BFX_SMOKE);return 0;
 default:return enemy_fault(w);
 }return !w->fault;
}
static void body_tail(struct WeaponWorld*w,struct NativeEntity*e,int dead){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(!dead){if(q->params[4]){if(!w->enemies){enemy_fault(w);return;}w->enemies->intro_gate=0;}if(a->hit.flags&8){a->hit.flags&=247;if(!tk_hit_sound(w,a))return;}if(a->hit.flags&1)schedule(e,DEAD);}
 if(q->params[1]){int foot=terrain_weapon_foot_contact(w->interactions->terrain,a);if(foot<0){enemy_fault(w);return;}if(!foot)schedule(e,EXPLODE);}
 if(interaction_outside(w,a,walker_bounds[0],0))schedule(e,DELETE_TREE);
}
static void body_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(s!=DEAD)interaction_body_contact(w,a,&walker_contact);if(interaction_move(w,a,WMOVE_SYNC)<0)return;int done=animate(w,e);if(w->fault)return;
 switch(s){
 case IDLE_A:
  if(tk_sx(w,a)<q->walker.limit){int i=enemy_nearest_x(w,a,0);if(i>=0&&tk_word((a->transform.x>>8)-(w->players[i]->actor.transform.x>>8))<=96)schedule(e,WAKE);}if(q->walker.trigger)schedule(e,WAKE);break;
 case IDLE_B:if(tk_sx(w,a)<q->walker.limit&&enemy_nearest_x(w,a,&walker_filter)>=0)schedule(e,FIRST);break;
 case WAKE:if(done)schedule(e,FIRST);break;
 case PAUSE:q->walker.timer=tk_word(q->walker.timer-1);if(q->walker.timer<=0){if(q->walker.mode==0||q->walker.mode==2)schedule(e,RECOUNT);if(enemy_nearest_x(w,a,&walker_filter)>=0)schedule(e,RECOUNT);}break;
 case FIRE:if(done){q->walker.timer=tk_word(q->walker.timer-1);if(q->walker.timer<=0){schedule(e,FIRE);q->walker.repeats=tk_word(q->walker.repeats-1);if(q->walker.repeats<=0)schedule(e,PAUSE);}}break;
 case DEAD:break;
 default:enemy_fault(w);return;
 }body_tail(w,e,s==DEAD);
}
static void parts_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;int done,r;
 switch(s){
 case SHELL:r=interaction_move(w,a,WMOVE_FALL);if(r<0)return;if(r)schedule(e,SHELL_IMPACT);animate(w,e);if(!enemy_attack(w,a))return;if(a->hit.flags&2)schedule(e,SMOKE_BLAST);if(interaction_outside(w,a,walker_bounds[1],1))schedule(e,DELETE_TREE);return;
 case TRAIL:{struct NativeEntity*p=entity_get(w->entities,e->parent);struct WeaponActor*owner=entity_weapon_actor(p);if(!owner){enemy_fault(w);return;}a->palette=owner->palette;}/* FALLTHROUGH */
 case MUZZLE:if(interaction_move(w,a,WMOVE_SYNC)<0)return;if(animate(w,e))schedule(e,DELETE_TREE);return;
 case IMPACT:if(interaction_move(w,a,WMOVE_SYNC)<0)return;done=animate(w,e);if(done)schedule(e,DELETE_TREE);if(!enemy_attack(w,a))return;q->walker.timer=tk_word(q->walker.timer-1);if(q->walker.timer<=0)attack(q,65535);if(interaction_outside(w,a,boss_fx_bounds[8],1))schedule(e,DELETE_TREE);return;
 default:enemy_fault(w);return;
 }}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;for(unsigned i=0;i<4;i++){
 if(q->actor.state==DELETE_TREE){entity_delete_children(p,e);entity_delete(p,e);return;}
 if(q->entry){q->entry=0;int r=enter(w,e);if(!r||w->fault)return;if(r==2)continue;}unsigned s=q->actor.state;if(s<SHELL)body_frame(w,e,s);else parts_frame(w,e,s);return;
 }enemy_fault(w);}
int walker_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){if(id<66||id>74)return enemy_fault(w);struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;init(e,NEW);struct EnemyActor*q=&e->payload.enemy;unsigned form=id-66;q->walker.mode=form%3;q->walker.stamp=form/3!=0;q->walker.variant=form/3==2;if(params)for(unsigned i=0;i<8;i++)q->params[i]=params[i];return 1;}
