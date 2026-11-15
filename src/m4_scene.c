#include "scene_runtime.h"
#include "m4_internal.h"
static uint16_t bounded(struct WeaponWorld*w,uint16_t n){if(!w->rng){enemy_fault(w);return 0;}return source_random(w->rng,n);}
static int floor_ready(struct WeaponWorld*w,struct WeaponActor*a){int live=0;for(unsigned i=0;i<2;i++){struct PlayerBodyState*p=w->players[i];if(!enemy_player_live(p)||(p->actor.hit.flags&1))continue;live=1;if((p->status_flags&2)||m4_word(transform_y_up(&p->actor.transform)+w->camera_y)<m4_sy(w,a))return 0;}return live;}
static int pattern(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p=m4_parent(w,e);if(!p)return 0;q->scale_x=p->scale_x;q->scale_y=p->scale_y;const struct BossPatternData*t=&m4_patterns[q->m4.pattern];
 int emit=q->m4.delay==255;if(!emit&&!--q->m4.delay){q->m4.delay=t->period;emit=1;}
 if(emit){if(t->follow){int16_t dx=p->actor.transform.facing&1?m4_word(-t->dx):t->dx,dy=p->actor.transform.facing&1?m4_word(-t->dy):t->dy;transform_integer_offset(&a->transform,m4_word((p->actor.transform.x>>8)+dx-(a->transform.x>>8)),m4_word(transform_y_up(&p->actor.transform)+dy-transform_y_up(&a->transform)));}
  for(unsigned budget=0;budget<ENTITY_CAPACITY;budget++){
   struct NativeEntity*n=t->kind==2?m4_child(w,e,M4_SPLASH,ENTITY_LAST_CHILD):boss_fx_spawn(w,entity_ref(e),ENTITY_LAST_CHILD,&a->transform,t->kind==0?BFX_SMOKE:t->kind==1?BFX_FIRE:BFX_SMALL);if(!n)return 0;
   int16_t dx=bounded(w,t->spread_x),dy=bounded(w,t->spread_y);if(a->transform.facing&1)dx=m4_word(-dx);if(a->transform.facing&2)dy=m4_word(-dy);struct EnemyActor*v=&n->payload.enemy;transform_integer_offset(&v->actor.transform,dx,dy);v->actor.display_flags|=a->display_flags&64;v->scale_x=q->scale_x;v->scale_y=q->scale_y;
   if(q->m4.delay!=255)break;if(!--q->m4.remaining){entity_delete(w->entities,e);return 1;}if(budget==ENTITY_CAPACITY-1)return enemy_fault(w);
  }
 }if(!--q->m4.remaining)entity_delete(w->entities,e);return !w->fault;
}
int m4_scene_enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*n;
 switch(a->state){
 case M4_RAIL_ROW:for(unsigned i=0;i<20;i++){n=m4_child(w,e,M4_RAIL,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.m4.variant=(uint8_t)i;transform_integer_offset(&a->transform,16,0);}entity_delete(w->entities,e);return 0;
 case M4_RAIL:break;
 case M4_WATER_PAIR:for(unsigned i=0;i<2;i++){n=m4_child(w,e,M4_WATER,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.m4.variant=(uint8_t)i;}entity_delete(w->entities,e);return 0;
 case M4_WATER:m4_start(q,M4A_WATER);break;
 case M4_SPLASH:a->palette=0x1b3;m4_start(q,M4A_SPLASH);a->transform.priority=0;m4_reset(a);break;
 case M4_DEBRIS_NEW:a->palette=0x1c9;a->transform.priority=0;m4_reset(a);a->transform.facing=q->params[1]&1;a->state=M4_DEBRIS;/* FALLTHROUGH */
 case M4_DEBRIS:m4_start_entry(q,m4_debris_idle[q->params[0]&3]);break;
 case M4_DEBRIS_HIT:m4_start_entry(q,m4_debris_hit[q->params[0]&3]);break;
 case M4_FLOOR:a->phase=0;w->enemies->m4_camera_floor=1072;w->enemies->m4_camera_floor_valid=1;if(w->scene_runtime)w->scene_runtime->camera.min_y=w->scene_runtime->camera.max_y=w->enemies->m4_camera_floor;m4_screen(w,a,160,m4_sy(w,a));if(w->enemies->debug_visible&1)m4_start(q,M4A_FLOOR);/* FALLTHROUGH */
 case M4_FLOOR_STEP:
  if(a->phase>=11)return enemy_fault(w);if(m4_floor[a->phase]==-1){entity_delete(w->entities,e);return 0;}
  {int16_t x=m4_sx(w,a);if(!m4_project(w,a,0,m4_floor[a->phase]))return 0;m4_screen(w,a,x,m4_sy(w,a));}a->state=M4_FLOOR_STEP;break;
 case M4_PATTERN:if(q->m4.pattern>=7)return enemy_fault(w);q->m4.delay=m4_patterns[q->m4.pattern].period;q->m4.remaining=m4_patterns[q->m4.pattern].life;break;
 default:return enemy_fault(w);
 }return !w->fault;
}
int m4_scene_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(s==M4_PATTERN)return pattern(w,e);
 if(s==M4_FLOOR_STEP){
  struct PlayerTransform saved=a->transform;m4_screen(w,a,0,252);m4_attack(q,M4B_FLOOR);hit_begin_attack(&a->hit);hit_begin_attack(&a->hit);int ok=enemy_attack(w,a);a->transform=saved;if(!ok||interaction_move(w,a,WMOVE_SYNC)<0)return 0;if(w->enemies->debug_visible&1)m4_animate(w,e,0);
  if(floor_ready(w,a)){int16_t y=0;if(!m4_secondary_y(w,a,&y))return 0;y=m4_word(y-256);if(y<0)y=0;w->enemies->m4_camera_floor=y;w->enemies->m4_camera_floor_valid=1;if(w->scene_runtime)w->scene_runtime->camera.min_y=w->scene_runtime->camera.max_y=w->enemies->m4_camera_floor;a->phase++;enemy_schedule(q,M4_FLOOR_STEP);}return !w->fault;
 }
 if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;
 if(s==M4_RAIL){if(interaction_outside(w,a,m4_bounds[0],0)){entity_delete(w->entities,e);return 1;}if(!m4_near_box(w,a,0)){if(!m4_patch(w,2+(q->m4.variant<20?q->m4.variant:0)))return 0;entity_delete(w->entities,e);}return !w->fault;}
 int done=m4_animate(w,e,0);
 if(s==M4_SPLASH){if(done)entity_delete(w->entities,e);return !w->fault;}
 if(s==M4_WATER){
  if(interaction_outside(w,a,m4_bounds[0],0)){entity_delete(w->entities,e);return 1;}if(m4_has_children(w,e))return 1;if(q->m4.variant>1)return enemy_fault(w);struct PlayerBodyState*p=w->players[q->m4.variant];
  /* The source tests the selected slot's box/velocity without a live-slot gate. */
  if(p&&p->vx&&m4_in_box(w,a,p,m4_boxes[1])){struct NativeEntity*n=m4_child(w,e,M4_SPLASH,ENTITY_FIRST_CHILD);if(!n)return 0;struct WeaponActor*b=&n->payload.enemy.actor;transform_integer_copy(&b->transform,&p->actor.transform);transform_integer_offset(&b->transform,0,-8);b->transform.priority=0;b->transform.facing=0;b->transform.display_group=0;n->payload.enemy.source_flags=0;}return !w->fault;
 }
 if(s==M4_DEBRIS){if(m4_near_box(w,a,2))enemy_schedule(q,M4_DEBRIS_HIT);}
 else if(s==M4_DEBRIS_HIT&&done){enemy_schedule(q,m4_near_box(w,a,2)?M4_DEBRIS_HIT:M4_DEBRIS);}
 else if(s!=M4_DEBRIS_HIT)return enemy_fault(w);
 if(interaction_outside(w,a,m4_bounds[1],0))entity_delete(w->entities,e);return !w->fault;
}
