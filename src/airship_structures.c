#include "airship_internal.h"
#include "fort_internal.h"
static int ladder_stop(const struct NativeEntity*e){return e->kind==ENTITY_ENEMY?e->payload.enemy.airship.ladder:-1;}
static int rank(struct WeaponWorld*w,struct EnemyActor*q){struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid){enemy_fault(w);return 0;}return airship_rank[(interaction_rank(w,&q->actor,runtime_owner_airship[0]))*8+(q->source_flags&3)*2+((r->player_count_flag&2)>>1)];}
static int tower_patch(struct WeaponWorld*w,struct NativeEntity*e,unsigned stage){
 unsigned v=e->payload.enemy.airship.variant;
 static const uint8_t patches[2][2]={{ASV_P2EF3E8,ASV_P2EF44C},{ASV_P2EF514,ASV_P2EF578}};
 return as_patch(w,patches[v][0]+stage)&&as_patch(w,patches[v][1]+stage)&&as_stamp(w,e,ASS_S2EF6A4+stage)&&as_pattern(w,e,ASP_P2EF286)!=0;
}
static int tower_destroy(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;unsigned v=q->airship.variant;
 static const int16_t offsets[6][2]={{192,-128},{48,-48},{112,-16},{144,-64},{192,-48},{256,-64}};
 static const uint8_t patches[2][3]={{ASV_P2EF438,ASV_P2EF424,ASV_P2EF488},{ASV_P2EF564,ASV_P2EF550,ASV_P2EF5B4}};
 static const uint8_t stamps[2][5]={{ASS_S2EF63E,ASS_S2EF66C,ASS_S2EF688,ASS_S2EF696,ASS_S2EF704},{ASS_S2EF740,ASS_S2EF77A,ASS_S2EF79A,ASS_S2EF704,255}};
 if(!interaction_sound(w,0x102f))return 0;
 for(unsigned i=0;i<6;i++)if(!as_blast(w,e,offsets[i][0],offsets[i][1]))return 0;
 if(!as_pattern(w,e,ASP_P2EF286))return 0;
 for(unsigned i=0;i<3;i++)if(!as_patch(w,patches[v][i]))return 0;
 for(unsigned i=0;i<5&&stamps[v][i]!=255;i++)if(!as_stamp(w,e,stamps[v][i]))return 0;
 q->airship.watch=q->airship.ladder=255;
 if(!as_child(w,e,AS_TOWER_BLAST,ENTITY_FIRST_CHILD,32,0))return 0;
 struct NativeEntity*n=as_child(w,e,AS_SMOKE,ENTITY_FIRST_CHILD,144,-16);if(!n)return 0;n->payload.enemy.airship.variant=(uint8_t)v;
 q->actor.phase=255;if(!as_lamp(w,q,v,0))return 0;entity_delete(w->entities,e);return 0;
}
int as_structures_enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned s=a->state,v=q->airship.variant;struct NativeEntity*n;
 switch(s){
 case AS_PROP:a->palette=v==2?0xd7:0xca;q->airship.margin=v==2?144:112;a->transform.facing=0;a->transform.priority=v==2?0:0xc000;as_start(q,v==2?ASA_PROP2:(enum AirshipAnimation)(ASA_PROP0+v));break;
 case AS_CAMP:
  if(!as_child(w,e,AS_DEPOT,ENTITY_LAST_CHILD,96,36))return 0;
  for(unsigned i=0;i<3;i++){n=as_child(w,e,AS_TENT,ENTITY_LAST_CHILD,i==0?0:i==1?56:128,32);if(!n)return 0;n->payload.enemy.airship.variant=(uint8_t)i;}
  as_schedule(e,AS_DELETE);return 0;
 case AS_TENT:if(v>=3)return enemy_fault(w);a->palette=0xcc;q->airship.margin=64;a->transform.facing=0;a->transform.priority=0;a->hit.hp=20;as_start_entry(q,airship_tent[2*v]);break;
 case AS_TENT_WRECK:if(!as_score(w,a,0x500))return 0;as_start_entry(q,airship_tent[2*v+1]);break;
 case AS_BUNKER:a->palette=0xcd;q->airship.variant=q->params[0];if(q->airship.variant>=4)return enemy_fault(w);q->airship.margin=64;a->hit.hp=40;a->transform.facing=0;a->transform.priority=0;as_start(q,ASA_BUNKER);as_defense(q,ASB_BUNKER);break;
 case AS_DEPOT:a->palette=0xcb;q->airship.margin=48;a->hit.hp=100;a->transform.facing=0;a->transform.priority=0;as_start(q,ASA_DEPOT);break;
 case AS_DEPOT_BLAST:as_attack(q,ASB_DEPOT_BLAST);hit_begin_attack(&a->hit);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;as_schedule(e,AS_DEPOT_END);return 0;
 case AS_DEPOT_END:hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;entity_delete(w->entities,e);return 0;
 case AS_CRATE:if(v>=5)return enemy_fault(w);a->palette=0xd6;q->airship.margin=64;a->hit.hp=20;a->transform.facing=0;a->transform.priority=0x8000;as_reset(a);if(interaction_move(w,a,WMOVE_POINT)<0)return 0;as_start_entry(q,airship_crate[v]);break;
 case AS_CRATE_WRECK:if(!as_score(w,a,0x500))return 0;as_defense(q,65535);as_start_entry(q,airship_crate_wreck[v]);break;
 case AS_SOLID:a->palette=0xd7;q->airship.margin=80;a->transform.facing=0;a->transform.priority=0x8000;as_reset(a);if(interaction_move(w,a,WMOVE_POINT)<0)return 0;as_start(q,v?ASA_SOLID1:ASA_SOLID0);break;
 case AS_BARRELS:{static const int16_t offsets[5][2]={{64,128},{144,128},{32,0},{128,0},{224,0}};q->dead=0;for(unsigned i=0;i<5;i++)if(!as_child(w,e,AS_BARREL_WAIT,ENTITY_FIRST_CHILD,offsets[i][0],offsets[i][1]))return 0;break;}
 case AS_BARREL_WAIT:break;
 case AS_BARREL:a->palette=0xcd;q->airship.margin=32;a->hit.hp=1;a->transform.facing=0;a->transform.priority=0;as_defense(q,ASB_BARREL);break;
 case AS_ROCKRAIN:break;
 case AS_ROCKRAIN_ACTIVE:q->airship.timer=15;break;
 case AS_ROCK:{a->palette=0xce;a->transform.facing=0;as_screen(w,a,0,160);as_reset(a);a->transform.priority=0xf000;a->display_flags|=64;uint16_t r=as_random(w)&255;transform_integer_offset(&a->transform,(int16_t)r,0);a->vy=(r&3)+16;as_start_entry(q,airship_rock[r&3]);a->transform.facing|=r&1;break;}
 case AS_TOWER:{
  if(v>1)return enemy_fault(w);q->airship.base_palette=0x1dc;q->airship.alternate_palette=a->palette=0xd3;a->transform.facing=0;a->phase=0;a->hit.hp=32767;a->transform.priority=0xc000;
  if(w->scene_number!=12&&!as_watch(w,e,v?0xf0638:0xf0612))return 0;
  static const uint8_t patches[]={ASV_P2EF5C8,ASV_P2EF49C,ASV_P2EF4C4,ASV_P2EF4EC};
  static const uint8_t second[]={ASV_P2EF5DC,ASV_P2EF4B0,ASV_P2EF4D8,ASV_P2EF500};
  for(unsigned i=0;i<(v?4:3);i++){
   n=as_child(w,e,AS_PORT_WAIT,ENTITY_FIRST_CHILD,v?(int)i*64-32:(int)i*64+32,0);if(!n)return 0;
   struct EnemyActor*r=&n->payload.enemy;r->airship.variant=(uint8_t)(v&&i==3);r->airship.patch=v?patches[i]:ASV_P2EF3AC+i;r->airship.second_patch=v?second[i]:65535;
  }
  q->airship.ladder=0;struct PlayerTransform t=a->transform;transform_integer_offset(&t,v?272:236,v?-72:-128);
  if(!climb_actor_spawn_watched(w,entity_ref(e),ENTITY_FIRST_CHILD,&t,v?73:128,ladder_stop))return 0;
  as_start(q,v?ASA_TOWER_B:ASA_TOWER_A);as_defense(q,ASB_TOWER);if(v)q->airship.hurt_pose=0;break;
 }
 case AS_TOWER_PATCH0:case AS_TOWER_PATCH1:case AS_TOWER_PATCH2:{
  unsigned stage=s==AS_TOWER_PATCH0?0:s==AS_TOWER_PATCH1?1:2;
  if(interaction_move(w,a,WMOVE_SYNC)<0||!tower_patch(w,e,stage))return 0;
  if(!stage){as_defense(q,ASB_TOWER_ACTIVE);if(v)q->airship.hurt_pose=0;a->state=AS_TOWER_WAIT;}
  else if(stage==1){if(v)as_start(q,ASA_TOWER_B_MID);a->state=AS_TOWER_MID;}
  else{as_start(q,v?ASA_TOWER_B_FINAL:ASA_TOWER_A_FINAL);a->state=AS_TOWER_FINAL;}break;
 }
 case AS_TOWER_ACTIVE:a->hit.hp=(int16_t)rank(w,q);break;
 case AS_TOWER_DESTROY:return tower_destroy(w,e);
 case AS_PORT_WAIT:break;
 case AS_PORT:
  a->hit.hp=20;as_defense(q,ASB_PORT);as_start(q,ASA_PORT);
  if(v){q->airship.base_palette=0x1dd;q->airship.alternate_palette=a->palette=0xd4;as_start(q,ASA_PORT_B);a->transform.priority=0x8000;as_reset(a);if(interaction_move(w,a,WMOVE_POINT)<0)return 0;}break;
 case AS_PORT_SECOND:a->hit.hp=20;if(v)as_start(q,ASA_PORT_B_HIT);break;
 case AS_PORT_WRECK:break;
 case AS_TOWER_BLAST:as_attack(q,ASB_TOWER_BLAST);hit_begin_attack(&a->hit);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;w->enemies->clear_ordnance=1;as_schedule(e,AS_TOWER_BLAST_END);return 0;
 case AS_TOWER_BLAST_END:w->enemies->clear_ordnance=0;as_attack(q,65535);hit_begin_attack(&a->hit);entity_delete(w->entities,e);return 0;
 /* This source animation is the whole falling bridge, despite its old
  * SMOKE name. Keep it in gameplay and Ending D through the original fall/splash. */
 case AS_SMOKE:if(v>1)return enemy_fault(w);a->palette=0xd5;q->scale_x=255;q->scale_y=80;a->transform.facing=0;a->transform.priority=0;as_reset(a);a->ay=-6;as_start_entry(q,airship_smoke[v]);if(!interaction_sound(w,0x1026))return 0;break;
 case AS_SMOKE_BURST:
  if(!interaction_sound(w,0x1051))return 0;for(unsigned i=0;i<5;i++){n=as_child(w,e,AS_SPARK_WAIT,ENTITY_FIRST_CHILD,112-(int)i*48,0);if(!n)return 0;as_screen(w,&n->payload.enemy.actor,as_sx(w,&n->payload.enemy.actor),293);n->payload.enemy.airship.timer=(int16_t)(i*6);}a->state=AS_SMOKE_FALL;break;
 default:return enemy_fault(w);
 }return !w->fault;
}
int as_structures_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned v=q->airship.variant;struct NativeEntity*n;struct EnemyActor*p;
 if(s==AS_ROCKRAIN||s==AS_ROCKRAIN_ACTIVE){
  if(!w->secondary_view_valid)return enemy_fault(w);if(s==AS_ROCKRAIN){if(w->secondary_y_offset<=64)return 1;as_schedule(e,AS_ROCKRAIN_ACTIVE);return 2;}
  if(!--q->airship.timer){if(!as_child(w,e,AS_ROCK,ENTITY_FIRST_CHILD,0,0))return 0;q->airship.timer=15;}if(w->secondary_y_offset>384)entity_delete(w->entities,e);return !w->fault;
 }
 if(s==AS_ROCK){transform_integer_offset(&a->transform,0,a->vy);as_animate(w,e);if(as_sy(w,a)>512)entity_delete(w->entities,e);return !w->fault;}
 if(s==AS_SMOKE||s==AS_SMOKE_FALL){if(interaction_move(w,a,WMOVE_POINT)<0)return 0;if(q->scale_y!=255)q->scale_y++;as_animate(w,e);if(s==AS_SMOKE&&as_sy(w,a)<=368)as_schedule(e,AS_SMOKE_BURST);else if(s==AS_SMOKE_FALL&&as_sy(w,a)<=128)entity_delete(w->entities,e);return !w->fault;}
 if(s==AS_PORT_WAIT){if(as_sx(w,a)>=320)return interaction_move(w,a,WMOVE_SYNC)>=0;as_schedule(e,AS_PORT);return 2;}
 if(s==AS_PORT||s==AS_PORT_SECOND||s==AS_PORT_WRECK){
  p=as_parent(w,e);if(!p)return 0;if(p->actor.phase==255){entity_delete(w->entities,e);return 1;}
  a->palette=q->airship.alternate_palette;if(v&&p->airship.hurt_pose==255)a->palette=q->airship.base_palette;
  if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;as_animate(w,e);if(s==AS_PORT_WRECK)return !w->fault;
  if(a->hit.flags&8){if(!as_hurt(w,a))return 0;a->hit.flags&=247;}
  if(a->hit.flags&1){if(s==AS_PORT)a->hit.flags&=254;else if(!as_score(w,a,0x1000))return 0;
   if(!interaction_sound(w,0x1027)||!as_patch(w,s==AS_PORT?q->airship.patch:q->airship.second_patch)||!as_pattern(w,e,ASP_P2EF274))return 0;
   if(s==AS_PORT&&q->airship.second_patch!=65535)as_schedule(e,AS_PORT_SECOND);
   else if(s==AS_PORT_SECOND&&v){as_start(q,ASA_PORT_B_WRECK);as_schedule(e,AS_PORT_WRECK);}else entity_delete(w->entities,e);
  }return !w->fault;
 }
 if(s==AS_TOWER_WAIT){if(!w->secondary_view_valid)return enemy_fault(w);if(w->secondary_x_offset>=(v?2384:2000)){as_schedule(e,AS_TOWER_ACTIVE);return 2;}}
 if(s==AS_TOWER||s==AS_TOWER_WAIT||s==AS_TOWER_ACTIVE||s==AS_TOWER_MID||s==AS_TOWER_FINAL){
  w->enemies->intro_gate=0;if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;
  if(s==AS_TOWER_WAIT){as_animate(w,e);if((a->hit.flags&8)&&!as_hurt(w,a))return 0;a->hit.flags&=246;return !w->fault;}
  if(s!=AS_TOWER||as_sx(w,a)<=176){
   if(v)q->airship.hurt_pose=0;int done=as_animate(w,e);
   if(done&&(a->hit.flags&8)){
    as_start(q,v?(s==AS_TOWER_FINAL?ASA_TOWER_B_FINAL_HIT:s==AS_TOWER_MID?ASA_TOWER_B_MID_HIT:ASA_TOWER_B_HIT):(s==AS_TOWER_FINAL?ASA_TOWER_A_FINAL_HIT:ASA_TOWER_A_HIT));if(v)q->airship.hurt_pose=255;as_animate(w,e);
   }
  }
  if(s==AS_TOWER){if(!(a->hit.flags&8))return !w->fault;a->hit.flags&=247;if(a->hit.last_kind==1||(v&&a->hit.last_kind==2))return !w->fault;
   if(w->scene_number==12){for(unsigned i=0;i<3;i++)if(!as_stamp(w,e,ASS_S2EF6A4+i))return 0;if(!as_patch(w,v?ASV_P2EF53C:ASV_P2EF410)||!as_patch(w,v?ASV_P2EF5A0:ASV_P2EF474))return 0;as_schedule(e,AS_TOWER_DESTROY);}
   else{if(!interaction_sound(w,0x1035)||!as_blast(w,e,192,-128))return 0;as_schedule(e,AS_TOWER_PATCH0);}return !w->fault;
  }
  if(a->hit.flags&8){if(!as_hurt(w,a))return 0;a->hit.flags&=247;}
  if(s==AS_TOWER_FINAL){if(a->hit.flags&1){if(!as_score(w,a,0x5000))return 0;as_schedule(e,AS_TOWER_DESTROY);return 2;}}
  else if(a->hit.hp<=(s==AS_TOWER_ACTIVE?666:333)){if(!interaction_sound(w,0x1035)||!as_blast(w,e,192,-128))return 0;as_schedule(e,s==AS_TOWER_ACTIVE?AS_TOWER_PATCH1:AS_TOWER_PATCH2);}
  return !w->fault;
 }
 if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;
 if(s==AS_BARRELS){w->enemies->intro_gate=0;if(q->dead==255){if(!interaction_sound(w,0x1028))return 0;entity_delete(w->entities,e);}return !w->fault;}
 if(s==AS_BARREL_WAIT){if(as_sx(w,a)<=320){as_schedule(e,AS_BARREL);return 2;}return 1;}
 if(s==AS_BARREL){p=as_parent(w,e);if(!p)return 0;
  if(p->dead!=255){if(a->hit.flags&8){if(!as_hurt(w,a))return 0;as_start(q,ASA_BUNKER_HIT);a->hit.flags&=247;}if(!(a->hit.flags&1))return !w->fault;p->dead=255;}
  if(!as_pattern(w,e,ASP_P2EF1BE)||!as_pattern(w,e,ASP_P2EF1E2))return 0;entity_delete(w->entities,e);return !w->fault;
 }
 as_animate(w,e);
 if(s==AS_SOLID){a->hit.flags&=246;a->hit.hp=32767;}
 if(s==AS_TENT||s==AS_BUNKER||s==AS_DEPOT||s==AS_CRATE){
  if(a->hit.flags&8){if(!as_hurt(w,a))return 0;if(s==AS_BUNKER)as_start(q,ASA_BUNKER_HIT);a->hit.flags&=247;}
  if(a->hit.flags&1){
   if(s==AS_TENT){if(!interaction_sound(w,0x1030)||!as_pattern(w,e,ASP_P2EF1D0))return 0;n=as_child(w,e,AS_IMPACT,ENTITY_FIRST_CHILD,0,0);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,as_word(a->hit.impact_x-(a->transform.x>>8)),as_word(a->hit.impact_y-transform_y_up(&a->transform)));if(!as_stamp(w,e,ASS_S2EF5F0+v))return 0;as_schedule(e,AS_TENT_WRECK);}
   else if(s==AS_BUNKER){if(!interaction_sound(w,0x1023)||!as_pattern(w,e,ASP_P2EF1AC)||!as_patch(w,ASV_P2EF34C+v)||!as_stamp(w,e,ASS_S2EF626)||!as_pattern(w,e,ASP_P2EF298))return 0;entity_delete(w->entities,e);return !w->fault;}
   else if(s==AS_CRATE){if(!interaction_sound(w,0x102e))return 0;n=as_pattern(w,e,ASP_P2EF1D0);if(!n)return 0;n->payload.enemy.actor.transform.priority=0xf000;as_schedule(e,AS_CRATE_WRECK);}
   else{if(!as_score(w,a,0x500)||!interaction_sound(w,0x1035))return 0;n=as_pattern(w,e,ASP_P2EF2EA);if(!n)return 0;n->payload.enemy.actor.transform.priority=0xc000;n=ft_child(w,e,FT_BLAST,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.fort.variant=1;n->payload.enemy.actor.transform.priority=0xc000;if(!as_watch(w,e,0xf0742))return 0;as_schedule(e,AS_DEPOT_BLAST);return !w->fault;}
  }
 }
 if(as_cull(w,e))entity_delete(w->entities,e);return !w->fault;
}
