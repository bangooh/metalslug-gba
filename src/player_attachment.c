#include "player_body.h"
static int fail(struct PlayerBodyContext*c,unsigned reason){c->air.actions->fault=(uint8_t)reason;return 0;}
static unsigned horizontal(struct PlayerBodyContext*c){return ((c->air.input.held&PB_LEFT)?1u:0u)|((c->air.input.held&PB_RIGHT)?2u:0u);}
static int16_t clamp(int16_t v,int bound){return v>bound?(int16_t)bound:v<-bound?(int16_t)-bound:v;}
int player_attachment_tick(struct PlayerBodyContext*c){
 struct PlayerBodyState*p=c->air.actions->body;enum PlayerState state=(enum PlayerState)p->body_state;
 if(p->state_enter){
  p->state_enter=0;
  if(state==PST_JUMP_DROP){ /* $0368E0, also used by wall rebound. */
   if(!p->ammo&&p->actor.local_weapon!=WEAPON_PISTOL){
    if(!c->drop_weapon||!c->drop_weapon(c->effects,p))return fail(c,26);
    p->actor.local_weapon=WEAPON_PISTOL;p->ammo=10;
   }
   player_body_schedule(p,PST_JUMP);return player_body_tick(c);
  }
  if(state==PST_ATTACH_LAND){
   if(c->slug_invulnerability)p->actor.hit.invulnerable=30;p->actor.damage_timer=30;
   player_body_schedule(p,PST_LAND);return player_body_tick(c);
  }
  p->actor.action_flags&=(uint8_t)~14u;
  if(state==PST_HANG_RING){
   p->actor.transform.facing&=(uint8_t)~1u;p->actor.defense=&player_body_defenses[PDEF_SLUG];
   p->vx=p->vy=p->ax=p->ay=0;
   if(!player_body_set_pose(c,3,PMOTION_ATTACH,0,16,1))return 0;
  }else if(state==PST_ATTACH_JUMP){
   p->body_contact=1;p->actor.action_flags&=(uint8_t)~1u;p->actor.defense=&player_body_defenses[PDEF_FALL];
   p->vy=0x87f;p->ay=-0x91;p->vx=player_input_velocity[3][horizontal(c)];
   if(!player_body_set_pose(c,BODY_FALL,PMOTION_ATTACH_JUMP,0,0,1))return 0;
  }else return fail(c,32);
 }
 if(state==PST_HANG_RING){
  if(!player_body_frame_common(&c->air,0,0)||!player_body_synchronize(&c->air))return 0;
  if(!c->ring_follow)return fail(c,24);int16_t x,y;
  int found=c->ring_follow(c->effects,p->ring_key,&x,&y);if(found<0)return fail(c,24);
  if(found){ /* MOVE.W leaves both fractional bytes untouched. */
   transform_integer_offset(&p->actor.transform,(int16_t)(x-(p->actor.transform.x>>8)),(int16_t)(y-transform_y_up(&p->actor.transform)));
  }else{player_body_schedule(p,PST_JUMP);transform_integer_offset(&p->actor.transform,0,32);p->actor.hit.invulnerable=20;}
  (void)player_body_animate(c->air.actions);
  if(c->air.input.actions&PB_JUMP){player_body_schedule(p,PST_ATTACH_JUMP);transform_integer_offset(&p->actor.transform,0,32);p->actor.hit.invulnerable=20;}
 }else if(state==PST_ATTACH_JUMP){
  p->ax=player_input_velocity[1][horizontal(c)];p->vx=clamp(p->vx,0x200);if(p->vy<=0)p->vy=clamp(p->vy,0x780);
  if(!player_body_frame_common(&c->air,0,0))return 0;p->status_flags|=2;
  int landed=player_body_terrain_move(&c->air,WMOVE_JUMP);if(landed<0)return 0;
  if(landed){player_body_schedule(p,PST_ATTACH_LAND);p->status_flags&=(uint8_t)~2u;}
  if(p->vy<=0&&(p->actor.movement_flags&3)&&(p->actor.movement_flags&4))player_body_schedule(p,PST_JUMP_DROP);
  (void)player_body_animate(c->air.actions);
  if(p->status_flags&2){int out=player_body_outside(&c->air);if(out<0)return 0;if(out)player_body_schedule(p,PST_PIT);}
 }else return fail(c,32);
 if(c->air.actions->fault||p->actor.animation.fault||!player_body_death_publish(&c->air))return 0;
 if(c->air.transition==PAIR_DEATH)player_body_schedule(p,PST_DEATH);return 1;
}
