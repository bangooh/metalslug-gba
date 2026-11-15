#include "siege_internal.h"
int sg_spawn_pow(struct WeaponWorld*w,struct NativeEntity*e){
 if(sg_random(w)&7)return !w->fault;unsigned count=sg_bounded(w,2)+1;
 for(unsigned i=0;i<count;i++){
  struct NativeEntity*n=pow_spawn_entity(w,entity_ref(e),ENTITY_FIRST_CHILD,POW_FREE,&e->payload.enemy.actor.transform,0);if(!n)return 0;
  struct WeaponActor*a=&n->payload.interaction.actor;int dx=sg_bounded(w,48);if(dx&1)dx=-dx;sg_screen(w,a,sg_sx(w,a)+dx,512);
 }return !w->fault;
}
int sg_spawn_guards(struct WeaponWorld*w,struct NativeEntity*e){
 unsigned count=w->scene_number==11?1:12;
 for(unsigned i=0;i<count;i++){const struct SiegeSpawn*t=&siege_guards[i];struct NativeEntity*n=sg_child(w,e,SG_GUARD,ENTITY_LAST_CHILD);if(!n)return 0;
  struct EnemyActor*q=&n->payload.enemy;q->siege.variant=0;q->dead=t->variant;q->actor.transform.priority=t->priority;q->actor.transform.facing|=t->facing;transform_integer_offset(&q->actor.transform,t->x,t->y);
 }return 1;
}
int sg_spawn_steps(struct WeaponWorld*w,struct NativeEntity*e){
 for(unsigned i=0;i<9;i++){const struct SiegeSpawn*t=&siege_steps[i];struct NativeEntity*n=sg_child(w,e,SG_STEP,i<7?ENTITY_LAST_CHILD:ENTITY_FIRST_CHILD);if(!n)return 0;
  struct EnemyActor*q=&n->payload.enemy;q->dead=t->variant;q->actor.transform.priority=t->priority;q->actor.transform.facing|=t->facing;transform_integer_offset(&q->actor.transform,t->x,t->y);
 }return 1;
}
static struct NativeEntity*factory(struct WeaponWorld*w,struct NativeEntity*e,unsigned kind){
 static const enum BossFxKind fx[]={BFX_SMALL,BFX_FIRE,BFX_BLAST,BFX_SMOKE,BFX_FIRE_BLUE,BFX_SMOKE_BLUE,BFX_DEBRIS_AD_A,BFX_DEBRIS_AD_B,BFX_DEBRIS77_A,BFX_DEBRIS77_B};
 if(kind>=SGFCT_SMALL&&kind<=SGFCT_DEBRIS77_B)return boss_fx_spawn(w,entity_ref(e),ENTITY_LAST_CHILD,&e->payload.enemy.actor.transform,fx[kind-SGFCT_SMALL]);
 if(kind>=SGFCT_DEBRIS0&&kind<=SGFCT_DEBRIS7)return boss_fx_spawn(w,entity_ref(e),ENTITY_LAST_CHILD,&e->payload.enemy.actor.transform,(enum BossFxKind)(BFX_DEBRIS0+kind-SGFCT_DEBRIS0));
 if(kind==SGFCT_CHUNK)return sg_child(w,e,SG_CHUNK,ENTITY_LAST_CHILD);
 if(kind>=SGFCT_SHARD_A&&kind<=SGFCT_SHARD_C){struct NativeEntity*n=sg_child(w,e,SG_SHARD,ENTITY_LAST_CHILD);if(n)n->payload.enemy.siege.variant=(uint8_t)(kind-SGFCT_SHARD_A);return n;}
 if(kind==SGFCT_HUMAN_LAUNCHED||kind==SGFCT_HUMAN_CORPSE){struct NativeEntity*n=sg_child(w,e,SG_DELETE,ENTITY_LAST_CHILD);if(!n)return 0;if(kind==SGFCT_HUMAN_LAUNCHED)human_launch_direct(w,n);else human_corpse_transform(w,n,0);return n;}
 enemy_fault(w);return 0;
}
static int pattern(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p=sg_parent(w,e);if(!p)return 0;
 q->scale_x=p->scale_x;q->scale_y=p->scale_y;const struct BossPatternData*t=&siege_patterns[q->siege.pattern];
 int emit=q->siege.delay==255;if(!emit&&!--q->siege.delay){q->siege.delay=t->period;emit=1;}
 if(emit){
  if(t->follow){int16_t dx=p->actor.transform.facing&1?sg_word(-t->dx):t->dx,dy=p->actor.transform.facing&1?sg_word(-t->dy):t->dy;
   transform_integer_offset(&a->transform,sg_word((p->actor.transform.x>>8)+dx-(a->transform.x>>8)),sg_word(transform_y_up(&p->actor.transform)+dy-transform_y_up(&a->transform)));
  }
  for(unsigned budget=0;budget<ENTITY_CAPACITY;budget++){
   unsigned kind=t->random?siege_pattern_choices[q->siege.pattern][sg_bounded(w,16)]:t->kind;struct NativeEntity*n=factory(w,e,kind);if(!n)return 0;
   int16_t dx=sg_bounded(w,t->spread_x),dy=sg_bounded(w,t->spread_y);if(a->transform.facing&1)dx=sg_word(-dx);if(a->transform.facing&2)dy=sg_word(-dy);
   struct EnemyActor*v=&n->payload.enemy;transform_integer_offset(&v->actor.transform,dx,dy);v->actor.display_flags|=a->display_flags&64;v->scale_x=q->scale_x;v->scale_y=q->scale_y;
   if(q->siege.delay!=255)break;if(!--q->siege.remaining){entity_delete(w->entities,e);return 1;}if(budget==ENTITY_CAPACITY-1)return enemy_fault(w);
  }
 }if(!--q->siege.remaining)entity_delete(w->entities,e);return !w->fault;
}
int sg_effects_enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;q->entry=0;
 switch(a->state){
 case SG_PATTERN:if(q->siege.pattern>=SGP_COUNT)return enemy_fault(w);q->siege.delay=siege_patterns[q->siege.pattern].period;q->siege.remaining=siege_patterns[q->siege.pattern].life;break;
 case SG_CHUNK:a->palette=11;sg_reset(a);a->vy=sg_masked(w,511);a->ay=-16;a->vx=sg_masked(w,255);if(a->vx&1)a->vx=sg_word(-a->vx);q->siege.timer=40;sg_start(q,SGA_CHUNK);break;
 case SG_FLYING:
  a->palette=0xad;sg_reset(a);p=sg_parent(w,e);if(!p)return 0;
  a->transform.x=(int32_t)p->actor.hit.impact_x*256;a->transform.y=(512-(int32_t)p->actor.hit.impact_y)*256;
  a->vy=3072;a->vx=w->frame_phase&1?-256:256;
  if(p->actor.hit.last_kind!=4){
   if(p->actor.hit.last_attacker>=ENTITY_CAPACITY)return enemy_fault(w);struct NativeEntity*source=&w->entities->nodes[p->actor.hit.last_attacker];if(!source->active)return enemy_fault(w);
   if(source->kind==ENTITY_PLAYER){a->vx=sg_word(source->payload.player.vx*2);a->vy=sg_word(source->payload.player.vy*2);}
   else{struct WeaponActor*s=entity_weapon_actor(source);if(!s)return enemy_fault(w);a->vx=sg_word(s->vx*2);a->vy=sg_word(s->vy*2);}
  }a->ay=sg_word(-((sg_masked(w,511)|256)&511));sg_start_entry(q,siege_flying[sg_masked(w,3)]);break;
 case SG_SHARD:
  if(q->siege.variant>=3)return enemy_fault(w);sg_start(q,(enum SiegeAnimation)(SGA_SHARD_A+q->siege.variant));a->vy=sg_word(-sg_masked(w,31));a->ay=sg_word(sg_masked(w,31)-48);q->params[0]&=1;a->transform.facing=q->params[0];a->palette=0xad;
  {struct NativeEntity*parent=entity_get(w->entities,e->parent);if(!parent)return enemy_fault(w);p=sg_parent(w,parent);if(!p)return 0;q->siege.ground=sg_sy(w,&p->actor);}break;
 case SG_SHARD_LAND:/* All three original comparisons test zero. */if(!q->siege.variant)sg_start(q,SGA_SHARD_LAND);break;
 case SG_NUKE:sg_attack(q,SGB_NUKE);sg_screen(w,a,0,256);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;w->enemies->clear_ordnance=1;sg_schedule(e,SG_NUKE_END);return 0;
 case SG_NUKE_END:w->enemies->clear_ordnance=0;sg_attack(q,65535);entity_delete(w->entities,e);return 0;
 default:return enemy_fault(w);
 }return !w->fault;
}
int sg_effects_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;int r;
 if(s==SG_PATTERN)return pattern(w,e);
 if(s==SG_CHUNK){if(interaction_move(w,a,WMOVE_POINT)<0)return 0;if(q->siege.timer==10)a->damage_timer=20;sg_animate(w,e);if(interaction_outside(w,a,siege_chunk_bounds,1)||!--q->siege.timer)entity_delete(w->entities,e);return !w->fault;}
 if(s==SG_FLYING){
  r=interaction_move(w,a,WMOVE_FALL);if(r<0)return 0;if(r){a->vy=sg_word(-a->vy)>>1;if(!boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMALL))return 0;if(a->vy<256){entity_delete(w->entities,e);return 1;}}
  sg_animate(w,e);if(interaction_outside(w,a,boss_fx_bounds[8],1))entity_delete(w->entities,e);return !w->fault;
 }
 if(s==SG_SHARD||s==SG_SHARD_LAND){
  if(s==SG_SHARD){a->transform.x=(int32_t)((uint32_t)a->transform.x+(int32_t)a->vx);a->transform.y=(int32_t)((uint32_t)a->transform.y-(int32_t)a->vy);a->vx=sg_word(a->vx+a->ax);a->vy=sg_word(a->vy+a->ay);if(q->siege.ground>=sg_sy(w,a))sg_schedule(e,SG_SHARD_LAND);}
  if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;
  if(interaction_outside(w,a,boss_fx_bounds[8],0)){entity_delete_children(w->entities,e);entity_delete(w->entities,e);return !w->fault;}
  r=sg_animate(w,e);if(s==SG_SHARD_LAND&&r)entity_delete(w->entities,e);return !w->fault;
 }
 return enemy_fault(w);
}
