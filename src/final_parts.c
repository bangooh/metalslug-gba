#include "final_internal.h"
#include "items.h"
/* Child command dispatch, independently destructible head, cannons, flame
 * followers and death emissions. Allocation order follows source $04AE. */
static unsigned command_slot(const struct EnemyActor*q){return q->final.part==5?0:q->final.part==4?4:q->final.part+1;}
static unsigned consume(struct EnemyActor*p,const struct EnemyActor*q){unsigned i=command_slot(q);unsigned c=p->final.commands[i];p->final.commands[i]=0;return c;}
static int part_start(struct WeaponWorld*w,struct EnemyActor*q,unsigned mode){
 if(q->final.part>=5||mode>=5)return enemy_fault(w);
 const uint16_t*t[]={final_part0,final_part1,final_part2,final_part3,final_part4};fb_start_entry(q,t[mode][q->final.part]);q->actor.state=FB_LIMB;return 1;
}
static int death_score(struct WeaponWorld*w,struct WeaponActor*a){
 if((a->display_flags&2)||(int8_t)a->score_owner<0)return 1;
 return w->items&&item_score_add(w->items,a->score_owner,enemy_hit_scores[a->hit.last_kind&31])?1:enemy_fault(w);
}
static int head_shard(struct WeaponWorld*w,struct NativeEntity*e){return fb_child(w,e,FB_HEAD_SHARD)!=0;}
static int cannon_palette(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*p=fb_parent(w,e);if(!p)return 0;e->payload.enemy.actor.palette=p->final.attached?0x5d:0x1e6;return 1;}
static void face_players(struct WeaponWorld*w,struct WeaponActor*a){
 /* $05E5E0 returns C=0 if either live player is in front; C=1 also when
  * neither player is live. The cannon has already cleared its facing byte. */
 int front=0;for(unsigned i=0;i<2;i++)if(enemy_player_live(w->players[i])){
  int16_t dx=fb_word((a->transform.x>>8)-(w->players[i]->actor.transform.x>>8));if(a->transform.facing&1?dx>0:dx<0)front=1;
 }if(!front)a->vx=fb_word(-a->vx);
}
int fb_parts_enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;unsigned state=a->state;q->entry=0;
 switch(state){
 case FB_LIMB_NEW:
  if(q->final.part>=5)return enemy_fault(w);q->final.priority=q->final.part? -1:0;
  fb_start(q,(enum FinalAnimation)(FBA_LIMB0+q->final.part));p=fb_parent(w,e);if(!p)return 0;q->final.attached=p->final.attached;
  a->hit_palette=0x80;a->palette=!(q->final.attached&1)?0x1e5:p->final.side?0x5c:0x5b;return part_start(w,q,0);
 case FB_LIMB_RESET:return part_start(w,q,0);
 case FB_LIMB_A:case FB_LIMB_B:case FB_LIMB_C:case FB_LIMB_D:return part_start(w,q,state-FB_LIMB_A+1);
 case FB_HEAD:q->final.priority=0;q->final.part=5;q->final.attack_wait=(fb_random(w)&63)+30;a->palette=0x38;a->state=FB_HEAD_IDLE;/* FALLTHROUGH */
 case FB_HEAD_IDLE:
  a->variant=1;a->hit.hp=1;a->hit.flags&=246;fb_start(q,FBA_HEAD);fb_animate(w,e);break;
 case FB_HEAD_A:case FB_HEAD_B:case FB_HEAD_C:
  a->variant=(uint8_t)(state-FB_HEAD_A+2);fb_start(q,(enum FinalAnimation)(FBA_HEAD_A+state-FB_HEAD_A));a->state=FB_HEAD_HIT;/* FALLTHROUGH */
 case FB_HEAD_HIT:a->hit.hp=1;a->hit.flags&=246;break;
 case FB_HEAD_DIE:
  if(a->variant==4&&!head_shard(w,e))return 0;a->variant=5;if(!fb_sound(w,0x1041,0))return 0;fb_start(q,FBA_HEAD_DIE);break;
 case FB_HEAD_FLASH:a->ttl=60;break; /* source $70 is BYTE in this state */
 case FB_HEAD_RESPAWN:q->final.wait=fb_rank(w,q,FBR_HEAD_RESPAWN);break;
 case FB_HEAD_DEATH:
  if(a->variant==4&&!head_shard(w,e))return 0;if(!fb_follow(w,e,1))return 0;fb_animate(w,e);interaction_priority(w,a,0xd000,20);
  a->palette=w->enemies->blood_enabled?0xe:0x2f;if(!human_corpse_transform(w,e,0))return 0;return 2;
 case FB_CANNON:case FB_CANNON_B:
  fb_start(q,state==FB_CANNON?FBA_CANNON:FBA_CANNON_B);if(!cannon_palette(w,e))return 0;a->hit.type_flags|=16;q->final.priority=0;a->state=FB_CANNON;break;
 case FB_SHELL:{
  const int16_t*t=final_ballistics[a->phase==1][q->params[0]&15];a->transform.facing=0;a->ay=t[1];a->vy=t[2];a->vx=fb_faced(a,t[0]);face_players(w,a);
  a->hit.hp=50;fb_defense(q,FBB_SHELL_DEF);fb_start(q,FBA_SHELL);break;
 }
 case FB_MUZZLE:if(!cannon_palette(w,e))return 0;q->final.priority=0;fb_start(q,FBA_MUZZLE);break;
 case FB_CASING:a->palette=8;fb_start(q,FBA_CASING);a->transform.facing^=1;interaction_priority(w,a,0x4000,28);break;
 case FB_FLAME:a->palette=0x5e;fb_start(q,FBA_FLAME);q->final.offset_x=fb_faced(a,-22);q->final.offset_y=40;q->final.priority=0;a->state=FB_MUZZLE;break;
 case FB_BEAM:case FB_BEAM_B:q->final.priority=1;a->palette=0x1cf;fb_start(q,state==FB_BEAM?FBA_BEAM:FBA_BEAM_B);break;
 case FB_BEAM_FLIGHT:case FB_BEAM_B_FLIGHT:
  a->hit.type_flags|=16;a->vx=fb_faced(a,fb_word(-fb_rank(w,q,state==FB_BEAM_FLIGHT?FBR_BEAM_SPEED:FBR_BEAM_B_SPEED)));
  interaction_priority(w,a,0xd000,28);a->palette=0x1cf;fb_start(q,state==FB_BEAM_FLIGHT?FBA_BEAM_FLIGHT:FBA_BEAM_B_FLIGHT);break;
 case FB_FLAME_FOLLOW:a->palette=0x39;interaction_priority(w,a,0xd000,20);fb_start(q,FBA_FLAME_FOLLOW);break;
 case FB_FLAME_FLIGHT:a->vx=fb_faced(a,fb_rank(w,q,FBR_FLAME_SPEED));a->ax=fb_faced(a,fb_rank(w,q,FBR_FLAME_ACCEL));fb_start(q,FBA_FLAME_FLIGHT);break;
 case FB_HEAD_SHARD:case FB_HEAD_SHARD_SMALL:
  a->vx=fb_faced(a,state==FB_HEAD_SHARD?273:-68);a->vy=state==FB_HEAD_SHARD?2175:810;a->ay=state==FB_HEAD_SHARD?-145:-54;a->ax=0;a->palette=0x38;fb_start(q,FBA_HEAD_SHARD);break;
 case FB_PUFF:case FB_FLASH:
  interaction_priority(w,a,0xc000,20);a->palette=state==FB_PUFF?0x148:4;fb_start(q,state==FB_PUFF?FBA_PUFF:FBA_FLASH);break;
 case FB_FIRE_LEFT40:case FB_FIRE_LEFT48:
  transform_integer_offset(&a->transform,fb_faced(a,state==FB_FIRE_LEFT40?-40:-48),0);/* FALLTHROUGH */
 case FB_FIRE:
  a->palette=65535;interaction_priority(w,a,0x4000,0);fb_defense(q,65535);if(!boss_fx_transform(w,e,BFX_FIRE))return 0;return 2;
 case FB_SHELL_HIT:if(!fb_sound(w,0x1022,0))return 0;/* FALLTHROUGH */
 case FB_SMOKE:interaction_priority(w,a,0x4000,0);fb_defense(q,65535);if(!boss_fx_transform(w,e,BFX_SMOKE))return 0;return 2;
 default:break;
 }return !w->fault;
}
static int offset_follow(struct WeaponWorld*w,struct NativeEntity*e){
 if(!fb_follow(w,e,0))return 0;struct EnemyActor*q=&e->payload.enemy;transform_integer_offset(&q->actor.transform,q->final.offset_x,q->final.offset_y);return 1;
}
static int head_proximity(struct WeaponWorld*w,struct EnemyActor*q){
 q->final.attack_wait=fb_word(q->final.attack_wait-1);if(q->final.attack_wait>0)return 0;
 const int16_t*b=final_head_proximity;struct HitRect rect={b[0],b[1],b[2],b[3]};
 if(enemy_nearest_x(w,&q->actor,&rect)<0)return 0;q->final.attack_wait=(fb_random(w)&63)+30;return 1;
}
/* Return 2 only for the source JMP through a part command table. */
int fb_parts_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;int r,done;
 switch(state){
 case FB_LIMB:{
  p=fb_parent(w,e);if(!p)return 0;a->phase=p->actor.phase;if(!fb_follow(w,e,1))return 0;
  unsigned c=consume(p,q)&15;if(c>=7)return enemy_fault(w);
  if(c){static const uint8_t states[]={FB_LIMB,FB_LIMB_RESET,FB_LIMB_A,FB_LIMB_B,FB_LIMB_C,FB_DELETE,FB_LIMB_D};
   /* $071866 schedules deletion and returns; it is not an immediate free. */
   enemy_schedule(q,states[final_limb_commands[c]]);return c==5?1:2;}
  if(p->actor.hit.events&128)a->hit.events|=1;fb_animate(w,e);return !w->fault;
 }
 case FB_HEAD_IDLE:{
  p=fb_parent(w,e);if(!p)return 0;a->phase=p->actor.phase;
  if(head_proximity(w,q))enemy_schedule(q,FB_HEAD_C);if(p->final.commands[0]==5)enemy_schedule(q,FB_HEAD_DEATH);
  unsigned c=consume(p,q)&15;if(c>=6)return enemy_fault(w);
  if(final_head_commands[c]){static const uint8_t states[]={FB_HEAD_IDLE,FB_HEAD_IDLE,FB_HEAD_A,FB_HEAD_B,FB_HEAD_C};enemy_schedule(q,states[final_head_commands[c]]);return 2;}return !w->fault;
 }
 case FB_HEAD_HIT:case FB_HEAD_DIE:case FB_HEAD_FLASH:case FB_HEAD_RESPAWN:
  p=fb_parent(w,e);if(!p)return 0;
  if(state==FB_HEAD_RESPAWN){q->final.wait=fb_word(q->final.wait-1);if(q->final.wait<=0)enemy_schedule(q,FB_HEAD_IDLE);}
  else{if(!fb_follow(w,e,1))return 0;
   if(state==FB_HEAD_FLASH){if(!--a->ttl)enemy_schedule(q,FB_HEAD_RESPAWN);if(!(a->ttl&1))fb_animate(w,e);}
   else{done=fb_animate(w,e);if(done)enemy_schedule(q,state==FB_HEAD_HIT?FB_HEAD_IDLE:FB_HEAD_FLASH);
    if(state==FB_HEAD_HIT&&(a->hit.flags&8)){if(!death_score(w,a))return 0;enemy_schedule(q,FB_HEAD_DIE);}}
  }
  if(consume(p,q)==5)enemy_schedule(q,state==FB_HEAD_RESPAWN?FB_DELETE_TREE:FB_HEAD_DEATH);return !w->fault;
 case FB_CANNON:case FB_MUZZLE:
  if(!offset_follow(w,e))return 0;if(fb_animate(w,e))enemy_schedule(q,state==FB_CANNON?FB_SHELL:FB_DELETE_TREE);
  if(entity_parent_deleting(w->entities,e))enemy_schedule(q,state==FB_CANNON?FB_SHELL_HIT:FB_DELETE_TREE);
  if(interaction_outside(w,a,final_bounds[1],1))enemy_schedule(q,FB_DELETE_TREE);return !w->fault;
 case FB_SHELL:case FB_SHELL_PRIORITY:
  if(state==FB_SHELL&&a->vy<=0){a->transform.priority=0xd000;a->state=FB_SHELL_PRIORITY;}
  r=interaction_move(w,a,WMOVE_FALL);if(r<0)return 0;if(r)enemy_schedule(q,FB_SHELL_HIT);
  if(a->vy>=0){p=fb_parent(w,e);if(!p)return 0;a->transform.priority=p->actor.transform.priority;}
  q->selector=enemy_angle(a->vx>>4,a->vy>>4)>>3;fb_animate(w,e);if(!enemy_attack(w,a))return 0;
  if(a->hit.flags&2)enemy_schedule(q,FB_SMOKE);if(!fb_hit_sound(w,a))return 0;a->hit.flags&=247;
  if((a->hit.flags&1)||entity_parent_deleting(w->entities,e))enemy_schedule(q,FB_SHELL_HIT);
  if(interaction_outside(w,a,final_bounds[1],1))enemy_schedule(q,FB_DELETE_TREE);return !w->fault;
 case FB_CASING:case FB_PUFF:case FB_FLASH:
  if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;if(fb_animate(w,e))enemy_schedule(q,FB_DELETE_TREE);
  if(state!=FB_PUFF&&interaction_outside(w,a,boss_fx_bounds[8],1))enemy_schedule(q,FB_DELETE_TREE);return !w->fault;
 case FB_BEAM:case FB_BEAM_B:
  if(!fb_follow(w,e,0))return 0;transform_integer_offset(&a->transform,fb_faced(a,state==FB_BEAM?-80:-60),state==FB_BEAM?56:40);
  if(fb_animate(w,e))enemy_schedule(q,state==FB_BEAM?FB_BEAM_FLIGHT:FB_BEAM_B_FLIGHT);if(!enemy_attack(w,a))return 0;
  if((a->hit.flags&2)||entity_parent_deleting(w->entities,e))enemy_schedule(q,FB_FIRE);
  if(interaction_outside(w,a,final_bounds[0],1))enemy_schedule(q,FB_DELETE_TREE);return !w->fault;
 case FB_BEAM_FLIGHT:case FB_BEAM_B_FLIGHT:case FB_FLAME_FLIGHT:
  r=interaction_move(w,a,WMOVE_POINT);if(r<0)return 0;
  {unsigned hit=state==FB_BEAM_FLIGHT?FB_FIRE_LEFT40:state==FB_BEAM_B_FLIGHT?FB_FIRE_LEFT48:FB_FIRE;
   if(r)enemy_schedule(q,hit);fb_animate(w,e);if(!enemy_attack(w,a))return 0;
   if((a->hit.flags&2)||entity_parent_deleting(w->entities,e))enemy_schedule(q,hit);
   if(interaction_outside(w,a,state==FB_FLAME_FLIGHT?boss_fx_bounds[8]:final_bounds[0],1))enemy_schedule(q,FB_DELETE_TREE);return !w->fault;}
 case FB_FLAME_FOLLOW:
  p=fb_parent(w,e);if(!p)return 0;if(p->actor.variant==5){enemy_schedule(q,FB_FIRE);return fb_parts_enter(w,e)==2?1:0;}
  /* $07217C copies integer XY only, preserving the child's fractional part. */
  transform_integer_offset(&a->transform,fb_word((p->actor.transform.x>>8)+q->final.offset_x-(a->transform.x>>8)),fb_word(transform_y_up(&p->actor.transform)+q->final.offset_y-transform_y_up(&a->transform)));
  if(fb_animate(w,e))enemy_schedule(q,FB_FLAME_FLIGHT);if(!enemy_attack(w,a))return 0;
  if((a->hit.flags&2)||entity_parent_deleting(w->entities,e))enemy_schedule(q,FB_FIRE);
  if(interaction_outside(w,a,boss_fx_bounds[8],1))enemy_schedule(q,FB_DELETE_TREE);return !w->fault;
 case FB_HEAD_SHARD:case FB_HEAD_SHARD_SMALL:
  r=interaction_move(w,a,WMOVE_BODY);if(r<0)return 0;if(r)enemy_schedule(q,FB_DELETE_TREE);fb_animate(w,e);
  if(interaction_outside(w,a,boss_fx_bounds[8],1))enemy_schedule(q,FB_DELETE_TREE);return !w->fault;
 default:return enemy_fault(w);
 }
}
int fb_crash_fragments(struct WeaponWorld*w,struct NativeEntity*e){
 for(unsigned i=0;i<16;i++){struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,BFX_CRASH_FRAGMENT);if(!n)return 0;n->payload.enemy.params[3]=e->payload.enemy.params[3];}return !w->fault;
}
int fb_emit(struct WeaponWorld*w,struct NativeEntity*e,unsigned callback){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*n;uint16_t random;
 switch(callback){
 case FBC_72DE8:return 1; /* Verified RTS, not camera shake. */
 case FBC_283CA:hit_begin_attack(&a->hit);return 1;
 case FBC_72DA4:return boss_fx_pattern(w,entity_ref(e),&a->transform,BP_FINAL_DEATH)&&fb_crash_fragments(w,e);
 case FBC_72D28:
  n=fb_child(w,e,FB_BEAM);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,fb_faced(a,-42),64);
  n=fb_child(w,e,FB_CASING);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,fb_faced(a,-16),22);return fb_sound(w,0x1064,0);
 case FBC_72D78:if(!fb_child(w,e,FB_BEAM_B)||!fb_child(w,e,FB_FLAME))return 0;return fb_sound(w,0x1066,0);
 case FBC_72E14:case FBC_72E80:
  n=fb_child(w,e,FB_MUZZLE);if(!n)return 0;n->payload.enemy.final.offset_x=fb_faced(a,callback==FBC_72E14?41:44);
  /* Source ADDI on the newly allocated $9C word: -1 + 30 = 29. */
  n->payload.enemy.final.offset_y=fb_word(n->payload.enemy.final.offset_y+30);random=fb_random(w);
  n=fb_child(w,e,callback==FBC_72E14?FB_CANNON:FB_CANNON_B);if(!n)return 0;
  if(callback==FBC_72E14){n->payload.enemy.params[0]=random&7;n->payload.enemy.actor.phase=a->phase;}
  else q->params[0]=(random&7)+8; /* $072EC6 writes A6, not child A0. */
  n->payload.enemy.final.offset_x=fb_faced(a,callback==FBC_72E14?37:44);n->payload.enemy.final.offset_y=68;return fb_sound(w,0x10c6,0);
 case FBC_72EE8:
  n=fb_child(w,e,FB_FLASH);if(!n)return 0;n->payload.enemy.actor.transform.facing^=1;transform_integer_offset(&n->payload.enemy.actor.transform,fb_faced(a,24),16);
  n=fb_child(w,e,FB_FLAME_FOLLOW);if(!n)return 0;n->payload.enemy.final.offset_x=fb_faced(a,32);n->payload.enemy.final.offset_y=fb_word(n->payload.enemy.final.offset_y+16);return fb_sound(w,0x10a4,0);
 case FBC_72F40:case FBC_72F64:case FBC_72F88:
  n=fb_child(w,e,FB_PUFF);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,fb_faced(a,callback==FBC_72F40?-16:callback==FBC_72F64?-12:4),callback==FBC_72F40?14:-2);return 1;
 default:return enemy_fault(w);
 }
}
