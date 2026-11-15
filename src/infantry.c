#include "enemies.h"
#include "slug_surface.h"
enum Kind {GUARD,GUARD_FIXED,CHARGER,SKIRMISHER,ESCAPING,SKIRMISHER_PRESERVE};
enum State {NEW,GUARD_BASE,GUARD_IDLE,GUARD_RECOVER,GUARD_ATTACK,GUARD_RETREAT,
 WALK_A,WALK_B,WALK_FAST,IDLE,MELEE,ATTACK,ATTACK_ANIM,EXIT,LOWER,RAISE,SKIRM_RUN,SKIRM_MELEE,
 ESCAPE_INIT,ESCAPE_FACE,ESCAPE_RUN,ESCAPE_WAIT,ESCAPE_STAND,ESCAPE_STAND_LOWER,
 ESCAPE_BRAKE,ESCAPE_LEAP,ESCAPE_FALL,ESCAPE_LAND,ESCAPE_GUARD,ESCAPE_CHARGER,ESCAPE_FACE_CLEAR};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static int16_t word(int n){return (int16_t)(uint16_t)n;}
static int16_t sx(struct WeaponWorld*w,struct WeaponActor*a){return word((a->transform.x>>8)-w->camera_x);}
static uint16_t random_word(struct WeaponWorld*w){if(!w->rng){enemy_fault(w);return 0;}return source_random_word(w->rng);}
static int rank(struct WeaponWorld*w,struct EnemyActor*q,unsigned table){
 struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid){enemy_fault(w);return 0;}
 unsigned level=interaction_rank(w,&q->actor,runtime_owner_infantry[table]),flags=q->source_flags&3;
 if(table<2)return infantry_rank[table][level][flags][(r->player_count_flag&2)>>1];
 unsigned row=infantry_speed_choices[table-2][level][flags][(uint8_t)(r->player_count_flag-1)&1];
 return infantry_speeds[table-2][row][(random_word(w)&14)>>1];
}
static int ground_test(struct WeaponWorld*w,struct EnemyActor*q){
 int r=terrain_weapon_ground_test(w->interactions->terrain,&q->actor);if(r<0)return enemy_fault(w);q->grounded=r?255:0;return 1;
}
static int physics(struct WeaponWorld*w,struct EnemyActor*q,int guard){
 if(guard&&interaction_move(w,&q->actor,WMOVE_SYNC)<0)return 0;
 if(guard&&q->infantry.kind==GUARD_FIXED)return 1;
 if(!enemy_ground_status(w,q,0))return 0;
 if(q->grounded)return guard?1:enemy_swapped_move(w,q,WMOVE_GROUND)>=0;
 int r=enemy_swapped_move(w,q,WMOVE_ITEM);if(r<0)return 0;q->grounded=r?255:0;return 1;
}
/* $0423AC returns C when the actor must turn. Null is distinct from the
 * source allocator's FF pointer sentinel, retained as target=-2 until set. */
