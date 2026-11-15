#include "player_arm.h"
#include "../generated/player-programs.h"
static int fail(struct PlayerActionContext*c,unsigned code){c->fault=(uint8_t)code;return 0;}
static int32_t coordinate(int32_t v,int offset){return (int16_t)((v>>8)+offset)*256;}
int player_arm_init(struct PlayerActionContext*c,uint16_t hit_id){
 struct PlayerBodyState*p=c->body;
 if(p->character>1)return fail(c,10);
 *c->layer=(struct PlayerActionLayer){
  .animation={.cursor=65535,.drawing=65535,.last_draw_cursor=65535},
  .transform=p->actor.transform,.hit={.id=hit_id,.type_flags=64},
  .owner=p->actor.owner,.damage_owner=p->actor.damage_owner,
  .shot_phase=255,.melee_selector=255,.local_weapon=255,
  .hit_palette=(uint8_t)(p->character?0x90:0x8f),
  .palette_base=(uint16_t)(p->character?0x177:0x176),
  .palette_alternate=0x190,.palette_impact=(uint16_t)(p->character?0x192:0x191)
 };
 c->layer->palette=c->layer->palette_base;
 p->palette_signal=0;
 animation_begin(&c->layer->animation,player_arm_initial_entries[p->character]);return 1;
}
/* $03937C/$05E4B2/$032D00: upper-body transform and palettes follow the
 * parent without touching the lower body's independent animation phase. */
static int follow(struct PlayerActionContext*c){
 struct PlayerBodyState*p=c->body;struct PlayerActionLayer*l=c->layer;
 l->hit.flags|=64;
 l->transform.priority=(uint16_t)(p->actor.transform.priority&0xfffc);
 l->transform.x=coordinate(p->actor.transform.x,(p->actor.transform.facing&1)?-p->arm_offset_x:p->arm_offset_x);
 l->transform.y=(512-(int32_t)(int16_t)(transform_y_up(&p->actor.transform)+p->arm_offset_y_up))*256;
 l->transform.facing=p->actor.transform.facing;
 l->hit.events|=p->actor.hit.events&128;
 l->damage_timer=p->actor.damage_timer; /* $59, distinct from hit kind $58. */
 l->hit.flip=l->transform.facing;
 l->hit.x=(int16_t)(l->transform.x>>8);
 l->hit.y=transform_y_up(&l->transform);
 switch(p->palette_signal&15){
  case 0:case 3:l->palette=l->palette_base;break;
  case 1:case 2:l->palette=l->palette_impact;break;
  case 4:l->palette=l->palette_alternate;break;
  default:return fail(c,11);
 }
 return 1;
}
static int select_and_copy_aim(struct PlayerActionContext*c,int copy_first){
 uint16_t aim=c->body->actor.aim;
 if(copy_first)c->layer->aim=aim;
 if(!player_arm_select(c,(enum PlayerArmSelection)c->body->arm_selection))return 0;
 if(!copy_first)c->layer->aim=aim;
 return 1;
}
int player_arm_tick(struct PlayerActionContext*c){
 struct PlayerBodyState*p=c->body;struct PlayerActionLayer*l=c->layer;
 if(c->fault||!follow(c))return 0;
 int watch_aim=0,copy_first=1;
 switch(p->action){
  case BODY_IDLE:
   if(!(l->action_flags&4)&&p->animation_changed){if(!select_and_copy_aim(c,0))return 0;break;}
   watch_aim=p->arm_selection==ARM_STAND||p->arm_selection==ARM_STANDALT;break;
  case BODY_WALK:
   if(!(l->action_flags&4)&&p->animation_changed){if(!select_and_copy_aim(c,0))return 0;break;}
   watch_aim=p->arm_selection==ARM_WALK||p->arm_selection==ARM_WALKALT;break;
  case BODY_WALK_LOOP:
   if(!(l->action_flags&4)&&p->animation_changed){if(!select_and_copy_aim(c,0))return 0;break;}
   watch_aim=p->arm_selection==ARM_WALKLOOP||p->arm_selection==ARM_WALKLOOPALT;break;
  case BODY_SHOOT:case BODY_WALK_SHOOT:case BODY_LOOP_SHOOT:
   if(p->animation_changed&&!select_and_copy_aim(c,0))return 0;break;
  case BODY_JUMP:case BODY_JUMP_SHOOT:
   if(p->animation_changed){if(!select_and_copy_aim(c,0))return 0;break;}
   watch_aim=p->arm_selection==ARM_JUMP;copy_first=0;break;
  case BODY_FALL:case BODY_FALL_SHOOT:
   if(p->animation_changed){if(!select_and_copy_aim(c,0))return 0;break;}
   watch_aim=p->arm_selection==ARM_FALL;copy_first=0;break;
  default:
   if(p->action>=0x34)return fail(c,12); /* Source TRAP, not idle fallback. */
   if(p->animation_changed&&!select_and_copy_aim(c,0))return 0;
   l->aim=0;break;
 }
 if(watch_aim&&p->actor.aim!=l->aim&&!select_and_copy_aim(c,copy_first))return 0;
 p->arm_busy=player_action_step(c)?0:255;
 p->actor.action_flags=(uint8_t)((p->actor.action_flags&251)|(l->action_flags&4));
 return !c->fault&&!l->animation.fault;
}
int player_body_motion(struct PlayerActionContext*c,unsigned motion,int restart){
 struct PlayerBodyState*p=c->body;
 if(motion>=PLAYER_BODY_MOTIONS||p->character>1)return fail(c,16);
 p->arm_selection=player_body_selectors[motion];
 if(restart){
  p->arm_busy=255;p->animation_changed=1;p->actor.movement_flags|=32;
  animation_begin(&p->actor.animation,player_body_entries[motion][p->character]);
 }
 return 1;
}
int player_body_animate(struct PlayerActionContext*c){
 /* $032AA8 completes when either the lower stream or the upper stream has
  * reached the source completion flag. Each keeps its independent phase. */
 struct PlayerActionContext owner=*c;owner.layer=&c->body->actor;
 int complete=player_action_step(&owner);c->fault=owner.fault;
 return complete||!c->body->arm_busy;
}
int player_body_stream(struct PlayerActionContext*c,unsigned motion){
 struct PlayerBodyState*p=c->body;
 if(motion>=PLAYER_BODY_MOTIONS||p->character>1)return fail(c,16);
 animation_begin(&p->actor.animation,player_body_entries[motion][p->character]);
 p->actor.movement_flags|=32;return 1;
}
