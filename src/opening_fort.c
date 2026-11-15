#include "opening_internal.h"
/*4E512..4F2A4. Parts carry patch IDs, not the source patch-pointer array. */
struct Part {uint16_t patch[2];int16_t x,y;uint8_t graph,bit,count;};
static const struct Part parts[]={
 {{OPV_P295826,OPV_P29583A},64,112,OPA_PART_A,1,2},{{OPV_P295862,OPV_P295876},32,80,OPA_PART_A,2,2},{{OPV_P29588A,OPV_P29589E},64,80,OPA_PART_A,4,2},{{OPV_P2958B2,0},96,80,OPA_PART_A,8,1},{{OPV_P295AF6,0},32,80,OPA_PART_C,0,1},{{OPV_P295B1E,OPV_P295B32},64,48,OPA_PART_C,0,2},
 {{OPV_P2958EE,0},16,144,OPA_PART_A,1,1},{{OPV_P295916,0},48,144,OPA_PART_A,2,1},{{OPV_P29593E,0},80,144,OPA_PART_A,4,1},{{OPV_P295966,0},112,144,OPA_PART_A,8,1},{{OPV_P29598E,0},16,112,OPA_PART_A,16,1},{{OPV_P2959B6,0},48,112,OPA_PART_A,32,1},{{OPV_P2959DE,0},80,112,OPA_PART_A,64,1},{{OPV_P295A06,0},112,112,OPA_PART_A,128,1},
 {{OPV_P295A6A,0},0,144,OPA_PART_D,1,1},{{OPV_P295A7E,0},32,144,OPA_PART_E,2,1},{{OPV_P295A92,0},32,112,OPA_PART_F,4,1},{{OPV_P295AA6,0},64,112,OPA_PART_G,8,1},{{OPV_P295ABA,OPV_P295ACE},96,16,OPA_PART_B,16,2}
};
static int spawn_parts(struct WeaponWorld*w,struct NativeEntity*e,unsigned first,unsigned count){for(unsigned i=first;i<first+count;i++){const struct Part*p=&parts[i];struct NativeEntity*n=op_child(w,e,OP_PART,p->x,p->y,1);if(!n)return 0;struct EnemyActor*q=&n->payload.enemy;q->dead=p->bit;q->opening.part=i;/*Source70 aliases the first pointer's high WORD0029. */q->opening.margin=41;op_start(q,p->graph);}return !w->fault;}
static int rank_hp(struct WeaponWorld*w,struct EnemyActor*q){struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid)return enemy_fault(w);q->actor.hit.hp=opening_fort_rank[(interaction_rank(w,&q->actor,runtime_owner_opening[0]))*8+(q->source_flags&3)*2+((r->player_count_flag&2)>>1)];return 1;}
static int triple(struct WeaponWorld*w,struct NativeEntity*e){return op_burst(w,e,OPP_P295C32,OPP_P295C44,OPP_P295CD4,0,0);}
int op_fort_enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned s=a->state;
 switch(s){
 case OP_BARRIER:a->palette=0x40;op_start(q,OPA_BARRIER);a->display_flags|=64;if(!op_child(w,e,OP_POST,0,16,1))return 0;a->state=OP_BARRIER_WAIT;break;
 case OP_BARRIER_WAIT:case OP_GATE_WAIT:case OP_FORT_WAIT:break;
 case OP_BARRIER_ACTIVE:op_defense(q,OPB_BARRIER_DEF);q->opening.margin=128;a->hit.hp=100;q->dead=a->phase=0;if(!spawn_parts(w,e,0,6)||!op_child(w,e,OP_LIGHT,32,32,1)||!op_watch(w,e,0xe9348))return 0;break;
 case OP_BARRIER_DAMAGED:if(!interaction_sound(w,0x102f))return 0;a->hit.hp=100;a->hit.flags&=254;if(!op_patch(w,OPV_P2957C2)||!op_patch(w,OPV_P2957EA))return 0;break;
 case OP_BARRIER_WRECK:if(!vg_score(w,a,0x1000))return 0;w->enemies->clear_ordnance=0;op_attack(q,OPB_BARRIER_BLAST);hit_begin_attack(&a->hit);hit_begin_attack(&a->hit);if(!enemy_attack(w,a)||!op_sync(w,a)||!op_stamp(w,a,OPS_S295DEE)||!interaction_sound(w,0x1030)||!op_patch(w,OPV_P2957D6)||!op_patch(w,OPV_P2957FE)||!op_child(w,e,OP_GATE,64,0,1))return 0;op_start(q,OPA_BARRIER_WRECK);q->opening.watch=255;op_defense(q,65535);break;
 case OP_GATE:if(!op_sync(w,a)||!op_child(w,e,OP_GATE_BLOCKER,52,0,1))return 0;a->state=OP_GATE_WAIT;break;
 case OP_GATE_ACTIVE:{if(!op_map_position(w,a,0x3a0,0xe0))return 0;q->dead=a->phase=0;if(!op_copy_sync(w,a))return 0;struct PlayerTransform t=a->transform;t.display_group=q->source_flags;transform_integer_offset(&t,88,21);if(!scene_prop_frog(w,entity_ref(e),ENTITY_FIRST_CHILD,&t))return 0;a->palette=0x40;q->opening.margin=112;a->hit.hp=180;a->display_flags|=64;a->transform.priority=0xf000;if(!spawn_parts(w,e,6,8)||!op_child(w,e,OP_GATE_ROOF,0,0,1)||!op_child(w,e,OP_DOOR,0,0,1))return 0;op_start(q,OPA_GATE);if(!op_watch(w,e,0xe93b0)||!op_sync(w,a)||!op_stamp(w,a,OPS_S295F12))return 0;break;}
 case OP_GATE_DAMAGED:if(!interaction_sound(w,0x102f))return 0;a->hit.hp=180;a->hit.flags&=254;for(unsigned i=0;i<8;i++)if(!op_patch(w,OPV_P295902+2*i))return 0;op_start(q,OPA_GATE2);break;
 case OP_GATE_WRECK:if(!vg_score(w,a,0x2000))return 0;w->enemies->intro_gate=2;w->enemies->clear_ordnance=0;if(!op_sync(w,a)||!op_stamp(w,a,OPS_S295E04))return 0;op_defense(q,65535);if(!interaction_sound(w,0x1030)||!op_stamp(w,a,OPS_S295F24)||!op_patch(w,OPV_P295812)||!op_patch(w,OPV_P2958DA)||!op_child(w,e,OP_FORT,160,0,1))return 0;op_start(q,OPA_GATE_WRECK);q->opening.watch=255;break;
 case OP_FORT:if(!op_map_position(w,a,0x440,0xe0))return 0;q->dead=a->phase=0;if(!op_child(w,e,OP_MOUNT,-16,81,1))return 0;a->palette=0x40;q->opening.stage=0;op_start_entry(q,opening_stages[0][0]);op_schedule(e,OP_FORT_WAIT);return 0;
 case OP_FORT_ACTIVE:q->opening.margin=288;if(!rank_hp(w,q))return 0;a->display_flags|=64;a->transform.priority=0xf000;if(!spawn_parts(w,e,14,5)||!op_watch(w,e,0xe9442)||!op_child(w,e,OP_FORT_BLOCKER,120,0,1))return 0;break;
 case OP_FORT_WRECK:{if(!vg_score(w,a,0x5000))return 0;/*86586 is allocated without5DD02: keep allocator transform/source11. */struct PlayerTransform t={.x=-256,.y=513*256,.priority=65535,.display_group=255};if(!op_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&t,OP_NUKE)||!op_sync(w,a)||!op_stamp(w,a,OPS_S295E14)||!op_stamp(w,a,OPS_S295E38))return 0;op_defense(q,65535);if(!interaction_sound(w,0x1023)||!op_patch(w,OPV_P295A42)||!op_patch(w,OPV_P295A56))return 0;op_start(q,OPA_FORT_WRECK);q->opening.watch=255;break;}
 case OP_GATE_BLOCKER:case OP_FORT_BLOCKER:break;
 case OP_POST:a->palette=0x3f;vg_reset(a);a->transform.priority=0;op_start(q,OPA_POST);break;
 case OP_LIGHT:q->opening.margin=128;a->hit.hp=60;op_start(q,OPA_LIGHT);a->display_flags|=64;break;
 case OP_LIGHT_WRECK:op_defense(q,65535);break;
 case OP_GATE_ROOF:a->palette=0x3f;q->opening.margin=64;a->hit.hp=120;op_start(q,OPA_GATE_ROOF);a->transform.priority=0xbfff;break;
 case OP_GATE_ROOF_WRECK:op_start(q,OPA_GATE_ROOF_WRECK);op_defense(q,65535);break;
 case OP_DOOR:a->palette=0x40;op_start(q,OPA_DOOR);a->display_flags|=64;a->transform.priority=0xbfff;break;
 case OP_MOUNT:a->palette=0x40;q->opening.margin=96;op_start(q,OPA_MOUNT);a->transform.priority=0xbfff;break;
 case OP_MOUNT_FALL:op_start(q,OPA_MOUNT_FALL);vg_reset(a);a->ay=-16;break;
 case OP_SIDE:a->palette=0x24;q->opening.margin=96;transform_integer_offset(&a->transform,168,0);a->transform.priority=0;break;
 case OP_PART:a->phase=0;a->hit.hp=30;break;
 case OP_NUKE:if(a->variant){op_attack(q,OPB_NUKE);hit_begin_attack(&a->hit);}a->transform.x=(int32_t)w->camera_x*256;a->transform.y=(int32_t)(w->camera_y+256)*256;hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;w->enemies->clear_ordnance=1;op_schedule(e,OP_NUKE_END);return 0;
 case OP_NUKE_END:w->enemies->clear_ordnance=0;op_attack(q,65535);hit_begin_attack(&a->hit);entity_delete(w->entities,e);return 0;
 default:return enemy_fault(w);
 }return !w->fault;
}
int op_fort_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;
 if(s==OP_BARRIER_WAIT||s==OP_GATE_WAIT){if(!op_sync(w,a)||!w->secondary_view_valid)return enemy_fault(w);if((int16_t)w->secondary_x_offset>(s==OP_BARRIER_WAIT?0xa10:0xa20)){a->state=s==OP_BARRIER_WAIT?OP_BARRIER_ACTIVE:OP_GATE_ACTIVE;if(!op_fort_enter(w,e))return 0;return op_fort_frame(w,e,a->state);}if(s==OP_BARRIER_WAIT){interaction_body_contact(w,a,&opening_contacts[0]);op_animate(w,e);}return !w->fault;}
 if(s==OP_FORT_WAIT){int16_t x=0;if(!op_before_sync_x(w,a,&x))return 0;if(x<=304){a->state=OP_FORT_ACTIVE;if(!op_fort_enter(w,e))return 0;return op_fort_frame(w,e,OP_FORT_ACTIVE);}w->enemies->intro_gate=0;if(!op_sync(w,a))return 0;if(op_sx(w,a)<=208)op_animate(w,e);return !w->fault;}
 if(s==OP_GATE_BLOCKER||s==OP_FORT_BLOCKER){interaction_body_contact(w,a,&opening_contacts[s==OP_GATE_BLOCKER?1:2]);if(!op_sync(w,a))return 0;p=op_parent(w,e);if(!p)return 0;if(p->actor.phase==(s==OP_GATE_BLOCKER?0x88:255))entity_delete(w->entities,e);return !w->fault;}
 if(s==OP_PART){p=op_parent(w,e);if(!p||q->opening.part>=sizeof(parts)/sizeof(*parts))return enemy_fault(w);const struct Part*t=&parts[q->opening.part];int finish=p->actor.phase==255;if(!finish){if(!op_sync(w,a))return 0;op_animate(w,e);if(!op_hit(w,a))return 0;if(a->hit.flags&1){a->hit.hp=30;a->hit.flags&=254;if(a->phase>=t->count)return enemy_fault(w);if(!op_patch(w,t->patch[a->phase++]))return 0;finish=a->phase==t->count;}}if(finish){p->dead|=q->dead;entity_delete(w->entities,e);}else op_cull(w,e);return !w->fault;}
 if(s==OP_MOUNT_FALL){if(interaction_move(w,a,WMOVE_POINT)<0)return 0;op_animate(w,e);if(op_word(transform_y_up(&a->transform)+w->camera_y)<=352){if(!interaction_sound(w,0x1030))return 0;transform_integer_offset(&a->transform,0,-32);if(!op_pattern(w,e,OPP_P295C32,0,32)||!op_pattern(w,e,OPP_P295C44,0,32)||!op_pattern(w,e,OPP_P295CD4,0,0))return 0;entity_delete(w->entities,e);}else op_cull(w,e);return !w->fault;}
 if(s==OP_BARRIER_ACTIVE||s==OP_BARRIER_DAMAGED||s==OP_FORT_ACTIVE)w->enemies->intro_gate=0;
 if(!op_sync(w,a))return 0;
 if(s==OP_SIDE){p=op_parent(w,e);if(!p)return 0;if(p->actor.phase==255){if(!op_stamp(w,a,OPS_S295F4A))return 0;transform_integer_offset(&a->transform,-80,0);if(!op_burst(w,e,OPP_P295B5A,OPP_P295B6C,OPP_P295CB0,0,0))return 0;entity_delete(w->entities,e);return !w->fault;}op_start_entry(q,opening_stages[1][p->opening.stage&3]);if(op_sx(w,a)<=400)op_animate(w,e);op_cull(w,e);return !w->fault;}
 if(s==OP_MOUNT){if(!w->secondary_view_valid)return enemy_fault(w);if((int16_t)w->secondary_x_offset>=0xa70)op_animate(w,e);p=op_parent(w,e);if(!p)return 0;if(p->actor.phase==255){if(!op_pattern(w,e,OPP_P295C32,0,0)||!op_pattern(w,e,OPP_P295CD4,0,0))return 0;op_schedule(e,OP_MOUNT_FALL);}op_cull(w,e);return !w->fault;}
 if(s==OP_BARRIER_ACTIVE||s==OP_BARRIER_DAMAGED)interaction_body_contact(w,a,&opening_contacts[0]);
 if(s!=OP_LIGHT_WRECK)op_animate(w,e);
 if(s==OP_POST||s==OP_DOOR||s==OP_LIGHT||s==OP_LIGHT_WRECK||s==OP_GATE_ROOF||s==OP_GATE_ROOF_WRECK){p=op_parent(w,e);if(!p)return 0;
  if(s==OP_POST||s==OP_DOOR){if(p->actor.phase==0x88)entity_delete(w->entities,e);return !w->fault;}
  if(s==OP_LIGHT||s==OP_LIGHT_WRECK){if(p->actor.phase==0x88){if(!op_patch(w,OPV_P295B46))return 0;entity_delete(w->entities,e);return !w->fault;}if(s==OP_LIGHT){if(a->hit.last_kind!=5&&!op_hit(w,a))return 0;if(a->hit.last_kind==5||(a->hit.flags&1)){a->hit.flags&=247;if(!op_patch(w,OPV_P295AE2)||!op_patch(w,OPV_P295B0A)||!op_patch(w,OPV_P29584E)||!op_patch(w,OPV_P2958C6))return 0;op_schedule(e,OP_LIGHT_WRECK);}}}
  else if(s==OP_GATE_ROOF){int collapse=p->actor.phase==255;if(!collapse){if(!op_hit(w,a))return 0;if(a->hit.flags&1){a->hit.flags&=254;collapse=1;}}if(collapse){if(!op_patch(w,OPV_P295A2E))return 0;op_schedule(e,OP_GATE_ROOF_WRECK);}}
  else if(p->actor.phase==0x88){entity_delete(w->entities,e);return !w->fault;}
 }else if(s==OP_BARRIER_ACTIVE||s==OP_GATE_ACTIVE||s==OP_BARRIER_DAMAGED||s==OP_GATE_DAMAGED||s==OP_FORT_ACTIVE){
  if(!op_hit(w,a))return 0;int dead=a->hit.flags&1;
  if(s==OP_BARRIER_ACTIVE||s==OP_GATE_ACTIVE){if(dead)a->hit.flags&=254;if(dead||q->dead==(s==OP_BARRIER_ACTIVE?15:255)){a->phase=255;if(!triple(w,e))return 0;op_schedule(e,s==OP_BARRIER_ACTIVE?OP_BARRIER_DAMAGED:OP_GATE_DAMAGED);}}
  else if(s==OP_BARRIER_DAMAGED||s==OP_GATE_DAMAGED){if(dead){if(!triple(w,e))return 0;if(s==OP_BARRIER_DAMAGED&&!op_stamp(w,a,OPS_S295E4E))return 0;a->phase=0x88;w->enemies->clear_ordnance=1;op_schedule(e,s==OP_BARRIER_DAMAGED?OP_BARRIER_WRECK:OP_GATE_WRECK);}}
  else{int destroy=0;if(dead){a->hit.flags&=254;q->opening.stage=op_word(q->opening.stage+1);op_start_entry(q,opening_stages[0][q->opening.stage&3]);if(!rank_hp(w,q))return 0;if(q->opening.stage>=3)destroy=1;}if(destroy||q->dead==31){a->phase=255;if(!triple(w,e))return 0;op_schedule(e,OP_FORT_WRECK);}}
 }
 op_cull(w,e);return !w->fault;
}
