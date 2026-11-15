#include "player_body.h"
static struct PlayerBodyState*body(struct PlayerBodyContext*c){return c->air.actions->body;}
static int held(struct PlayerBodyContext*c,unsigned key){return !!(c->air.input.held&key);}
static void clear_actions(struct PlayerBodyState*p){p->actor.action_flags&=(uint8_t)~14u;}
static void stop(struct PlayerBodyState*p){p->vx=p->vy=p->ax=0;transform_integer_copy(&p->actor.transform,&p->actor.transform);}
static int fail(struct PlayerBodyContext*c,unsigned code){c->air.actions->fault=(uint8_t)code;return 0;}
static int crawl(enum PlayerState s){return s>=PST_CRAWL_RIGHT&&s<=PST_CRAWL_TURN_LEFT;}
int player_crouch_enter(struct PlayerBodyContext*c,enum PlayerState state){
 struct PlayerBodyState*p=body(c);unsigned motion,action=0x30;int x=p->arm_offset_x,y=p->arm_offset_y_up;
 if(state==PST_CROUCH_REENTER){player_body_reenter(c,1,1);return 1;}
 p->body_contact=state==PST_CROUCH_EXIT?1:2;p->actor.defense=&player_body_defenses[PDEF_CROUCH];
 switch(state){
  case PST_CROUCH_ENTER:case PST_CROUCH_ENTER_B:case PST_CROUCH_EXIT:
   clear_actions(p);p->ax=0;x=y=0;motion=state==PST_CROUCH_EXIT?PMOTION_CROUCH_EXIT:PMOTION_CROUCH_ENTER;
   if(state==PST_CROUCH_ENTER_B)p->actor.lifecycle_flags|=128;break;
  case PST_CROUCH_IDLE:case PST_CROUCH_AFTER_SHOT:case PST_CROUCH_AFTER_THROW:
   clear_actions(p);stop(p);motion=state==PST_CROUCH_IDLE?PMOTION_CROUCH_IDLE:
    state==PST_CROUCH_AFTER_SHOT?PMOTION_CROUCH_AFTER_SHOT:PMOTION_CROUCH_AFTER_THROW;break;
  case PST_CRAWL_RIGHT:case PST_CRAWL_LEFT:case PST_CRAWL_TURN_RIGHT:case PST_CRAWL_TURN_LEFT:
   clear_actions(p);p->ground_left=state==PST_CRAWL_LEFT||state==PST_CRAWL_TURN_LEFT;
   p->actor.transform.facing=(uint8_t)((p->actor.transform.facing&254)|p->ground_left);
   p->vx=p->ground_left?-0x120:0x120;p->ax=0;
   motion=(state==PST_CRAWL_TURN_RIGHT||state==PST_CRAWL_TURN_LEFT)?PMOTION_CRAWL_TURN:PMOTION_CRAWL;break;
  case PST_CROUCH_SHOOT:case PST_CROUCH_GRENADE:
   p->actor.action_flags&=(uint8_t)~2u;stop(p);motion=state==PST_CROUCH_SHOOT?PMOTION_CROUCH_SHOOT:PMOTION_CROUCH_GRENADE;break;
  case PST_CROUCH_MELEE:
   p->actor.action_flags&=(uint8_t)~2u;p->vx=p->vy=p->ax=p->ay=0;action=0;motion=PMOTION_CROUCH_MELEE;
   p->actor.attack=&player_body_attacks[PATT_MELEE];hit_begin_attack(&p->actor.hit);
   p->actor.defense=&player_body_defenses[PDEF_CROUCH_MELEE];break;
  case PST_CROUCH_RELOAD:
   clear_actions(p);p->actor.action_flags|=4;stop(p);x=y=0;action=0x33;motion=PMOTION_CROUCH_RELOAD;break;
  case PST_CROUCH_EMPTY:
   if(p->actor.local_weapon==WEAPON_PISTOL){player_body_schedule(p,PST_CROUCH_AFTER_SHOT);return 1;}
   clear_actions(p);p->actor.action_flags|=4;p->actor.defense=0;action=0x20;motion=PMOTION_CROUCH_EMPTY;
   /* Source child allocation precedes the parent's switch to pistol. */
   if(!player_body_set_pose(c,action,motion,x,y,1))return 0;
   if(!c->drop_weapon||!c->drop_weapon(c->effects,p))return fail(c,26);
   p->actor.local_weapon=WEAPON_PISTOL;p->ammo=10;p->actor.lifecycle_flags|=128;stop(p);return 1;
  default:return fail(c,25);
 }
 return player_body_set_pose(c,action,motion,x,y,1);
}
static void opposite_turn(struct PlayerBodyContext*c){
 struct PlayerBodyState*p=body(c);int left=p->actor.transform.facing&1;
 if(held(c,left?PB_RIGHT:PB_LEFT))player_body_schedule(p,left?PST_CRAWL_TURN_RIGHT:PST_CRAWL_TURN_LEFT);
}
static void empty_check(struct PlayerBodyState*p){
 if(!p->ammo&&p->actor.local_weapon!=WEAPON_PISTOL)player_body_schedule(p,PST_CROUCH_EMPTY);
}
static void reload_check(struct PlayerBodyState*p,int crouch){
 if(!p->burst_remaining&&p->actor.local_weapon==WEAPON_SHOTGUN)player_body_schedule(p,crouch?PST_CROUCH_RELOAD:PST_RELOAD);
}
static void release_down(struct PlayerBodyContext*c){if(!held(c,PB_DOWN))player_body_schedule(body(c),PST_CROUCH_EXIT);}
static int entry_exit_frame(struct PlayerBodyContext*c,enum PlayerState state){
 struct PlayerBodyState*p=body(c);int complete;
 if(!player_body_frame_common(&c->air,1,0)||player_announce_weapon(c->air.actions)<0)return 0;
 p->vx=(state==PST_CROUCH_ENTER_B||held(c,PB_RIGHT))?0x300:0;
 if(held(c,PB_LEFT))p->vx=-0x300;
 if(player_body_terrain_move(&c->air,WMOVE_GROUND)<0)return 0;p->vx=0;
 complete=player_body_animate(c->air.actions);if(c->air.actions->fault||p->actor.animation.fault)return 0;
 if(complete){
  if(state==PST_CROUCH_EXIT){
   player_body_schedule(p,PST_STAND);if(!player_body_idle_checks(c,1,1))return 0;
   if(held(c,PB_RIGHT))player_body_schedule(p,PST_LOOP_RIGHT);
   if(held(c,PB_LEFT))player_body_schedule(p,PST_LOOP_LEFT);
  }else{
   player_body_schedule(p,PST_CROUCH_IDLE);
   if(held(c,PB_RIGHT))player_body_schedule(p,PST_CRAWL_RIGHT);
   if(held(c,PB_LEFT))player_body_schedule(p,PST_CRAWL_LEFT);
  }
 }
 if(!player_body_actions(c,0,state!=PST_CROUCH_EXIT))return 0;
 if(state==PST_CROUCH_ENTER){reload_check(p,0);empty_check(p);}
 return player_body_tail(c,PST_FALL,state==PST_CROUCH_ENTER_B,0,0,1);
}
int player_crouch_frame(struct PlayerBodyContext*c,enum PlayerState state){
 struct PlayerBodyState*p=body(c);int complete;
 if(state==PST_CROUCH_ENTER||state==PST_CROUCH_ENTER_B||state==PST_CROUCH_EXIT)return entry_exit_frame(c,state);
 p->status_flags|=64;
 if(!player_body_frame_common(&c->air,1,0))return 0;
 int notice=player_announce_weapon(c->air.actions);if(notice<0)return 0;
 int idle=state==PST_CROUCH_IDLE||state==PST_CROUCH_AFTER_SHOT||state==PST_CROUCH_AFTER_THROW;
 if(notice&&(idle||crawl(state)))player_body_schedule(p,PST_CROUCH_REENTER);
 if(crawl(state))p->vx=p->ground_left?-0x120:0x120;
 if(player_body_terrain_move(&c->air,WMOVE_GROUND)<0)return 0;
 complete=player_body_animate(c->air.actions);if(c->air.actions->fault||p->actor.animation.fault)return 0;
 if(idle){
  player_body_reenter(c,1,0);release_down(c);
  if(!player_body_actions(c,0,1))return 0;empty_check(p);
  return player_body_tail(c,PST_FALL_DELAY,0,0,1,1);
 }
 if(crawl(state)){
  if(!held(c,p->ground_left?PB_LEFT:PB_RIGHT))player_body_schedule(p,PST_CROUCH_IDLE);
  release_down(c);if(!player_body_actions(c,0,1))return 0;
  return player_body_tail(c,PST_FALL_DELAY,0,0,0,1);
 }
 if(complete)player_body_schedule(p,(state==PST_CROUCH_GRENADE||state==PST_CROUCH_MELEE)?PST_CROUCH_AFTER_THROW:PST_CROUCH_AFTER_SHOT);
 if(state==PST_CROUCH_MELEE)return player_body_death_publish(&c->air)&&
  (c->air.transition!=PAIR_DEATH||(player_body_schedule(p,PST_DEATH),1));
 if(state==PST_CROUCH_EMPTY){
  release_down(c);if(!(p->actor.action_flags&4)&&!player_body_actions(c,0,1))return 0;
  return player_body_tail(c,PST_FALL,0,0,0,1);
 }
 if(state==PST_CROUCH_RELOAD){
  if(!(p->actor.action_flags&4))p->burst_remaining=1;
  release_down(c);if(!player_body_actions(c,0,1))return 0;
  return player_body_tail(c,PST_FALL_DELAY,0,1,0,1);
 }
 if(!(p->actor.action_flags&4)){
  opposite_turn(c);if(!player_body_actions(c,0,1))return 0;
  if(state==PST_CROUCH_SHOOT){reload_check(p,1);empty_check(p);}
 }
 release_down(c);
 return player_body_tail(c,PST_FALL_DELAY,0,state==PST_CROUCH_SHOOT,state==PST_CROUCH_SHOOT,state==PST_CROUCH_SHOOT);
}
