#include "tank_internal.h"
#include "m4_internal.h"
#include "../generated/nest185-programs.h"
enum State {ROOT,NEST,NEST_HIT,NEST_RESET,NEST_DEAD,NEST_WRECK,FALL_INIT,FALL,NEST2,NEST2_HIT,NEST2_CLEAR,NEST2_WAIT,
 SWARM_NEW,AIM_PARENT,AIM_RANDOM,FLY,FLEE,WANDER_INIT,WANDER,STATIC,CAMERA,LOB,LOB_LAND,SHARD,SHARD_LAND,SHARD_WAIT};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static void schedule(struct NativeEntity*e,unsigned s){e->tick=tick;enemy_schedule(&e->payload.enemy,s);}
static struct NativeEntity*allocate(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,unsigned s){struct NativeEntity*e=enemy_allocate(w,p,o,t,tick);if(e){struct EnemyActor*q=&e->payload.enemy;q->nest185=(__typeof__(q->nest185)){.timer=-1,.floor=-1,.stage=255,.attached=0,.variant=255,.land_alias=0};q->selector=q->scale=65535;schedule(e,s);}return e;}
static unsigned random_mask(struct WeaponWorld*w,unsigned n){if(!w->rng){enemy_fault(w);return 0;}return source_random_mask(w->rng,n);}
static void start(struct EnemyActor*q,unsigned n){animation_begin(&q->actor.animation,nest185_entries[n]);q->actor.movement_flags|=32;}
static void defense(struct EnemyActor*q,unsigned n){q->actor.defense=n==65535?0:nest185_bindings[n].defense;q->actor.defense_count=n==65535?0:nest185_bindings[n].count;}
struct Context{struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read(void*ctx,unsigned f,unsigned width){(void)width;struct Context*c=ctx;if(f!=N185F_PHASE){enemy_fault(c->w);return 0;}return c->e->payload.enemy.actor.phase;}
static int field(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){struct Context*c=ctx;uint32_t old=read(ctx,f,width);if(op==AN_FIELD_AND)v&=old;else if(op==AN_FIELD_OR)v|=old;else if(op==AN_FIELD_XOR)v^=old;else if(op!=AN_FIELD_SET)return enemy_fault(c->w);c->e->payload.enemy.actor.phase=v;return !c->w->fault;}
static int sound(void*ctx,uint16_t n,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,n,stop)?1:enemy_fault(c->w);}
static int animate(struct WeaponWorld*w,struct NativeEntity*e){struct Context c={w,e};struct AnimationOwner o={.context=&c,.read=read,.write=field,.sound=sound};int r=animation_step(&e->payload.enemy.actor.animation,&nest185_program,&o);if(e->payload.enemy.actor.animation.fault)enemy_fault(w);return r;}
static void face(struct EnemyActor*q){q->params[0]&=1;q->actor.transform.facing=q->params[0];}
static void speed(struct EnemyActor*q){q->actor.vx=tk_word((q->params[1]<<5)*(q->actor.transform.facing?1:-1));}
static void erase(struct WeaponWorld*w,struct NativeEntity*e,int tree){if(tree)entity_delete_children(w->entities,e);entity_delete(w->entities,e);}
static int bounds(struct WeaponWorld*w,struct NativeEntity*e){static const int16_t b[]={0,1,0,1};if(interaction_outside(w,&e->payload.enemy.actor,b,0)){erase(w,e,1);return 0;}return !w->fault;}
static int latched(struct WeaponWorld*w,struct NativeEntity*e,int tree,int clear){struct WeaponActor*a=&e->payload.enemy.actor;int16_t x=tk_sx(w,a),y=tk_sy(w,a);if(a->hit.flags&128){if(x<0||x>320){if(clear)defense(&e->payload.enemy,65535);erase(w,e,tree);return 0;}}else if(x>0&&x<320&&y>256&&y<496)a->hit.flags|=128;return !w->fault;}
static int parent_tail(struct WeaponWorld*w,struct NativeEntity*e){if(!e->payload.enemy.nest185.attached)return 1;struct NativeEntity*p=entity_get(w->entities,e->parent);struct WeaponActor*a=entity_weapon_actor(p);if(!a)return enemy_fault(w);/* $08EA50 installs $08EA16 directly, skipping $08E9FC's velocity/defense init. */if(!a->defense)schedule(e,FALL);return 1;}
static int spawn_swarm(struct WeaponWorld*w,struct NativeEntity*e,unsigned param){struct EnemyActor*q=&e->payload.enemy;for(unsigned i=0;i<256;i++){q->params[param]--;if((int8_t)q->params[param]<0)return 1;if(!allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&q->actor.transform,SWARM_NEW))return 0;}return enemy_fault(w);}
static void vector(struct EnemyActor*q,int16_t magnitude){weapon_vector(q->selector,magnitude,&q->actor.vx,&q->actor.vy);q->actor.transform.facing=q->actor.vx>=0;}
static int aim(struct WeaponWorld*w,struct NativeEntity*e,int away){struct EnemyActor*q=&e->payload.enemy,*p=tk_parent(w,e);if(!p)return 0;int16_t dx=tk_word((p->actor.transform.x>>8)-12-(q->actor.transform.x>>8)),dy=tk_word(transform_y_up(&p->actor.transform)-24-transform_y_up(&q->actor.transform));if(!(q->actor.hit.flags&64)){if(!w->interactions||!w->interactions->terrain)return enemy_fault(w);dx=tk_word(dx-w->interactions->terrain->delta_x);dy=tk_word(dy+w->interactions->terrain->delta_y);}if(away){dx=tk_word(-dx);dy=tk_word(-dy);}q->selector=enemy_angle(dx,dy);return 1;}
int scene_preserve_screen(struct WeaponWorld*w,struct WeaponActor*a){if(a->hit.flags&64)return 1;if(!w->interactions||!w->interactions->terrain)return enemy_fault(w);struct TerrainWorld*t=w->interactions->terrain;transform_integer_offset(&a->transform,t->delta_x,-t->delta_y);a->hit.flags|=64;return 1;}
int scene_scroll_tertiary(struct WeaponWorld*w,struct WeaponActor*a){if(!w->tertiary_delta_valid||!scene_preserve_screen(w,a))return enemy_fault(w);transform_integer_offset(&a->transform,-w->tertiary_delta_x,w->tertiary_delta_y);return 1;}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned v=q->nest185.variant;
 switch(a->state){
 case ROOT:
  if(v==185){if(!q->nest185.attached)a->transform.priority=0x8018;face(q);a->palette=0x125;start(q,N185A_NEST);defense(q,0);a->hit.flags&=247;a->hit.hp=100;q->nest185.stage=0;if(!spawn_swarm(w,e,2))return 0;a->state=NEST;}
  else if(v==187){a->transform.priority=0x8018;defense(q,1);a->hit.flags&=247;a->hit.hp=100;a->phase=0;if(!spawn_swarm(w,e,0))return 0;a->state=NEST2;}
  else if(v==186){a->transform.priority=0x8018;face(q);a->palette=0x125;start(q,N185A_SWARM);schedule(e,WANDER_INIT);return 2;}
  else if(v==188){a->transform.priority=0;face(q);a->palette=0x12a;start(q,N185A_STATIC);a->state=STATIC;}
  else if(v==189||v==190){start(q,v==189?N185A_CAMERA0:N185A_CAMERA1);a->transform.priority=0;a->palette=0xf4;a->state=CAMERA;}
  else if(v==191){face(q);speed(q);a->vy=tk_word(-(q->params[1]<<5));a->transform.priority=0;a->palette=0xf4;start(q,N185A_LOB);a->state=LOB;}
  else return enemy_fault(w);break;
 case NEST_RESET:a->hit.flags&=247;a->hit.hp=100;a->state=NEST;break;
 case NEST_HIT:if(q->nest185.stage>=3)return enemy_fault(w);start(q,N185A_HIT0+q->nest185.stage);break;
 case NEST_DEAD:defense(q,65535);start(q,N185A_DEAD);break;
 case FALL_INIT:defense(q,65535);a->vy=-128;a->ay=-64;a->state=FALL;break;
 case FALL:case AIM_PARENT:case AIM_RANDOM:break;
 case NEST2_HIT:a->phase=1;q->nest185.timer=20;schedule(e,NEST2_CLEAR);return 0;/* Exactly one scheduled frame with phase1. */
 case NEST2_CLEAR:a->phase=0;a->state=NEST2_WAIT;break;
 case SWARM_NEW:a->palette=0x125;start(q,N185A_SWARM);a->state=AIM_PARENT;break;
 case FLEE:if(!aim(w,e,1))return 0;vector(q,768);q->nest185.timer=4;break;
 case WANDER_INIT:q->selector=random_mask(w,255);vector(q,256+random_mask(w,255));q->nest185.timer=2+random_mask(w,3);a->state=WANDER;break;
 case SHARD:{if(v>2)return enemy_fault(w);start(q,N185A_SHARD0+2*v);a->vy=tk_word(-random_mask(w,31));a->ay=tk_word(random_mask(w,31)-48);face(q);a->palette=0xad;struct NativeEntity*p=entity_get(w->entities,e->parent),*g=p?entity_get(w->entities,p->parent):0;struct WeaponActor*owner=entity_weapon_actor(g);if(!owner)return enemy_fault(w);q->nest185.floor=tk_sy(w,owner);break;}
 case SHARD_LAND:/* $08EF40/$08EF5A/$08EF74 all compare variant with ZERO. */if(!q->nest185.variant)start(q,N185A_LAND0+2*q->nest185.land_alias);a->state=SHARD_WAIT;break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static int frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(s==NEST||s==NEST_HIT||s==NEST_DEAD||s==NEST_WRECK){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;int done=animate(w,e);if(s==NEST&&(a->hit.flags&8))schedule(e,q->nest185.stage<3?NEST_HIT:NEST_DEAD);if(s==NEST_HIT&&done){q->nest185.stage++;schedule(e,NEST_RESET);}if(s==NEST_DEAD&&done)a->state=NEST_WRECK;if(!latched(w,e,1,0))return 0;return parent_tail(w,e);}
 if(s==FALL){int r=interaction_move(w,a,WMOVE_ITEM);if(r<0)return 0;if(r){erase(w,e,1);return 0;}if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;animate(w,e);return latched(w,e,1,0);}
 if(s==NEST2||s==NEST2_WAIT){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;if(s==NEST2){if(a->hit.flags&8)schedule(e,NEST2_HIT);}else{q->nest185.timer=tk_word(q->nest185.timer-1);if(q->nest185.timer<=0){a->hit.flags&=247;a->hit.hp=100;a->phase=0;a->state=NEST2;}}return latched(w,e,1,1);}
 if(s==AIM_PARENT||s==AIM_RANDOM){if(s==AIM_PARENT){if(!aim(w,e,0))return 0;}else q->selector=random_mask(w,255);vector(q,384+random_mask(w,255));q->nest185.timer=1+random_mask(w,3);a->state=s=FLY;}
 if(s==FLY||s==FLEE||s==WANDER){weapon_integrate_free(a);if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;q->nest185.timer=tk_word(q->nest185.timer-1);if(q->nest185.timer<=0)schedule(e,s==WANDER?WANDER_INIT:s==FLEE?AIM_PARENT:random_mask(w,3)?AIM_RANDOM:AIM_PARENT);if(s==FLY){struct EnemyActor*p=tk_parent(w,e);if(!p)return 0;if(p->actor.phase)schedule(e,FLEE);}animate(w,e);return bounds(w,e);}
 if(s==STATIC||s==CAMERA){if(s==STATIC){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;}else if(!scene_scroll_tertiary(w,a))return 0;if(!latched(w,e,0,0))return 0;animate(w,e);return !w->fault;}
 if(s==LOB){if(!scene_preserve_screen(w,a))return 0;weapon_integrate_free(a);if(tk_sy(w,a)<=400){start(q,N185A_LOB_LAND);a->state=LOB_LAND;}if(!bounds(w,e))return 0;animate(w,e);return !w->fault;}
 if(s==LOB_LAND){if(!scene_scroll_tertiary(w,a)||!bounds(w,e))return 0;if(animate(w,e))erase(w,e,0);return !w->fault;}
 if(s==SHARD){weapon_integrate_free(a);a->vx=tk_word(a->vx+a->ax);a->vy=tk_word(a->vy+a->ay);if(q->nest185.floor>=tk_word(tk_sy(w,a)-((a->hit.flags&64)?0:w->interactions->terrain->delta_y)))schedule(e,SHARD_LAND);if(interaction_move(w,a,WMOVE_SYNC)<0||!bounds(w,e))return 0;animate(w,e);return !w->fault;}
 if(s==SHARD_WAIT){if(interaction_move(w,a,WMOVE_SYNC)<0||!bounds(w,e))return 0;if(animate(w,e))erase(w,e,0);return !w->fault;}
 return enemy_fault(w);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){(void)p;struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;for(unsigned b=0;b<5;b++){if(q->entry){q->entry=0;int r=enter(w,e);if(!r||w->fault)return;if(r==2)continue;}if(!e->active||e->deleting)return;frame(w,e,q->actor.state);return;}enemy_fault(w);}
