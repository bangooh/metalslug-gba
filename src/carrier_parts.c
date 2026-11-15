#include "carrier_internal.h"
/* Hull sections, deck contacts, cockpit and hatch all remain real children;
 * the original parent-byte handshakes determine the destruction sequence. */
static int rank_hull(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned denom=(uint16_t)q->carrier_part.max_hp/4;
 a->hit.flags&=247;if(!denom)return enemy_fault(w);int16_t stage=cr_word(3-(uint16_t)a->hit.hp/denom);if(stage<0)stage=0;
 if(stage==q->carrier_part.stage)return 1;if(stage>=4||q->carrier_part.side>=3)return enemy_fault(w);q->carrier_part.stage=stage;
 const uint16_t*rows[]={carrier_hull_a,carrier_hull_b,carrier_hull_c};cr_start_entry(q,rows[q->carrier_part.side][stage]);
 struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_FIRE);if(!n)return 0;
 struct WeaponActor*b=&n->payload.enemy.actor;transform_integer_offset(&b->transform,cr_word(a->hit.impact_x-(b->transform.x>>8)),cr_word(a->hit.impact_y-transform_y_up(&b->transform)));b->transform.priority+=40;
 n=boss_fx_pattern(w,entity_ref(e),&a->transform,BP_CARRIER_HIT);if(!n)return 0;b=&n->payload.enemy.actor;
 transform_integer_offset(&b->transform,cr_word(a->hit.impact_x-(b->transform.x>>8)),cr_word(a->hit.impact_y-transform_y_up(&b->transform)));b->transform.priority+=40;return 1;
}
int cr_parts_enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy,*p=cr_parent(w,e);if(!p)return 0;struct WeaponActor*a=&q->actor;unsigned state=a->state;q->entry=0;
 switch(state){
 case CR_HULL:
  a->palette=0x24;a->phase=q->dead=0;a->hit.hp=(int16_t)((uint16_t)p->actor.hit.hp/3);q->carrier_part.max_hp=a->hit.hp;q->carrier_part.stage=-1;a->state=CR_HULL_RANK;
  /* FALLTHROUGH */
 case CR_HULL_RANK:return rank_hull(w,e);
 case CR_ROW_SMOKE:{
  static const int8_t dx[]={-48,0,64};if(q->carrier_part.side>=3)return enemy_fault(w);
  struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMOKE);if(!n)return 0;
  n->payload.enemy.actor.transform.priority+=40;transform_integer_offset(&n->payload.enemy.actor.transform,dx[q->carrier_part.side],0);
  a->state=CR_ROW;
 }/* FALLTHROUGH */
 case CR_ROW:{
  static const int8_t dx[]={-72,0,64};static const enum CarrierAnimation anim[]={CRA_HULL_WRECK0,CRA_HULL_WRECK1,CRA_HULL_WRECK2};
  if(q->carrier_part.side>=3)return enemy_fault(w);q->carrier_part.offset=dx[q->carrier_part.side];cr_start(q,anim[q->carrier_part.side]);break;
 }
 case CR_ROW_DEPLOYED:break;
 case CR_COCKPIT:
  a->palette=0x24;a->display_flags|=2;a->phase=q->dead=0;a->transform.priority=0x5000;
  a->hit.hp=cr_rank(w,q,p->carrier.variant?CRR_COCKPIT_HP_B:CRR_COCKPIT_HP);cr_start(q,CRA_COCKPIT);
  if(!cr_child(w,e,CR_MARK_COCKPIT))return 0;return 0;/* constructor returns before the frame */
 case CR_EJECT:
  if(!cr_follow(w,e))return 0;transform_integer_offset(&a->transform,0,-4);p->actor.phase=4;cr_reset(a);a->vy=-192;
  q->dead=0;a->hit.hp=0;q->carrier.bob_angle=q->carrier.bob_y=0;
  /* $0434DC is RTS in the verified source. */
  a->display_flags&=253;cr_start(q,CRA_EJECT);if(!boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMOKE))return 0;break;
 case CR_MARK_LEFT:case CR_MARK_RIGHT:return 0;/* frame begins next tick */
 case CR_MARK_COCKPIT:break;
 case CR_HATCH:
  a->palette=0x25;a->transform.priority=p->actor.transform.priority+24;a->phase=q->dead=0;q->carrier_part.direction=0;cr_start(q,CRA_HATCH);break;
 case CR_OPEN:cr_start(q,CRA_OPEN);break;
 case CR_OPEN2:cr_start(q,CRA_OPEN2);break;
 case CR_DROP:cr_start(q,CRA_DROP);break;
 case CR_HATCH_FALL:a->palette=12;p->dead=255;transform_integer_offset(&a->transform,0,-4);cr_start(q,CRA_FADE);break;
 case CR_CANNON:a->palette=4;a->transform.priority=p->actor.transform.priority+48;a->phase=q->dead=0;cr_reset(a);cr_start(q,CRA_CANNON);break;
 case CR_CANNON_FALL:cr_reset(a);a->vy=256;cr_start(q,CRA_CANNON_FALL);break;
 default:return enemy_fault(w);
 }return !w->fault;
}
int cr_parts_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct EnemyActor*q=&e->payload.enemy,*p=cr_parent(w,e);if(!p)return 0;struct WeaponActor*a=&q->actor;int done;
 switch(state){
 case CR_HULL:case CR_HULL_RANK:
  if(!cr_follow(w,e))return 0;cr_animate(w,e);if(a->hit.flags&8)cr_schedule(e,CR_HULL_RANK);
  if(p->actor.phase==3)cr_schedule(e,CR_ROW);if(a->hit.flags&1)cr_schedule(e,CR_ROW_SMOKE);cr_parent_tail(w,e,0);break;
 case CR_ROW:
  if(!cr_follow(w,e))return 0;cr_animate(w,e);a->hit.flags&=247;
  if(p->actor.phase==3){
   if(!q->carrier_part.offset){
    struct NativeEntity*n=boss_fx_pattern(w,entity_ref(e),&a->transform,BP_CARRIER_BODY);if(!n)return 0;n->payload.enemy.actor.transform.priority+=40;
    n=boss_fx_pattern(w,entity_ref(e),&a->transform,BP_CARRIER_DEBRIS);if(!n)return 0;n->payload.enemy.actor.transform.priority+=40;
    n=cr_child(w,e,CR_RIDER);if(!n)return 0;n->payload.enemy.actor.transform.priority+=0xc000;
   }cr_defense(q,65535);cr_schedule(e,CR_ROW_DEPLOYED);
  }if(interaction_outside(w,a,carrier_bounds[0],0))cr_schedule(e,CR_DELETE);break;
 case CR_ROW_DEPLOYED:
  if(!cr_follow(w,e))return 0;cr_animate(w,e);
  if(p->dead==1){transform_integer_offset(&a->transform,q->carrier_part.offset,0);a->transform.priority+=40;cr_defense(q,65535);if(!boss_fx_schedule(w,e,BFX_SMOKE))return 0;}
  if(interaction_outside(w,a,carrier_bounds[0],0))cr_schedule(e,CR_DELETE);break;
 case CR_COCKPIT:
  if(!cr_follow(w,e))return 0;transform_integer_offset(&a->transform,0,-4);p->carrier.cockpit_x=cr_sx(w,a);p->carrier.cockpit_y=cr_sy(w,a);
  cr_animate(w,e);if(!cr_platforms(w,a,1))return 0;if(a->hit.flags&8){a->hit.flags&=247;if(!cr_hit_sound(w,a))return 0;}
  if(!a->hit.hp)a->hit.flags|=1;if(a->hit.flags&1)cr_schedule(e,CR_EJECT);cr_parent_tail(w,e,1);break;
 case CR_EJECT:
  if(interaction_move(w,a,WMOVE_POINT)<0)return 0;p->carrier.cockpit_x=cr_sx(w,a);p->carrier.cockpit_y=cr_sy(w,a);
  for(unsigned i=2;i<5;i++)interaction_body_contact(w,a,&carrier_contacts[i]);
  cr_animate(w,e);if(!cr_platforms(w,a,1))return 0;cr_bob(q);
  if(cr_sy(w,a)<=208){p->actor.phase=5;a->vy=-1088;}cr_parent_tail(w,e,1);break;
 case CR_MARK_LEFT:case CR_MARK_RIGHT:case CR_MARK_COCKPIT:{
  if(p->dead==255||entity_parent_deleting(w->entities,e)){entity_delete(w->entities,e);return 1;}
  if(!cr_follow(w,e))return 0;unsigned index=state==CR_MARK_LEFT?0:state==CR_MARK_RIGHT?1:5;
  transform_integer_offset(&a->transform,index==0?-56:index==1?72:32,0);interaction_body_contact(w,a,&carrier_contacts[index]);
  if(index==5?p->dead!=0:p->dead==1)entity_delete(w->entities,e);break;
 }
 case CR_HATCH:case CR_OPEN:case CR_OPEN2:case CR_DROP:
  if(!cr_follow(w,e))return 0;transform_integer_offset(&a->transform,state==CR_DROP?cr_word(-q->carrier_part.offset):0,cr_word(-p->carrier.bob_y));
  if(state==CR_HATCH){q->carrier_part.direction=p->carrier.direction;q->carrier_part.delay_row=q->carrier_part.direction&3;}
  done=cr_animate(w,e);
  if(state==CR_HATCH&&p->dead==1)cr_schedule(e,CR_OPEN);
  if(state==CR_OPEN&&p->dead==2)cr_schedule(e,CR_OPEN2);
  if(state==CR_OPEN2&&p->dead==3)cr_schedule(e,CR_DROP);
  if(state==CR_DROP&&done)cr_schedule(e,CR_HATCH_FALL);cr_parent_tail(w,e,0);break;
 case CR_HATCH_FALL:
  if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;if(cr_animate(w,e))cr_schedule(e,CR_DELETE);break;
 case CR_CANNON:case CR_CANNON_FALL:
  if(interaction_move(w,a,state==CR_CANNON?WMOVE_SYNC:WMOVE_POINT)<0)return 0;
  if(cr_animate(w,e))cr_schedule(e,state==CR_CANNON?CR_CANNON_FALL:CR_DELETE);
  if(interaction_outside(w,a,boss_fx_bounds[8],1))cr_schedule(e,CR_DELETE);break;
 default:return enemy_fault(w);
 }return !w->fault;
}
