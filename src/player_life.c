#include "player_body.h"
/* $0375D2..$037C72. These phases are native states, not CPU addresses. */
enum DeathPhase {DL_MUSIC,DL_ALT,DL_AIR,DL_AIR_FX,DL_LAND_ENTER,DL_LAND_FX_ENTER,
 DL_LAND,DL_LAND_FX,DL_GENERIC,DL_TIMED,DL_DESPAWN_ENTER,DL_DESPAWN,DL_PIT};
static struct PlayerBodyState*body(struct PlayerBodyContext*c){return c->air.actions->body;}
static int fail(struct PlayerBodyContext*c,unsigned reason){c->air.actions->fault=(uint8_t)reason;return 0;}
static int visual(struct PlayerBodyContext*c,enum PlayerVisualEffect effect){
 return c->visual&&c->visual(c->effects,effect)?1:fail(c,29);
}
static int sound(struct PlayerBodyContext*c,unsigned command){
 struct PlayerActionContext*a=c->air.actions;return a->sound&&a->sound(a->world,(uint16_t)command,0)?1:fail(c,1);
}
/* $0267E2, including the source's two fractional coordinate bytes. */
static void reset(struct PlayerBodyState*p){p->vx=p->vy=p->ax=p->ay=0;transform_integer_copy(&p->actor.transform,&p->actor.transform);}
static int pose(struct PlayerBodyContext*c,unsigned motion){
 struct PlayerBodyState*p=body(c);p->actor.action_flags&=(uint8_t)~14u;p->actor.hit.flags|=1;
 if(!player_body_set_pose(c,0,motion,0,0,1))return 0;reset(p);return 1;
}
static void toss(struct PlayerBodyState*p){
 p->vx=p->actor.transform.facing&1?0x117:-0x117;p->vy=0x457;p->ay=-0x65;p->ax=0;
}
static int bounds(struct PlayerBodyContext*c){
 int outside=player_body_outside(&c->air);if(outside<0)return 0;
 if(outside)player_body_schedule(body(c),PST_PIT);return 1;
}
static int animate(struct PlayerBodyContext*c,int*complete){
 *complete=player_body_animate(c->air.actions);
 return !c->air.actions->fault&&!body(c)->actor.animation.fault;
}
static int death_enter(struct PlayerBodyContext*c){
 struct PlayerBodyState*p=body(c);unsigned motion;
 p->death_class=c->air.death_route;
 switch(p->death_class){
  case PDEATH_FALL:{ /* $037684 chooses an alternate, otherwise $0375D2. */
   if(!c->air.actions->rng)return fail(c,3);
   int ordinary=source_random_word(c->air.actions->rng)&11;p->role=255;
   if(ordinary){
    if(!visual(c,PVIS_DEATH_SMOKE)||!sound(c,0x1053))return 0;
    if(!player_body_set_pose(c,0,PMOTION_DEATH_SMOKE,p->arm_offset_x,p->arm_offset_y_up,1))return 0;
    p->life_phase=DL_MUSIC;return bounds(c); /* Entry has no animation tick. */
   }
   if(!visual(c,PVIS_DEATH_ALT)||!visual(c,PVIS_DEATH_SMOKE_HIGH)||!sound(c,0x1053))return 0;
   motion=PMOTION_DEATH_ALT;p->life_phase=DL_ALT;break;
  }
  case PDEATH_FALL_FX:
   if(!visual(c,PVIS_RIPPLE)||!visual(c,PVIS_SPLASH))return 0;
   motion=PMOTION_DEATH_AIR_FX;p->life_phase=DL_AIR_FX;break;
  case PDEATH_AIR:motion=PMOTION_DEATH_AIR;p->life_phase=DL_AIR;break;
  case PDEATH_DEBUG:
   if(c->blood_enabled&&!visual(c,PVIS_BLOOD))return 0;
   if(!sound(c,0x10fb))return 0;
   /* $037AF2 falls through into the timed handler. */
   motion=PMOTION_DEATH_TIMED;p->life_phase=DL_TIMED;break;
  case PDEATH_TIMED:motion=PMOTION_DEATH_TIMED;p->life_phase=DL_TIMED;break;
  case PDEATH_PRIORITY:p->actor.transform.priority=0xe000;
   /* fall through */
  case PDEATH_GENERIC:motion=PMOTION_DEATH_GENERIC;p->life_phase=DL_GENERIC;break;
  default:return fail(c,30);
 }
 if(!pose(c,motion))return 0;
 if(p->life_phase==DL_AIR||p->life_phase==DL_AIR_FX||p->life_phase==DL_GENERIC)toss(p);
 if(p->life_phase==DL_GENERIC||p->life_phase==DL_TIMED)p->life_timer=40;
 return 1;
}
static int retire(struct PlayerBodyContext*c){
 struct PlayerBodyState*p=body(c);p->actor.lifecycle_flags&=(uint8_t)~128u;p->life_retired=1;
 return c->retire&&c->retire(c->effects)?1:fail(c,31);
}
static int death_frame(struct PlayerBodyContext*c){
 struct PlayerBodyState*p=body(c);int done=0;
 if(p->life_phase==DL_MUSIC){
  p->life_phase=DL_DESPAWN_ENTER;
  if(!sound(c,p->character?0x1087:0x1123))return 0;return bounds(c);
 }
 if(p->life_phase==DL_DESPAWN_ENTER){
  reset(p);p->arm_offset_x=p->arm_offset_y_up=0;p->life_timer=40;p->actor.defense=0;p->life_phase=DL_DESPAWN;
 }
 if(p->life_phase==DL_DESPAWN||p->life_phase==DL_PIT){
  if(p->life_phase==DL_PIT){if(player_body_terrain_move(&c->air,WMOVE_POINT)<0)return 0;}
  else if(!player_body_synchronize(&c->air))return 0;
  if(!--p->life_timer){if(p->life_phase==DL_PIT)reset(p);return retire(c);}return 1;
 }
 if(p->life_phase==DL_LAND_ENTER||p->life_phase==DL_LAND_FX_ENTER){
  int fx=p->life_phase==DL_LAND_FX_ENTER;
  if(!pose(c,fx?PMOTION_DEATH_LANDED_FX:PMOTION_DEATH_LANDED))return 0;p->life_phase=fx?DL_LAND_FX:DL_LAND;
 }
 if(!player_body_frame_common(&c->air,1,0))return 0;
 unsigned phase=p->life_phase;
 if(phase==DL_ALT){if(!player_body_synchronize(&c->air)||!animate(c,&done))return 0;
  if(done)p->life_phase=DL_DESPAWN_ENTER;return bounds(c);}
 if(phase==DL_LAND||phase==DL_LAND_FX||phase==DL_TIMED){
  int landed=player_body_terrain_move(&c->air,WMOVE_GROUND);if(landed<0)return 0;
  if(phase==DL_TIMED&&landed)p->vx=p->vy=0;
 }else if(phase==DL_AIR||phase==DL_AIR_FX||phase==DL_GENERIC){
  p->status_flags|=2;int landed=player_body_terrain_move(&c->air,WMOVE_BODY);if(landed<0)return 0;
  if(landed){if(phase==DL_GENERIC)p->vx=p->vy=0;else p->life_phase=phase==DL_AIR?DL_LAND_ENTER:DL_LAND_FX_ENTER;}
  /* $037806/$037974 can route a falling corpse into scene-1 water death. */
  if(phase!=DL_GENERIC&&c->air.terrain->scene==1&&
    (int16_t)(transform_y_up(&p->actor.transform)+c->air.terrain->camera_y)<0x10c){
   struct TerrainMotion m={.transform=&p->actor.transform,.hit=&p->actor.hit,.collision_flags=p->collision_flags};struct TerrainSample s;
   if(!terrain_lookup(c->air.terrain,&m,(int16_t)(p->actor.transform.x>>8),transform_y_up(&p->actor.transform),&s))return fail(c,22);
   if(s.raw==0x40){if(!player_body_death_publish(&c->air))return 0;
    if(c->air.transition==PAIR_DEATH)player_body_schedule(p,PST_DEATH);}
  }
 }else return fail(c,30);
 if(!animate(c,&done))return 0;
 if(phase==DL_GENERIC||phase==DL_TIMED){if(done||!--p->life_timer)p->life_phase=DL_DESPAWN_ENTER;}
 else if(phase==DL_LAND||phase==DL_LAND_FX){if(done)p->life_phase=DL_DESPAWN_ENTER;}
 else return bounds(c);
 return 1;
}
static int spawn_enter(struct PlayerBodyContext*c,enum PlayerState state){
 struct PlayerBodyState*p=body(c);
 switch(state){
  case PST_SPAWN_FALL:
   transform_integer_offset(&p->actor.transform,0,3);p->actor.defense=0;p->actor.lifecycle_flags|=128;p->vy=-0x400;
   return player_body_set_pose(c,4,PMOTION_SPAWN_FALL,p->arm_offset_x,p->arm_offset_y_up,1);
  case PST_SPAWN_LAND:case PST_SPAWN_HOLD:
   p->arm_offset_x=p->arm_offset_y_up=0;p->actor.lifecycle_flags|=128;p->vx=p->ax=0;
   if(state==PST_SPAWN_HOLD)p->vy=p->ay=0;return 1;
  case PST_SPAWN_PARACHUTE:
   if(!visual(c,PVIS_PARACHUTE))return 0;
   p->vy=-256;p->actor.defense=0;return player_body_set_pose(c,4,PMOTION_JUMP_STANDING,0,0,1);
  case PST_SPAWN_FREEFALL:
   if(!visual(c,PVIS_ICON))return 0;
   p->vx=p->ax=0;p->ay=-128;p->actor.action_flags&=(uint8_t)~14u;
   p->actor.defense=0;p->actor.damage_timer=p->actor.hit.invulnerable=0;p->air_phase=PAIR_FALL;
   return player_body_set_pose(c,BODY_FALL,PMOTION_JUMP_STANDING,0,0,1);
  case PST_SPAWN_DONE:
   if(!visual(c,PVIS_ICON))return 0;
   p->actor.damage_timer=p->actor.hit.invulnerable=80;player_body_schedule(p,PST_LAND);return 1;
  default:return fail(c,30);
 }
}
static int spawn_frame(struct PlayerBodyContext*c,enum PlayerState state){
 struct PlayerBodyState*p=body(c);int done;
 if(state==PST_SPAWN_FREEFALL){
  p->actor.damage_timer|=60;p->actor.hit.invulnerable|=60;p->actor.hit.flags|=64;
  if(!player_air_step(&c->air))return 0;
  switch(c->air.transition){
   case PAIR_LAND:player_body_schedule(p,PST_LAND);p->actor.damage_timer=p->actor.hit.invulnerable=80;break;
   case PAIR_RIDE_SLUG:player_body_schedule(p,PST_RIDE_SLUG);break;
   case PAIR_HANG_RING:player_body_schedule(p,PST_HANG_RING);break;
   case PAIR_REJUMP:player_body_schedule(p,PST_JUMP_DROP);p->actor.damage_timer=p->actor.hit.invulnerable=80;break;
   case PAIR_PIT:player_body_schedule(p,PST_PIT);break;
   case PAIR_DEATH:player_body_schedule(p,PST_DEATH);break;
   default:break;
  }
  return 1;
 }
 if(state==PST_SPAWN_FALL||state==PST_SPAWN_PARACHUTE)p->actor.hit.flags|=64;
 if(state!=PST_SPAWN_LAND)p->status_flags|=2;
 if(!player_body_frame_common(&c->air,1,0)||player_announce_weapon(c->air.actions)<0)return 0;
 if(state==PST_SPAWN_FALL){
  player_body_schedule(p,PST_SPAWN_LAND);p->status_flags&=(uint8_t)~2u;
  int hit=player_body_terrain_move(&c->air,WMOVE_BODY);if(hit<0)return 0;
  if(!hit){hit=player_body_terrain_move(&c->air,WMOVE_BODY);if(hit<0)return 0;}
  if(!hit){player_body_schedule(p,PST_SPAWN_HOLD);p->status_flags|=2;}
  return animate(c,&done)&&bounds(c);
 }
 if(state==PST_SPAWN_LAND){
  if(player_body_terrain_move(&c->air,WMOVE_GROUND)<0||!animate(c,&done))return 0;
  if(done){player_body_schedule(p,PST_STAND);if(!visual(c,PVIS_ICON))return 0;p->actor.damage_timer=p->actor.hit.invulnerable=80;}
  int contact=player_body_foot_contact(&c->air);if(contact<0)return 0;
  if(!contact)player_body_schedule(p,PST_SPAWN_FREEFALL);return 1;
 }
 if(state==PST_SPAWN_HOLD){
  if(player_body_terrain_move(&c->air,WMOVE_BODY)<0||!animate(c,&done))return 0;
  if(done)player_body_schedule(p,PST_SPAWN_FREEFALL);return 1;
 }
 if(state==PST_SPAWN_PARACHUTE){
  if(p->vy>0x400)p->vy=0x400;if(p->vy<-0x400)p->vy=-0x400;
  int hit=player_body_terrain_move(&c->air,WMOVE_BODY);if(hit<0)return 0;
  if(hit){player_body_schedule(p,PST_SPAWN_DONE);p->status_flags&=(uint8_t)~2u;}
  p->actor.transform.priority=(uint16_t)((p->actor.transform.priority&0xffe3)|0x9ff0);
  if(!animate(c,&done))return 0;
  if(player_body_jump_pressed(&c->air,0)||(int16_t)(transform_y_up(&p->actor.transform)+c->air.terrain->camera_y)<=0x170)
   player_body_schedule(p,PST_SPAWN_FREEFALL);
  return bounds(c);
 }
 return fail(c,30);
}
int player_life_tick(struct PlayerBodyContext*c){
 struct PlayerBodyState*p=body(c);if(p->life_retired)return 1;
 if(!c->air.terrain)return fail(c,22);enum PlayerState state=(enum PlayerState)p->body_state;
 if(p->state_enter){
  p->state_enter=0;
  if(state==PST_PIT){
   if(!(p->actor.hit.flags&1)&&!sound(c,p->character?0x1087:0x1123))return 0;
   p->actor.hit.flags|=1;p->arm_offset_x=p->arm_offset_y_up=0;p->actor.defense=0;p->life_timer=6;p->life_phase=DL_PIT;
  }else if(state==PST_DEATH){
   if(!death_enter(c))return 0;
   if(p->life_phase==DL_MUSIC)return 1;
  }else {
   if(!spawn_enter(c,state))return 0;
   if(p->state_enter)return player_body_tick(c); /* Direct source JMP into landing. */
  }
 }
 return state==PST_PIT||state==PST_DEATH?death_frame(c):spawn_frame(c,state);
}
