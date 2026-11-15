#include "campaign.h"
#include "../generated/mission_data.h"
#include "terrain_edits.h"
struct Campaign campaign;
void campaign_reset(void){
 campaign=(struct Campaign){0};combat_reset();actors_reset();terrain_edits_reset();mission_init(&campaign.mission,1);
 mission_start(&campaign.mission,MISSION_SOURCE_BASE);
}
int campaign_spawn(void*user,const struct MissionSpawn*s){
 (void)user;
 // These are nonvisual original events, not silently ignored templates.
 if(s->template==0x13c){campaign.combo_timer=campaign.combo_count=0;campaign.combo_active=1;return 1;}
 if(s->template==0x147){campaign.continue_count=s->params[0]==255?0:s->params[0];return 1;}
 if(s->template==0x125){if(combat_spawn(0,s))return 1;}
 else if(actors_spawn(s,&campaign.mission.rng,game.camera_x*4/3))return 1;
 campaign.failed_source=s->source;campaign.failed_template=s->template;campaign.unsupported=1;return 0;
}
void campaign_step(struct Player*p,unsigned keys,unsigned pressed){
 combat_begin_frame(p);campaign.ticks++;
 if(campaign.combo_active){ // $09B82C, signed BYTE countdown and clamp.
  campaign.combo_timer--;if((int8_t)campaign.combo_timer<=0)campaign.combo_timer=campaign.combo_count=0;
  if((int8_t)campaign.combo_count>14)campaign.combo_count=14;
 }
 int cx=game.camera_x*4/3,cy=game.camera_y*4/3;
 struct MissionInput in={.players={{(int16_t)((p->x>>8)-cx),(int16_t)((p->y>>8)-cy),1,0}},
   .camera_x=(int16_t)cx,.camera_y=(int16_t)cy,.progress=(uint16_t)cx,
   .enemies=(uint16_t)actors_enemies(),.active=1,.player_count=1};
 mission_tick(&campaign.mission,&in,campaign_spawn,0);
 actors_step(p,&campaign.mission.rng,cx,cy);combat_step(p,keys,pressed);
 if(combat.player_dead&&!--combat.player_dead){
  // Native temporary respawn policy, not the unported original life/continue flow.
  combat.player.hp=1;combat.player.flags=0;combat.player.invulnerable=90;campaign.respawns++;
 }
}
