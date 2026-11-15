#include "bosses.h"
/* $07CF02..$07DFA8: templates 138/139/140, two claws, three legs,
 * detached attack chains and both fragment entries. The source's final
 * transition in each frame wins, including detach after a parent-death test. */
enum State {ROOT,SPAWN,IDLE,CHARGE,PAUSE,RETREAT,DEATH,EXPLODE,KNOCKBACK,
 CLAW0,CLAW1,CLAW_FOLLOW,CLAW_OPEN,CLAW_CLOSE,LEG0,LEG1,LEG2,LEG_FOLLOW,
 LEG_STEP,DETACH,DETACHED,SHARD_BIG,SHARD_SMALL,PATROL_ROOT,PATROL_WALK,
 PATROL_PAUSE,PATROL_BACK,MARK,DELETE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static int16_t word(int n){return (int16_t)(uint16_t)n;}
static int16_t absolute(int16_t n){return n<0?word(-n):n;}
static int16_t signed_x(struct WeaponActor*a,int16_t n){return a->transform.facing&1?word(-n):n;}
static uint16_t random_word(struct WeaponWorld*w){if(!w->rng){enemy_fault(w);return 0;}return source_random_word(w->rng);}
static void reset(struct WeaponActor*a){a->vx=a->vy=a->ax=a->ay=0;transform_integer_copy(&a->transform,&a->transform);}
static int animate(struct WeaponWorld*w,struct NativeEntity*e){return boss_animate(w,e,0);}
static int stop(struct WeaponWorld*w){return w->sound&&w->sound(w->scene,0x109e,1)?1:enemy_fault(w);}
static struct EnemyActor*parent(struct WeaponWorld*w,struct NativeEntity*e){
 struct NativeEntity*p=entity_get(w->entities,e->parent);if(!p||p->kind!=ENTITY_ENEMY){enemy_fault(w);return 0;}return &p->payload.enemy;
}
static struct EnemyActor*grandparent(struct WeaponWorld*w,struct NativeEntity*e){
 struct NativeEntity*p=entity_get(w->entities,e->parent);if(!p){enemy_fault(w);return 0;}return parent(w,p);
}
static struct NativeEntity*child(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct NativeEntity*n=enemy_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,tick);
 if(n){n->payload.enemy.crab=(__typeof__(n->payload.enemy.crab)){0};enemy_schedule(&n->payload.enemy,state);}return n;
}
static int16_t rank(struct WeaponWorld*w,struct EnemyActor*q,unsigned index){
 if(!w->interactions->rank_valid){enemy_fault(w);return 0;}
 return (int16_t)crab_rank[index][interaction_rank(w,&q->actor,runtime_owner_crab[index])][q->source_flags&3][(w->interactions->player_count_flag&2)>>1];
}
static int16_t player_delta(struct WeaponWorld*w,struct EnemyActor*q){
 int slot=enemy_nearest_x(w,&q->actor,0);if(slot<0)slot=1;
 int16_t x=w->players[slot]?word((w->players[slot]->actor.transform.x>>8)-w->camera_x):-1;
 return word(word((q->actor.transform.x>>8)-w->camera_x)-x);
}
static int near(struct WeaponWorld*w,struct EnemyActor*q){
 if(q->actor.phase==1)return 0;q->crab.travel=0;
 if((uint16_t)absolute(player_delta(w,q))>(uint16_t)q->crab.radius)return 0;
 q->actor.phase=1;return 1;
}
static int hit(struct WeaponWorld*w,struct WeaponActor*a){
 if(!(a->hit.flags&8))return 1;
 if(!(a->hit.flags&1)&&(a->hit.events&1)&&a->hit.last_kind==1&&!interaction_sound(w,0x108d))return 0;
 a->hit.flags&=247;return 1;
}
static int foot(struct WeaponWorld*w,struct WeaponActor*a){
 int r=terrain_weapon_foot_contact(w->interactions->terrain,a);if(r<0)enemy_fault(w);return r;
}
static void tail(struct WeaponWorld*w,struct EnemyActor*q,int hit_first){
 if(hit_first&&!hit(w,&q->actor))return;
 int r=foot(w,&q->actor);if(r<0)return;if(!r)enemy_schedule(q,KNOCKBACK);
 if(q->actor.hit.flags&1)enemy_schedule(q,DEATH);
 if(interaction_outside(w,&q->actor,crab_bounds[0],0))enemy_schedule(q,MARK);
}
static int settle(struct WeaponWorld*w,struct WeaponActor*a){
 /* $05E7C0 operates on the SCREEN word, then invokes item gravity until
  * contact or negative Y. Preserve its odd off-map fallback verbatim. */
 int16_t y=word(transform_y_up(&a->transform)+w->camera_y);
 transform_integer_offset(&a->transform,0,word((y&1023)-y));
 for(unsigned i=0;i<2048;i++){
  if(word(transform_y_up(&a->transform)+w->camera_y)<0){
   transform_integer_offset(&a->transform,word(160+w->camera_x-(a->transform.x>>8)),word(384-w->camera_y-transform_y_up(&a->transform)));return 1;
  }
  int r=interaction_move(w,a,WMOVE_ITEM);if(r<0)return 0;if(r)return 1;
 }return enemy_fault(w);
}
static int spawn_claws(struct WeaponWorld*w,struct NativeEntity*e){
 struct WeaponActor*a=&e->payload.enemy.actor;if(!settle(w,a))return 0;
 interaction_priority(w,a,0x4001,12);return child(w,e,CLAW0)&&child(w,e,CLAW1);
}
static int surface(struct WeaponWorld*w,struct NativeEntity*e,int big){
 struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,big?BFX_SURFACE40:BFX_SURFACE20);
 if(!n)return 0;n->payload.enemy.actor.transform.priority=65535;return 1;
}
static int broken(uint8_t phase){return phase==255||phase==128||phase==64;}
static int follow_claw(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy,*p=parent(w,e);if(!p)return 0;
 struct PlayerTransform*t=&q->actor.transform,*s=&p->actor.transform;
 transform_integer_offset(t,word((s->x>>8)-(t->x>>8)),word(transform_y_up(s)+p->crab.offset_y-transform_y_up(t)));
 t->priority=(uint16_t)(s->priority+q->crab.priority);
 if(p->actor.hit.events&128)q->actor.hit.events|=1;return 1;
}
static int follow_leg(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy,*p=parent(w,e),*g=grandparent(w,e);if(!p||!g)return 0;
 struct PlayerTransform*t=&q->actor.transform,*s=&p->actor.transform;
 transform_integer_offset(t,word((s->x>>8)+q->crab.offset_x-(t->x>>8)),
  word(transform_y_up(s)+q->crab.offset_y+(q->selector==2?g->crab.offset_y:0)-transform_y_up(t)));
 t->priority=(uint16_t)((s->priority&0xffe3)|8);return 1;
}
/* 2 means the original entry returned after changing task or controller. */
static int enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned state=a->state;q->entry=0;
 switch(state){
 case ROOT:case PATROL_ROOT:
  a->palette=0x4b;a->hit_palette=0x7e;a->phase=q->dead=0;a->hit.hp=rank(w,q,0);reset(a);
  if(state==ROOT){q->crab.no_surface=q->params[4];a->transform.facing=q->params[2];enemy_schedule(q,SPAWN);}
  else{q->crab.radius=rank(w,q,1);q->crab.max_speed=(int16_t)(q->params[0]<<4);q->crab.min_speed=word(-q->crab.max_speed);
   a->transform.facing=word((a->transform.x>>8)-w->camera_x)>=160;enemy_schedule(q,PATROL_WALK);}return w->fault?0:2;
 case SPAWN:if(!spawn_claws(w,e))return 0;a->state=IDLE;/* FALLTHROUGH */
 case IDLE:q->crab.blocked=0;q->crab.offset_y=0;boss_start(q,BA_CRAB_IDLE);return 1;
 case CHARGE:case RETREAT:case PATROL_BACK:
  reset(a);a->ax=signed_x(a,state==RETREAT?32:-32);q->crab.offset_y=0;
  if(state==CHARGE)q->crab.blocked=0;boss_start(q,state==RETREAT?BA_CRAB_BACK:BA_CRAB_FORWARD);return 1;
 case PAUSE:case PATROL_PAUSE:
  reset(a);q->crab.offset_y=0;if(!stop(w))return 0;boss_start(q,BA_CRAB_IDLE);return 1;
 case DEATH:
  if(!boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMOKE))return 0;
  a->phase=255;a->display_flags&=253;boss_start(q,BA_CRAB_DIE);return stop(w);
 case EXPLODE:
  interaction_priority(w,a,0xc000,20);
  if(!boss_fx_pattern(w,entity_ref(e),&a->transform,BP_CRAB_DEATH)||!interaction_sound(w,0x1033))return 0;
  return boss_fx_transform(w,e,BFX_FIRE)?2:0;
 case KNOCKBACK:
  q->dead=0;a->phase=128;a->display_flags&=253;a->vx>>=1;a->ax=0;a->ay=-16;
  interaction_priority(w,a,0xc000,28);boss_start(q,BA_CRAB_FALL);return stop(w);
 case CLAW0:case CLAW1:
  a->palette=0x4b;a->hit_palette=0x7e;reset(a);a->phase=q->dead=0;q->crab.offset_x=(int16_t)(state==CLAW1);q->crab.priority=state==CLAW0?1:-1;
  if(state==CLAW0&&(!child(w,e,LEG0)||!child(w,e,LEG1)||!child(w,e,LEG2)))return 0;
  boss_start(q,state==CLAW0?BA_CRAB_CLAW0:BA_CRAB_CLAW1);a->state=CLAW_FOLLOW;return 1;
 case CLAW_OPEN:case CLAW_CLOSE:
  if((unsigned)q->crab.offset_x>=2)return enemy_fault(w);
  boss_start_entry(q,(state==CLAW_OPEN?boss_crab_open:boss_crab_close)[q->crab.offset_x]);return 1;
 case LEG0:case LEG1:case LEG2:
  q->selector=(uint16_t)(state-LEG0);q->crab.offset_y=(int16_t)(38+9*q->selector);
  a->palette=0x4c;a->phase=q->dead=0;a->hit.type_flags|=16;a->hit.hp=1;reset(a);
  q->crab.offset_x=(random_word(w)&7)-4;boss_start(q,BA_CRAB_LEG);a->state=LEG_FOLLOW;return !w->fault;
 case LEG_STEP:reset(a);q->crab.speed=0;boss_start(q,BA_CRAB_STEP);return 1;
 case DETACH:
  a->vx=word((random_word(w)&127)+(a->vx>>5));a->vy>>=4;a->ax=0;a->ay=-32;
  q->crab.delays=(random_word(w)&3)+3;interaction_priority(w,a,0xd000,20);
  if(q->selector==2&&!interaction_sound(w,0x1061))return 0;
  q->selector=0;q->crab.return_travel=-1;boss_start(q,BA_CRAB_DETACH);a->state=DETACHED;return !w->fault;
 case SHARD_BIG:case SHARD_SMALL:
  a->vx=(int16_t)(random_word(w)&1023);a->vy=word((state==SHARD_BIG?400:280)+(random_word(w)&(state==SHARD_BIG?1023:511)));
  a->transform.facing^=random_word(w)&1;a->vx=signed_x(a,a->vx);a->ax=0;a->ay=state==SHARD_BIG?-16:-8;
  q->crab.delays=random_word(w)&3;interaction_priority(w,a,0xd000,20);
  q->selector=0;q->crab.return_travel=0;a->hit.flags|=8;boss_start(q,BA_CRAB_SHARD);a->state=DETACHED;return !w->fault;
 case PATROL_WALK:
  if(!spawn_claws(w,e))return 0;a->ax=signed_x(a,32);a->vx=0;q->crab.blocked=0;q->crab.offset_y=q->crab.travel=0;
  q->crab.no_surface=q->params[1];boss_start(q,BA_CRAB_BACK);return 1;
 case MARK:a->phase=64;/* FALLTHROUGH */
 case DELETE:entity_delete(w->entities,e);return 2;
 default:return enemy_fault(w);
 }
}
static void leg_step(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy,*p=parent(w,e),*g=grandparent(w,e);if(!p||!g||!follow_leg(w,e))return;
 unsigned i=q->selector,j=p->actor.phase;if(i>=3||j>=9){enemy_fault(w);return;}
 struct WeaponActor*a=&q->actor;int16_t dx=signed_x(a,crab_offsets[i][j][0]);
 transform_integer_offset(&a->transform,dx,crab_offsets[i][j][1]);q->crab.travel=word(a->transform.x>>8);
 q->crab.speed=word(q->crab.speed+crab_accel[i][j]);int angle=crab_angles[j];if(a->transform.facing&1)angle=-angle-128;
 weapon_vector((uint8_t)angle,q->crab.speed,&a->vx,&a->vy);weapon_integrate_free(a);
 if(animate(w,e))enemy_schedule(q,DETACH);if(broken(g->actor.phase))enemy_schedule(q,DELETE);
 if(absolute(word((a->transform.x>>8)-q->crab.travel))>=28)enemy_schedule(q,DETACH);
}
static void detached(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(interaction_move(w,a,WMOVE_POINT)<0)return;animate(w,e);
 if(q->crab.return_travel){
  if(a->hit.flags&8){
   if(!hit(w,a))return;
   if(a->hit.last_kind==2||a->hit.last_kind==3){
    boss_start(q,BA_CRAB_SHARD);q->crab.delays=0;a->hit.flags|=8;
    if(a->hit.last_attacker>=ENTITY_CAPACITY){enemy_fault(w);return;}
    struct NativeEntity*s=&w->entities->nodes[a->hit.last_attacker];struct WeaponActor*v=entity_weapon_actor(s);
    if(!v){enemy_fault(w);return;}a->vx=word(a->vx+v->vx);q->crab.return_travel=0;
   }
  }
  if(q->crab.return_travel){
   if(q->selector>=8){enemy_fault(w);return;}
   for(unsigned i=0;i<7&&crab_attacks[q->selector][i]!=65535;i++){
    boss_attack_binding(q,crab_attacks[q->selector][i]);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return;if(a->hit.flags&2)break;
   }
  }
 }
 if(word(transform_y_up(&a->transform)+w->camera_y)<=288&&!q->dead){
  q->dead=255;struct EnemyActor*g=grandparent(w,e);if(!g)return;
  if(g->crab.no_surface){if(interaction_outside(w,a,crab_bounds[1],0))enemy_schedule(q,MARK);return;}
  if(!surface(w,e,0))return;
 }
 if(interaction_outside(w,a,crab_bounds[1],1))enemy_schedule(q,DELETE);
}
static void tick(struct EntityPool*pool,struct NativeEntity*e,void*world){
 (void)pool;struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(q->entry&&enter(w,e)!=1)return;if(w->fault||e->deleting)return;unsigned state=a->state;
 switch(state){
 case IDLE:{
  if(interaction_move(w,a,WMOVE_SYNC)<0)return;animate(w,e);int16_t threshold=word((q->params[0]<<8)|q->params[1]);
  if((q->selector||w->secondary_x_offset>=threshold)&&player_delta(w,q)<=threshold){a->phase=1;q->crab.travel=0;enemy_schedule(q,CHARGE);}tail(w,q,1);return;
 }
 case CHARGE:case RETREAT:{
  int r=interaction_move(w,a,WMOVE_FLAT);if(r<0)return;
  if(r){if(state==CHARGE)q->crab.blocked=255;else{a->phase=4;enemy_schedule(q,PAUSE);}}
  if(a->vx<-1024||a->vx>=1024){a->ax=0;a->vx=signed_x(a,state==CHARGE?-1024:1024);}animate(w,e);
  if(state==CHARGE){
   if(!q->crab.blocked)q->crab.travel=word(q->crab.travel+a->vx);
   if(q->crab.blocked||absolute(q->crab.travel)>=0x6000){a->phase=2;q->crab.blocked=0;q->crab.return_travel=absolute(q->crab.travel);q->crab.pause=40;enemy_schedule(q,PAUSE);}
  }else{q->crab.travel=word(q->crab.travel+a->vx);if(absolute(q->crab.travel)>=q->crab.return_travel){a->phase=4;enemy_schedule(q,PAUSE);}}
  tail(w,q,1);return;
 }
 case PAUSE:
  if(interaction_move(w,a,WMOVE_SYNC)<0)return;animate(w,e);
  if(q->params[3]&&a->phase!=4&&!--q->crab.pause){a->phase=3;q->crab.travel=0;enemy_schedule(q,RETREAT);}tail(w,q,1);return;
 case DEATH:
  if(interaction_move(w,a,WMOVE_SYNC)<0)return;if(animate(w,e))enemy_schedule(q,EXPLODE);
  {int r=foot(w,a);if(r<0)return;if(!r)enemy_schedule(q,EXPLODE);}
  if(interaction_outside(w,a,crab_bounds[0],0))enemy_schedule(q,MARK);return;
 case KNOCKBACK:
  a->hit.flags|=64;if(interaction_move(w,a,WMOVE_POINT)<0)return;a->hit.flags&=191;animate(w,e);
  if(word(transform_y_up(&a->transform)+w->camera_y)<=288&&!q->dead){q->dead=255;if(!q->crab.no_surface&&!surface(w,e,1))return;}
  if(interaction_outside(w,a,crab_bounds[1],0))enemy_schedule(q,MARK);return;
 case CLAW_FOLLOW:case CLAW_OPEN:case CLAW_CLOSE:{
  if(!follow_claw(w,e))return;int done=animate(w,e);struct EnemyActor*p=parent(w,e);if(!p)return;
  if(state==CLAW_FOLLOW&&p->actor.phase==1)enemy_schedule(q,CLAW_OPEN);
  if(state==CLAW_OPEN&&done)enemy_schedule(q,CLAW_CLOSE);if(broken(p->actor.phase))enemy_schedule(q,DELETE);return;
 }
 case LEG_FOLLOW:{
  if(!follow_leg(w,e))return;animate(w,e);struct EnemyActor*g=grandparent(w,e);if(!g)return;
  if(g->actor.phase==1)enemy_schedule(q,LEG_STEP);if(broken(g->actor.phase))enemy_schedule(q,DELETE);return;
 }
 case LEG_STEP:leg_step(w,e);return;
 case DETACHED:detached(w,e);return;
 case PATROL_WALK:case PATROL_BACK:{
  int r=interaction_move(w,a,WMOVE_FLAT);if(r<0)return;if(r){if(state==PATROL_BACK)q->dead=255;enemy_schedule(q,PATROL_PAUSE);}
  if(a->vx>q->crab.max_speed||a->vx<q->crab.min_speed){a->ax=0;a->vx=signed_x(a,state==PATROL_BACK?q->crab.min_speed:q->crab.max_speed);}
  animate(w,e);if(!hit(w,a))return;if(state==PATROL_WALK&&near(w,q))enemy_schedule(q,PATROL_PAUSE);tail(w,q,0);return;
 }
 case PATROL_PAUSE:
  if(interaction_move(w,a,WMOVE_SYNC)<0)return;animate(w,e);if(!hit(w,a))return;
  if(q->dead!=255){q->crab.travel=word(q->crab.travel+1);if(q->crab.travel>=60)enemy_schedule(q,PATROL_BACK);near(w,q);}tail(w,q,0);return;
 default:enemy_fault(w);return;
 }
}
int crab_spawn(struct WeaponWorld*w,struct EntityRef parent_ref,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){
 if(id<138||id>140)return enemy_fault(w);struct NativeEntity*e=enemy_allocate(w,parent_ref,order,t,tick);if(!e)return 0;
 struct EnemyActor*q=&e->payload.enemy;q->crab=(__typeof__(q->crab)){0};q->selector=(uint16_t)(id==139);
 if(params)for(unsigned i=0;i<8;i++)q->params[i]=params[i];enemy_schedule(q,id==140?PATROL_ROOT:ROOT);return 1;
}
struct NativeEntity*crab_shard(struct WeaponWorld*w,struct EntityRef parent_ref,enum EntityInsert order,const struct PlayerTransform*t,int small){
 struct NativeEntity*e=enemy_allocate(w,parent_ref,order,t,tick);if(!e)return 0;
 e->payload.enemy.crab=(__typeof__(e->payload.enemy.crab)){0};enemy_schedule(&e->payload.enemy,small?SHARD_SMALL:SHARD_BIG);return e;
}
