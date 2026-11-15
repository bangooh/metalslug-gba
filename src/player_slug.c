#include "player_body.h"
#include "slug_support.h"
static int fail(struct PlayerBodyContext*c){c->air.actions->fault=27;return 0;}
int player_slug_tick(struct PlayerBodyContext*c){
 struct PlayerBodyState*p=c->air.actions->body;struct SlugSharedState*s=c->slug;
 if(!s||s->fault)return fail(c);
 /* The source fixed Slug slot survives retirement. Its reset coordinates
  * send an attached rider through $036628 -> $037B8E (pit/death), never an
  * invalid-owner exception. Keep that transition explicit in native state. */
 if(!s->present){player_body_schedule(p,PST_PIT);return 1;}
 enum PlayerState state=(enum PlayerState)p->body_state;
 if(state!=PST_RIDE_SLUG&&state!=PST_SLUG_POSE)return fail(c);
 int pose=state==PST_SLUG_POSE;
 if(p->state_enter){
  p->state_enter=0;p->actor.action_flags&=(uint8_t)~14u;p->actor.defense=&player_body_defenses[PDEF_DEATH];
  if(pose){ /* $0366FE: only AX is changed; velocities are retained. */
   unsigned h=((c->air.input.held&PB_LEFT)?1u:0u)|((c->air.input.held&PB_RIGHT)?2u:0u);
   p->ax=player_input_velocity[0][h];
   if(!player_body_set_pose(c,2,PMOTION_SLUG_POSE,0,0,1))return 0;
   p->actor.transform.display_group=0;
  }else{ /* $0364A2 */
   p->actor.transform.facing&=(uint8_t)~1u;
   if(!player_body_set_pose(c,2,PMOTION_ATTACH,0,16,1))return 0;
   p->vx=p->vy=p->ax=p->ay=0;
  }
 }
 if(!player_body_frame_common(&c->air,0,0)||!player_body_synchronize(&c->air))return 0;
 /* Both source stores are WORD writes; preserve the player's fractions. */
 transform_integer_offset(&p->actor.transform,
  (int16_t)((s->actor.transform.x>>8)-(p->actor.transform.x>>8)),
  (int16_t)(transform_y_up(&s->actor.transform)+(pose?0:1)-transform_y_up(&p->actor.transform)));
 int complete=player_body_animate(c->air.actions);
 if(pose&&complete)player_body_schedule(p,PST_RIDE_SLUG);
 /* $03652E shared tail. Jump from DOWN/dead skips combo and scene tests. */
 slug_rider_refresh(s,p);
 if(!(s->status_flags&2)&&((c->air.input.held&PB_DOWN)||(s->actor.hit.flags&1))){
  if(!(p->actor.action_flags&4)&&(c->air.input.actions&PB_GRENADE)&&
    (s->status_flags&16)&&!(s->actor.hit.flags&1))player_body_schedule(p,PST_SLUG_POSE);
  if((c->air.input.actions&PB_JUMP)&&(s->status_flags&16)){
   player_body_schedule(p,PST_ATTACH_JUMP);transform_integer_offset(&p->actor.transform,0,32);goto bounds;
  }
 }
 if(!(s->status_flags&2)&&((c->air.input.actions&(PB_FIRE|PB_JUMP))==(PB_FIRE|PB_JUMP))){
  player_body_schedule(p,PST_ATTACH_JUMP);transform_integer_offset(&p->actor.transform,0,32);
  p->actor.hit.invulnerable=p->actor.damage_timer=60;
 }
 if(!c->slug_riders_enabled)player_body_schedule(p,PST_ATTACH_JUMP);
bounds:
 {int outside=player_body_outside(&c->air);if(outside<0)return 0;if(outside)player_body_schedule(p,PST_PIT);}
 if(c->air.actions->fault||p->actor.animation.fault||!player_body_death_publish(&c->air))return 0;
 if(c->air.transition==PAIR_DEATH)player_body_schedule(p,PST_DEATH);return 1;
}
