#include "player_body.h"
static int fail(struct PlayerBodyContext*c,unsigned code){c->air.actions->fault=(uint8_t)code;return 0;}
static struct PlayerBodyState*body(struct PlayerBodyContext*c){return c->air.actions->body;}
static int held(struct PlayerBodyContext*c,unsigned key){return !!(c->air.input.held&key);}
static void clear_actions(struct PlayerBodyState*p){p->actor.action_flags&=(uint8_t)~14u;}
/* $0267E6 clears both fractional bytes and VX/VY/AX, preserving AY. */
static void stop(struct PlayerBodyState*p){p->vx=p->vy=p->ax=0;transform_integer_copy(&p->actor.transform,&p->actor.transform);}
static void halt_velocity(struct PlayerBodyState*p){p->vx=p->vy=p->ax=p->ay=0;}
static void face(struct PlayerBodyState*p,int left){p->actor.transform.facing=(uint8_t)((p->actor.transform.facing&254)|!!left);}
static int shooting(const struct PlayerBodyState*p){return p->action>=0x13&&p->action<0x24;}
static void notify_arm(struct PlayerBodyState*p){p->animation_changed=1;p->actor.movement_flags|=32;}
void player_body_schedule(struct PlayerBodyState*p,enum PlayerState state){p->body_state=(uint8_t)state;p->state_enter=1;}
int player_body_set_pose(struct PlayerBodyContext*c,unsigned action,unsigned motion,int x,int y,int restart){
 struct PlayerBodyState*p=body(c);p->action=(uint8_t)action;p->arm_offset_x=(int16_t)x;p->arm_offset_y_up=(int16_t)y;
 return player_body_motion(c->air.actions,motion,restart);
}
int player_body_frame(struct PlayerBodyContext*c,int*complete,int announce){
 if(!player_body_frame_common(&c->air,1,0))return 0;
 if(announce&&player_announce_weapon(c->air.actions)<0)return 0;
 if(player_body_terrain_move(&c->air,WMOVE_GROUND)<0)return 0;
 *complete=player_body_animate(c->air.actions);
 return !c->air.actions->fault&&!body(c)->actor.animation.fault;
}
int player_body_tail(struct PlayerBodyContext*c,enum PlayerState fall,int crouch_jump,int jump_first,int bounds,int allow_jump){
 struct PlayerBodyState*p=body(c);
 if(allow_jump&&jump_first&&player_body_jump_pressed(&c->air,crouch_jump))player_body_schedule(p,PST_JUMP);
 int contact=player_body_foot_contact(&c->air);if(contact<0)return 0;
 if(!contact)player_body_schedule(p,fall);
 if(allow_jump&&!jump_first&&player_body_jump_pressed(&c->air,crouch_jump))player_body_schedule(p,PST_JUMP);
 if(!player_body_death_publish(&c->air))return 0;
 if(c->air.transition==PAIR_DEATH)player_body_schedule(p,PST_DEATH);
 if(bounds){int outside=player_body_outside(&c->air);if(outside<0)return 0;if(outside)player_body_schedule(p,PST_PIT);}
 return !c->air.actions->fault;
}
/* Source checks RIGHT first and LEFT second; LEFT wins when both are held.
 * Immediate re-entry instead gives the facing direction first refusal. */
