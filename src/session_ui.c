#include "session_internal.h"
enum {UI_TIMER_NEW,UI_TIMER_WARM,UI_TIMER_RUN,UI_TIMER_WAIT,UI_TIMEUP,
 UI_START_DELAY,UI_START_SPAWN,UI_START_WAIT,UI_CONTINUE_DIGIT,
 UI_CONTINUE_RELAY,UI_CONTINUE_CENTER,UI_RELAY_RETIRE,UI_CONTINUE_DROP};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
int session_text(struct WeaponWorld*w,uint16_t cell,unsigned id,uint8_t pal,int large){
 if(id>=SST_COUNT)return enemy_fault(w);const struct ResultBytes*b=&session_texts[id];
 for(unsigned i=0;i<b->count;i++){uint8_t c=b->bytes[i];if(c==255)return 1;if(!result_character(w,cell,c,pal,large))return 0;cell+=large?64:32;}return enemy_fault(w);
}
static struct NativeEntity*allocate(struct NativePlayWorld*w,struct EntityRef parent,enum EntityInsert order,unsigned state,unsigned slot){
 struct PlayerTransform t={.x=-256,.y=513*256,.priority=65535,.display_group=255};
 struct NativeEntity*e=enemy_allocate(&w->weapons,parent,order,&t,tick);
 if(e){e->payload.enemy.actor.state=state;e->payload.enemy.result.slot=slot;}return e;
}
static int phase(struct WeaponWorld*w,struct NativeEntity*e){struct NativeEntity*p=entity_get(w->entities,e->parent);if(!p){enemy_fault(w);return 0;}if(p->kind==ENTITY_GROUP)return p->payload.group.phase;if(p->kind==ENTITY_ENEMY)return p->payload.enemy.actor.phase;enemy_fault(w);return 0;}
static int time_hud(struct NativePlayWorld*w,int visible){
 uint16_t time=(uint16_t)w->enemies.mission_time;
 uint16_t a,b;
 if(time>99){a=0x63b6;b=0x63b8;}
 else if(!visible)a=b=0x4b40;
 else {a=session_time_digits[time/10];b=session_time_digits[time%10];}
 return result_rect(&w->weapons,0x243,a,2,2,1)&&result_rect(&w->weapons,0x283,b,2,2,1);
}
static int accelerate(struct NativePlayWorld*w,unsigned slot,uint16_t digit){return w->results.live_active[slot]==2&&digit<=8&&(w->results.edges[slot]&240);}
static int center_blank(struct NativePlayWorld*w,struct NativeEntity*e){if(!session_text(&w->weapons,0x16a,SST_BLANK_CENTER,4,1))return 0;entity_delete(&w->entities,e);return 1;}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*user){
 struct NativePlayWorld*w=user;struct WeaponWorld*k=&w->weapons;struct NativeSession*s=w->session;
 if(w->fault||!s)return;struct EnemyActor*q=&e->payload.enemy;unsigned slot=q->result.slot;
 if(q->entry){
  q->entry=0;
  switch(q->actor.state){
  case UI_TIMER_NEW:
   w->enemies.mission_time=session_time_settings[s->config.time_setting];if(!time_hud(w,1))return;
   q->result.clock=90;q->actor.state=UI_TIMER_WARM;return;
  case UI_TIMEUP:q->result.clock=90;if(!session_text(k,0x1af,SST_TIMEUP,4,1))return;break;
  case UI_START_DELAY:q->result.clock=15;break;
  case UI_CONTINUE_DIGIT:
   if(!session_hud_clear(w,slot)||!session_text(k,slot?0x343:0x63,SST_CONTINUE,3,0))return;
   q->result.cell=slot?0x463:0x183;q->result.cursor=9;q->result.clock=59;
   if(!result_rect(k,q->result.cell,0x3469,1,2,1))return;break;
  case UI_CONTINUE_RELAY:{
   unsigned other=w->results.live_active[slot^1];
   if(other==1||other==2){entity_delete(p,e);return;}
   q->actor.phase=q->result.ack=255;
   if(!allocate(w,entity_ref(e),ENTITY_LAST_CHILD,UI_CONTINUE_CENTER,slot)){session_fail(w);return;}break;
  }
  case UI_CONTINUE_CENTER:
   q->actor.palette=0x5f;q->private_palette_flags=8;q->actor.display_flags|=64;
   q->actor.transform.x=(int32_t)(w->weapons.camera_x+160)*256;
   q->actor.transform.y=(int32_t)(512-376+w->weapons.camera_y)*256;
   animation_begin(&q->actor.animation,SESSION_CONTINUE_INITIAL);
   q->result.cursor=9;q->result.clock=59;q->result.scale_hold=255;
   if(!session_text(k,0x16a,SST_CENTER,4,1)||!interaction_sound(k,0x10d4))return;break;
  default:break;
  }
 }
 switch(q->actor.state){
 case UI_TIMER_WARM:if(!--q->result.clock)q->actor.state=UI_TIMER_RUN;return;
 case UI_TIMER_RUN:{
  if(w->results.live_active[0]!=1&&w->results.live_active[1]!=1)return;
  if(!w->enemies.damage_enabled||w->enemies.siege_phase)return;
  for(unsigned i=0;i<2;i++)if(w->weapons.players[i]&&(w->weapons.players[i]->status_flags&8))return;
  unsigned frame=k->frame_phase;int visible=1;
  if(w->enemies.mission_time<=10&&(frame&16)){if(!interaction_sound(k,0x10d7))return;visible=0;}
  if((uint16_t)w->enemies.mission_time<100){if(frame&15)return;if(!(frame&127))w->enemies.mission_time=(int16_t)(uint16_t)(w->enemies.mission_time-1);}
  if(w->enemies.mission_time<=0){
   q->result.ack=255;if(!allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,UI_TIMEUP,0)){session_fail(w);return;}
   w->enemies.mission_time=0;visible=0;q->actor.state=UI_TIMER_WAIT;
  }
  time_hud(w,visible);return;
 }
 case UI_TIMER_WAIT:if(!q->result.ack){q->actor.state=UI_TIMER_NEW;q->entry=1;}return;
 case UI_TIMEUP:
  if((int16_t)--q->result.clock<=0){if(!session_text(k,0x1af,SST_BLANK_TIMEUP,4,1)||!result_parent_signal(k,e,0,0,0))return;entity_delete(p,e);}return;
 case UI_START_DELAY:if(!--q->result.clock)q->actor.state=UI_START_SPAWN;return;
 case UI_START_SPAWN:
  q->result.ack=255;if(!mission_banner_spawn(k,entity_ref(e),0))return;q->actor.state=UI_START_WAIT;return;
 case UI_START_WAIT:if(!q->result.ack)entity_delete(p,e);return;
 case UI_CONTINUE_DIGIT:
  if(!phase(k,e)){entity_delete(p,e);return;}
  if(accelerate(w,slot,q->result.cursor))q->result.clock=0;
  if((int16_t)--q->result.clock<=0){
   if((int16_t)--q->result.cursor<0){if(!result_parent_signal(k,e,0,0,0))return;entity_delete(p,e);return;}
   if(!result_rect(k,q->result.cell,0x3460+q->result.cursor,1,2,1))return;q->result.clock=59;
  }return;
 case UI_CONTINUE_RELAY:{
  q->actor.phase=(uint8_t)phase(k,e);unsigned other=w->results.live_active[slot^1];
  if(other!=0&&other!=3)q->actor.phase=0;
  if(!q->actor.phase||!q->result.ack)q->actor.state=UI_RELAY_RETIRE;return;
 }
 case UI_RELAY_RETIRE:entity_delete(p,e);return;
 case UI_CONTINUE_CENTER:{
  if(!phase(k,e)){center_blank(w,e);return;}
  /*469CE is OR (early return on P1 carry), not an AND of two players. */
  if(accelerate(w,0,q->result.cursor)||accelerate(w,1,q->result.cursor)){q->result.scale_hold=255;q->result.clock=0;}
  if((int16_t)--q->result.clock<=0){
   if((int16_t)--q->result.cursor<0){if(!result_parent_signal(k,e,0,0,0))return;center_blank(w,e);return;}
   if(!interaction_sound(k,0x10d4))return;q->result.clock=59;animation_begin(&q->actor.animation,session_continue_digits[q->result.cursor]);
  }
  uint8_t scale=session_continue_scale[q->result.clock];
  if(q->result.clock>55&&q->result.scale_hold)scale=255;else if(q->result.clock<=55)q->result.scale_hold=0;
  q->scale_x=scale;
  if(!scene_preserve_screen(k,&q->actor))return;
  struct AnimationOwner owner={0};animation_step(&q->actor.animation,&session_program,&owner);if(q->actor.animation.fault)session_fail(w);return;
 }
 case UI_CONTINUE_DROP:{
  struct SceneSpawnHint hint;if(!scene_runtime_spawn_hint(w->scene_runtime,&hint)){session_fail(w);return;}
  int16_t x=(int16_t)(uint16_t)(hint.x+(slot?-(int)hint.spacing:hint.spacing));
  struct PlayerTransform t={.x=(int32_t)(int16_t)(uint16_t)(x+k->camera_x)*256,
   .y=(int32_t)(int16_t)(uint16_t)(512-hint.y_up-8+k->camera_y)*256};
  /*9B872 replaces the selected pickup entry with9A8BE's counted fly-in. */
  if(!item_spawn_drop(k,&t,hint.item,hint.quantity,hint.lifetime,1,1)){session_fail(w);return;}
  entity_delete(p,e);return;
 }
 default:session_fail(w);return;
 }
}
int session_ui_timer(struct NativePlayWorld*w){return allocate(w,w->session->boot,ENTITY_FIRST_CHILD,UI_TIMER_NEW,0)!=0;}
int session_ui_start(struct NativePlayWorld*w){return allocate(w,w->session->boot,ENTITY_FIRST_CHILD,UI_START_DELAY,0)!=0;}
int session_ui_continue(struct NativePlayWorld*w,unsigned slot){
 struct EntityRef parent=w->session->controllers[slot];
 return allocate(w,parent,ENTITY_FIRST_CHILD,UI_CONTINUE_DIGIT,slot)&&allocate(w,parent,ENTITY_FIRST_CHILD,UI_CONTINUE_RELAY,slot);
}
int session_ui_continue_drop(struct NativePlayWorld*w,unsigned slot){return allocate(w,w->weapons.projectile_group,ENTITY_FIRST_CHILD,UI_CONTINUE_DROP,slot)!=0;}
