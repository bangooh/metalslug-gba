#include "carrier_internal.h"
/* $07EB76..$07FC18, $079B6E/$079D9A. Slot ownership, death counts and
 * gunner replacement clocks are explicit, including source byte wrapping. */
int cr_arc(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(!cr_follow(w,e))return 0;
 int16_t dx=q->carrier_crew.offset>>8;transform_integer_offset(&a->transform,dx,8);int index=dx+80;
 if(index<0){q->carrier_crew.offset=(int16_t)0xb800;index=0;}if(index>144){q->carrier_crew.offset=0x4800;index=144;}
 if(q->carrier_crew.lane!=1)transform_integer_offset(&a->transform,0,carrier_arc[(unsigned)index>>2]);return 1;
}
int cr_gunner_follow(struct WeaponWorld*w,struct NativeEntity*e,int offset){
 struct EnemyActor*q=&e->payload.enemy,*g=cr_grandparent(w,e);if(!g)return 0;struct WeaponActor*a=&q->actor;
 cr_screen(w,a,g->carrier.cockpit_x-offset,g->carrier.cockpit_y);
 if(offset==8){q->carrier_crew.offset=cr_word(q->carrier_crew.offset+a->vx);transform_integer_offset(&a->transform,q->carrier_crew.offset>>8,0);
  if(q->carrier_crew.arc)transform_integer_offset(&a->transform,0,carrier_gunner_arc[(uint16_t)q->carrier_crew.offset>>9]);}
 return 1;
}
static void cooldown(struct WeaponWorld*w,struct NativeEntity*e,int late){
 struct EnemyActor*q=&e->payload.enemy,*p=cr_parent(w,e);if(!p)return;
 q->carrier_crew.cooldown=cr_rank(w,q,late||p->carrier_spawner.deaths>15?CRR_CREW_WAIT_LATE:CRR_CREW_WAIT);
}
static int leader_gone(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*p=cr_parent(w,e);if(!p)return 0;if(p->actor.phase==255)return human_corpse_transform(w,e,0);return 1;
}
static int depart(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy,*p=cr_parent(w,e);if(!p)return 0;q->carrier_crew.goal=cr_word(cr_sy(w,&p->actor)-12);
 if(q->carrier_crew.lane==1)return human_corpse_transform(w,e,0);cr_schedule(e,CR_RIDE);return 1;
}
static int troop_hit(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(!(a->hit.flags&8))return 1;
 struct EnemyActor*p=cr_parent(w,e);if(!p)return 0;p->carrier_spawner.busy&=(uint8_t)~(1u<<(q->carrier_crew.lane&7));p->carrier_spawner.deaths=cr_word(p->carrier_spawner.deaths+1);
 interaction_priority(w,a,0xc000,16);return human_death_hit_tail(w,e);
}
static int troop_tail(struct WeaponWorld*w,struct NativeEntity*e,int battle){
 struct EnemyActor*p=cr_parent(w,e);if(!p)return 0;
 if(battle){if(p->actor.phase==255&&!depart(w,e))return 0;if(!troop_hit(w,e))return 0;}
 else{if(!troop_hit(w,e)||!leader_gone(w,e))return 0;}
 cr_parent_tail(w,e,0);return !w->fault;
}
static int deferred(struct WeaponWorld*w,struct NativeEntity*e){
 if(!leader_gone(w,e))return 0;struct EnemyActor*q=&e->payload.enemy,*p=cr_parent(w,e);if(!p)return 0;
 if(q->dead){if(!(q->actor.hit.flags&8))return 1;p->dead&=(uint8_t)~(1u<<(q->carrier_crew.slot&7));if(!q->carrier_crew.slot)p->carrier_gunners.hidden=1;}
 return human_death_hit_tail(w,e);
}
static int gunner_tail(struct WeaponWorld*w,struct NativeEntity*e){if(!deferred(w,e))return 0;cr_parent_tail(w,e,0);return !w->fault;}
static int16_t player_x(struct WeaponWorld*w,struct WeaponActor*a){
 int i=enemy_nearest_x(w,a,0);struct PlayerBodyState*p=w->players[i<0?1:i];return p?cr_word((p->actor.transform.x>>8)-w->camera_x):-1;
}
static int rearm(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy,*p=cr_parent(w,e),*g=cr_grandparent(w,e);if(!p||!g)return 0;cooldown(w,e,0);cr_schedule(e,CR_IDLE);
 if(g->actor.hit.hp<=500&&(cr_random(w)&15)>=9){p->carrier_spawner.busy&=(uint8_t)~(1u<<(q->carrier_crew.lane&7));q->carrier_crew.lane=(cr_random(w)&1)*2;return depart(w,e);}
 if(q->carrier_crew.lane==1)cr_schedule(e,CR_WALK);return troop_tail(w,e,1);
}
static int decide(struct WeaponWorld*w,struct NativeEntity*e){
 struct WeaponActor*a=&e->payload.enemy.actor;int16_t x=player_x(w,a),own=cr_sx(w,a);int right=x>own;
 int16_t distance=right?cr_word(x-own):cr_word(own-x);if(distance<48)return rearm(w,e);
 cr_schedule(e,!!(a->transform.facing&1)==right?CR_FIRE:CR_AIM);return 1;
}
int cr_crew_enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy,*p=cr_parent(w,e);if(!p)return 0;struct WeaponActor*a=&q->actor;unsigned state=a->state;q->entry=0;struct NativeEntity*n;
 switch(state){
 case CR_SPAWNER:
  a->phase=0;if(q->carrier_spawner.guards){
   n=cr_child(w,e,CR_GUARD);if(!n)return 0;n->payload.enemy.carrier_crew.lane=0;n->payload.enemy.carrier_crew.offset=(int16_t)0xb800;
   n=cr_child(w,e,CR_GUARD);if(!n)return 0;n->payload.enemy.actor.transform.facing=1;n->payload.enemy.carrier_crew.lane=2;n->payload.enemy.carrier_crew.offset=0x4800;
  }
  q->dead=q->carrier_spawner.busy=0;q->carrier_spawner.clocks[0]=q->carrier_spawner.clocks[1]=q->carrier_spawner.clocks[2]=0;q->carrier_spawner.deaths=0;return 0;
 case CR_SPAWNER_WAIT:break;
 case CR_TROOPER:
  p->carrier_spawner.busy|=(uint8_t)(1u<<(q->carrier_crew.lane&7));q->carrier_crew.arc=0;a->palette=0x1d;a->variant=1;cooldown(w,e,0);
  a->phase=q->dead=0;a->hit.hp=1;cr_reset(a);cr_start(q,CRA_EMERGE);break;
 case CR_TROOPER_JUMP:a->palette=14;cr_start(q,CRA_JUMP);break;
 case CR_TROOPER_LAND:cr_start(q,CRA_TURN);break;
 case CR_RUN_SELECT:if(q->carrier_crew.lane){cr_schedule(e,CR_TROOPER_LAND);return 2;}a->state=CR_RUN;/* FALLTHROUGH */
 case CR_RUN:case CR_RUN_RIGHT:
  if(state==CR_RUN_RIGHT)a->transform.facing=1;a->palette=14;cr_reset(a);a->vx=a->transform.facing&1?512:-512;q->carrier_crew.offset=0;cr_start(q,CRA_RUN);break;
 case CR_STAND:case CR_STOP:cr_start(q,CRA_STAND);break;
 case CR_IDLE:cr_start(q,CRA_IDLE);break;
 case CR_AIM:cr_start(q,CRA_AIM);break;
 case CR_WALK_TURN:a->transform.facing^=1;a->state=CR_WALK;/* FALLTHROUGH */
 case CR_WALK:cr_reset(a);a->vx=a->transform.facing&1?512:-512;q->carrier_crew.arc=0;cr_start(q,CRA_RUN);break;
 case CR_TURN:cr_start(q,CRA_TURN);break;
 case CR_FIRE:case CR_GUNNER_THROW:a->variant=cr_random(w)&3;cr_start(q,CRA_FIRE);break;
 case CR_RIDER:
  a->transform.facing=cr_random(w)&1;q->carrier_crew.lane=cr_random(w)&3;if(q->carrier_crew.lane==3)q->carrier_crew.lane=2;
  q->carrier_crew.arc=0;q->carrier_crew.goal=cr_word(cr_sy(w,&p->actor)-12);
  if(q->carrier_crew.lane==1)human_corpse_transform(w,e,0);else cr_schedule(e,CR_LEAP);return 0;
 case CR_RIDE:a->palette=40;cr_start(q,CRA_RIDE);break;
 case CR_LEAP:case CR_LEAP_B:
  /* $013600 only releases the previous palette; the next call assigns $28. */
  a->palette=40;interaction_priority(w,a,0xc000,16);a->phase=0;cr_reset(a);cr_start(q,q->carrier_crew.lane?CRA_LEAP_B:CRA_LEAP);break;
 case CR_FALL:break;
 case CR_LAND:
  a->palette=12;interaction_priority(w,a,0xc000,16);cr_reset(a);if(a->transform.facing&1)a->vx=cr_word(-a->vx);cr_start(q,CRA_LAND);cr_defense(q,65535);break;
 case CR_FLEE:cr_reset(a);a->vx=a->transform.facing&1?96:-96;cr_start(q,CRA_FADE);cr_defense(q,65535);break;
 case CR_DIVE:a->palette=40;transform_integer_offset(&a->transform,0,-24);interaction_priority(w,a,0x8000,12);cr_start(q,CRA_DIVE);break;
 case CR_DIVE_LOOP:cr_start(q,CRA_DIVE_LOOP);q->carrier_crew.goal=0;break;
 case CR_DIVE_LAND:cr_start(q,CRA_DIVE_LAND);break;
 case CR_FLEE_B:transform_integer_offset(&a->transform,0,24);cr_reset(a);cr_start(q,CRA_FADE);break;
 case CR_GUNNERS:
  if(!cr_child(w,e,CR_GUN_POSE)||!cr_child(w,e,CR_RELOAD))return 0;
  a->phase=0;q->dead=1;q->carrier_gunners.pose=0;q->carrier_gunners.reload=40;q->carrier_gunners.special=240;q->carrier_gunners.second=60;q->carrier_gunners.hidden=0;break;
 case CR_RELOAD:
  a->palette=14;a->transform.priority+=24;p->carrier_gunners.hidden=1;p->dead|=1;q->dead=255;cr_reset(a);
  q->carrier_crew.arc=q->carrier_crew.slot=0;cr_start(q,CRA_EMERGE);break;
 case CR_GUN_HOLD:p->carrier_gunners.hidden=0;cr_start(q,CRA_GUN_HOLD);break;
 case CR_GUN_POSE:a->palette=38;a->transform.priority+=16;a->phase=q->dead=0;cr_reset(a);cr_start(q,CRA_GUN_POSE);return 0;
 case CR_GUNNER_FIRE:
  p->dead|=2;q->dead=255;q->carrier_crew.arc=-1;q->carrier_crew.slot=1;a->transform.priority+=28;
  a->palette=14;cr_reset(a);cr_start(q,CRA_EMERGE);break;
 case CR_GUNNER_SPECIAL:
  q->dead=0;q->carrier_crew.arc=0;q->carrier_crew.slot=2;a->transform.priority+=32;
  a->palette=14;cr_reset(a);cr_start(q,CRA_EMERGE);break;
 case CR_GUNNER_LAND:q->carrier_crew.offset=0;cr_start(q,CRA_JUMP);break;
 case CR_GUNNER_STAND:
  if(!q->carrier_crew.arc&&(cr_random(w)&1)){cr_schedule(e,CR_GUNNER_WALK);return 2;}cr_start(q,CRA_TURN);break;
 case CR_GUNNER_RIGHT:a->transform.facing=1;a->state=CR_GUNNER_WALK;/* FALLTHROUGH */
 case CR_GUNNER_WALK:cr_reset(a);a->vx=a->transform.facing&1?512:-512;q->carrier_crew.offset=0;cr_start(q,CRA_RUN);break;
 case CR_GUNNER_STOP:cr_start(q,CRA_STAND);break;
 case CR_GUNNER_RIDE:
  if(q->carrier_crew.arc){cr_schedule(e,CR_GUNNER_IDLE);return 2;}a->palette=40;cr_start(q,CRA_RIDE);break;
 case CR_GUNNER_IDLE:cooldown(w,e,1);cr_start(q,CRA_IDLE);break;
 case CR_GUNNER_DECIDE:{
  int right=player_x(w,a)>cr_sx(w,a);cr_schedule(e,!!(a->transform.facing&1)==right?CR_GUNNER_THROW:CR_GUNNER_AIM);return 2;
 }
 case CR_GUNNER_AIM:cr_start(q,CRA_AIM);break;
 case CR_GUARD:
  q->carrier_crew.guard_variant=cr_random(w)&1;p->carrier_spawner.busy|=(uint8_t)(1u<<(q->carrier_crew.lane&7));a->palette=25;
  a->transform.priority+=3;interaction_priority(w,a,a->transform.priority,a->transform.priority&28);a->hit.hp=1;cr_reset(a);
  cr_start(q,q->carrier_crew.guard_variant?CRA_GUARD_B:CRA_GUARD);break;
 case CR_GUARD_LAND:
  if(q->carrier_crew.guard_variant&&!w->enemies->blood_enabled)a->palette=0x1d5;
  cr_start(q,q->carrier_crew.guard_variant?CRA_GUARD_B_HIT:CRA_GUARD_HIT);break;
 case CR_GUARD_FADE:a->damage_timer=40;break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static int spawner_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct EnemyActor*q=&e->payload.enemy,*p=cr_parent(w,e);if(!p)return 0;
 if(state==CR_SPAWNER){
  q->carrier_spawner.period=(uint8_t)((uint8_t)cr_rank(w,q,q->carrier_spawner.deaths>15?CRR_SPAWN_LATE:CRR_SPAWN)*30);
  static const int8_t goals[]={-72,40,72};
  for(unsigned i=0;i<3;i++)if(!(q->carrier_spawner.busy&(1u<<i))){q->carrier_spawner.clocks[i]++;
   if(q->carrier_spawner.clocks[i]>=q->carrier_spawner.period){q->carrier_spawner.clocks[i]=0;struct NativeEntity*n=cr_child(w,e,CR_TROOPER);if(!n)return 0;n->payload.enemy.carrier_crew.goal=goals[i];n->payload.enemy.carrier_crew.lane=(int16_t)i;}}
  if(!cr_follow(w,e))return 0;if(p->actor.phase==3){q->actor.phase=255;cr_schedule(e,CR_SPAWNER_WAIT);}
 }else{if(!cr_follow(w,e))return 0;if(p->dead==1)cr_schedule(e,CR_DELETE);}
 cr_parent_tail(w,e,1);return !w->fault;
}
static int troop_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(!cr_arc(w,e))return 0;int done=cr_animate(w,e),battle=0;
 switch(state){
 case CR_TROOPER:if(done)cr_schedule(e,CR_TROOPER_JUMP);break;
 case CR_TROOPER_JUMP:if(done)cr_schedule(e,CR_RUN_SELECT);break;
 case CR_TROOPER_LAND:if(done)cr_schedule(e,CR_RUN_RIGHT);break;
 case CR_RUN:case CR_RUN_RIGHT:
  if((q->carrier_crew.offset>>8)==q->carrier_crew.goal){cr_reset(a);cr_schedule(e,CR_STAND);}else q->carrier_crew.offset=cr_word(q->carrier_crew.offset+a->vx);break;
 case CR_STAND:if(done)cr_schedule(e,CR_IDLE);battle=1;break;
 case CR_IDLE:
  if(!q->carrier_crew.cooldown&&!decide(w,e))return 0;q->carrier_crew.cooldown=cr_word(q->carrier_crew.cooldown-1);battle=1;break;
 case CR_AIM:if(done)cr_schedule(e,CR_FIRE);battle=1;break;
 case CR_WALK:
  q->carrier_crew.cooldown=cr_word(q->carrier_crew.cooldown-1);
  if(q->carrier_crew.cooldown){q->carrier_crew.offset=cr_word(q->carrier_crew.offset+a->vx);int16_t x=q->carrier_crew.offset>>8;
   if(a->vx>=0?x<q->carrier_crew.goal:x>q->carrier_crew.arc)break;}
  cr_reset(a);cr_schedule(e,CR_STOP);break;
 case CR_STOP:if(done)cr_schedule(e,q->carrier_crew.cooldown?CR_TURN:CR_IDLE);break;
 case CR_TURN:if(done)cr_schedule(e,CR_WALK_TURN);break;
 case CR_FIRE:if(done)return rearm(w,e);battle=1;break;
 default:return enemy_fault(w);
 }return troop_tail(w,e,battle);
}
static int rider_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;int done;
 switch(state){
 case CR_RIDE:if(!cr_arc(w,e))return 0;if(cr_animate(w,e))cr_schedule(e,CR_LEAP);break;
 case CR_LEAP:case CR_LEAP_B:
  if(a->phase){a->state=CR_FALL;state=CR_FALL;}
  else{if(!(state==CR_LEAP?cr_arc(w,e):cr_gunner_follow(w,e,8)))return 0;goto fall_animate;}
  /* FALLTHROUGH */
 case CR_FALL:
  if(interaction_move(w,a,WMOVE_POINT)<0)return 0;
 fall_animate:
  cr_animate(w,e);if(cr_word(q->carrier_crew.goal-16)>=cr_sy(w,a)){cr_screen(w,a,cr_sx(w,a),q->carrier_crew.goal);cr_schedule(e,CR_LAND);}break;
 case CR_LAND:case CR_FLEE:
  if(interaction_move(w,a,WMOVE_POINT)<0)return 0;if(cr_animate(w,e))cr_schedule(e,state==CR_LAND?CR_FLEE:CR_DIVE);break;
 case CR_DIVE:case CR_DIVE_LAND:case CR_FLEE_B:
  if(interaction_move(w,a,state==CR_FLEE_B?WMOVE_SYNC:WMOVE_POINT)<0)return 0;done=cr_animate(w,e);
  if(done)cr_schedule(e,state==CR_DIVE?CR_DIVE_LOOP:state==CR_DIVE_LAND?CR_FLEE_B:CR_DELETE);break;
 case CR_DIVE_LOOP:{
  if(interaction_move(w,a,WMOVE_POINT)<0)return 0;q->carrier_crew.goal=cr_word(q->carrier_crew.goal+a->vx);int16_t distance=q->carrier_crew.goal;if(distance<0)distance=cr_word(-distance);
  if(distance>=0x4000)a->vy=-96;cr_animate(w,e);if(a->hit.flags&8)cr_schedule(e,CR_DIVE_LAND);break;
 }
 default:return enemy_fault(w);
 }
 if(interaction_outside(w,a,state>=CR_DIVE?carrier_bounds[1]:boss_fx_bounds[8],1))cr_schedule(e,CR_DELETE);return !w->fault;
}
static int gunners_frame(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy,*p=cr_parent(w,e);if(!p)return 0;
 if(!(q->dead&1)){q->carrier_gunners.reload=cr_word(q->carrier_gunners.reload-1);if(!q->carrier_gunners.reload){q->carrier_gunners.reload=40;if(!cr_child(w,e,CR_RELOAD))return 0;}}
 if(!(q->dead&2)){q->carrier_gunners.second=cr_word(q->carrier_gunners.second-1);if(!q->carrier_gunners.second){q->carrier_gunners.second=60;if(!cr_child(w,e,CR_GUNNER_FIRE))return 0;}}
 q->carrier_gunners.special=cr_word(q->carrier_gunners.special-1);if(!q->carrier_gunners.special){q->carrier_gunners.special=(cr_random(w)&127)-63+600;if(!cr_child(w,e,CR_GUNNER_SPECIAL))return 0;}
 if(p->actor.phase!=3){q->actor.phase=255;cr_schedule(e,CR_DELETE);}cr_parent_tail(w,e,1);return !w->fault;
}
static int gunner_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct EnemyActor*q=&e->payload.enemy,*p=cr_parent(w,e);if(!p)return 0;struct WeaponActor*a=&q->actor;
 if(!cr_gunner_follow(w,e,state<=CR_GUNNER_SPECIAL?9:8))return 0;
 if(state==CR_GUN_POSE){
  if(p->carrier_gunners.hidden!=1){if(p->actor.phase==255){entity_delete(w->entities,e);return 1;}
   if((uint16_t)p->carrier_gunners.pose>=6)return enemy_fault(w);cr_start_entry(q,carrier_gun_poses[p->carrier_gunners.pose]);}
  cr_animate(w,e);cr_parent_tail(w,e,0);return !w->fault;
 }
 int done=cr_animate(w,e);
 switch(state){
 case CR_RELOAD:if(done)cr_schedule(e,CR_GUN_HOLD);break;
 case CR_GUN_HOLD:p->carrier_gunners.pose=q->carrier_crew.goal;break;
 case CR_GUNNER_FIRE:case CR_GUNNER_SPECIAL:if(done){q->carrier_crew.lane=(cr_random(w)&1)*2;cr_schedule(e,CR_GUNNER_LAND);}break;
 case CR_GUNNER_LAND:if(done)cr_schedule(e,CR_GUNNER_STAND);break;
 case CR_GUNNER_STAND:if(done)cr_schedule(e,CR_GUNNER_RIGHT);break;
 case CR_GUNNER_WALK:{int16_t d=q->carrier_crew.offset;if(d<0)d=cr_word(-d);if(d>=0x2000){cr_reset(a);cr_schedule(e,CR_GUNNER_STOP);}break;}
 case CR_GUNNER_STOP:if(done)cr_schedule(e,CR_GUNNER_RIDE);break;
 case CR_GUNNER_RIDE:if(done){q->carrier_crew.goal=cr_word(cr_sy(w,&p->actor)-12);cr_schedule(e,CR_LEAP_B);}break;
 case CR_GUNNER_IDLE:if(!q->carrier_crew.cooldown)cr_schedule(e,CR_GUNNER_DECIDE);q->carrier_crew.cooldown=cr_word(q->carrier_crew.cooldown-1);break;
 case CR_GUNNER_AIM:if(done)cr_schedule(e,CR_GUNNER_THROW);break;
 case CR_GUNNER_THROW:if(done)cr_schedule(e,CR_GUNNER_IDLE);break;
 default:return enemy_fault(w);
 }return gunner_tail(w,e);
}
int cr_crew_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 if(state<=CR_SPAWNER_WAIT)return spawner_frame(w,e,state);
 if(state<=CR_FIRE)return troop_frame(w,e,state);
 if(state<=CR_FLEE_B)return rider_frame(w,e,state);
 if(state==CR_GUNNERS)return gunners_frame(w,e);
 if(state<CR_GUARD)return gunner_frame(w,e,state);
 struct EnemyActor*q=&e->payload.enemy,*p=cr_parent(w,e);if(!p||!cr_arc(w,e))return 0;int done=cr_animate(w,e);
 if(state==CR_GUARD){
  if(p->dead==3||p->actor.phase==255)if(!boss_fx_schedule(w,e,BFX_BLAST))return 0;
  if(q->actor.hit.flags&8)cr_schedule(e,CR_GUARD_LAND);cr_parent_tail(w,e,0);
 }else if(state==CR_GUARD_LAND){if(done)cr_schedule(e,CR_GUARD_FADE);}
 else if(state==CR_GUARD_FADE){if(!q->actor.damage_timer){p->carrier_spawner.busy&=(uint8_t)~(1u<<(q->carrier_crew.lane&7));entity_delete(w->entities,e);}}
 else return enemy_fault(w);return !w->fault;
}
