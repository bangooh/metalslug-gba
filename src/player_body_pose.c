#include "player_body_pose.h"
static int fail(struct PlayerActionContext*c){c->fault=20;return 0;}
static int select_pose(struct PlayerActionContext*c,unsigned motion,int x,int y,int aim,int notify){
 struct PlayerBodyState*p=c->body;
 p->arm_offset_x=(int16_t)x;p->arm_offset_y_up=(int16_t)y;
 p->actor.aim=(uint16_t)aim;
 if(notify){p->animation_changed=1;p->actor.movement_flags|=32;}
 return player_body_motion(c,motion,0);
}
int player_air_neutral_pose(struct PlayerActionContext*c){
 struct PlayerBodyState*p=c->body;if(c->fault)return 0;
 if(p->actor.action_flags&6)return 1;
 int standing=p->action==BODY_FALL;
 if(p->aim_direction==8){
  if(p->actor.aim==3)return 1;
  p->actor.transform.priority&=(uint16_t)~1u;
  p->action=standing?BODY_FALL:BODY_JUMP;
  return select_pose(c,standing?PMOTION_AIR_STANDING_DOWN:PMOTION_AIR_RUNNING_DOWN,
   standing?0:2,standing?7:2,3,p->direction_change==0x18);
 }
 if(!p->actor.aim)return 1;
 p->action=standing?BODY_FALL:BODY_JUMP;
 if(!standing&&p->actor.local_weapon==WEAPON_PISTOL)p->actor.transform.priority|=1;
 return select_pose(c,standing?PMOTION_AIR_STANDING_NEUTRAL:PMOTION_AIR_RUNNING_NEUTRAL,
  0,standing?7:2,0,p->direction_change==0x81);
}
static int fire_back(const struct PlayerBodyState*p,unsigned horizontal,int use_neutral_aim){
 unsigned compare=(p->actor.transform.facing&1)?2:1;
 if(use_neutral_aim&&!horizontal){
  if(p->aim_direction==1)compare=255;
  if(p->aim_direction==2)compare=0;
 }
 return compare==horizontal;
}
int player_air_action_pose(struct PlayerActionContext*c,enum PlayerIntent intent,uint8_t horizontal){
 struct PlayerBodyState*p=c->body;if(c->fault)return 0;
 if(intent==INTENT_NONE)return 1;
 if(horizontal>3)return fail(c);
 int standing=p->action==BODY_FALL;
 p->action=standing?BODY_FALL:BODY_JUMP;
 p->actor.action_flags|=4;p->animation_changed=1;p->actor.movement_flags|=32;
 if(intent==INTENT_MELEE)return select_pose(c,PMOTION_AIR_MELEE,0,standing?7:2,0,1);
 if(intent==INTENT_GRENADE){
  int back=fire_back(p,horizontal,!standing);
  return select_pose(c,back?PMOTION_AIR_GRENADE_BACK:PMOTION_AIR_GRENADE_FORWARD,
   back?(standing?0:3):0,back?(standing?0:-6):(standing?7:2),back?2:0,1);
 }
 p->actor.action_flags|=2;
 if(intent==INTENT_DOWN){
  p->actor.transform.priority&=(uint16_t)~1u;
  return select_pose(c,standing?PMOTION_AIR_STANDING_DOWN_FIRE:PMOTION_AIR_RUNNING_DOWN_FIRE,
   standing?0:2,standing?7:2,3,1);
 }
 if(intent==INTENT_UP)return select_pose(c,standing?PMOTION_AIR_STANDING_UP_FIRE:PMOTION_AIR_RUNNING_UP_FIRE,
  0,standing?6:2,1,1);
 if(intent!=INTENT_BACK&&intent!=INTENT_FIRE)return fail(c);
 if(intent==INTENT_BACK||fire_back(p,horizontal,1))return select_pose(c,
  standing?PMOTION_AIR_STANDING_BACK_FIRE:PMOTION_AIR_RUNNING_BACK_FIRE,standing?0:6,standing?0:-3,2,1);
 if(!standing&&p->actor.local_weapon==WEAPON_PISTOL)p->actor.transform.priority|=1;
 return select_pose(c,standing?PMOTION_AIR_STANDING_FORWARD_FIRE:PMOTION_AIR_RUNNING_FORWARD_FIRE,0,standing?7:2,0,1);
}
enum PlayerDeathRoute player_death_route(uint8_t kind,int airborne){
 /* $033572: two 34-entry tables, NOT the 16 entries described in the ASM
  * commentary. The >=34 guard substitutes index 15. */
 return (enum PlayerDeathRoute)player_death_routes[!!airborne][kind<34?kind:15];
}