static int face_target(struct WeaponWorld*w,struct EnemyActor*q){
 uint16_t distance;int slot=enemy_nearest_player(w,&q->actor,&distance);q->infantry.target=(int8_t)slot;
 if(slot<0)return 0;int16_t dx=word((w->players[slot]->actor.transform.x>>8)-(q->actor.transform.x>>8));
 return !((!!(q->actor.transform.facing&1))^(dx<0));
}
static int close_player(struct WeaponWorld*w,struct EnemyActor*q){
 int slot=enemy_nearest_x(w,&q->actor,0);if(slot<0)slot=1;
 /* Source $05E0D4 returns the fixed P2 address even with C set. Native
  * retired storage is retained; an unplaced fixed slot has the FF words. */
 int16_t x=w->players[slot]?word((w->players[slot]->actor.transform.x>>8)-w->camera_x):-1;
 int16_t y=w->players[slot]?word(transform_y_up(&w->players[slot]->actor.transform)+w->camera_y):-1;
 return (uint16_t)word(y-word(transform_y_up(&q->actor.transform)+w->camera_y))<64&&(uint16_t)word(x-sx(w,&q->actor)+32)<64;
}
static int melee_probe(struct WeaponWorld*w,struct EnemyActor*q){
 int16_t x=sx(w,&q->actor);if(x<32||x>=768)return 0;
 enemy_attack_binding(q,ENB_MELEE_PROBE);hit_begin_attack(&q->actor.hit);hit_begin_attack(&q->actor.hit);
 if(!enemy_attack(w,&q->actor))return 0;return !!(q->actor.hit.flags&2);
}
static void guard_timers(struct WeaponWorld*w,struct EnemyActor*q){
 if(!q->infantry.engaged){if(word(320-(q->params[0]<<3)-sx(w,&q->actor))>=0)q->infantry.engaged=1;return;}
 if(q->infantry.cooldown)q->infantry.cooldown--;if(q->infantry.lifetime)q->infantry.lifetime--;
}
static void guard_gate(struct WeaponWorld*w,struct EnemyActor*q){
 if(q->infantry.cooldown||(uint16_t)word(sx(w,&q->actor)-48)>=240||q->infantry.target==-1)return;
 q->infantry.cooldown=(q->infantry.cooldown&0xff00)|q->params[2];enemy_schedule(q,GUARD_ATTACK);
}
static int guard_think(struct WeaponWorld*w,struct EnemyActor*q){
 if(!(w->frame_phase&31)){
  if(face_target(w,q))q->actor.transform.facing^=1;
  if(q->infantry.target>=0){int16_t dy=word(transform_y_up(&q->actor.transform)-transform_y_up(&w->players[(unsigned)q->infantry.target]->actor.transform));q->infantry.attack_choice=dy<64?6:16;}
 }
 if(close_player(w,q))enemy_schedule(q,GUARD_RETREAT);
 int behind;if(enemy_near_death(w,&q->actor,&behind))enemy_schedule(q,ESCAPE_GUARD);return !w->fault;
}
static void trajectory(struct WeaponWorld*w,struct EnemyActor*q){
 struct WeaponActor*a=&q->actor;uint8_t low=a->transform.facing&1?17:4,high=a->transform.facing&1?18:5,down=a->transform.facing&1?19:16;
 if((uint16_t)word(transform_y_up(&a->transform)+w->camera_y)<384&&q->infantry.target>=0){
  int16_t dy=word(transform_y_up(&w->players[(unsigned)q->infantry.target]->actor.transform)-transform_y_up(&a->transform));
  if(dy>96)low=high;else if(dy<-64)low=down;
 }else if((uint16_t)word(transform_y_up(&a->transform)+w->camera_y)>=384)low=down;
 a->variant=low;
}
/* Escape entries share the complete $056B92 decision helper. */
static int escape_think(struct WeaponWorld*w,struct NativeEntity*e){return soldier_think(w,e);}
static int initialize(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 unsigned kind=q->infantry.kind;
 if(kind==ESCAPING){a->palette=14;interaction_priority(w,a,0x8000,24);if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;enemy_schedule(q,ESCAPE_INIT);return 2;}
 if(kind==SKIRMISHER)a->transform.facing|=1;
 if(!ground_test(w,q))return 0;a->palette=14;
 if(kind==SKIRMISHER||kind==SKIRMISHER_PRESERVE){enemy_schedule(q,SKIRM_RUN);return 1;}
 interaction_priority(w,a,0x8000,24);if(!pow_spawn_surface(w,entity_ref(e),&a->transform))return 0;
 if(kind==CHARGER){
  q->infantry.origin=word(a->transform.x>>8);q->infantry.radius=q->params[0]<<3;q->infantry.target=-1;q->infantry.angry=0;
  unsigned n=(unsigned)rank(w,q,0)*q->params[2]>>8;q->infantry.cooldown=(uint8_t)((uint16_t)n>=256?255:n);
  n=(unsigned)rank(w,q,1)*q->params[3]>>8;q->params[3]=(uint8_t)((uint16_t)n>=256?255:n);
  if(!enemy_ground_status(w,q,0))return 0;if(sx(w,a)<160)a->transform.facing|=1;else a->transform.facing&=254;
  enemy_schedule(q,!q->infantry.radius?IDLE:!q->params[5]?WALK_FAST:q->params[5]==1?WALK_B:WALK_A);return 2;
 }
 q->infantry.engaged=0;q->infantry.target=-2;q->infantry.attack_choice=6;
 q->infantry.cooldown=q->params[1]?(uint16_t)((unsigned)rank(w,q,0)*q->params[1]>>8):65535;
 unsigned n=(unsigned)rank(w,q,1)*q->params[2]>>8;q->params[2]=(uint8_t)((uint16_t)n>=256?255:n);
 q->infantry.lifetime=q->params[3]?(uint16_t)(q->params[3]<<6):65535;enemy_schedule(q,GUARD_BASE);return 1;
}
/* Returns 2 when the constructor only schedules next tick, without running it. */
static int enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 for(unsigned budget=0;budget<12;budget++){
  unsigned state=a->state;q->entry=0;int r;
  switch(state){
  case NEW:r=initialize(w,e);if(r!=1)return r;continue;
  case GUARD_BASE:enemy_start(q,q->infantry.kind==GUARD?EAN_GUARD:EAN_GUARD_FIXED);break;
  case GUARD_IDLE:enemy_start(q,EAN_RAISE);break;
  case GUARD_RECOVER:enemy_start(q,EAN_GUARD_RECOVER);break;
  case GUARD_ATTACK:
   a->variant=q->infantry.attack_choice;enemy_start(q,q->infantry.kind==GUARD?EAN_GUARD_ATTACK:q->infantry.attack_choice==7?EAN_GUARD_THROW_A:EAN_GUARD_THROW_B);break;
  case GUARD_RETREAT:enemy_start(q,EAN_GUARD_RETREAT);break;
  case WALK_A:case WALK_B:case WALK_FAST:
   if(state==WALK_B)q->infantry.angry=255;enemy_start(q,state==WALK_A?EAN_WALK_A:state==WALK_B?EAN_WALK_B:EAN_RUN);
   a->vx=(int16_t)(state==WALK_FAST?rank(w,q,2):256);if(!(a->transform.facing&1))a->vx=word(-a->vx);break;
  case IDLE:enemy_start(q,q->infantry.angry?EAN_IDLE_ARMED:EAN_IDLE);hit_begin_attack(&a->hit);a->vx=0;break;
  case MELEE:case SKIRM_MELEE:
   a->vx=0;enemy_start(q,EAN_MELEE);enemy_attack_binding(q,ENB_MELEE);hit_begin_attack(&a->hit);hit_begin_attack(&a->hit);break;
  case ATTACK:
   if(!q->params[1]){enemy_schedule(q,EXIT);continue;}
   q->infantry.continuation=ATTACK_ANIM;
   if(!q->infantry.angry&&q->params[4]>=4){enemy_schedule(q,RAISE);continue;}
   if(q->infantry.angry&&q->params[4]<4){enemy_schedule(q,LOWER);continue;}
   enemy_schedule(q,ATTACK_ANIM);continue;
  case ATTACK_ANIM:
   switch(q->params[4]){
   case 0:enemy_start(q,EAN_THROW);a->variant=5;break;
   case 1:q->infantry.angry=255;enemy_start(q,EAN_BOUNCE_THROW);break;
   case 2:enemy_start(q,EAN_MORTAR_THROW);break;
   case 3:enemy_start(q,EAN_ROLLER_THROW);break;
   default:enemy_start(q,EAN_GUARD_ATTACK);break;
   }trajectory(w,q);a->vx=0;break;
  case EXIT:
   if(q->infantry.angry){q->infantry.continuation=ESCAPE_CHARGER;enemy_schedule(q,LOWER);continue;}
   enemy_schedule(q,ESCAPE_CHARGER);continue;
  case LOWER:case RAISE:enemy_start(q,state==LOWER?EAN_LOWER:EAN_RAISE);a->vx=0;break;
  case SKIRM_RUN:
   a->vx=(int16_t)rank(w,q,3);if(!(a->transform.facing&1))a->vx=word(-a->vx);enemy_start(q,EAN_RUN);hit_begin_attack(&a->hit);break;
  case ESCAPE_INIT:case ESCAPE_FACE:case ESCAPE_FACE_CLEAR:case ESCAPE_GUARD:case ESCAPE_CHARGER:
   a->display_flags|=2;q->params[0]=1;if(state==ESCAPE_CHARGER||state==ESCAPE_FACE_CLEAR)q->infantry.flags&=239;else q->infantry.flags|=16;
   if(!ground_test(w,q))return 0;
   if(state==ESCAPE_INIT){if((int8_t)((uint8_t)(sx(w,a)>>8)-160)<0)a->transform.facing|=1;else a->transform.facing&=254;enemy_schedule(q,ESCAPE_RUN);continue;}
   if(state==ESCAPE_FACE||state==ESCAPE_FACE_CLEAR){if(!interaction_sound(w,65535))return 0;if(sx(w,a)<160)a->transform.facing&=254;else a->transform.facing|=1;enemy_schedule(q,ESCAPE_RUN);continue;}
   enemy_schedule(q,state==ESCAPE_GUARD?ESCAPE_STAND:ESCAPE_BRAKE);continue;
  case ESCAPE_RUN:
   enemy_start(q,(q->infantry.flags&16)?EAN_BURN_RUN:EAN_RUN);q->scale=(uint16_t)rank(w,q,2);
   a->vx=a->transform.facing&1?(int16_t)q->scale:word(-(int16_t)q->scale);a->vy=0;break;
  case ESCAPE_WAIT:
   if(q->infantry.flags&1){enemy_schedule(q,ESCAPE_BRAKE);continue;}a->ttl=180;enemy_start(q,EAN_EXIT_WAIT);break;
  case ESCAPE_STAND:enemy_start(q,EAN_EXIT_STAND);a->vx=0;break;
  case ESCAPE_STAND_LOWER:enemy_start(q,EAN_LOWER);break;
  case ESCAPE_BRAKE:enemy_start(q,EAN_EXIT_BRAKE);break;
  case ESCAPE_LEAP:enemy_start(q,EAN_EXIT_LEAP);a->vx=a->transform.facing&1?491:-491;a->vy=0x7aa;a->ay=-157;a->ax=0;break;
  case ESCAPE_FALL:enemy_start(q,EAN_EXIT_FALL);a->vy=0;break;
  case ESCAPE_LAND:{
   uint16_t x=a->vx<0?(uint16_t)word(-a->vx):(uint16_t)a->vx,y=a->vy<0?(uint16_t)word(-a->vy):(uint16_t)a->vy;
   enemy_start(q,x>=y?EAN_EXIT_LAND_HORIZONTAL:EAN_EXIT_LAND_VERTICAL);a->vx=0;break;
  }
  default:return enemy_fault(w);
  }return w->fault?0:1;
 }return enemy_fault(w);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(q->entry&&enter(w,e)!=1)return;unsigned state=a->state;int done,r;
 switch(state){
 case GUARD_BASE:
  guard_timers(w,q);if(!physics(w,q,1))return;enemy_animate(w,e);
  if(q->infantry.kind==GUARD){if(!guard_think(w,q))return;if(!q->grounded)enemy_schedule(q,GUARD_IDLE);}
  else q->infantry.attack_choice=face_target(w,q)?8:7;
  guard_gate(w,q);if(!q->infantry.lifetime)enemy_schedule(q,q->infantry.kind==GUARD?ESCAPE_GUARD:ESCAPE_FACE);break;
 case GUARD_IDLE:
  if(!physics(w,q,1))return;done=enemy_animate(w,e);if(done&&q->grounded)enemy_schedule(q,GUARD_RECOVER);break;
 case GUARD_RECOVER:
  if(!physics(w,q,1))return;if(enemy_animate(w,e))enemy_schedule(q,GUARD_BASE);break;
 case GUARD_ATTACK:
  guard_timers(w,q);if(!physics(w,q,1))return;if(enemy_animate(w,e))enemy_schedule(q,GUARD_BASE);
  if(q->infantry.kind==GUARD&&close_player(w,q))enemy_schedule(q,GUARD_RETREAT);break;
 case GUARD_RETREAT:
  if(!physics(w,q,1))return;enemy_animate(w,e);if(!close_player(w,q))enemy_schedule(q,GUARD_BASE);break;
 case WALK_A:case WALK_B:case WALK_FAST:{
  if(!physics(w,q,0))return;enemy_animate(w,e);int16_t d=word((a->transform.x>>8)-q->infantry.origin);if(d<0)d=word(-d);
  if((uint16_t)d>=q->infantry.radius)enemy_schedule(q,IDLE);if(melee_probe(w,q))enemy_schedule(q,MELEE);break;
 }
 case IDLE:
  if(face_target(w,q))a->transform.facing^=1;if(!physics(w,q,0))return;enemy_animate(w,e);
  if(q->infantry.cooldown)q->infantry.cooldown=(uint8_t)(q->infantry.cooldown-1);else if(q->infantry.target>=0)enemy_schedule(q,ATTACK);
  if(!q->infantry.angry&&melee_probe(w,q))enemy_schedule(q,MELEE);break;
 case MELEE:case SKIRM_MELEE:
  if(state==SKIRM_MELEE&&interaction_move(w,a,WMOVE_SYNC)<0)return;if(!physics(w,q,0))return;
  if(enemy_animate(w,e))enemy_schedule(q,state==MELEE?IDLE:SKIRM_RUN);break;
 case ATTACK_ANIM:
  if(q->infantry.cooldown)q->infantry.cooldown=(uint8_t)(q->infantry.cooldown-1);
  if(!physics(w,q,0))return;if(enemy_animate(w,e)){q->infantry.cooldown=q->params[3];enemy_schedule(q,--q->params[1]?IDLE:EXIT);}break;
 case LOWER:case RAISE:
  if(!physics(w,q,0))return;if(enemy_animate(w,e)){enemy_schedule(q,q->infantry.continuation);q->infantry.angry=state==LOWER?0:255;}break;
 case SKIRM_RUN:
  if(interaction_move(w,a,WMOVE_SYNC)<0||!physics(w,q,0))return;if(melee_probe(w,q))enemy_schedule(q,SKIRM_MELEE);enemy_animate(w,e);break;
 case ESCAPE_RUN:
  if(!physics(w,q,0)||!escape_think(w,e)||!e->active||e->deleting)return;enemy_animate(w,e);
  if(q->infantry.flags&1){uint16_t r=random_word(w);if((r&255)<q->infantry.probability&&(r>>8)<q->infantry.probability)enemy_schedule(q,ESCAPE_BRAKE);}
  else if(q->infantry.flags&64)enemy_schedule(q,ESCAPE_LEAP);
  if(q->infantry.flags&32)enemy_schedule(q,ESCAPE_WAIT);if(!q->grounded)enemy_schedule(q,ESCAPE_LEAP);break;
 case ESCAPE_WAIT:
  if(!physics(w,q,0)||!escape_think(w,e)||!e->active||e->deleting)return;enemy_animate(w,e);
  if((q->infantry.flags&1)&&!q->infantry.missing)enemy_schedule(q,ESCAPE_BRAKE);
  if(!(q->infantry.flags&32))enemy_schedule(q,ESCAPE_RUN);if(!q->grounded)enemy_schedule(q,ESCAPE_FALL);
  if(!--a->ttl)enemy_schedule(q,ESCAPE_BRAKE);break;
 case ESCAPE_STAND:case ESCAPE_STAND_LOWER:
  if(!physics(w,q,0))return;if(enemy_animate(w,e)){enemy_schedule(q,ESCAPE_BRAKE);enemy_publish_death(w,a);}
  if(!q->grounded)enemy_schedule(q,ESCAPE_FALL);break;
 case ESCAPE_BRAKE:
  if(!physics(w,q,0))return;if(enemy_animate(w,e))enemy_schedule(q,ESCAPE_RUN);break;
 case ESCAPE_LEAP:
  r=enemy_swapped_move(w,q,WMOVE_BODY);if(r<0)return;q->grounded=r?255:0;if(r)enemy_schedule(q,ESCAPE_LAND);enemy_animate(w,e);break;
 case ESCAPE_FALL:
  a->vx=word(a->vx-(a->vx>>6));if(!physics(w,q,0))return;if(q->grounded)enemy_schedule(q,ESCAPE_LAND);enemy_animate(w,e);break;
 case ESCAPE_LAND:
  if(!physics(w,q,0))return;if(enemy_animate(w,e))enemy_schedule(q,ESCAPE_RUN);if(!q->grounded)enemy_schedule(q,ESCAPE_FALL);break;
 default:enemy_fault(w);return;
 }
 if(w->fault)return;
 if(!human_death_hit_tail(w,e))return;
 if(interaction_outside(w,a,infantry_bounds[state>=ESCAPE_RUN?1:0],1))entity_delete(p,e);
}
int infantry_spawn_template(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8],uint8_t flags){
 unsigned kind;if(id==27)kind=GUARD;else if(id==28)kind=GUARD_FIXED;else if(id==249)kind=CHARGER;else if(id==282)kind=SKIRMISHER;else if(id==281)kind=SKIRMISHER_PRESERVE;else if(!id)kind=ESCAPING;else return 0;
 struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;struct EnemyActor*q=&e->payload.enemy;
 q->infantry.kind=(uint8_t)kind;q->source_flags=flags;q->actor.transform.display_group=flags;if(params)for(unsigned i=0;i<8;i++)q->params[i]=params[i];return 1;
}
void infantry_escape(struct WeaponWorld*w,struct NativeEntity*e,enum InfantryExit kind){
 struct EnemyActor*q=&e->payload.enemy;e->tick=tick;
 if(kind==INFANTRY_EXIT_FACE){
  /* $059062 starts below the $058F82 forced-escape initializer. Preserve
   * the ordinary soldier's behavior parameters, flags and ground state. */
  if(!interaction_sound(w,65535))return;
  if(sx(w,&q->actor)<160)q->actor.transform.facing&=254;else q->actor.transform.facing|=1;
  enemy_schedule(q,ESCAPE_RUN);
 }else enemy_schedule(q,ESCAPE_STAND_LOWER);
 tick(w->entities,e,w);
}
void infantry_force_escape(struct NativeEntity*e,uint8_t flags){
 /* $058F82: a turret gunner becomes the existing full escape controller.
  * Think rewrites target weights; retain the high source behavior bits. */
 struct EnemyActor*q=&e->payload.enemy;uint8_t*b=(uint8_t*)&q->infantry;
 for(unsigned i=0;i<sizeof(q->infantry);i++)b[i]=255;
 q->infantry.flags=flags;q->infantry.target=-2;e->tick=tick;enemy_schedule(q,ESCAPE_FACE);
}
struct NativeEntity*infantry_spawn_forced_escape(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,const struct PlayerTransform*t,const uint8_t params[8]){
 /* $058F58 differs from $058F82 only by its palette assignment. */
 struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;
 if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];
 e->payload.enemy.actor.palette=14;infantry_force_escape(e,255);return e;
}

/* $058FE2 uses the shared escape initializer with the burning bit cleared. */
void infantry_free_escape(struct NativeEntity*e,uint8_t flags){infantry_force_escape(e,flags);enemy_schedule(&e->payload.enemy,ESCAPE_FACE_CLEAR);}

/* $058FC2 preserves facing and enters the standing escape animation. */
void infantry_stand_escape(struct NativeEntity*e,uint8_t flags){infantry_force_escape(e,flags);enemy_schedule(&e->payload.enemy,ESCAPE_GUARD);}
