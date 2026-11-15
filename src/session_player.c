#include "session_internal.h"
/*2575C/25766 and25780..25E90. Body movement remains in its later fixed
 * slot; this controller owns lives/joins and never calls body_tick twice. */
static void schedule(struct NativePlayerControl*c,unsigned state){c->state=state;c->entry=1;}
static int body_idle(struct NativePlayWorld*w,unsigned slot){struct PlayerBodyState*p=w->weapons.players[slot];return !p||p->life_retired;}
static int prompt(struct NativePlayWorld*w,unsigned slot,unsigned text){
 /* CONNECT CABLE is wider and P2 starts one column earlier than the
  * source prompts. Replace both glyph rows before changing the message. */
 if(!result_rect(&w->weapons,slot?0x2c3:0x43,0x300,16,2,0))return 0;
 uint16_t cell=slot?0x343:0x63;
 if(text==SST_START){
  /* Thirteen glyphs must fit inside the P2 clear span (columns 22..37). */
  if(slot)cell-=32;
  static const char cable[]="CONNECT CABLE";
  for(unsigned i=0;i<sizeof(cable)-1;i++,cell+=32)
   if(!result_character(&w->weapons,cell,cable[i],3,0))return 0;
  return 1;
 }
 return session_text(&w->weapons,cell,text,3,0);
}
static int spawn_body(struct NativePlayWorld*w,unsigned slot,int reset_lives){
 struct NativeSession*s=w->session;struct NativePlayerControl*c=&s->player[slot];
 if(reset_lives)c->lives=s->config.initial_lives;
 if(w->terrain.scene<9&&!play_world_spawn_player_from_scene(w,slot))return 0;
 uint16_t time=session_time_settings[s->config.time_setting];
 if(time>=(uint16_t)w->enemies.mission_time)w->enemies.mission_time=(int16_t)time;
 return session_hud_player(w,slot,1);
}
static int highscore_begin(struct NativePlayWorld*w,struct NativeEntity*e,unsigned slot){
 w->results.live_active[slot]=1;e->payload.group.phase=e->payload.group.command=255;
 w->session->player[slot].state=PC_HIGHSCORE;
 return results_highscore_enter(&w->weapons,e,slot,slot?0x323:0x63);
}
static void live_frame(struct NativePlayWorld*w,unsigned slot){
 struct NativePlayerControl*c=&w->session->player[slot];struct PlayerBodyState*p=w->weapons.players[slot];c->state=PC_LIVE;
 if(!w->enemies.scripted_player_lock){schedule(c,PC_FINISH);return;}
 if(!p){session_fail(w);return;}
 if(!w->enemies.damage_enabled){p->actor.hit.invulnerable|=60;p->actor.hit.flags&=(uint8_t)~9;}
 if(p->actor.hit.flags&1){
  w->interactions.rescued[slot]=0;w->session->transition_hold=255;
  if(w->control_mode[slot]==2){
   unsigned count=0;for(unsigned i=0;i<2;i++){
    struct NativeEntity*e=entity_get(&w->entities,w->hits.players[i]);
    if(e&&!e->deleting&&enemy_player_live(w->weapons.players[i]))count++;
   }
   w->session->frame_hold=count==1?30:0;
  }
  schedule(c,PC_WAIT_TIME);
 }
 session_hud_player(w,slot,0);
}
void session_player_tick(struct EntityPool*pool,struct NativeEntity*e,void*user){
 (void)pool;struct NativePlayWorld*w=user;struct NativeSession*s=w->session;
 if(w->fault||!s)return;
 unsigned slot=e->index==s->controllers[0].index?0:1;
 if(e->index!=s->controllers[slot].index){session_fail(w);return;}
 struct NativePlayerControl*c=&s->player[slot];
 if(c->reset_timer)c->reset_timer--;
 if(c->entry){
  c->entry=0;
  switch(c->state){
  case PC_INIT:w->items.scores[slot]=0;schedule(c,w->results.live_active[slot]==1?PC_NEW:PC_IDLE);return;
  case PC_IDLE:case PC_READY:if(!prompt(w,slot,c->state==PC_IDLE?SST_INSERT:SST_START))return;break;
  case PC_SUSPEND:if(!prompt(w,slot,SST_WAIT))return;break;
  case PC_NEW:if(!spawn_body(w,slot,1)){session_fail(w);return;}c->state=PC_LIVE;break;
  case PC_CONTINUE_BEGIN:
   if(!session_hud_clear(w,slot))return;
   if(!s->config.allow_continue){if(!highscore_begin(w,e,slot))return;break;}
   if(!session_ui_continue_drop(w,slot)){session_fail(w);return;}
   w->results.live_active[slot]=2;
   /* fall through */
  case PC_CONTINUE_RESTART:
   e->payload.group.phase=e->payload.group.command=255;c->credit_ready=session_credit_ready(s);
   if(!session_ui_continue(w,slot)){session_fail(w);return;}c->state=PC_CONTINUE;break;
  case PC_HIGHSCORE_BEGIN:
   if(!highscore_begin(w,e,slot))return;break;
  case PC_RESET_BEGIN:
   if(!prompt(w,slot,SST_OVER))return;w->interactions.rescued[slot]=0;
   c->reset_timer=30;c->state=PC_RESET;break;
  default:break;
  }
 }
 switch(c->state){
 case PC_STOP:return;
 case PC_IDLE:case PC_READY:{
  unsigned old=c->state;
  if(!session_input_allowed(w)){
   c->saved_state=old;c->saved_join_blocked=c->join_blocked;c->join_blocked=1;schedule(c,PC_SUSPEND);
  }
  unsigned phase=w->weapons.frame_phase&31;
  if(!phase&&!prompt(w,slot,old==PC_IDLE?SST_INSERT:SST_START))return;
  if(phase==24&&!session_hud_clear(w,slot))return;
  if(w->results.live_active[slot]==1){if(old==PC_READY)w->items.scores[slot]=0;e->payload.group.phase=0;schedule(c,PC_NEW);return;}
  if(old==PC_IDLE&&session_credit_ready(s))schedule(c,PC_READY);
  else if(old==PC_READY&&!session_credit_ready(s))schedule(c,PC_IDLE);
  return;
 }
 case PC_SUSPEND:
  if(session_input_allowed(w)){c->join_blocked=c->saved_join_blocked;schedule(c,c->saved_state);}return;
 case PC_LIVE:
  live_frame(w,slot);return;
 case PC_WAIT_TIME:
  s->transition_hold=255;if(!w->enemies.mission_time)return;
  /*25964 falls through2596A in the same frame. */
  c->state=PC_WAIT_BODY;
  /* fall through */
 case PC_WAIT_BODY:
  s->transition_hold=255;if(!body_idle(w,slot))return;
  c->lives--;
  if(c->lives){if(!spawn_body(w,slot,0)){session_fail(w);return;}live_frame(w,slot);}
  else schedule(c,PC_CONTINUE_BEGIN);
  return;
 case PC_FINISH:
  if(!session_hud_player(w,slot,-1))return;
  if(!body_idle(w,slot)||!w->enemies.scripted_player_lock||!session_input_allowed(w))return;
  if(!spawn_body(w,slot,0)){session_fail(w);return;}live_frame(w,slot);return;
 case PC_CONTINUE:
  s->transition_hold=255;
  if(w->results.live_active[slot]==1){
   e->payload.group.phase=0;uint32_t score=w->items.scores[slot];if(score%10<9)w->items.scores[slot]++;
   results_continue(&w->results,slot);schedule(c,PC_NEW);return;
  }
  if(!c->credit_ready&&session_credit_ready(s)){
   e->payload.group.phase=0;schedule(c,PC_CONTINUE_RESTART);return;
  }
  if(!e->payload.group.command){w->results.live_active[slot]=1;schedule(c,PC_HIGHSCORE_BEGIN);}return;
 case PC_HIGHSCORE:
  s->transition_hold=255;
  if(e->payload.group.command)return;
  if(!session_hud_clear(w,slot))return;
  e->payload.group.phase=0;w->results.live_active[slot]=3;
  schedule(c,w->results.progress<6?PC_RESET_BEGIN:PC_STOP);return;
 case PC_RESET:
  if(c->reset_timer||!session_input_allowed(w))return;
  w->results.live_active[slot]=0;schedule(c,session_credit_ready(s)?PC_READY:PC_IDLE);return;
 default:session_fail(w);return;
 }
}
