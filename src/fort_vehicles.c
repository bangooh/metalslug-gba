#include "fort_internal.h"
static int rank(struct WeaponWorld*w,struct EnemyActor*q,unsigned row){
 struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid||row>=2){enemy_fault(w);return 0;}
 return fort_rank[row][(interaction_rank(w,&q->actor,runtime_owner_fort[row]))*8+(q->source_flags&3)*2+((r->player_count_flag&2)>>1)];
}
static int ladder_stop(const struct NativeEntity*e){return e->kind==ENTITY_ENEMY?e->payload.enemy.fort.ladder:-1;}
static int rotor(struct WeaponWorld*w,struct EnemyActor*q){
 if(w->frame_phase&(q->fort.timer>0?3:15))return 1;
 uint16_t palette=q->fort.alternate_palette;q->fort.alternate_palette=q->actor.palette;q->actor.palette=palette;
 if(q->fort.timer>0)q->fort.timer--;return 1;
}
int ft_vehicles_enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned s=a->state;
 switch(s){
 case FT_CAR:a->palette=0xe5;a->transform.facing=0;q->fort.margin=240;a->transform.priority=0;ft_start(q,FTA_CAR);if(!ft_child(w,e,FT_TURRET,ENTITY_FIRST_CHILD))return 0;break;
 case FT_CAR_ACTIVE:break;
 case FT_CAR_HIT1:case FT_CAR_HIT2:if(!interaction_sound(w,0x102b))return 0;ft_start(q,s==FT_CAR_HIT1?FTA_CAR_HIT1:FTA_CAR_HIT2);break;
 case FT_CAR_WRECK:if(!ft_score(w,a,0x5000))return 0;ft_defense(q,65535);ft_start(q,FTA_CAR_WRECK);break;
 case FT_CAR_BLAST:q->fort.timer=16;break;
 case FT_TURRET:a->palette=0xe5;a->transform.facing=0;interaction_priority(w,a,0x8000,128);break;
 case FT_HELI:
  if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;a->palette=0x87;q->fort.alternate_palette=0x86;a->transform.facing=0;q->fort.margin=80;
  a->hit.hp=ft_word(rank(w,q,1)+380);q->fort.timer=0;a->phase=0;a->transform.priority=0x2000;ft_start(q,FTA_HELI);
  if(!ft_child(w,e,FT_LADDER,ENTITY_FIRST_CHILD)||!ft_stamp(w,e,FTS_HELI))return 0;break;
 case FT_HELI_ENTER:ft_start(q,FTA_HELI_ENTER);break;
 case FT_HELI_ACTIVE:if(!ft_watch(w,e,0xee042))return 0;break;
 case FT_HELI_HIT1:case FT_HELI_HIT2:if(!interaction_sound(w,0x1027))return 0;ft_start(q,s==FT_HELI_HIT1?FTA_HELI_HIT1:FTA_HELI_HIT2);break;
 case FT_HELI_WRECK:
  if(!ft_score(w,a,0x5000)||!interaction_sound(w,0x1023)||interaction_move(w,a,WMOVE_SYNC)<0)return 0;
  q->fort.watch=255;a->phase=255;
  if(!boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMOKE)||!ft_stamp(w,e,FTS_HELI_WRECK)||!ft_pattern(w,e,FTP_P2ED176)||!ft_pattern(w,e,FTP_P2ED188))return 0;
  ft_start(q,FTA_HELI_WRECK);break;
 case FT_LADDER:{
  a->palette=0x88;a->transform.facing=0;a->transform.priority=0;ft_start(q,FTA_LADDER);q->fort.ladder=0;
  struct PlayerTransform t=a->transform;transform_integer_offset(&t,88,0);
  if(!climb_actor_spawn_watched(w,entity_ref(e),ENTITY_FIRST_CHILD,&t,48,ladder_stop))return 0;break;
 }
 case FT_BARRIER:a->palette=0xe6;q->fort.margin=64;a->hit.hp=50;a->transform.priority=0x8000;ft_reset(a);if(interaction_move(w,a,WMOVE_POINT)<0)return 0;break;
 case FT_BARRIER_OPEN:ft_start(q,FTA_BARRIER);break;
 case FT_BARRIER_ACTIVE:if(!ft_watch(w,e,0xee062))return 0;break;
 case FT_BARRIER_HIT1:case FT_BARRIER_HIT2:case FT_BARRIER_HIT3:ft_start(q,(enum FortAnimation)(FTA_BARRIER_HIT1+s-FT_BARRIER_HIT1));break;
 case FT_BARRIER_WRECK:ft_start(q,FTA_BARRIER_WRECK);break;
 default:return enemy_fault(w);
 }return !w->fault;
}
int ft_vehicles_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;int done=0;
 if(s==FT_TURRET){struct EnemyActor*p=ft_parent(w,e);if(!p)return 0;transform_integer_copy(&a->transform,&p->actor.transform);
  if(p->actor.phase==3){entity_delete(w->entities,e);return 1;}if(p->actor.phase>=3)return enemy_fault(w);ft_start_entry(q,fort_turret[p->actor.phase]);ft_animate(w,e);return !w->fault;}
 if(s==FT_CAR){
  if(ft_sx(w,a)>=256){w->enemies->intro_gate=0;if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;ft_animate(w,e);return !w->fault;}
  a->hit.flags&=246;a->hit.hp=ft_word(rank(w,q,0)+740);a->state=s=FT_CAR_ACTIVE;
 }
 if(s==FT_BARRIER_WRECK&&ft_children(w,e)){if(ft_cull(w,e))entity_delete(w->entities,e);return !w->fault;}
 if(s>=FT_HELI&&s<=FT_HELI_HIT2)interaction_body_contact(w,a,&fort_contacts[1]);
 if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;
 if(s>=FT_CAR_ACTIVE&&s<=FT_CAR_HIT2){w->enemies->intro_gate=0;interaction_body_contact(w,a,&fort_contacts[0]);}
 if(s>=FT_HELI&&s<=FT_HELI_HIT2)rotor(w,q);
 if(s!=FT_CAR_BLAST&&s!=FT_BARRIER)done=ft_animate(w,e);
 if(s==FT_CAR_BLAST){
  ft_attack(q,FTB_CAR_BLAST);hit_begin_attack(&a->hit);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;
  q->fort.timer=ft_word(q->fort.timer-1);if(q->fort.timer<0)entity_delete(w->entities,e);return !w->fault;
 }
 if(s==FT_LADDER){
  struct EnemyActor*p=ft_parent(w,e);if(!p)return 0;if(p->actor.phase==255){q->fort.ladder=255;
   struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_FIRE);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,88,0);entity_delete(w->entities,e);}
  return !w->fault;
 }
 if(s==FT_HELI_WRECK){if(ft_cull(w,e))entity_delete(w->entities,e);return !w->fault;}
 if(s==FT_CAR_WRECK){if(!enemy_attack(w,a))return 0;if(ft_cull(w,e))entity_delete(w->entities,e);return !w->fault;}
 if(s==FT_HELI_ENTER&&done)ft_schedule(e,FT_HELI_ACTIVE);
 if(s==FT_BARRIER_OPEN&&done)ft_schedule(e,FT_BARRIER_ACTIVE);
 if(a->hit.flags&8){if(!ft_hurt(w,a,0))return 0;a->hit.flags&=247;if(s>=FT_HELI&&s<=FT_HELI_HIT2)q->fort.timer=15;}
 switch(s){
 case FT_CAR_ACTIVE:if(a->hit.hp<=666){if(!ft_pattern(w,e,FTP_P2ED0C0))return 0;ft_schedule(e,FT_CAR_HIT1);}break;
 case FT_CAR_HIT1:if(a->hit.hp<=246){if(!ft_pattern(w,e,FTP_P2ED0D2))return 0;ft_schedule(e,FT_CAR_HIT2);}break;
 case FT_CAR_HIT2:if(a->hit.flags&1){
  if(!interaction_sound(w,0x1038)||!ft_pattern(w,e,FTP_P2ED0E4)||!ft_pattern(w,e,FTP_P2ECFCC)||!ft_stamp(w,e,FTS_CAR0)||!ft_stamp(w,e,FTS_CAR1)||!ft_child(w,e,FT_CAR_BLAST,ENTITY_FIRST_CHILD))return 0;ft_schedule(e,FT_CAR_WRECK);
 }break;
 case FT_HELI:case FT_HELI_ENTER:a->hit.flags&=254;a->hit.hp=380;if(s==FT_HELI&&ft_sx(w,a)<=256)ft_schedule(e,FT_HELI_ENTER);break;
 case FT_HELI_ACTIVE:if(a->hit.hp<=252)ft_schedule(e,FT_HELI_HIT1);break;
 case FT_HELI_HIT1:if(a->hit.hp<=126)ft_schedule(e,FT_HELI_HIT2);break;
 case FT_HELI_HIT2:if(a->hit.flags&1)ft_schedule(e,FT_HELI_WRECK);break;
 case FT_BARRIER:case FT_BARRIER_OPEN:a->hit.hp=50;a->hit.flags&=254;if(s==FT_BARRIER&&ft_sx(w,a)<=80)ft_schedule(e,FT_BARRIER_OPEN);break;
 case FT_BARRIER_ACTIVE:if(a->hit.hp<=40)ft_schedule(e,FT_BARRIER_HIT1);break;
 case FT_BARRIER_HIT1:if(a->hit.hp<=30)ft_schedule(e,FT_BARRIER_HIT2);break;
 case FT_BARRIER_HIT2:if(a->hit.hp<=20)ft_schedule(e,FT_BARRIER_HIT3);break;
 case FT_BARRIER_HIT3:if(a->hit.hp<=10){if(!ft_pattern(w,e,FTP_P2ED1E2))return 0;ft_schedule(e,FT_BARRIER_WRECK);}break;
 case FT_BARRIER_WRECK:/* $0883D6 is unconditional, regardless of death carry. */entity_delete(w->entities,e);return !w->fault;
 default:return enemy_fault(w);
 }
 if(ft_cull(w,e)){if(s>=FT_HELI&&s<=FT_HELI_HIT2){a->phase=255;if(s>=FT_HELI_ACTIVE)q->fort.watch=255;}entity_delete(w->entities,e);}return !w->fault;
}
