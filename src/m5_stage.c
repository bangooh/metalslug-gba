#include "m5_internal.h"
#include "scene_runtime.h"
/* $0682D0..$068B9C: arena gate, moving deck, lights and five hatch states.
 * Camera relinks are explicit scene outputs; platforms enter the shared
 * terrain ring used by players, vehicles and enemy probes. */
enum State {GATE,GATE_WAIT,GATE_BEGIN,GATE_HOLD,GATE_RELEASE,DECK,PART,PART_IDLE,
 PART_HOLD,PART_OPEN,PART_SWAY,PART_CLOSE,LIGHT,LIGHT_ON,HULL,NOSE,NOSE_DRAW,
 HATCH_INIT,HATCH,HATCH_OPEN,HATCH_ANIM,HATCH_CLOSE,HATCH_FINAL,DELETE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static struct NativeEntity*spawn(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct NativeEntity*n=enemy_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,tick);if(!n)return 0;
 uint8_t*b=(uint8_t*)&n->payload.enemy.m5;for(unsigned i=0;i<sizeof(n->payload.enemy.m5);i++)b[i]=255;enemy_schedule(&n->payload.enemy,state);return n;
}
static int active(struct WeaponWorld*w){return w->enemies->m5_scene.active&&w->enemies->m5_scene.scroll_x_q16;}
static void scroll(struct WeaponWorld*w){struct M5SceneState*s=&w->enemies->m5_scene;w->scroll_seed_latch=s->active;if(w->scene_runtime){struct SceneCamera*c=&w->scene_runtime->camera;c->seed_latch=s->active;c->seed_x=s->scroll_x_q16;c->seed_y=s->scroll_y_q16;}}
static int relink(struct WeaponWorld*w,struct EnemyActor*q,int wrap){
 struct M5SceneState*s=&w->enemies->m5_scene;if(m5_word(q->m5.target_x-8)>s->secondary_x)return 0;
 s->relink_x=m5_word(s->secondary_x-q->m5.target_x-320);s->relink_y=m5_word(s->secondary_y-16);s->map_linked=1;s->relink_generation++;
 if(w->scene_runtime){if(!scene_runtime_m5_relink(w->scene_runtime,w,q->m5.target_x,wrap))return enemy_fault(w);}else if(wrap)s->secondary_x=m5_word(s->secondary_x-320);return 1;
}
static void platform(struct WeaponWorld*w,struct WeaponActor*a,int dx,int dy,int width){
 struct TerrainMotion m={.platform_ids=a->platform_ids};
 if(!terrain_platform_publish(w->interactions->terrain,&m,m5_word((a->transform.x>>8)+dx),m5_word(transform_y_up(&a->transform)+dy),(int16_t)width,0))enemy_fault(w);a->platform_ids=m.platform_ids;
}
static int follow(struct WeaponWorld*w,struct NativeEntity*e,int grand){
 struct EnemyActor*q=&e->payload.enemy,*p=m5_parent(w,e);if(!p)return 0;m5_follow(q,p,1,1,q->m5.target_x,q->m5.target_y,q->m5.threshold);
 if(grand){struct NativeEntity*pe=entity_get(w->entities,e->parent);struct EnemyActor*g=m5_parent(w,pe);if(!g)return 0;
  transform_integer_offset(&q->actor.transform,0,m5_word(transform_y_up(&g->actor.transform)+q->m5.target_y-transform_y_up(&q->actor.transform)));}
 return 1;
}
static void animate(struct WeaponWorld*w,struct NativeEntity*e){if(m5_sx(w,&e->payload.enemy.actor)<=416)m5_animate(w,e);}
static int settle(struct WeaponWorld*w,struct WeaponActor*a){
 int16_t oldx=m5_sx(w,a),oldy=m5_sy(w,a)&1023;m5_screen(w,a,oldx,oldy);
 for(unsigned budget=0;budget<2048;budget++){
  if(m5_sy(w,a)<0){m5_screen(w,a,160,384);return 1;}
  int r=interaction_move(w,a,WMOVE_ITEM);if(r<0)return 0;if(r)return 1;
 }return enemy_fault(w);
}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned state=a->state;q->entry=0;struct EnemyActor*p;
 switch(state){
 case GATE:q->scale=(uint16_t)m5_rank_value(w,q,M5R_GATE);q->m5.target_x=0xec0;break;
 case GATE_WAIT:w->enemies->siege_phase=w->enemies->m5_scene.scroll_mode=4;q->m5.target_x=0x1000;break;
 case GATE_BEGIN:
  if(!m5_sound(w,0x1032))return 0;p=m5_parent(w,e);if(!p)return 0;p->dead=1;w->enemies->m5_scene.active=1;w->enemies->m5_scene.scroll_x_q16=w->enemies->m5_scene.scroll_y_q16=0;q->m5.target_x=0x1000;break;
 case GATE_HOLD:w->enemies->m5_scene.scroll_x_q16=0x60000;break;
 case GATE_RELEASE:break;
 case DECK:{
  struct M5SceneState*s=&w->enemies->m5_scene;
  m5_screen(w,a,m5_word(0xf00-s->secondary_x),m5_word(512-0xf8+s->secondary_y));if(!settle(w,a))return 0;transform_integer_offset(&a->transform,304,0);
  struct NativeEntity*n=spawn(w,e,NOSE);if(!n)return 0;n->payload.enemy.m5.target_x=0;
  for(unsigned i=0;i<3;i++){n=spawn(w,e,PART);if(!n)return 0;n->payload.enemy.m5.target_x=(int16_t)(112*i);}
  interaction_priority(w,a,0xc000,8);break;
 }
 case PART:
  if(!follow(w,e,0))return 0;for(unsigned i=0;i<3;i++)if(!spawn(w,e,i==0?LIGHT:i==1?HULL:HATCH_INIT))return 0;
  a->palette=0x1b7;q->m5.target_y=q->m5.threshold=0;a->display_flags|=64;a->hit.hp=32767;m5_defense(q,M5_STAGE_DEFENSE);a->state=PART_IDLE;/* FALLTHROUGH */
 case PART_IDLE:a->phase=0;m5_start(q,M5A_ST_PART);break;
 case PART_HOLD:q->m5.timer=150;m5_start(q,M5A_ST_HOLD);break;
 case PART_OPEN:a->phase=1;break;
 case PART_SWAY:m5_start(q,M5A_ST_SWAY);break;
 case PART_CLOSE:a->phase=3;break;
 case LIGHT:q->m5.target_y=q->m5.threshold=0;a->palette=0x1b7;a->display_flags|=64;m5_start(q,M5A_ST_LIGHT);break;
 case LIGHT_ON:m5_start(q,M5A_ST_LIGHT_ON);break;
 case HULL:case NOSE:
  q->m5.target_y=q->m5.threshold=0;a->palette=0x1b7;a->display_flags|=64;m5_start(q,state==HULL?M5A_ST_HULL:M5A_ST_NOSE);break;
 case NOSE_DRAW:break;
 case HATCH_INIT:q->m5.segment=0;q->m5.threshold=-1;a->palette=0x1b7;a->state=HATCH;/* FALLTHROUGH */
 case HATCH:q->m5.target_y=8;m5_start(q,M5A_ST_HATCH);break;
 case HATCH_OPEN:if(!m5_sound(w,0x10a8))return 0;m5_start(q,M5A_ST_HATCH);break;
 case HATCH_ANIM:
  p=m5_parent(w,e);if(!p)return 0;p->actor.phase=2;q->m5.target_y=m5_word(q->m5.target_y-1);q->m5.segment=a->animation.pose&15;
  if(q->m5.segment>=6)return enemy_fault(w);m5_start_entry(q,m5_st_open[q->m5.segment]);break;
 case HATCH_CLOSE:case HATCH_FINAL:if((uint16_t)q->m5.segment>=6)return enemy_fault(w);m5_start_entry(q,m5_st_close[q->m5.segment]);break;
 case DELETE:entity_delete_children(w->entities,e);entity_delete(w->entities,e);return 2;
 default:return enemy_fault(w);
 }return !w->fault;
}
static void tick(struct EntityPool*pool,struct NativeEntity*e,void*world){
 (void)pool;struct WeaponWorld*w=world;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct M5SceneState*s=&w->enemies->m5_scene;
 if(w->fault)return;if(!s->valid){enemy_fault(w);return;}if(q->entry&&enter(w,e)!=1)return;unsigned state=a->state;struct EnemyActor*p;
 switch(state){
 case GATE:if(s->secondary_x>=0xe20)w->enemies->siege_phase=s->scroll_mode=4;if(relink(w,q,0))enemy_schedule(q,GATE_WAIT);break;
 case GATE_WAIT:if(s->secondary_x>=0x1000)enemy_schedule(q,GATE_BEGIN);if(relink(w,q,1))enemy_schedule(q,GATE_BEGIN);break;
 case GATE_BEGIN:s->scroll_x_q16+=0x8000;if(s->scroll_x_q16>=0x60000)enemy_schedule(q,GATE_HOLD);relink(w,q,1);break;
 case GATE_HOLD:relink(w,q,1);if(s->release)enemy_schedule(q,GATE_RELEASE);break;
 case GATE_RELEASE:s->scroll_x_q16-=0x2000;if(s->scroll_x_q16<=0){s->scroll_x_q16=s->scroll_y_q16=0;scroll(w);relink(w,q,1);entity_delete(w->entities,e);return;}relink(w,q,1);break;
 case DECK:{
  if(active(w)&&!scene_preserve_screen(w,a))return;interaction_move(w,a,WMOVE_SYNC);int offset=active(w)&&((m5_random(w)&3)!=0)?-1:0;platform(w,a,-56,offset+30,320);
  int16_t x=m5_sx(w,a);if(s->active&&x>48)transform_integer_offset(&a->transform,-1,0);if(m5_sx(w,a)<=48)m5_screen(w,a,48,m5_sy(w,a));break;
 }
 case PART_IDLE:case PART_HOLD:case PART_OPEN:case PART_SWAY:case PART_CLOSE:
  if(active(w))q->m5.target_y=m5_random(w)&3?-1:0;if(!follow(w,e,0))return;animate(w,e);
  if(state==PART_IDLE){struct NativeEntity*pe=entity_get(w->entities,e->parent);p=m5_parent(w,pe);if(!p)return;if(!(p->actor.hit.flags&1)&&active(w)&&!(w->frame_phase&31)&&!(m5_random(w)&3))enemy_schedule(q,PART_HOLD);}
  else if(state==PART_HOLD){q->m5.timer=m5_word(q->m5.timer-1);if(q->m5.timer<=0)enemy_schedule(q,PART_OPEN);}
  else if(state==PART_OPEN){if(a->phase!=1)enemy_schedule(q,PART_SWAY);}
  else if(state==PART_SWAY){if(!(w->frame_phase&31)&&!(m5_random(w)&7))enemy_schedule(q,PART_CLOSE);}
  else if(a->phase!=3)enemy_schedule(q,PART_IDLE);
  a->hit.flags&=246;if(state!=PART_IDLE&&!w->enemies->damage_enabled)enemy_schedule(q,state==PART_HOLD||state==PART_OPEN?PART_CLOSE:PART_IDLE);break;
 case LIGHT:case LIGHT_ON:
  if(state==LIGHT&&active(w)){m5_start(q,M5A_ST_LIGHT_ON);a->state=LIGHT_ON;}else if(state==LIGHT_ON&&!active(w))enemy_schedule(q,LIGHT);
  if(!follow(w,e,1))return;animate(w,e);break;
 case HULL:case NOSE:case NOSE_DRAW:
  if(state==NOSE&&m5_sx(w,a)<=384){interaction_move(w,a,WMOVE_SYNC);s->nose_x=m5_word(a->transform.x>>8);s->nose_y=m5_word(a->transform.y>>8);s->nose_patch=1;const struct M5TerrainStamp*t=&m5_nose_stamp;if(!terrain_stamp(w->interactions->terrain,&a->transform,t->dx,t->dy,t->width,t->height,t->cells)){enemy_fault(w);return;}a->state=NOSE_DRAW;}
  if(!follow(w,e,0))return;animate(w,e);break;
 case HATCH:case HATCH_OPEN:case HATCH_ANIM:case HATCH_CLOSE:case HATCH_FINAL:
  if(!follow(w,e,0))return;if(state!=HATCH_FINAL||q->m5.target_y>=0){interaction_priority(w,a,0x4000,8);platform(w,a,-32,31,64);}animate(w,e);p=m5_parent(w,e);if(!p)return;
  if(state==HATCH&&p->actor.phase==1)enemy_schedule(q,HATCH_OPEN);
  if(state==HATCH_OPEN){q->m5.target_y=m5_word(q->m5.target_y+1);if(q->m5.target_y==40)enemy_schedule(q,HATCH_ANIM);}
  if(state==HATCH_ANIM&&p->actor.phase==3)enemy_schedule(q,HATCH_CLOSE);
  if(state==HATCH_CLOSE){q->m5.target_y=m5_word(q->m5.target_y-1);if(q->m5.target_y==8){p->actor.phase=4;enemy_schedule(q,HATCH);}}
  if(state==HATCH_FINAL){q->m5.target_y=m5_word(q->m5.target_y-1);if(q->m5.target_y<=-8)enemy_schedule(q,DELETE);}
  else if(!w->enemies->damage_enabled)enemy_schedule(q,HATCH_FINAL);break;
 default:enemy_fault(w);return;
 }
 if(state==GATE_BEGIN||state==GATE_HOLD||state==GATE_RELEASE)scroll(w);
}
int m5_stage_spawn(struct WeaponWorld*w,struct NativeEntity*e){return spawn(w,e,GATE)&&spawn(w,e,DECK);}
