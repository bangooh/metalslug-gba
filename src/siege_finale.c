#include "siege_internal.h"
#include "late_guard.h"
static void bob(struct EnemyActor*q){uint8_t high=(uint8_t)((uint16_t)q->siege.timer>>8);high++;q->siege.timer=(int16_t)(((uint16_t)high<<8)|((uint16_t)q->siege.timer&255));transform_integer_offset(&q->actor.transform,0,high&1?1:-1);}
static int animation_pair(struct WeaponWorld*w,struct EnemyActor*q,unsigned second){if(q->dead>=9)return enemy_fault(w);sg_start_entry(q,siege_step[q->dead*2+second]);return 1;}
static int escorts(struct WeaponWorld*w,struct NativeEntity*e){static const int16_t offsets[10][2]={{48,32},{32,80},{64,112},{96,16},{128,64},{144,112},{176,32},{208,96},{224,32},{240,112}};for(unsigned i=0;i<10;i++){struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,BFX_SMOKE);if(!n)return 0;n->payload.enemy.actor.transform.priority=0xc000;transform_integer_offset(&n->payload.enemy.actor.transform,offsets[i][0],offsets[i][1]);}return 1;}
int sg_finale_enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*n;struct EnemyActor*p;q->entry=0;
 switch(a->state){
 case SG_FINALE:
  a->palette=0xa9;a->transform.priority=1;
  for(unsigned i=0;i<45;i++){n=sg_child(w,e,SG_PANEL,ENTITY_LAST_CHILD);if(!n)return 0;n->payload.enemy.dead=(uint8_t)i;}
  for(unsigned i=0;i<4;i++){n=sg_child(w,e,SG_AUX,ENTITY_LAST_CHILD);if(!n)return 0;n->payload.enemy.dead=(uint8_t)i;}
  if(!sg_child(w,e,SG_BODY_WAIT,ENTITY_FIRST_CHILD))return 0;q->dead=0;sg_start(q,SGA_FINALE);break;
 case SG_FINALE_BREAK:
  w->enemies->siege_phase=3;n=sg_child(w,e,SG_FLASH,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.actor.transform=(struct PlayerTransform){0};n->payload.enemy.siege.timer=10;
  sg_defense(q,65535);if(!interaction_sound(w,0x1033))return 0;
  if(!w->video||!w->video->palette_cycle||!w->video->palette_cycle(w->video->palette_context,0x27,scene_palette_high(0x129),2,1))return enemy_fault(w);
  n=sg_child(w,e,SG_NUKE,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.actor.transform=(struct PlayerTransform){0};
  if(!sg_patch(w,SGV_FINALE)||!escorts(w,e)||!sg_pattern(w,e,SGP_FINALE_SMOKE))return 0;
  n=sg_pattern(w,e,SGP_FINALE_HUMANS);if(!n)return 0;n->payload.enemy.actor.transform.priority=0xc000;if(!sg_spawn_steps(w,e))return 0;q->dead=0;break;
 case SG_FINALE_CLOSE:if(interaction_move(w,a,WMOVE_SYNC)<0||!sg_stamp(w,e,SGS_CLOSE))return 0;sg_start(q,SGA_CLOSING);break;
 case SG_FINALE_END:
  w->enemies->siege_phase=0;
  for(unsigned i=0;i<2;i++)if(enemy_player_live(w->players[i])){unsigned id=w->players[i]->actor.hit.id;if(id>=ENTITY_CAPACITY)return enemy_fault(w);if(!weapon_spawn_player_effect(w,entity_ref(&w->entities->nodes[id]),PVIS_ICON))return 0;}
  entity_delete(w->entities,e);return 0;
 case SG_GUARDIAN:q->siege.margin=256;sg_defense(q,SGB_GUARDIAN);break;
 case SG_FLASH:w->enemies->scene_flags[1]=1;break;
 case SG_PANEL:break;
 case SG_PANEL_HIT:if(!sg_score(w,a,0x1000)||!interaction_sound(w,0x108d))return 0;break;
 case SG_PANEL_EMPTY:if(!interaction_sound(w,0x108d))return 0;break;
 case SG_AUX:{
  a->palette=0xa9;q->scale_x=q->scale_y=255;a->transform.facing=0;transform_integer_offset(&a->transform,41+64*q->dead,128);a->hit.hp=20;
  if(q->dead>=4)return enemy_fault(w);sg_start_entry(q,siege_finale[q->dead]);
  unsigned choice=sg_random(w)&15;n=late_guard_spawn_choice(w,entity_ref(e),&a->transform,choice);if(!n)return 0;
  transform_integer_offset(&n->payload.enemy.actor.transform,4,1);
  /* Source D0 is the aligned table byte offset here, hence bit zero is 0. */
  n->payload.enemy.params[1]=(uint8_t)(n->payload.enemy.params[1]+127);n->payload.enemy.source_flags=1;n->payload.enemy.actor.transform.display_group=1;
  a->transform.priority=0x8000;sg_reset(a);if(interaction_move(w,a,WMOVE_POINT)<0)return 0;break;}
 case SG_STEP:a->palette=0xa9;q->scale_x=q->scale_y=255;if(!animation_pair(w,q,0))return 0;break;
 case SG_STEP_BREAK:if(!animation_pair(w,q,1))return 0;break;
 case SG_STEP_FALL:p=sg_parent(w,e);if(!p)return 0;p->dead++;sg_reset(a);a->ay=-16;break;
 case SG_STEP_SINK:if(q->dead==1||q->dead==5){if(!interaction_sound(w,0x1026))return 0;}else if(q->dead==8&&!interaction_sound(w,0x1032))return 0;break;
 case SG_BODY_WAIT:{
  struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid)return enemy_fault(w);a->palette=0x85;q->scale_x=q->scale_y=255;a->transform.facing=0;
  a->hit.hp=(int16_t)siege_body_rank[(interaction_rank(w,&q->actor,runtime_owner_siege[0]))*8+(q->source_flags&3)*2+((r->player_count_flag&2)>>1)];a->transform.priority=0;transform_integer_offset(&a->transform,152,-8);a->phase=0;break;}
 case SG_BODY:
  sg_start(q,SGA_BODY);sg_defense(q,65535);if(!sg_child(w,e,SG_CREW,ENTITY_FIRST_CHILD))return 0;
  n=tank_ally_spawn(w,entity_ref(e),&a->transform);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,101,0);n->payload.enemy.actor.transform.facing=1;n->payload.enemy.params[0]=18;break;
 case SG_CREW:a->palette=14;q->scale_x=q->scale_y=255;a->transform.facing=1;transform_integer_offset(&a->transform,23,40);a->transform.priority=0;sg_start(q,SGA_CREW);sg_defense(q,65535);break;
 case SG_CREW_FALL:sg_reset(a);a->ay=-32;sg_start(q,SGA_CREW_FALL);sg_defense(q,65535);break;
 case SG_CREW_LAND:sg_reset(a);sg_start(q,SGA_CREW_LAND);sg_defense(q,65535);break;
 case SG_CREW_RISE:sg_reset(a);sg_start(q,SGA_CREW_RISE);sg_defense(q,65535);break;
 case SG_CREW_RUN:sg_reset(a);a->vx=-384;a->vy=39;sg_start(q,SGA_CREW_RUN);sg_defense(q,65535);break;
 case SG_CREW_EXIT:break;
 case SG_PARACHUTE:a->palette=4;q->scale_x=q->scale_y=255;sg_start(q,SGA_PARACHUTE);break;
 default:return enemy_fault(w);
 }return !w->fault;
}
int sg_finale_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;struct NativeEntity*n;int done,r;
 if(s==SG_FLASH){q->siege.timer=sg_word(q->siege.timer-1);if(!q->siege.timer)entity_delete(w->entities,e);else if(q->siege.timer&1)w->enemies->scene_flags[1]=1;return 1;}
 if(s>=SG_PANEL&&s<=SG_PANEL_EMPTY){
  p=sg_parent(w,e);if(!p)return 0;if(q->dead>=45)return enemy_fault(w);
  if((p->actor.hit.flags&8)&&p->actor.phase==q->dead){p->actor.hit.flags&=247;if(s!=SG_PANEL_EMPTY){a->hit.last_kind=p->actor.hit.last_kind;if(!sg_patch(w,siege_patch_panels[q->dead][s==SG_PANEL?0:1]))return 0;sg_schedule(e,s==SG_PANEL?SG_PANEL_HIT:SG_PANEL_EMPTY);}}
  if(p->dead==255)entity_delete(w->entities,e);return !w->fault;
 }
 if(s==SG_PARACHUTE){p=sg_parent(w,e);if(!p)return 0;sg_screen(w,a,sg_sx(w,&p->actor),sg_sy(w,&p->actor)+30);sg_animate(w,e);return !w->fault;}
 if(s==SG_FINALE||s==SG_FINALE_BREAK||s==SG_FINALE_CLOSE)w->enemies->intro_gate=0;
 if(s==SG_STEP_FALL||s==SG_STEP_SINK)bob(q);
 if(s==SG_BODY)interaction_body_contact(w,a,&siege_body_contact);
 r=interaction_move(w,a,s==SG_STEP_FALL||s==SG_STEP_SINK||s==SG_CREW_FALL||s==SG_CREW_RUN||s==SG_CREW_EXIT?WMOVE_POINT:WMOVE_SYNC);if(r<0)return 0;
 if(s==SG_BODY_WAIT){if(sg_sx(w,a)<=336){sg_schedule(e,SG_BODY);return 2;}return 1;}
 if(s==SG_FINALE_BREAK){if((int8_t)q->dead>=9){w->enemies->intro_gate=2;sg_schedule(e,SG_FINALE_CLOSE);}return 1;}
 if(s==SG_GUARDIAN){a->hit.hp=32767;if(a->hit.flags&8){if(!sg_hurt(w,a,0))return 0;a->hit.flags&=247;if((a->hit.last_kind==1||a->hit.last_kind==2)&&!(w->frame_phase&1)&&!sg_child(w,e,SG_FLYING,ENTITY_FIRST_CHILD))return 0;}a->hit.flags&=254;sg_cull(w,e,0);return !w->fault;}
 if(s==SG_AUX){if(sg_sx(w,a)>336){entity_delete_children(w->entities,e);return 1;}done=sg_animate(w,e);(void)done;p=sg_parent(w,e);if(!p)return 0;
  if(p->dead==255){entity_delete_children(w->entities,e);entity_delete(w->entities,e);return 1;}
  if(a->animation.cursor!=65535){if(a->hit.flags&8){if(!sg_hurt(w,a,0))return 0;a->hit.flags&=247;}if(a->hit.flags&1){if(!interaction_sound(w,0x102e))return 0;animation_begin(&a->animation,65535);a->animation.drawing=65535;if(!sg_pattern(w,e,SGP_AUX))return 0;entity_delete_children(w->entities,e);return 1;}}
  sg_cull(w,e,1);return !w->fault;
 }
 done=sg_animate(w,e);
 switch(s){
 case SG_FINALE:a->hit.hp=32767;if(q->dead==255){q->siege.closing=255;sg_schedule(e,SG_FINALE_BREAK);}break;
 case SG_FINALE_CLOSE:if(done&&e->depth>=w->entities->nodes[e->next].depth)sg_schedule(e,SG_FINALE_END);break;
 case SG_STEP:p=sg_parent(w,e);if(!p)return 0;if(p->dead==q->dead)sg_schedule(e,SG_STEP_BREAK);break;
 case SG_STEP_BREAK:if(done)sg_schedule(e,SG_STEP_FALL);break;
 case SG_STEP_FALL:if(r){/* $0434CE is a verified RTS. */n=sg_pattern(w,e,SGP_STEP_SMOKE);if(!n)return 0;n->payload.enemy.actor.display_flags|=64;transform_integer_offset(&n->payload.enemy.actor.transform,0,-64);sg_schedule(e,SG_STEP_SINK);}break;
 case SG_STEP_SINK:if(sg_sy(w,a)<=64)entity_delete(w->entities,e);break;
 case SG_BODY:
  if(a->hit.flags&8){if(!sg_hurt(w,a,0))return 0;a->hit.flags&=247;}if(!w->secondary_view_valid)return enemy_fault(w);if(w->secondary_x_offset>=1264)sg_defense(q,SGB_BODY);
  if(a->hit.flags&1){a->phase=255;if(!interaction_sound(w,0x1028)||!sg_pattern(w,e,SGP_BODY_FIRE)||!sg_pattern(w,e,SGP_BODY_BLAST)||!sg_pattern(w,e,SGP_BODY_DEBRIS)||!sg_stamp(w,e,SGS_BODY))return 0;entity_delete(w->entities,e);}break;
 case SG_CREW:p=sg_parent(w,e);if(!p)return 0;if(p->actor.phase==255){if(!sg_child(w,e,SG_PARACHUTE,ENTITY_FIRST_CHILD))return 0;sg_schedule(e,SG_CREW_FALL);}break;
 case SG_CREW_FALL:if(r)sg_schedule(e,SG_CREW_LAND);break;
 case SG_CREW_LAND:if(done)sg_schedule(e,SG_CREW_RISE);break;
 case SG_CREW_RISE:if(done)sg_schedule(e,SG_CREW_RUN);break;
 case SG_CREW_RUN:if(sg_sx(w,a)<=132){if(!interaction_sound(w,0x1040))return 0;sg_schedule(e,SG_CREW_EXIT);}break;
 case SG_CREW_EXIT:if(sg_sx(w,a)<=100){n=entity_get(w->entities,e->parent);if(!n)return enemy_fault(w);p=sg_parent(w,n);if(!p)return 0;p->dead=255;entity_delete_children(w->entities,e);entity_delete(w->entities,e);}break;
 default:return enemy_fault(w);
 }return !w->fault;
}