int nest185_spawn(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){if(id<185||id>191)return enemy_fault(w);struct NativeEntity*e=allocate(w,p,o,t,ROOT);if(!e)return 0;e->payload.enemy.nest185.variant=id;if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];return 1;}
struct NativeEntity*nest185_attached(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,const uint8_t params[8]){struct NativeEntity*e=allocate(w,p,o,t,ROOT);if(!e)return 0;struct EnemyActor*q=&e->payload.enemy;q->nest185.variant=185;q->nest185.attached=1;if(params)for(unsigned i=0;i<8;i++)q->params[i]=params[i];q->params[2]=3;return e;}
struct NativeEntity*nest185_shard(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,unsigned v,const uint8_t params[8]){if(v>2){enemy_fault(w);return 0;}struct NativeEntity*e=allocate(w,p,o,t,SHARD);if(e){e->payload.enemy.nest185.variant=v;if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];}return e;}
int nest185_fall(struct WeaponWorld*w,struct NativeEntity*e){if(!e||e->kind!=ENTITY_ENEMY)return enemy_fault(w);schedule(e,FALL_INIT);tick(w->entities,e,w);return !w->fault;}
int nest185_shard_land_alias(struct WeaponWorld*w,struct NativeEntity*e,unsigned which){if(!e||e->kind!=ENTITY_ENEMY||which<1||which>2)return enemy_fault(w);e->payload.enemy.nest185.land_alias=which;schedule(e,SHARD_LAND);tick(w->entities,e,w);return !w->fault;}
