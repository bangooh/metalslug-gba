#include "m5_internal.h"
#include "slug_surface.h"
#include "scene_video.h"
/* $04CBD4 attached M5 form and $04CBEA standalone entry share $04CBFC.
 * Source angle step deliberately returns 255, not -1. */
enum State {ROOT,IDLE,TRACK,RECOIL,BODY,DRIVER,READY,FIRE,CANNON,SHELL,DELETE,WAIT_X,BASE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static struct NativeEntity*spawn(struct WeaponWorld*w,struct NativeEntity*e,unsigned state,enum EntityInsert order){
 struct NativeEntity*n=enemy_allocate(w,entity_ref(e),order,&e->payload.enemy.actor.transform,tick);if(!n)return 0;
 uint8_t*b=(uint8_t*)&n->payload.enemy.turret;for(unsigned i=0;i<sizeof(n->payload.enemy.turret);i++)b[i]=255;enemy_schedule(&n->payload.enemy,state);return n;
}
static void offset(struct WeaponActor*a,unsigned table,unsigned angle){const int16_t*v=m5_turret_offsets[table][angle&31];transform_integer_offset(&a->transform,v[0],v[1]);}
static int step(unsigned angle,unsigned target){int16_t d=m5_word(target-angle);return !d?0:d&16?255:1;}
static struct PlayerTransform pending(struct WeaponWorld*w,struct WeaponActor*a){
 struct PlayerTransform t=a->transform;
 if(!(a->hit.flags&64)){if(!w->interactions||!w->interactions->terrain){enemy_fault(w);return t;}transform_integer_offset(&t,w->interactions->terrain->delta_x,-w->interactions->terrain->delta_y);}return t;
}
static int aim(struct WeaponWorld*w,struct EnemyActor*q,int mirror,int before_move){
 struct WeaponActor query={.transform=before_move?pending(w,&q->actor):q->actor.transform};
 int slot=enemy_nearest_x(w,&query,0);
 /* $05E136 preserves D0=0 from the empty player mask on carry. */
 unsigned raw=slot<0?0:enemy_angle(m5_word(m5_player_x(w,(unsigned)slot)-m5_sx(w,&query)),m5_word(m5_player_y(w,(unsigned)slot)-m5_sy(w,&query)));
 int angle=(raw+8)>>3;
 if(mirror&&!(angle&16))angle=-angle;return angle&31;
}
static int probe(struct WeaponWorld*w,struct WeaponActor*a){
 struct PlayerTransform t=pending(w,a);if(w->fault)return 0;
 for(unsigned i=1;i<7;i++){uint8_t shape;if(!terrain_weapon_point_shape(w->interactions->terrain,a,m5_word(t.x>>8),m5_word(transform_y_up(&t)-8*i),&shape))return enemy_fault(w);if(shape==1)return 1;}return 0;
}
static void sync_root(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;if(q->actor.phase!=0x44){interaction_move(w,&q->actor,WMOVE_SYNC);return;}struct EnemyActor*p=m5_parent(w,e);if(!p)return;uint16_t priority=q->actor.transform.priority;m5_follow(q,p,0,0,-35,45,0);q->actor.transform.priority=priority;}
static void follow_driver(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy,*p=m5_parent(w,e);if(!p)return;uint16_t priority=q->actor.transform.priority;m5_follow(q,p,0,0,0,0,0);q->actor.transform.priority=priority;q->selector=p->selector;
}
static void release_parent(struct WeaponWorld*w,struct NativeEntity*e){
 if(e->payload.enemy.actor.phase!=0x44)return;struct EnemyActor*p=m5_parent(w,e);if(p)p->actor.phase=0;
}
static int prepare_escape(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(q->dead==255){
  if(a->phase==0x88){struct EnemyActor*p=m5_parent(w,e);if(!p)return 0;p->dead++;}
  q->turret.retired=255;
  if(!pow_spawn_surface(w,entity_ref(e),&a->transform))return 0;offset(a,2,q->selector);
  release_parent(w,e);return 1;
 }return 0;
}
static void root_tail(struct WeaponWorld*w,struct NativeEntity*e,int escape){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(w->fault)return;
 int retire=(a->phase==0x44&&!q->turret.held)||interaction_outside(w,a,m5_turret_bounds[0],0);
 if(retire){q->turret.retired=255;release_parent(w,e);entity_delete(w->entities,e);return;}
 /* Later TRACK/RECOIL assignments overwrite the earlier escape schedule. */
 if(escape)infantry_force_escape(e,(uint8_t)((uint16_t)q->turret.wait>>8));
}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned state=a->state;q->entry=0;struct EnemyActor*p;
 switch(state){
 case ROOT:
  a->phase=0x44;q->turret.held=255;q->params[0]=1;/* FALLTHROUGH */
 case BASE:
  a->palette=14;q->scale_x=q->scale_y=255;a->transform.priority=0x8000;q->turret.ready=255;q->turret.retired=0;q->dead=0;q->selector=0;m5_reset(a);
  if(!spawn(w,e,BODY,ENTITY_FIRST_CHILD))return 0;a->state=IDLE;/* FALLTHROUGH */
 case IDLE:q->turret.target=q->selector&8?31:17;q->turret.fire=0;q->turret.driver=0;a->timer=0;m5_start(q,M5A_TC_IDLE);break;
 case TRACK:q->turret.wait=m5_rank_value(w,q,M5R_TC_WAIT);break;
 case RECOIL:q->turret.fire=0;a->transform.facing|=(q->selector>>3)&1;a->bounces=50;m5_start(q,M5A_TC_RECOIL);break;
 case BODY:
  if(!entity_get(w->entities,e->parent)||entity_get(w->entities,e->parent)->tick!=tick){entity_delete(w->entities,e);return 2;}
  a->palette=0x22;q->scale_x=q->scale_y=255;a->hit.hp=m5_rank_value(w,q,M5R_TC_HP);m5_reset(a);p=m5_parent(w,e);if(!p)return 0;q->selector=p->selector;m5_start(q,M5A_TC_BODY);break;
 case DRIVER:
  a->palette=0x23;p=m5_parent(w,e);if(!p)return 0;a->transform.facing|=(p->selector>>3)&1;q->scale_x=q->scale_y=255;a->hit.hp=m5_rank_value(w,q,M5R_TC_DRIVER_HP);
  q->attack_set=(struct HitAttackSet){m5_bindings[M5_TURRET_ATTACK].attack,m5_bindings[M5_TURRET_ATTACK].count};a->attack=&q->attack_set;hit_begin_attack(&a->hit);m5_reset(a);m5_start(q,M5A_TC_DRIVER);break;
 case READY:a->transform.facing=0;a->timer=300;m5_start(q,M5A_TC_READY);break;
 case FIRE:if(!m5_sound(w,0x10a3))return 0;m5_start(q,M5A_TC_FIRE);break;
 case CANNON:
  offset(a,1,q->selector);a->transform.priority+=2;q->turret.fled=0;a->timer=65535;q->scale=(uint16_t)m5_rank_value(w,q,M5R_TC_SPEED);p=m5_parent(w,e);if(!p)return 0;q->selector=p->selector;m5_start(q,M5A_TC_CANNON);
  /* Source loop fills $89..$97; $88 is set by the first history shift. */
  for(unsigned i=1;i<16;i++)q->turret.history[i]=(uint8_t)q->selector;
  if(q->selector==17)q->selector+=2;else if(q->selector==31)q->selector-=2;break;
 case SHELL:{
  m5_start(q,M5A_TC_SHELL);a->palette=13;q->scale_x=q->scale_y=255;q->scale=128;
  if(!w->rng)return enemy_fault(w);int16_t r=(int16_t)source_random(w->rng,2);if(!(r&1))r=m5_word(-r);q->selector=(q->selector+r)&31;
  weapon_vector((uint8_t)(q->selector*8),(int16_t)q->scale,&a->vx,&a->vy);
  if(interaction_outside(w,a,m5_turret_bounds[2],1)){entity_delete(w->entities,e);return 2;}break;
 }
 case DELETE:entity_delete(w->entities,e);return 2;
 case WAIT_X:break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static void tick(struct EntityPool*pool,struct NativeEntity*e,void*world){
 (void)pool;struct WeaponWorld*w=world;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(w->fault)return;
 if(q->entry&&enter(w,e)!=1)return;unsigned state=a->state;struct EnemyActor*p;
 if(state==WAIT_X){if(interaction_move(w,a,WMOVE_SYNC)<0)return;if(m5_sx(w,a)<=320)return;enemy_schedule(q,BASE);if(enter(w,e)!=1)return;state=a->state;}
 if(state==IDLE||state==TRACK||state==RECOIL){
  sync_root(w,e);if(w->fault)return;
  if(state==RECOIL){int16_t x=m5_word(a->transform.x>>8),y=transform_y_up(&a->transform);offset(a,2,q->selector);m5_animate(w,e);if(!--a->bounces){a->transform.facing=0;enemy_schedule(q,IDLE);}transform_integer_offset(&a->transform,m5_word(x-(a->transform.x>>8)),m5_word(y-transform_y_up(&a->transform)));int escape=prepare_escape(w,e);root_tail(w,e,escape);return;}
  a->timer=(a->timer+1)&(q->params[0]?1:7);
  if(!a->timer){unsigned target=(uint16_t)q->turret.target;if(state==TRACK)target=(uint16_t)m5_turret_pose[aim(w,q,1,0)];q->selector=(q->selector+step(q->selector,target))&31;}
  m5_animate(w,e);
  int escape=prepare_escape(w,e);if(w->fault)return;
  if(state==IDLE&&q->selector==(uint16_t)q->turret.target){
   if(q->turret.driver==255){enemy_schedule(q,TRACK);escape=0;}
   else if(q->turret.driver!=128){m5_start(q,M5A_TC_EMPTY);a->transform.facing|=(q->selector>>3)&1;if(!spawn(w,e,DRIVER,ENTITY_LAST_CHILD))return;q->turret.driver=128;}
  }else if(state==TRACK&&q->turret.ready==255){q->turret.wait=m5_word(q->turret.wait-1);if(!q->turret.wait){q->turret.ready=0;q->turret.fire=255;enemy_schedule(q,RECOIL);escape=0;}}
  root_tail(w,e,escape);return;
 }
 p=m5_parent(w,e);if(!p)return;
 switch(state){
 case BODY:
  if(!entity_get(w->entities,e->parent)||entity_get(w->entities,e->parent)->tick!=tick){entity_delete(w->entities,e);return;}
  m5_follow(q,p,0,0,0,0,0);q->selector=p->selector;if(p->turret.fire==255)m5_start(q,M5A_TC_BODY_FIRE);m5_animate(w,e);
  {struct NativeEntity*pe=entity_get(w->entities,e->parent);int parent_gone=p->actor.phase==0x44&&entity_parent_deleting(w->entities,pe);
   if(!parent_gone&&p->turret.retired==255){entity_delete(w->entities,e);return;}
   if(parent_gone||(a->hit.flags&8)){if(!m5_hit_sound(w,a))return;a->hit.flags&=247;}}
  if(a->hit.flags&1){if(!m5_sound(w,0x1023))return;p->dead=255;
   if(!boss_fx_pattern(w,entity_ref(e),&a->transform,BP_TURRET_BODY0)||!boss_fx_pattern(w,entity_ref(e),&a->transform,BP_TURRET_BODY1))return;
   if(p->actor.phase==0x88&&!scene_video_patch(w->video,&m5_turret_death_patch)){enemy_fault(w);return;}entity_delete(w->entities,e);}return;
 case DRIVER:case READY:case FIRE:{
  if(!entity_get(w->entities,e->parent)||entity_get(w->entities,e->parent)->tick!=tick){entity_delete(w->entities,e);return;}
  follow_driver(w,e);int done=m5_animate(w,e);
  if(state==DRIVER&&done){p->turret.driver=255;enemy_schedule(q,READY);}else if(state==READY&&p->turret.fire==255)enemy_schedule(q,FIRE);else if(state==FIRE&&done)enemy_schedule(q,CANNON);
  if(p->turret.retired==255)entity_delete(w->entities,e);return;
 }
 case CANNON:{
  a->timer++;int target=q->turret.target;
  if((int16_t)a->timer<=600){
   if(!(a->timer&15)){target=aim(w,q,0,1);if(q->turret.fled!=255){if(!w->rng){enemy_fault(w);return;}target=source_random(w->rng,16);struct WeaponActor query={.transform=pending(w,a)};if(m5_sy(w,&query)>=448)q->turret.fled=255;}if(probe(w,a))target=8;q->turret.target=(int16_t)target;}
   else goto history;
  }
  {int turn=step(q->selector,(unsigned)target)|1;if(!(a->timer&31))q->turret.step=(int16_t)turn;}
 history:
  if(!(a->timer&3))q->selector=(q->selector+q->turret.step)&31;
  for(unsigned i=15;i;i--)q->turret.history[i]=q->turret.history[i-1];q->turret.history[0]=(uint8_t)q->selector;
  unsigned angle=q->turret.history[15]&31;int16_t speed=(int16_t)q->scale;if(probe(w,a)&&(angle&16))speed>>=1;weapon_vector((uint8_t)(angle*8),speed,&a->vx,&a->vy);a->ay=angle&16?-96:0;
  if(w->fault)return;
  /* Attached mode deliberately bypasses scroll, but still keeps screen XY. */
  if(p->actor.phase==0x44){a->transform=pending(w,a);a->hit.flags|=64;}
  uint16_t selector=q->selector;int collision=interaction_move(w,a,p->actor.phase==0x44?WMOVE_BODY:WMOVE_POINT);q->selector=selector;if(collision<0)return;
  int explode=collision;
  if(!collision){m5_animate(w,e);if(!(a->timer&7)){
    struct NativeEntity*n=spawn(w,e,SHELL,q->selector<=16?ENTITY_FIRST_CHILD:ENTITY_LAST_CHILD);if(!n)return;if(q->selector>16)n->payload.enemy.actor.transform.priority--;
    offset(&n->payload.enemy.actor,0,q->selector);n->payload.enemy.selector=q->turret.history[15]&31;
   }
   if(!enemy_attack(w,a))return;explode=!!(a->hit.flags&2);
   if(!explode&&(a->hit.flags&8)){if(!m5_hit_sound(w,a))return;a->hit.flags&=247;}if(a->hit.flags&1)explode=1;
  }
  if(explode){struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_FIRE);if(!n)return;n->payload.enemy.actor.transform.priority=0x4000;
   if(!boss_fx_pattern(w,entity_ref(e),&a->transform,BP_TURRET_CANNON)||!m5_sound(w,0x1022))return;if(entity_get(w->entities,e->parent)->tick==tick)p->turret.ready=255;entity_delete(w->entities,e);return;}
  if(interaction_outside(w,a,m5_turret_bounds[1],1)){if(entity_get(w->entities,e->parent)->tick==tick)p->turret.ready=255;entity_delete(w->entities,e);}return;
 }
 case SHELL:interaction_move(w,a,WMOVE_POINT);if(m5_animate(w,e)||interaction_outside(w,a,m5_turret_bounds[2],1))entity_delete(w->entities,e);return;
 default:enemy_fault(w);return;
 }
}
struct NativeEntity*m5_turret_spawn(struct WeaponWorld*w,struct NativeEntity*e){return spawn(w,e,ROOT,ENTITY_FIRST_CHILD);}
void m5_turret_release(struct NativeEntity*e){e->payload.enemy.turret.held=0;}
int turret29_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,const struct PlayerTransform*t,const uint8_t params[8]){
 struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;uint8_t*b=(uint8_t*)&e->payload.enemy.turret;for(unsigned i=0;i<sizeof(e->payload.enemy.turret);i++)b[i]=255;if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];enemy_schedule(&e->payload.enemy,WAIT_X);return 1;
}
