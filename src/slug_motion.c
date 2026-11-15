#include "slug_motion.h"
static int16_t word(int v){return (int16_t)(uint16_t)v;}
static int failure(struct SlugSharedState*s){if(!s->fault)s->fault=3;return -1;}
void slug_input_flags(struct SlugSharedState*s,struct PlayerSourceInput in){
 s->actor.hit.flags=(uint8_t)((s->actor.hit.flags&~48u)|((in.held&PB_UP)?16:0)|((in.held&PB_DOWN)?32:0));
}
void slug_motion_display(struct SlugSharedState*s,struct PlayerSourceInput in){
 /* $02A478: $02ABCC always clears carry, then $05CF1C tests any L/R. */
 if(in.held&(PB_LEFT|PB_RIGHT))s->actor.display_flags|=16;
 else s->actor.display_flags&=(uint8_t)~16u;
}
unsigned slug_jump_input(struct PlayerSourceInput in,unsigned layout,int alternate){
 int active;
 if(layout==0||layout==4)active=!!(in.actions&PB_JUMP);
 else if(alternate)active=(in.actions&PB_UP)||((in.held&PB_UP)&&(in.actions&(PB_LEFT|PB_RIGHT)));
 else active=!!(in.held&PB_UP);
 return active?1+((in.held>>2)&3):0;
}
int slug_exit_combo(struct PlayerSourceInput in){return (in.actions&(PB_FIRE|PB_JUMP))==(PB_FIRE|PB_JUMP);}
static int move(struct SlugSharedState*s,struct TerrainWorld*w,enum WeaponMove policy){
 struct WeaponActor*a=&s->actor;
 struct TerrainMotion m={.transform=&a->transform,.hit=&a->hit,.vx=a->vx,.vy=a->vy,.ax=a->ax,.ay=a->ay,
  .slope=a->ground_slope,.ground_tag=a->ground_tag,.movement_flags=a->movement_flags,
  .collision_flags=s->lifecycle_flags,.platform_ids=a->platform_ids,.player_boundary=!(s->status_flags&32),
  .previous_direction=a->previous_direction};
 int result=terrain_move_ground_origin(w,&m,policy,&s->ground_y_up);
 a->vx=m.vx;a->vy=m.vy;a->ax=m.ax;a->ay=m.ay;a->ground_slope=m.slope;a->ground_tag=m.ground_tag;
 a->platform_ids=m.platform_ids;return result<0?failure(s):result;
}
int slug_motion_step(struct SlugSharedState*s,struct TerrainWorld*w,struct PlayerSourceInput in,enum SlugMotionPolicy policy){
 if(s->fault||w->fault)return -1;if((unsigned)policy>SLMOVE_AIR)return failure(s);
 /* $028992 is ANDI #$EE,CCR; RTS. It has no missing body-collision call. */
 if(policy==SLMOVE_SYNC)return move(s,w,WMOVE_SYNC);
 if(policy==SLMOVE_GROUND_B||policy==SLMOVE_GROUND_C||policy==SLMOVE_GROUND_F||policy==SLMOVE_FALL_STEP)
  slug_chassis_sync_flags(s);
 if(policy!=SLMOVE_FALL_STEP){int stop=slug_chassis_wall(s);if(stop)w->stop_x=(int16_t)stop;}
 if(policy==SLMOVE_GROUND_E)slug_chassis_sync_flags(s);
 int result=move(s,w,(policy==SLMOVE_AIR||policy==SLMOVE_FALL_STEP)?WMOVE_BODY:WMOVE_GROUND);
 if(result<0)return -1;
 if(policy==SLMOVE_GROUND_RAW)return result;
 if(policy==SLMOVE_AIR){
  if(!result&&s->actor.vy<0){result=slug_ground_contact(s,w,0);if(result<0)return -1;}
  if(result)s->status_flags&=(uint8_t)~2u;else s->status_flags|=2;
  return !!result;
 }
 slug_input_flags(s,in);
 if((policy==SLMOVE_GROUND_B||policy==SLMOVE_GROUND_C||policy==SLMOVE_GROUND_E)&&(s->actor.hit.events&32))
  s->ground_y_up=word(s->ground_y_up+16);
 return slug_chassis_step_mismatch(s,w->camera_y);
}
