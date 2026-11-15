#include "airship_internal.h"
static struct NativeEntity*factory(struct WeaponWorld*w,struct NativeEntity*e,unsigned kind){
 if(kind>=ASFX_LOCAL0&&kind<=ASFX_LOCAL2){struct NativeEntity*n=as_child(w,e,AS_DEBRIS,ENTITY_LAST_CHILD,0,0);if(n)n->payload.enemy.airship.variant=(uint8_t)(kind-ASFX_LOCAL0);return n;}
 static const enum BossFxKind kinds[]={BFX_FIRE,BFX_SMOKE,BFX_DEBRIS1,BFX_DEBRIS2,BFX_DEBRIS3,BFX_DEBRIS4,BFX_DEBRIS5,BFX_DEBRIS9,BFX_DEBRIS10};
 if(kind>=sizeof(kinds)/sizeof(*kinds)){enemy_fault(w);return 0;}return boss_fx_spawn(w,entity_ref(e),ENTITY_LAST_CHILD,&e->payload.enemy.actor.transform,kinds[kind]);
}
static int pattern(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p=as_parent(w,e);if(!p)return 0;
 q->scale_x=p->scale_x;q->scale_y=p->scale_y;const struct BossPatternData*t=&airship_patterns[q->airship.pattern];
 int emit=q->airship.delay==255;if(!emit&&!--q->airship.delay){q->airship.delay=t->period;emit=1;}
 if(emit){
  if(t->follow){int16_t dx=p->actor.transform.facing&1?as_word(-t->dx):t->dx,dy=p->actor.transform.facing&1?as_word(-t->dy):t->dy;
   transform_integer_offset(&a->transform,as_word((p->actor.transform.x>>8)+dx-(a->transform.x>>8)),as_word(transform_y_up(&p->actor.transform)+dy-transform_y_up(&a->transform)));
  }
  for(unsigned budget=0;budget<ENTITY_CAPACITY;budget++){
   unsigned kind=t->random?airship_pattern_choices[q->airship.pattern][as_bounded(w,16)]:t->kind;struct NativeEntity*n=factory(w,e,kind);if(!n)return 0;
   int16_t dx=as_bounded(w,t->spread_x),dy=as_bounded(w,t->spread_y);if(a->transform.facing&1)dx=as_word(-dx);if(a->transform.facing&2)dy=as_word(-dy);
   struct EnemyActor*v=&n->payload.enemy;transform_integer_offset(&v->actor.transform,dx,dy);v->actor.display_flags|=a->display_flags&64;v->scale_x=q->scale_x;v->scale_y=q->scale_y;
   if(q->airship.delay!=255)break;if(!--q->airship.remaining){entity_delete(w->entities,e);return 1;}if(budget==ENTITY_CAPACITY-1)return enemy_fault(w);
  }
 }if(!--q->airship.remaining)entity_delete(w->entities,e);return !w->fault;
}
static void param_velocity(struct EnemyActor*q){q->params[0]&=1;q->actor.transform.facing=q->params[0];q->actor.vx=as_word(q->params[1]*32);if(!q->params[0])q->actor.vx=as_word(-q->actor.vx);}
static int ricochet(struct WeaponWorld*w,struct NativeEntity*e){
 struct WeaponActor*a=&e->payload.enemy.actor;if(a->hit.last_attacker>=ENTITY_CAPACITY)return enemy_fault(w);
 struct NativeEntity*source=&w->entities->nodes[a->hit.last_attacker];struct WeaponActor*owner=entity_weapon_actor(source);if(!source->active||!owner)return enemy_fault(w);
 struct NativeEntity*n=as_child(w,e,AS_RICOCHET,ENTITY_FIRST_CHILD,0,0);if(!n)return 0;struct WeaponActor*b=&n->payload.enemy.actor;
 transform_integer_copy(&b->transform,&owner->transform);b->transform.priority=owner->transform.priority;b->transform.facing=w->frame_phase&1;b->ground_slope=owner->ground_slope;b->animation.pose=owner->animation.pose;b->owner=owner->owner;b->damage_owner=owner->damage_owner;return 1;
}
int as_effects_enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned s=a->state,v=q->airship.variant;
 switch(s){
 case AS_PATTERN:if(q->airship.pattern>=ASP_COUNT)return enemy_fault(w);q->airship.delay=airship_patterns[q->airship.pattern].period;q->airship.remaining=airship_patterns[q->airship.pattern].life;break;
 case AS_DEBRIS:e->cosmetic|=ENTITY_COSMETIC_PRESSURE;
  if(v>=3)return enemy_fault(w);as_start(q,(enum AirshipAnimation)(ASA_DEBRIS0+v));a->palette=10;q->scale_x=q->scale_y=255;a->transform.facing|=as_random(w)&1;a->vy=2040;a->ay=-68;a->ax=0;a->vx=(int16_t)as_bounded(w,1023);if(a->vx&1)a->vx=as_word(-a->vx);
  {uint16_t r=as_bounded(w,2047);a->vy=as_word(a->vy-r);a->transform.facing|=r&1;}
  if(interaction_outside(w,a,airship_bounds[9+v],1)){entity_delete(w->entities,e);return 0;}as_defense(q,65535);break;
 case AS_IMPACT:e->cosmetic|=ENTITY_COSMETIC_PRESSURE;a->transform.priority=0x2000;a->palette=0x6d;as_reset(a);as_start(q,ASA_IMPACT);break;
 case AS_SPLASH:e->cosmetic|=ENTITY_COSMETIC_PRESSURE;a->palette=0x139;as_start(q,ASA_SPLASH);q->scale_x=q->scale_y=255;a->transform.facing|=as_random(w)&1;a->display_flags|=64;if(interaction_outside(w,a,airship_bounds[8],1)){entity_delete(w->entities,e);return 0;}as_defense(q,65535);break;
 case AS_SPARK_WAIT:break;
 case AS_SPARK:e->cosmetic|=ENTITY_COSMETIC_PRESSURE;a->palette=0x139;q->scale_x=q->scale_y=255;as_start(q,ASA_SPARK);break;
 case AS_THROWN:a->palette=q->params[4]?0xd8:0x162;q->scale_x=q->scale_y=255;as_reset(a);param_velocity(q);if(q->params[2]==1){a->display_flags|=64;a->transform.priority=65535;}else a->transform.priority=0x8000;if(q->params[5]>=3)return enemy_fault(w);as_start_entry(q,airship_thrown[q->params[5]]);break;
 case AS_DROP:
  if(v>=4||!w->secondary_view_valid)return enemy_fault(w);as_start(q,(enum AirshipAnimation)(ASA_DROP0+v));q->scale_x=q->scale_y=q->params[2];as_reset(a);param_velocity(q);
  a->transform.priority=q->params[3]?0x1fff:0;a->palette=q->params[3]?(w->secondary_x_offset>1648?0xce:0x16e):(w->secondary_x_offset>1648?0x16d:0x16f);break;
 case AS_RICOCHET:
  a->vx=(as_random(w)&0x700)+0x300;if(a->transform.facing&1)a->vx=as_word(-a->vx);a->vy=(as_random(w)&0x700)+0x500;a->hit.type_flags|=16;
  if(!interaction_sound(w,0x108e))return 0;as_start(q,ASA_RICOCHET);as_defense(q,65535);as_attack(q,ASB_RICOCHET);a->palette=2;q->airship.timer=0;hit_begin_attack(&a->hit);interaction_priority(w,a,0xd000,20);break;
 case AS_RICOCHET_IMPACT:{a->palette=4;int16_t r=(as_random(w)&15)-8;transform_integer_offset(&a->transform,r,0);as_start_entry(q,airship_ricochet_impact[r&1]);break;}
 default:return enemy_fault(w);
 }return !w->fault;
}
int as_effects_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(s==AS_PATTERN)return pattern(w,e);
 if(s==AS_SPARK_WAIT){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;q->airship.timer=as_word(q->airship.timer-1);if(q->airship.timer<=0){as_schedule(e,AS_SPARK);return 2;}return 1;}
 if(s==AS_RICOCHET_IMPACT){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;if(as_animate(w,e)){entity_delete_children(w->entities,e);entity_delete(w->entities,e);}return !w->fault;}
 if(s==AS_RICOCHET){
  int r=interaction_move(w,a,WMOVE_POINT);if(r<0)return 0;if(r)as_schedule(e,AS_RICOCHET_IMPACT);as_animate(w,e);q->airship.timer=as_word(q->airship.timer+1);if(q->airship.timer>4&&!enemy_attack(w,a))return 0;if(a->hit.flags&2)as_schedule(e,AS_RICOCHET_IMPACT);if(interaction_outside(w,a,boss_fx_bounds[8],1))as_schedule(e,AS_DELETE_TREE);return !w->fault;
 }
 if(s==AS_THROWN||s==AS_DROP){
  if(interaction_move(w,a,WMOVE_POINT)<0)return 0;as_animate(w,e);
  if(s==AS_THROWN){if(!q->params[3])as_defense(q,65535);
   if(a->hit.flags&8){if(!as_hurt(w,a))return 0;a->hit.flags&=247;if(a->hit.last_kind==1){a->hit.hp=as_word(a->hit.hp+10);a->hit.flags&=254;if((w->frame_phase&1)&&!ricochet(w,e))return 0;}}
   if(q->params[3]){hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;}
  }
  if(interaction_outside(w,a,airship_bounds[s==AS_THROWN?2:3+q->airship.variant],0))entity_delete(w->entities,e);return !w->fault;
 }
 if(s==AS_IMPACT){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;int done=as_animate(w,e);if(done||interaction_outside(w,a,boss_fx_bounds[8],1))as_schedule(e,AS_DELETE_TREE);return !w->fault;}
 if(s==AS_DEBRIS){if(a->vy<=-1280)a->vy=-1279;if(interaction_move(w,a,WMOVE_POINT)<0)return 0;int done=0;if(boss_sprite_admitted(w))done=as_animate(w,e);if(done||interaction_outside(w,a,airship_bounds[9+q->airship.variant],1))entity_delete(w->entities,e);return !w->fault;}
 int done=as_animate(w,e);if(done||interaction_outside(w,a,airship_bounds[s==AS_SPARK?0:8],1))entity_delete(w->entities,e);return !w->fault;
}
