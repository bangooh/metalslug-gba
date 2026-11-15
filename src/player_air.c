#include "player_air.h"
#include "player_body.h"
static int fail(struct PlayerAirContext*c,unsigned code){c->actions->fault=(uint8_t)code;return 0;}
static int16_t clamp(int16_t value,int limit){return value>limit?(int16_t)limit:value<-limit?(int16_t)-limit:value;}
static struct TerrainMotion motion(struct PlayerBodyState*p){
 return (struct TerrainMotion){.transform=&p->actor.transform,.hit=&p->actor.hit,
  .vx=p->vx,.vy=p->vy,.ax=p->ax,.ay=p->ay,.slope=p->ground_slope,.ground_tag=p->ground_tag,
  .movement_flags=p->actor.movement_flags,.collision_flags=p->collision_flags,.platform_ids=p->platform_ids,.player_boundary=1,
  .previous_direction=p->previous_direction};
}
static void update_motion(struct PlayerBodyState*p,const struct TerrainMotion*m){
 p->vx=m->vx;p->vy=m->vy;p->ax=m->ax;p->ay=m->ay;
 p->ground_slope=m->slope;p->ground_tag=m->ground_tag;p->platform_ids=m->platform_ids;
}
static unsigned horizontal(struct PlayerSourceInput input){
 return ((input.held&PB_LEFT)?1:0)|((input.held&PB_RIGHT)?2:0);
}
int player_body_terrain_move(struct PlayerAirContext*c,enum WeaponMove policy){
 if(!c->terrain){fail(c,22);return -1;}
 struct TerrainMotion m=motion(c->actions->body);struct WeaponContact hit;
 int result=terrain_move(c->terrain,&m,policy,&hit);update_motion(c->actions->body,&m);
 if(result<0)fail(c,22);return result;
}
int player_body_synchronize(struct PlayerAirContext*c){
 if(!c->terrain)return fail(c,22);struct TerrainMotion m=motion(c->actions->body);
 if(!terrain_synchronize(c->terrain,&m))return fail(c,22);update_motion(c->actions->body,&m);return 1;
}
int player_body_foot_contact(struct PlayerAirContext*c){
 if(!c->terrain){fail(c,22);return -1;}
 struct TerrainMotion m=motion(c->actions->body);
 int result=terrain_foot_contact(c->terrain,&m);update_motion(c->actions->body,&m);
 if(result<0)fail(c,22);return result;
}
int player_body_outside(struct PlayerAirContext*c){
 if(!c->terrain){fail(c,22);return -1;}
 struct PlayerBodyState*p=c->actions->body;
 const int16_t*b=player_ground_bounds[c->terrain->scene==3?0:1];
 int16_t x=(int16_t)((p->actor.transform.x>>8)-c->terrain->camera_x);
 int16_t y=(int16_t)(transform_y_up(&p->actor.transform)+c->terrain->camera_y);
 p->actor.hit.flags|=128;
 return x<b[0]||x>320+b[1]||y<256-b[3]||y>496-b[2];
}
int player_body_jump_pressed(const struct PlayerAirContext*c,int crouching){
 /* $033034/$033080: mode 0/4 uses action-latched jump. Other modes use
  * UP, with a directional action edge required when already crouched. */
 if(c->control_mode==0||c->control_mode==4)return !!(c->input.actions&PB_JUMP);
 if(!crouching)return !!(c->input.held&PB_UP);
 return !!((c->input.actions&PB_UP)||((c->input.held&PB_UP)&&(c->input.actions&(PB_LEFT|PB_RIGHT))));
}
int player_body_frame_common(struct PlayerAirContext*c,int align_priority,int vertical_only){
 struct PlayerBodyState*p=c->actions->body;
 hit_begin_attack(&p->actor.hit);
 if(align_priority)p->actor.transform.priority&=(uint16_t)~3u;
 if(!player_step_weapon_flash(p))return fail(c,21);
 /* $02A720 marks input choices in collision flags, not damage flags. */
 p->actor.hit.flags&=(uint8_t)~48u;
 if(c->input.held&PB_UP)p->actor.hit.flags|=16;
 if(c->input.held&PB_DOWN)p->actor.hit.flags|=32;
 p->direction_change=player_read_aim_direction(p,c->input,vertical_only);
 return 1;
}
static int sound(struct PlayerAirContext*c,uint16_t command){
 struct PlayerActionContext*a=c->actions;
 return a->sound&&a->sound(a->world,command,0)?1:fail(c,1);
}
int player_body_death_publish(struct PlayerAirContext*c){
 if(!c->terrain)return fail(c,22);
 struct PlayerBodyState*p=c->actions->body;struct HitActor*hit=&p->actor.hit;
 if(c->invincible){hit->flags&=(uint8_t)~9u;hit->hp=1;return 1;}
 if(!c->campaign_timer){hit->flags|=1;hit->hp=0;}
 struct TerrainMotion m=motion(p);
 if(c->terrain->scene==1&&(int16_t)(transform_y_up(&p->actor.transform)+c->terrain->camera_y)<0x10c){
  struct TerrainSample s;
  if(!terrain_lookup(c->terrain,&m,(int16_t)(p->actor.transform.x>>8),transform_y_up(&p->actor.transform),&s))return fail(c,22);
  if(s.raw==0x40){hit->flags|=9;hit->hp=0;hit->last_kind=5;}
 }
 if(!(hit->flags&1))return 1;
 int contact=terrain_foot_contact(c->terrain,&m);update_motion(p,&m);
 if(contact<0)return fail(c,22);
 c->transition=PAIR_DEATH;c->death_route=(uint8_t)player_death_route(hit->last_kind,!contact);
 return hit->last_kind==5?1:sound(c,p->character?0x1087:0x1123);
}
static int reset_fall_pose(struct PlayerAirContext*c){
 struct PlayerBodyState*p=c->actions->body;
 p->arm_offset_x=p->arm_offset_y_up=0;p->actor.action_flags&=(uint8_t)~14u;
 p->action=BODY_FALL;p->body_contact=1;p->actor.defense=&player_body_defenses[PDEF_AIR];p->air_phase=PAIR_FALL;
 return player_body_motion(c->actions,PMOTION_JUMP_STANDING,1);
}
int player_air_begin_jump(struct PlayerAirContext*c){
 struct PlayerBodyState*p=c->actions->body;
 p->arm_offset_x=p->arm_offset_y_up=0;p->actor.action_flags&=(uint8_t)~14u;
 p->action=p->vx?BODY_JUMP:BODY_FALL;
 if(!player_body_motion(c->actions,p->vx?PMOTION_JUMP_RUNNING:PMOTION_JUMP_STANDING,1))return 0;
 p->direction_change=player_read_aim_direction(p,c->input,0);
 if(p->aim_direction==8){
  p->actor.transform.priority&=(uint16_t)~1u;p->actor.aim=3;
  int standing=p->action==BODY_FALL;
  p->arm_offset_x=standing?0:2;p->arm_offset_y_up=standing?7:2;
  if(!player_body_motion(c->actions,standing?PMOTION_AIR_STANDING_DOWN:PMOTION_AIR_RUNNING_DOWN,1))return 0;
 }
 p->vy=0x9cd;p->ay=-0xc1;p->jump_hold=5;p->air_phase=PAIR_JUMP_WINDOW;
 p->body_contact=1;p->actor.defense=&player_body_defenses[PDEF_AIR];return player_air_step(c);
}
int player_air_begin_fall(struct PlayerAirContext*c,enum PlayerAirPhase phase){
 struct PlayerBodyState*p=c->actions->body;
 if(phase!=PAIR_FALL&&phase!=PAIR_FALL_DELAY&&phase!=PAIR_FALL_HOLD)return fail(c,23);
 p->vx=p->ax=0;p->ay=-128;p->actor.action_flags&=(uint8_t)~14u;
 if(phase==PAIR_FALL){if(!reset_fall_pose(c))return 0;}
 else if(phase==PAIR_FALL_DELAY)p->actor.animation.delay=12;
 p->air_phase=(uint8_t)phase;return player_air_step(c);
}
static int air_bounds(struct PlayerAirContext*c){
 struct PlayerBodyState*p=c->actions->body;
 if(p->status_flags&2){
  if(player_body_outside(c)>0)c->transition=PAIR_PIT;
 }
 return player_body_death_publish(c);
}
int player_air_step(struct PlayerAirContext*c){
 struct PlayerActionContext*actions=c->actions;struct PlayerBodyState*p=actions->body;
 if(actions->fault)return 0;if(!c->terrain)return fail(c,22);
 c->transition=PAIR_CONTINUE;
 if(p->air_phase==PAIR_JUMP_WINDOW){
  p->jump_hold--;
  if(p->jump_hold==4&&(c->input.held&PB_JUMP))p->air_phase=PAIR_JUMP;
  if(!p->jump_hold){p->vy=(int16_t)(p->vy>>2);p->air_phase=PAIR_JUMP;}
 }else if(p->air_phase==PAIR_FALL_DELAY||p->air_phase==PAIR_FALL_HOLD){
  if((p->air_phase==PAIR_FALL_DELAY&&(int8_t)p->actor.animation.delay<=0)||
    (c->input.actions&(PB_FIRE|PB_GRENADE))){if(!reset_fall_pose(c))return 0;}
 }
 if(p->air_phase>PAIR_FALL_HOLD)return fail(c,23);
 int jumping=p->air_phase==PAIR_JUMP_WINDOW||p->air_phase==PAIR_JUMP;
 /* Original spawn fall $036D20 bypasses horizontal input at $036DCA. */
 p->ax=p->body_state==PST_SPAWN_FREEFALL?0:player_input_velocity[0][horizontal(c->input)];
 if((c->input.actions&(PB_FIRE|PB_GRENADE))&&p->ax){
  if(p->ax>0)p->actor.transform.facing&=(uint8_t)~1u;else p->actor.transform.facing|=1;
 }
 if(!p->ax&&p->vx)p->ax=p->vx>0?-24:24;
 p->vx=clamp(p->vx,0x300);if(p->vy<=0)p->vy=clamp(p->vy,jumping?0x780:0x400);
 /* The jump path sets these after frame-common; the fall path before it.
  * Frame-common does not consume $8D, so the typed operation is shared. */
 p->status_flags=(uint8_t)((p->status_flags|2)&~32u);
 if(!player_body_frame_common(c,0,0))return 0;
 int16_t saved_vx=p->vx,saved_vy=p->vy,saved_ay=p->ay;
 if(jumping&&c->pause_motion)p->vx=p->vy=p->ay=0;
 struct TerrainMotion m=motion(p);struct WeaponContact contact;
 int landed=terrain_move(c->terrain,&m,jumping?WMOVE_JUMP:WMOVE_BODY,&contact);update_motion(p,&m);
 if(landed<0)return fail(c,22);
 if(landed){c->transition=PAIR_LAND;p->status_flags&=(uint8_t)~34u;}
 if(player_body_animate(actions))p->actor.action_flags&=(uint8_t)~2u;
 if(actions->fault||p->actor.animation.fault)return 0;
 int locked=c->transition==PAIR_CONTINUE&&(p->air_phase==PAIR_FALL_DELAY||p->air_phase==PAIR_FALL_HOLD);
 if(!locked&&!player_air_neutral_pose(actions))return 0;
 if(jumping&&c->pause_motion){p->vx=saved_vx;p->vy=saved_vy;p->ay=saved_ay;}
 if(p->vy<=0){
  p->status_flags|=32;
  if(p->actor.action_flags&1){c->transition=PAIR_RIDE_SLUG;return air_bounds(c);}
  if(p->status_flags&2){
   int16_t key=-1;
   if(!c->ring_find)return fail(c,24);
   int pending=c->ring_find(c->world,p,&key);if(pending<0)return fail(c,24);
   if(pending&&key>=0){
    p->ring_key=(uint8_t)key;c->transition=PAIR_HANG_RING;
    if(!c->ring_claim||!c->ring_claim(c->world,p,p->ring_key))return fail(c,24);
    p->status_flags&=(uint8_t)~32u;return air_bounds(c);
   }
  }
  if((p->actor.movement_flags&3)&&(p->actor.movement_flags&4)){
   c->transition=PAIR_REJUMP;p->vx=(p->actor.movement_flags&2)?0x400:-0x400;p->ax=0;return air_bounds(c);
  }
 }
 if(!locked){
  enum PlayerIntent intent=player_select_action(actions,c->input,0);
  if(actions->fault||!player_air_action_pose(actions,intent,(uint8_t)horizontal(c->input)))return 0;
 }
 return air_bounds(c);
}
