#include "boss117_internal.h"
/* $082480..$0832CE: attached attacks, projectile physics, paired blasts,
 * callback emissions and the scene-signalling destruction overlay. */
static void offset(struct WeaponActor*a,const int16_t*t,int mirror){transform_integer_offset(&a->transform,mirror?b117_word(-t[0]):t[0],t[1]);}
static int outside(struct WeaponWorld*w,struct NativeEntity*e,const int16_t*b){
 if(interaction_outside(w,&e->payload.enemy.actor,b,1))b117_schedule(e,B117_DELETE_TREE);return !w->fault;
}
static int floor_y(struct WeaponWorld*w,int16_t*y){if(!w->secondary_view_valid){enemy_fault(w);return 0;}*y=b117_word(408+w->secondary_y_offset);return 1;}
int b117_troops(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;q->boss117.mask=10;
 do{q->boss117.mask--;struct NativeEntity*n=b117_child(w,e,B117_TROOP);if(!n)return 0;n->payload.enemy.boss117.shot=q->boss117.mask&15;n->payload.enemy.boss117.selector=q->boss117.selector;}while(q->boss117.mask);return 1;
}
int b117_side_projectiles(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;unsigned i=(uint16_t)q->boss117.selector;if(i>=6)return enemy_fault(w);int flip=q->actor.transform.facing&1;
 struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&q->actor.transform,BFX_SMALL);if(!n)return 0;offset(&n->payload.enemy.actor,boss117_projectile_offsets[i],flip);
 n=b117_child(w,e,B117_PROJECTILE);if(!n)return 0;struct WeaponActor*a=&n->payload.enemy.actor;offset(a,boss117_projectile_offsets[i],flip);
 a->vx=flip?b117_word(-boss117_projectile_motion[i][0]):boss117_projectile_motion[i][0];a->ay=boss117_projectile_motion[i][1];a->vy=boss117_projectile_motion[i][2];return 1;
}
int b117_side_burst(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;unsigned i=(uint16_t)q->boss117.shot;if(i>=5)return enemy_fault(w);int flip=q->actor.transform.facing&1;
 struct NativeEntity*n=b117_child(w,e,B117_PELLET);if(!n)return 0;struct WeaponActor*a=&n->payload.enemy.actor;offset(a,boss117_burst_offsets[i],flip);
 a->vx=b117_word(a->vx+(flip?-boss117_burst_motion[i][0]:boss117_burst_motion[i][0]));a->ay=boss117_burst_motion[i][1];a->vy=boss117_burst_motion[i][2];
 if(!interaction_sound(w,0x10ff))return 0;n=b117_child(w,e,B117_BALLISTIC);if(!n)return 0;
 n->payload.enemy.boss117.selector=(int16_t)i;n->payload.enemy.actor.trail_angle=i*8+32;offset(&n->payload.enemy.actor,boss117_ballistic_offsets[i],flip);return 1;
}
int b117_callback(struct WeaponWorld*w,struct NativeEntity*e,unsigned callback){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*n;struct EnemyActor*p;
 switch(callback){
 case B117C_434CE:return 1; /* verified RTS */
 case B117C_283CA:hit_begin_attack(&a->hit);return 1;
 case B117C_832B6:{
  unsigned i=(uint16_t)q->boss117.selector;if(i>=7)return enemy_fault(w);unsigned b=boss117_attack[i];
  if(b==65535)a->attack=0;else{q->attack_set=(struct HitAttackSet){boss117_bindings[b].attack,boss117_bindings[b].count};a->attack=&q->attack_set;}hit_begin_attack(&a->hit);return 1;
 }
 case B117C_83158:if(q->boss117.mask==3)return 1;
  /* Source $083164 writes $5E to the parent three times. */
  /* fall through */
 case B117C_83164:
  for(int i=0;i<3;i++){n=b117_child(w,e,B117_SCATTER);if(!n)return 0;n->payload.enemy.boss117.selector=q->boss117.selector;q->boss117.shot=(int16_t)i;}return 1;
 case B117C_831B8:w->enemies->scene_flags[1]=1;return 1;
 case B117C_831C2:n=b117_child(w,e,B117_PAIRED_BLAST);if(!n)return 0;n->payload.enemy.boss117.selector=q->boss117.selector;return 1;
 case B117C_8321C:{
  n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMOKE);if(!n)return 0;
  int16_t dx=(b117_random(w)&31)-16,dy=(b117_random(w)&63)-16;transform_integer_offset(&n->payload.enemy.actor.transform,dx,dy);return !w->fault;
 }
 case B117C_83254:return b117_pattern(w,e,B117P_FIRE4)!=0;
 case B117C_83262:return b117_pattern(w,e,B117P_FIRE2)!=0;
 case B117C_83270:
  w->enemies->scene_flags[0]=4;w->enemies->scene_flags[1]=0;p=b117_parent(w,e);if(!p)return 0;p->actor.phase=0x77;return 1;
 case B117C_8328C:
  if(!b117_pattern(w,e,B117P_FINAL_FIRE)||!b117_pattern(w,e,B117P_FINAL_DEBRIS))return 0;return interaction_sound(w,0x102f);
 default:return enemy_fault(w);
 }
}
int b117_effects_enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned s=a->state;q->entry=0;unsigned i=(uint16_t)q->boss117.selector;int16_t y;
 switch(s){
 case B117_TROOP:
  if(i>=7||(uint16_t)q->boss117.shot>=10)return enemy_fault(w);a->palette=0x74;
  q->boss117.anchor_x=boss117_troop_offsets[i][q->boss117.shot][0];q->boss117.anchor_y=boss117_troop_offsets[i][q->boss117.shot][1];b117_start_entry(q,boss117_troop[q->boss117.shot]);return 1;
 case B117_CENTRAL_ATTACHED:
  q->boss117.mask=(enemy_player_live(w->players[0])?1:0)|(enemy_player_live(w->players[1])?2:0);a->palette=0x74;b117_start(q,B117A_CENTRAL_ATTACHED);return 1;
 case B117_SCATTER:{
  int shot=q->boss117.shot;if(i>=7||shot < -1||shot>3)return enemy_fault(w);transform_integer_offset(&a->transform,boss117_scatter_offsets[i][shot+1],-24);
  const int16_t*t=boss117_scatter_motion[i][b117_random(w)&15];a->vx=t[0];a->ay=b117_word(-t[1]);a->vy=b117_word(-t[2]);
  a->palette=0x76;interaction_priority(w,a,0xc000,20);b117_start(q,B117A_SCATTER);return !w->fault;
 }
 case B117_BEAM:a->palette=0x75;b117_start(q,B117A_BEAM);return 1;
 case B117_PAIRED_BLAST:
  if(i>=7)return enemy_fault(w);q->boss117.clock=q->boss117.path_step=0;q->boss117.mask=6;return 1;
 case B117_PROJECTILE:
  a->palette=0x73;interaction_priority(w,a,0xd000,20);a->hit.hp=b117_rank(w,q,B117R_PROJECTILE_HP);b117_start(q,B117A_PROJECTILE);return !w->fault;
 case B117_PROJECTILE_FALL:a->hit.type_flags|=16;a->ay=-64;b117_start(q,B117A_PROJECTILE_FALL);return 1;
 case B117_PROJECTILE_IMPACT:
  if(!boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMALL))return 0;b117_start(q,B117A_PROJECTILE_IMPACT);return 1;
 case B117_BOUNCE_BLAST:b117_defense(q,65535);enemy_bounce_explode(w,e);return 0;
 case B117_SIDE_ATTACHED:a->palette=0x193;b117_start(q,B117A_SIDE_ATTACHED);return 1;
 case B117_PELLET:
  a->palette=0xd2;interaction_priority(w,a,0xc000,20);a->vx=b117_word(a->vx+(b117_random(w)&127)-64);a->vy=b117_word(a->vy+(b117_random(w)&63));b117_start(q,B117A_PELLET);return !w->fault;
 case B117_GROUND_IMPACT:
  if(i>=6||!floor_y(w,&y))return enemy_fault(w);a->palette=0x1b4;
  b117_screen(w,a,q->boss117.shot+((a->transform.facing&1)?-boss117_ground_offsets[i]:boss117_ground_offsets[i]),y);
  {int dx=(b117_random(w)&7)-4,dy=-(int)(b117_random(w)&3);transform_integer_offset(&a->transform,dx,dy);}
  interaction_priority(w,a,0x8000,20);b117_start(q,B117A_GROUND_IMPACT);return !w->fault;
 case B117_BALLISTIC:{
  unsigned angle=a->trail_angle;if(angle>=256)return enemy_fault(w);a->vx=b117_word(((int32_t)boss_trig[1][angle]*8192)>>8);a->vy=b117_word(((int32_t)boss_trig[0][angle]*8192)>>8);
  if(a->transform.facing&1)a->vx=b117_word(-a->vx);a->trail_angle=0;q->boss117.shot=b117_sx(w,a);a->palette=2;a->hit.type_flags|=16;
  interaction_priority(w,a,0xd000,20);b117_start(q,B117A_BALLISTIC);return 1;
 }
 case B117_GROUND_BLAST:{
  a->palette=4;int dx=(b117_random(w)&15)-8;transform_integer_offset(&a->transform,dx,0);b117_start(q,(dx&1)?B117A_GROUND_BLAST_B:B117A_GROUND_BLAST_A);return !w->fault;
 }
 case B117_PARACHUTE:a->palette=0x76;interaction_priority(w,a,0xd000,28);a->vx=16;a->vy=192;b117_start(q,B117A_PARACHUTE);return 1;
 case B117_DEATH_OVERLAY:interaction_priority(w,a,0xc000,28);b117_start(q,B117A_DEATH_OVERLAY);return 1;
 case B117_DEBRIS:{
  static const uint8_t drawings[]={8,9,10,6,7,8,9,10};unsigned n=b117_random(w)&7;
  boss_fx_scatter_selected(w,e,drawings[n],boss117_debris_palettes[n],2048);return 0;
 }
 case B117_PATTERN:
  if(q->boss117.pattern>=B117P_COUNT)return enemy_fault(w);q->boss117.delay=boss117_patterns[q->boss117.pattern].period;q->boss117.remaining=boss117_patterns[q->boss117.pattern].life;return 1;
 case B117_FIRE_SOUND:if(!interaction_sound(w,0x109f))return 0;
  /* fall through */
 case B117_FIRE:b117_defense(q,65535);boss_fx_transform(w,e,BFX_FIRE);return 0;
 default:return enemy_fault(w);
 }
}
static int emit_pattern(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy,*p=b117_parent(w,e);if(!p)return 0;struct WeaponActor*a=&q->actor;
 q->scale_x=p->scale_x;q->scale_y=p->scale_y;const struct BossPatternData*t=&boss117_patterns[q->boss117.pattern];
 int emit=q->boss117.delay==255;if(!emit){q->boss117.delay--;if(!q->boss117.delay){q->boss117.delay=t->period;emit=1;}}
 if(emit){
  if(t->follow){int flip=p->actor.transform.facing&1;b117_screen(w,a,b117_sx(w,&p->actor)+(flip?-t->dx:t->dx),b117_sy(w,&p->actor)+(flip?-t->dy:t->dy));}
  for(unsigned budget=0;budget<ENTITY_CAPACITY;budget++){
   struct NativeEntity*n;
   if(!t->kind)n=boss_fx_spawn(w,entity_ref(e),ENTITY_LAST_CHILD,&a->transform,BFX_FIRE);
   else{
    n=enemy_allocate(w,entity_ref(e),ENTITY_LAST_CHILD,&a->transform,e->tick);if(n){uint8_t*bytes=(uint8_t*)&n->payload.enemy.boss117;for(unsigned i=0;i<sizeof(n->payload.enemy.boss117);i++)bytes[i]=255;b117_schedule(n,t->kind==1?B117_DEBRIS:B117_PARACHUTE);}
   }
   if(!n)return 0;if(!w->rng)return enemy_fault(w);int16_t dx=(int16_t)source_random(w->rng,t->spread_x),dy=(int16_t)source_random(w->rng,t->spread_y);
   if(a->transform.facing&1)dx=b117_word(-dx);if(a->transform.facing&2)dy=b117_word(-dy);transform_integer_offset(&n->payload.enemy.actor.transform,dx,dy);
   n->payload.enemy.actor.display_flags|=a->display_flags&64;n->payload.enemy.scale_x=q->scale_x;n->payload.enemy.scale_y=q->scale_y;
   if(q->boss117.delay!=255)break;if(!--q->boss117.remaining){entity_delete(w->entities,e);return 1;}
   if(budget==ENTITY_CAPACITY-1)return enemy_fault(w);
  }
 }
 if(!--q->boss117.remaining)entity_delete(w->entities,e);return !w->fault;
}
static int projectile_tail(struct WeaponWorld*w,struct NativeEntity*e){
 struct WeaponActor*a=&e->payload.enemy.actor;if(!enemy_attack(w,a))return 0;if(a->hit.flags&2)b117_schedule(e,B117_FIRE);
 if(a->hit.flags&1)b117_schedule(e,B117_FIRE_SOUND);return outside(w,e,boss_fx_bounds[8]);
}
static int knockback(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(!(a->hit.flags&8))return 1;a->hit.flags&=247;
 if(a->hit.last_kind!=32&&a->hit.last_kind!=1)return 1;if(a->hit.last_attacker>=ENTITY_CAPACITY)return enemy_fault(w);
 struct NativeEntity*n=&w->entities->nodes[a->hit.last_attacker];struct WeaponActor*other=entity_weapon_actor(n);int16_t vx,vy;
 if(!n->active)return enemy_fault(w);
 if(other){vx=other->vx;vy=other->vy;}
 else if(n->kind==ENTITY_PLAYER){vx=n->payload.player.vx;vy=n->payload.player.vy;}
 else return enemy_fault(w);
 a->vx=b117_word(a->vx+vx);
 /* Source deliberately compares the WORD containing kind AND damage timer. */
 a->vy=b117_word(a->vy+((((uint16_t)a->hit.last_kind<<8)|a->damage_timer)==32?-2048:vy));
 a->hit.flags&=254;a->hit.hp=b117_rank(w,q,B117R_PROJECTILE_HP);b117_schedule(e,B117_PROJECTILE_FALL);return !w->fault;
}
int b117_effects_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;int r,done;int16_t y;
 switch(s){
 case B117_TROOP:
  if(!b117_full_follow(w,e))return 0;transform_integer_offset(&a->transform,q->boss117.anchor_x,q->boss117.anchor_y);
  if(b117_animate(w,e))b117_schedule(e,B117_DELETE_TREE);return outside(w,e,boss_fx_bounds[8]);
 case B117_CENTRAL_ATTACHED:case B117_SIDE_ATTACHED:
  if(!b117_full_follow(w,e))return 0;p=b117_parent(w,e);if(!p)return 0;
  if(s==B117_CENTRAL_ATTACHED){q->boss117.selector=p->boss117.selector;transform_integer_offset(&a->transform,0,-56);}else q->boss117.shot=p->boss117.shot;
  b117_animate(w,e);if(p->actor.phase)b117_schedule(e,B117_DELETE);return !w->fault;
 case B117_SCATTER:case B117_PARACHUTE:
  if(interaction_move(w,a,s==B117_SCATTER?WMOVE_FALL:WMOVE_POINT)<0)return 0;if(b117_animate(w,e))b117_schedule(e,B117_DELETE_TREE);return outside(w,e,boss117_bounds);
 case B117_BEAM:
  if(!b117_full_follow(w,e))return 0;a->transform.priority--;done=b117_animate(w,e);p=b117_parent(w,e);if(!p)return 0;
  if(done){p->dead=1;b117_schedule(e,B117_DELETE);}if(!enemy_attack(w,a))return 0;if(p->actor.phase)b117_schedule(e,B117_DELETE);return !w->fault;
 case B117_PAIRED_BLAST:
  if(!(q->boss117.clock&1)){
   if((uint16_t)q->boss117.selector>=7||q->boss117.path_step>=6)return enemy_fault(w);interaction_priority(w,a,0x4000,20);
   if(!--q->boss117.mask)b117_schedule(e,B117_DELETE);const int16_t*t=boss117_blast_path[q->boss117.selector][q->boss117.path_step++];
   for(unsigned i=0;i<2;i++){struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_BLAST_B);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,t[i],t[2]);}
  }q->boss117.clock++;return !w->fault;
 case B117_PROJECTILE:case B117_PROJECTILE_FALL:
  r=interaction_move(w,a,s==B117_PROJECTILE?WMOVE_BODY:WMOVE_FALL);if(r<0)return 0;if(r)b117_schedule(e,B117_PROJECTILE_IMPACT);b117_animate(w,e);
  if(s==B117_PROJECTILE){if(!knockback(w,e))return 0;}else a->hit.flags&=247;return projectile_tail(w,e);
 case B117_PROJECTILE_IMPACT:
  if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;if(b117_animate(w,e))b117_schedule(e,B117_BOUNCE_BLAST);a->hit.flags&=247;return projectile_tail(w,e);
 case B117_PELLET:
  r=interaction_move(w,a,WMOVE_BODY);if(r<0)return 0;if(r)b117_schedule(e,B117_DELETE_TREE);b117_animate(w,e);return outside(w,e,boss_fx_bounds[8]);
 case B117_GROUND_IMPACT:
  if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;if(b117_animate(w,e))b117_schedule(e,B117_DELETE_TREE);if(!enemy_attack(w,a))return 0;return outside(w,e,boss_fx_bounds[8]);
 case B117_BALLISTIC:
  r=interaction_move(w,a,WMOVE_POINT);if(r<0)return 0;if(r)b117_schedule(e,B117_GROUND_BLAST);b117_animate(w,e);if(!floor_y(w,&y))return 0;
  if(y<=b117_sy(w,a))b117_schedule(e,B117_GROUND_IMPACT);if(!enemy_attack(w,a))return 0;if(a->hit.flags&2)b117_schedule(e,B117_GROUND_BLAST);return outside(w,e,boss_fx_bounds[8]);
 case B117_GROUND_BLAST:
  if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;if(b117_animate(w,e)){entity_delete_children(w->entities,e);entity_delete(w->entities,e);}return !w->fault;
 case B117_DEATH_OVERLAY:
  if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;if(b117_animate(w,e))b117_schedule(e,B117_DELETE);if(entity_parent_deleting(w->entities,e))b117_schedule(e,B117_DELETE_TREE);return !w->fault;
 case B117_PATTERN:return emit_pattern(w,e);
 default:return enemy_fault(w);
 }
}