void player_body_reenter(struct PlayerBodyContext*c,int crouching,int immediate){
 struct PlayerBodyState*p=body(c);int left=!!(p->actor.transform.facing&1);
 enum PlayerState same=crouching?(left?PST_CRAWL_LEFT:PST_CRAWL_RIGHT):(left?PST_WALK_LEFT:PST_WALK_RIGHT);
 enum PlayerState opposite=crouching?(left?PST_CRAWL_TURN_RIGHT:PST_CRAWL_TURN_LEFT):(left?PST_TURN_RIGHT:PST_TURN_LEFT);
 if(immediate){
  player_body_schedule(p,held(c,left?PB_LEFT:PB_RIGHT)?same:held(c,left?PB_RIGHT:PB_LEFT)?opposite:crouching?PST_CROUCH_IDLE:PST_STAND);
 }else{
  if(held(c,PB_RIGHT))player_body_schedule(p,left?opposite:same);
  if(held(c,PB_LEFT))player_body_schedule(p,left?same:opposite);
 }
}
int player_body_actions(struct PlayerBodyContext*c,unsigned gait,int crouching){
 struct PlayerBodyState*p=body(c);
 enum PlayerIntent intent=player_select_action(c->air.actions,c->air.input,0);
 if(c->air.actions->fault)return 0;if(intent==INTENT_NONE)return 1;
 if(crouching){
  player_body_schedule(p,intent==INTENT_MELEE?PST_CROUCH_MELEE:intent==INTENT_GRENADE?PST_CROUCH_GRENADE:PST_CROUCH_SHOOT);return 1;
 }
 /* $034A8A has an additional action-lock branch only in the stand frame. */
 if(!(gait==3&&(p->actor.action_flags&8))){
  if(intent==INTENT_MELEE){player_body_schedule(p,PST_MELEE);return 1;}
  if(intent==INTENT_GRENADE){player_body_schedule(p,PST_GRENADE);return 1;}
  if(intent==INTENT_DOWN){player_body_schedule(p,PST_CROUCH_SHOOT);return 1;}
 }
 int up=intent==INTENT_UP,left=p->ground_left;
 player_body_schedule(p,gait==1?(up?(left?PST_WALK_UP_LEFT:PST_WALK_UP_RIGHT):(left?PST_WALK_SHOOT_LEFT:PST_WALK_SHOOT_RIGHT)):
  gait==2?(up?(left?PST_LOOP_UP_LEFT:PST_LOOP_UP_RIGHT):(left?PST_LOOP_SHOOT_LEFT:PST_LOOP_SHOOT_RIGHT)):
  up?PST_SHOOT_UP:PST_SHOOT_STAND);
 return 1;
}
int player_body_idle_checks(struct PlayerBodyContext*c,int low_ammo,int force_empty){
 struct PlayerBodyState*p=body(c);
 if(p->crouch_flags&1)player_body_schedule(p,PST_IDLE_SPECIAL);
 if(!p->burst_remaining&&p->actor.local_weapon==WEAPON_SHOTGUN)player_body_schedule(p,PST_RELOAD);
 if(low_ammo){
  /* $03491A consumes RNG even though MOVEQ #4 replaces the masked value. */
  if(!c->air.actions->rng)return fail(c,3);
  (void)source_random_word(c->air.actions->rng);
  if(p->ammo==4)player_body_schedule(p,PST_IDLE_LOW_AMMO);
 }
 if(!p->ammo&&(force_empty||p->actor.local_weapon!=WEAPON_PISTOL))player_body_schedule(p,PST_EMPTY);
 return 1;
}
static int pose_fire(struct PlayerBodyContext*c,unsigned gait,int up,int alternate){
 struct PlayerBodyState*p=body(c);int changed=p->actor.aim!=(unsigned)up;unsigned motion;
 int restart=gait==0||p->action>=0x24;
 if(gait==0)motion=up?(changed?PMOTION_STAND_FIRE_TO_UP:alternate?PMOTION_STAND_UP_FIRE_ALT:PMOTION_STAND_UP_FIRE):
  (changed?PMOTION_STAND_FIRE_FROM_UP:alternate?PMOTION_STAND_FIRE_ALT:PMOTION_STAND_FIRE);
 else motion=up?(changed?PMOTION_WALK_FIRE_TO_UP:alternate?PMOTION_WALK_UP_FIRE_ALT:PMOTION_WALK_UP_FIRE):
  (changed?PMOTION_WALK_FIRE_FROM_UP:alternate?PMOTION_WALK_FIRE_ALT:PMOTION_WALK_FIRE);
 p->actor.action_flags|=2;p->actor.aim=(uint16_t)up;
 if(!restart)notify_arm(p);
 return player_body_set_pose(c,gait==0?BODY_SHOOT:gait==1?BODY_WALK_SHOOT:BODY_LOOP_SHOOT,motion,
  gait==2&&!up?1:0,gait==0?0:gait==1?-1:up?4:3,restart);
}
static int set_ground(struct PlayerBodyContext*c,enum PlayerState state){
 struct PlayerBodyState*p=body(c);int keep=shooting(p);
 p->body_contact=1;p->actor.defense=&player_body_defenses[PDEF_STAND];p->ax=0;
 switch(state){
  case PST_STAND:
   p->actor.lifecycle_flags|=128;stop(p);
   if(keep){p->arm_offset_x=p->arm_offset_y_up=0;p->animation_changed=0;
    if(!player_body_stream(c->air.actions,PMOTION_STAND))return 0;p->actor.movement_flags&=(uint8_t)~32u;
   }else{clear_actions(p);if(!player_body_set_pose(c,BODY_IDLE,PMOTION_STAND,0,0,1))return 0;}
   break;
  case PST_STAND_FROM_WALK:clear_actions(p);p->arm_offset_x=0;p->arm_offset_y_up=-1;p->actor.lifecycle_flags|=128;stop(p);break;
  case PST_WALK_RIGHT:case PST_WALK_LEFT:case PST_LOOP_RIGHT:case PST_LOOP_LEFT:{
   int loop=state==PST_LOOP_RIGHT||state==PST_LOOP_LEFT,left=state==PST_WALK_LEFT||state==PST_LOOP_LEFT;
   p->ground_left=(uint8_t)left;face(p,left);p->vx=left?-0x300:0x300;
   p->arm_offset_x=0;p->arm_offset_y_up=loop&&keep?3:-1;
   if(keep){if(!player_body_stream(c->air.actions,loop?PMOTION_WALK_LOOP:PMOTION_WALK))return 0;
    p->animation_changed=0;p->actor.movement_flags&=(uint8_t)~32u;
   }else{clear_actions(p);if(!player_body_set_pose(c,loop?BODY_WALK_LOOP:BODY_WALK,loop?PMOTION_WALK_LOOP:PMOTION_WALK,0,-1,1))return 0;}
   break;
  }
  case PST_TURN_RIGHT:case PST_TURN_LEFT:
   clear_actions(p);p->ground_left=state==PST_TURN_LEFT;face(p,p->ground_left);p->vx=p->ground_left?-0x300:0x300;
   if(!player_body_set_pose(c,0x31,p->actor.aim==1?PMOTION_TURN_UP:PMOTION_TURN,0,0,1))return 0;break;
  default:return fail(c,25);
 }
 return 1;
}
static int special_entry(struct PlayerBodyContext*c,enum PlayerState state){
 struct PlayerBodyState*p=body(c);clear_actions(p);unsigned motion=PMOTION_GROUND_GRENADE,action=0x33;
 p->arm_offset_x=p->arm_offset_y_up=0;p->body_contact=1;p->actor.defense=&player_body_defenses[PDEF_STAND];
 switch(state){
  case PST_MELEE:case PST_MELEE_ALT:case PST_MELEE_ALT_WALK:
   p->actor.attack=&player_body_attacks[PATT_MELEE];hit_begin_attack(&p->actor.hit);
   p->actor.defense=&player_body_defenses[PDEF_MELEE];
   motion=state==PST_MELEE?PMOTION_GROUND_MELEE:PMOTION_GROUND_MELEE_ALT;
   if(state==PST_MELEE)halt_velocity(p);if(state==PST_MELEE_ALT_WALK)p->arm_offset_y_up=-1;break;
  case PST_GRENADE:halt_velocity(p);break;
  case PST_GRENADE_WALK:case PST_GRENADE_LOOP:
   motion=PMOTION_WALK_GRENADE;if(state==PST_GRENADE_LOOP)p->arm_offset_y_up=-1;break;
  case PST_IDLE_SPECIAL:case PST_IDLE_LOW_AMMO:
   motion=state==PST_IDLE_SPECIAL?PMOTION_IDLE_SPECIAL:PMOTION_IDLE_LOW_AMMO;action=5;stop(p);break;
  case PST_RELOAD:
   motion=p->actor.aim==1?PMOTION_SHOTGUN_RELOAD_UP:PMOTION_SHOTGUN_RELOAD;p->actor.action_flags|=4;stop(p);break;
  case PST_EMPTY:{
   int restart=!(shooting(p)&&p->actor.local_weapon!=WEAPON_PISTOL);
   p->actor.action_flags|=4;
   if(!player_body_set_pose(c,0x21,PMOTION_EMPTY_WEAPON,0,0,restart))return 0;
   if(p->actor.local_weapon!=WEAPON_PISTOL){
    p->actor.defense=0;
    if(!c->drop_weapon||!c->drop_weapon(c->effects,p))return fail(c,26);
   }
   p->actor.local_weapon=WEAPON_PISTOL;p->ammo=10;p->actor.lifecycle_flags|=128;stop(p);return 1;
  }
  case PST_LAND:case PST_STOP:
   if(state==PST_LAND){p->actor.action_flags&=(uint8_t)~1u;if(p->actor.aim==2)p->actor.aim=0;}
   motion=PMOTION_STOP;action=state==PST_LAND?1:5;p->actor.lifecycle_flags|=128;stop(p);break;
  default:return fail(c,25);
 }
 return player_body_set_pose(c,action,motion,p->arm_offset_x,p->arm_offset_y_up,1);
}
static int stand_aim(struct PlayerBodyContext*c,unsigned gait,int initial){
 struct PlayerBodyState*p=body(c);unsigned up=p->aim_direction==4;
 if(initial){
  if(p->arm_selection!=(gait?ARM_WALK:ARM_STAND))return 1;
 }else if(p->actor.aim==up)return 1;
 p->actor.aim=(uint16_t)up;
 if(!player_body_motion(c->air.actions,gait==0?(up?PMOTION_STAND_UP:PMOTION_STAND):
   gait==1?(up?PMOTION_WALK_UP:PMOTION_WALK):(up?PMOTION_WALK_LOOP_UP:PMOTION_WALK_LOOP),0))return 0;
 p->action=(uint8_t)(gait==0?BODY_IDLE:gait==1?BODY_WALK:BODY_WALK_LOOP);
 if(!initial&&(p->direction_change==(up?0x14:0x41)))notify_arm(p);
 return 1;
}
static int walk_tail(struct PlayerBodyContext*c){
 struct PlayerBodyState*p=body(c);
 if(held(c,PB_DOWN))player_body_schedule(p,PST_CROUCH_ENTER);
 if(!p->burst_remaining&&p->actor.local_weapon==WEAPON_SHOTGUN)player_body_schedule(p,PST_RELOAD);
 if(!p->ammo&&p->actor.local_weapon!=WEAPON_PISTOL)player_body_schedule(p,PST_EMPTY);
 return player_body_tail(c,PST_FALL,0,0,0,1);
}
static int stand_frame(struct PlayerBodyContext*c,enum PlayerState state){
 struct PlayerBodyState*p=body(c);int complete;
 if(!player_body_frame_common(&c->air,1,0))return 0;
 int notice=player_announce_weapon(c->air.actions);if(notice<0)return 0;
 if(notice&&state==PST_STAND)player_body_schedule(p,PST_REENTER);
 if(player_body_terrain_move(&c->air,WMOVE_GROUND)<0)return 0;
 complete=player_body_animate(c->air.actions);if(c->air.actions->fault||p->actor.animation.fault)return 0;
 if(state==PST_STAND_FROM_WALK){
  if(complete)player_body_schedule(p,PST_STAND);
  if(!stand_aim(c,0,1))return 0;
  if(p->aim_direction!=4&&(p->crouch_flags&1))player_body_schedule(p,PST_IDLE_SPECIAL);
 }else{
  if(complete){
   p->arm_offset_x=p->arm_offset_y_up=0;clear_actions(p);p->actor.aim=p->aim_direction==4;p->action=BODY_IDLE;
   if(p->actor.aim){if(!player_body_motion(c->air.actions,PMOTION_STAND_UP,0))return 0;notify_arm(p);}
   else if(!player_body_motion(c->air.actions,PMOTION_STAND,1))return 0;
   if(!player_body_idle_checks(c,1,1))return 0;
  }
  if(p->action==BODY_IDLE&&!stand_aim(c,0,0))return 0;
 }
 if(!(p->actor.action_flags&4)){
  player_body_reenter(c,0,0);if(held(c,PB_DOWN))player_body_schedule(p,PST_CROUCH_ENTER);
 }
 if(!(p->actor.action_flags&4)||p->actor.local_weapon==WEAPON_HMG)if(!player_body_actions(c,3,0))return 0;
 return player_body_tail(c,PST_FALL,0,0,1,1);
}
static int walk_frame(struct PlayerBodyContext*c,enum PlayerState state){
 struct PlayerBodyState*p=body(c);int loop=state==PST_LOOP_RIGHT||state==PST_LOOP_LEFT;
 int turning=state==PST_TURN_RIGHT||state==PST_TURN_LEFT,left=p->ground_left;
 if(!player_body_frame_common(&c->air,1,0))return 0;
 int notice=player_announce_weapon(c->air.actions);if(notice<0)return 0;if(notice)player_body_schedule(p,PST_REENTER);
 p->vx=left?-0x300:0x300;if(player_body_terrain_move(&c->air,WMOVE_GROUND)<0)return 0;
 int complete=player_body_animate(c->air.actions);if(c->air.actions->fault||p->actor.animation.fault)return 0;
 if(turning){
  if(complete)player_body_schedule(p,(p->crouch_flags&1)?PST_IDLE_SPECIAL:PST_STAND);
  if(!(p->actor.action_flags&4)&&!player_body_actions(c,0,0))return 0;
  return walk_tail(c);
 }
 if(!loop){
  if(complete)player_body_schedule(p,left?PST_LOOP_LEFT:PST_LOOP_RIGHT);
  if(!stand_aim(c,1,1))return 0;
  if(!(p->actor.action_flags&4)){
   if(!held(c,left?PB_LEFT:PB_RIGHT))player_body_schedule(p,PST_STAND_FROM_WALK);
   if(held(c,left?PB_RIGHT:PB_LEFT))player_body_schedule(p,left?PST_TURN_RIGHT:PST_TURN_LEFT);
   if(!player_body_actions(c,1,0))return 0;
  }
 }else{
  if(complete){
   int keep=shooting(p);clear_actions(p);
   if(!player_body_set_pose(c,BODY_WALK_LOOP,PMOTION_WALK_LOOP,0,-1,!keep))return 0;
  }
  if(p->action==BODY_WALK_LOOP&&!stand_aim(c,2,0))return 0;
  if(!held(c,left?PB_LEFT:PB_RIGHT))player_body_schedule(p,(p->actor.action_flags&4)?PST_STAND:PST_STOP);
  if(held(c,left?PB_RIGHT:PB_LEFT)){
   if(p->actor.action_flags&4)p->actor.transform.facing^=1;
   else player_body_schedule(p,left?PST_TURN_RIGHT:PST_TURN_LEFT);
  }
  if(!(p->actor.action_flags&4)&&!player_body_actions(c,2,0))return 0;
 }
 return walk_tail(c);
}
static int special_frame(struct PlayerBodyContext*c,enum PlayerState state,int entered){
 struct PlayerBodyState*p=body(c);int complete;
 if(state==PST_EMPTY&&(p->actor.action_flags&16)){
  if(player_announce_weapon(c->air.actions)<0)return 0;player_body_schedule(p,PST_REENTER);return player_body_tick(c);
 }
 if(state==PST_MELEE&&!entered){
  int16_t v=held(c,PB_LEFT)?-0x300:held(c,PB_RIGHT)?0x300:0;
  if(v!=p->vx){
   unsigned motion=!v?PMOTION_GROUND_MELEE:v>p->vx?PMOTION_GROUND_MELEE_RIGHT:PMOTION_GROUND_MELEE_LEFT;
   if(!player_body_stream(c->air.actions,motion))return 0;p->actor.movement_flags&=(uint8_t)~32u;p->animation_changed=0;
  }
  p->vx=v;
 }
 /* MOVE.W after the first right check clears carry in these two source
  * variants; the second BCC skips the left helper. Preserve that oddity. */
 if(state==PST_MELEE_ALT_WALK||state==PST_GRENADE_LOOP){p->vx=held(c,PB_RIGHT)?0x300:0;if(p->vx)face(p,0);}
 if(!player_body_frame(c,&complete,state!=PST_EMPTY))return 0;
 if(complete){
  player_body_schedule(p,PST_STAND);
  if(state==PST_IDLE_SPECIAL||state==PST_IDLE_LOW_AMMO||state==PST_RELOAD||state==PST_EMPTY){
   if(p->vx)player_body_schedule(p,p->vx<0?PST_WALK_LEFT:PST_WALK_RIGHT);
  }else{
   p->direction_change=(uint8_t)((p->aim_direction<<4)|1);p->aim_direction=1;
  }
  if(state==PST_MELEE){if(p->crouch_flags&1)player_body_schedule(p,PST_IDLE_SPECIAL);if(!p->ammo)player_body_schedule(p,PST_EMPTY);}
  if(state==PST_LAND||state==PST_STOP){if(!player_body_idle_checks(c,p->action==1,1))return 0;}
  if(state==PST_RELOAD){
   if(!c->air.actions->rng)return fail(c,3);(void)source_random_word(c->air.actions->rng);
   if(p->ammo==4)player_body_schedule(p,PST_IDLE_LOW_AMMO);
   if(p->actor.aim!=1&&!p->ammo)player_body_schedule(p,PST_EMPTY);
  }
 }
 if(state==PST_RELOAD){
  if(!(p->actor.action_flags&4))p->burst_remaining=1;
  int notice=player_announce_weapon(c->air.actions);if(notice<0)return 0;if(notice)player_body_schedule(p,PST_REENTER);
 }
 if(state==PST_MELEE){if(!player_body_actions(c,0,0))return 0;return player_body_tail(c,PST_FALL,0,0,1,1);}
 if(state==PST_MELEE_ALT)return walk_tail(c);
 if(state==PST_MELEE_ALT_WALK){p->ground_left=0;if(!player_body_actions(c,2,0))return 0;return walk_tail(c);}
 if(state==PST_GRENADE||state==PST_GRENADE_WALK||state==PST_GRENADE_LOOP){
  if(!(p->actor.action_flags&4)){
   int left=p->actor.transform.facing&1;
   if(held(c,left?PB_RIGHT:PB_LEFT))player_body_schedule(p,left?PST_TURN_RIGHT:PST_TURN_LEFT);
   if(held(c,PB_DOWN))player_body_schedule(p,PST_CROUCH_ENTER);
   if(player_body_jump_pressed(&c->air,0))player_body_schedule(p,PST_JUMP);
  }
  if(state!=PST_GRENADE_WALK){if(state==PST_GRENADE_LOOP)p->ground_left=0;if(!player_body_actions(c,state==PST_GRENADE_LOOP?2:0,0))return 0;}
  return state==PST_GRENADE?player_body_tail(c,PST_FALL,0,0,1,1):walk_tail(c);
 }
 if(!(p->actor.action_flags&4)){
  player_body_reenter(c,0,0);if(held(c,PB_DOWN))player_body_schedule(p,PST_CROUCH_ENTER);
  if(!player_body_actions(c,0,0))return 0;
 }
 return player_body_tail(c,PST_FALL_HOLD,0,0,1,1);
}
static int entry(struct PlayerBodyContext*c,enum PlayerState state){
 struct PlayerBodyState*p=body(c);
 if(state==PST_REENTER){player_body_reenter(c,0,1);return 1;}
 if(state==PST_STOP&&(shooting(p)||p->actor.aim==1)){player_body_schedule(p,PST_STAND);return 1;}
 if(state<=PST_TURN_LEFT)return set_ground(c,state);
 if(state>=PST_SHOOT_STAND&&state<=PST_LOOP_UP_LEFT){
  unsigned gait=state<=PST_SHOOT_UP_ALT?0:state<=PST_WALK_UP_LEFT?1:2;
  int up=state==PST_SHOOT_UP||state==PST_SHOOT_UP_ALT||state==PST_WALK_UP_RIGHT||state==PST_WALK_UP_LEFT||state==PST_LOOP_UP_RIGHT||state==PST_LOOP_UP_LEFT;
  int left=state==PST_WALK_SHOOT_LEFT||state==PST_WALK_UP_LEFT||state==PST_LOOP_SHOOT_LEFT||state==PST_LOOP_UP_LEFT;
  if(!pose_fire(c,gait,up,state==PST_SHOOT_STAND_ALT||state==PST_SHOOT_UP_ALT))return 0;
  p->body_contact=1;p->actor.defense=&player_body_defenses[PDEF_STAND];p->ax=0;
  p->body_state=(uint8_t)(gait==0?PST_STAND:gait==1?(left?PST_WALK_LEFT:PST_WALK_RIGHT):(left?PST_LOOP_LEFT:PST_LOOP_RIGHT));
  p->ground_left=(uint8_t)left;if(gait){face(p,left);p->vx=left?-0x300:0x300;}else{stop(p);p->actor.lifecycle_flags|=128;}
  return 1;
 }
 if(state>=PST_CROUCH_ENTER&&state<=PST_CROUCH_EMPTY)return player_crouch_enter(c,state);
 if(state>=PST_MELEE&&state<=PST_STOP){
  if(!special_entry(c,state))return 0;
  if(state==PST_LAND||state==PST_STOP){
   if(state==PST_LAND&&held(c,PB_DOWN))player_body_schedule(p,PST_CROUCH_IDLE);
   else if(!p->ammo)player_body_schedule(p,PST_EMPTY);
   else if(!p->burst_remaining&&p->actor.local_weapon==WEAPON_SHOTGUN)player_body_schedule(p,PST_RELOAD);
   else if(p->crouch_flags&1)player_body_schedule(p,PST_IDLE_SPECIAL);
  }
  return 1;
 }
 return fail(c,25);
}
int player_body_tick(struct PlayerBodyContext*c){
 struct PlayerBodyState*p=body(c);if(c->air.actions->fault)return 0;
 c->air.transition=PAIR_CONTINUE;int entered=p->state_enter;
 /* Only direct source entry branches repeat here, never a frame loop. */
 for(unsigned budget=0;budget<12;budget++){
  enum PlayerState state=(enum PlayerState)p->body_state;
  if(state>=PST_STATE_COUNT)return fail(c,25);
  if(state>=PST_HANG_RING&&state<=PST_JUMP_DROP)return player_attachment_tick(c);
  if(state>=PST_PIT)return player_life_tick(c);
  if(state>=PST_JUMP&&state<=PST_FALL_HOLD){
   int ok;
   if(p->state_enter){p->state_enter=0;ok=state==PST_JUMP?player_air_begin_jump(&c->air):player_air_begin_fall(&c->air,
    state==PST_FALL?PAIR_FALL:state==PST_FALL_DELAY?PAIR_FALL_DELAY:PAIR_FALL_HOLD);}
   else ok=player_air_step(&c->air);
   if(!ok)return 0;
   switch(c->air.transition){
    case PAIR_LAND:player_body_schedule(p,PST_LAND);break;
    case PAIR_RIDE_SLUG:player_body_schedule(p,PST_RIDE_SLUG);break;
    case PAIR_HANG_RING:player_body_schedule(p,PST_HANG_RING);break;
    case PAIR_REJUMP:player_body_schedule(p,PST_JUMP_DROP);break;
    case PAIR_PIT:player_body_schedule(p,PST_PIT);break;
    case PAIR_DEATH:player_body_schedule(p,PST_DEATH);break;
    default:break;
   }
   return 1;
  }
  if(state==PST_RIDE_SLUG||state==PST_SLUG_POSE)return player_slug_tick(c);
  if(p->state_enter){p->state_enter=0;if(!entry(c,state))return 0;if(p->state_enter)continue;state=(enum PlayerState)p->body_state;}
  if(state==PST_STAND||state==PST_STAND_FROM_WALK)return stand_frame(c,state);
  if(state>=PST_WALK_RIGHT&&state<=PST_TURN_LEFT)return walk_frame(c,state);
  if(state>=PST_CROUCH_ENTER&&state<=PST_CROUCH_EMPTY)return player_crouch_frame(c,state);
  return special_frame(c,state,entered);
 }
 return fail(c,28);
}
