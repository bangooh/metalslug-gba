#include "emitter59_internal.h"
int e59_fx_enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned i;
 switch(a->state){
 case E59_SHELL:{a->hit.type_flags|=16;a->palette=0x58;a->transform.priority=0xd000;unsigned v=e59_rank(w,q,E59R_TRAJECTORY)&3;i=q->emitter59.angle&3;const int16_t*t=emitter59_vectors[v][i];a->vx=tk_faced(a,t[0]);a->ay=t[1];a->vy=t[2];e59_start(q,E59A_SHELL);break;}
 case E59_BLAST:if(!tk_sound(w,0x1022))return 0;/* FALLTHROUGH */
 case E59_FIRE_FX:interaction_priority(w,a,0x4000,28);e59_defense(q,65535);return boss_fx_transform(w,e,BFX_FIRE),0;
 case E59_MUZZLE:interaction_priority(w,a,0x4000,0);a->transform.facing^=1;a->palette=8;e59_start(q,E59A_MUZZLE);break;
 case E59_RELEASED:a->palette=29;e59_start(q,E59A_RELEASED);break;
 case E59_SELECT:{
  /* Native correction:63106 calls RNG intoD0, then accidentally indexes
   * staleD1. Use that same single RNG result; never add another RNG draw.
   * An explicit research override still consumes the original RNG call. */
  unsigned choice=tk_random(w)&3;if(q->emitter59.release_valid)choice=q->emitter59.release_choice;
  if(choice>=4)return enemy_fault(w);
  struct NativeEntity*n=e59_allocate(w,w->projectile_group,ENTITY_FIRST_CHILD,&a->transform,E59_SOLDIER);if(!n)return 0;
  n->payload.enemy.actor.display_flags|=2;n->payload.enemy.emitter59.release_choice=emitter59_release_choices[choice];e59_schedule(e,E59_DELETE_TREE);return 0;}
 case E59_SOLDIER:i=q->emitter59.release_choice;if(i==0){a->palette=65535;return soldier_mortar_eject(w,e),0;}if(i>2)return enemy_fault(w);a->palette=w->enemies->blood_enabled?14:47;if(i==1)return soldier_tank_eject(w,e),0;infantry_stand_escape(e,255);e->tick(w->entities,e,w);return 0;
 case E59_SMOKE:interaction_priority(w,a,0x4000,28);interaction_priority(w,a,0x8000,28);e59_start(q,E59A_SMOKE);a->palette=0x6c;break;
 case E59_EXHAUST:a->transform.priority=0x2000;a->palette=0x6d;m4_reset(a);e59_start(q,E59A_EXHAUST);break;
 case E59_FRAGMENT:interaction_priority(w,a,0x8000,0);i=tk_random(w)&1;return boss_fx_scatter_graph(w,e,i?BA_FX_FRAGMENT59_A:BA_FX_FRAGMENT59_B,0x6e,512),0;
 case E59_METAL:interaction_priority(w,a,0x8000,0);i=tk_random(w)&1;return boss_fx_scatter_selected(w,e,i?10:6,i?0x77:0x78,q->scale),0;
 case E59_SPARK:a->palette=4;e59_start(q,E59A_SPARK);break;
 case E59_PUFF:a->palette=4;e59_start(q,E59A_PUFF);q->emitter59.timer=(tk_random(w)&31)+20;transform_integer_offset(&a->transform,(tk_random(w)&63)-32,0);a->transform.priority--;m4_reset(a);a->vy=(tk_random(w)&255)+64;a->ay=-(a->vy>>6);q->emitter59.bob=0;if(q->emitter59.timer<=10)q->emitter59.timer=10;break;
 default:return enemy_fault(w);
 }return !w->fault;
}
int e59_fx_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;int r;unsigned bounds=2;
 if(s==E59_SHELL){r=interaction_move(w,a,WMOVE_FALL);if(r<0)return 0;if(r)e59_schedule(e,E59_BLAST);e59_animate(w,e);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;if(a->hit.flags&2)e59_schedule(e,E59_FIRE_FX);bounds=1;}
 else if(s==E59_PUFF){int16_t random=(int16_t)tk_random(w),delta=(uint16_t)random&1023;if(random<=0)delta=tk_word(-delta);q->emitter59.bob=tk_word(q->emitter59.bob+delta);a->vx=tk_word(boss_trig[0][(uint16_t)q->emitter59.bob>>8]*2);if(interaction_move(w,a,WMOVE_POINT)<0)return 0;if(e59_animate(w,e))e59_schedule(e,E59_DELETE_TREE);q->emitter59.timer=tk_word(q->emitter59.timer-1);if(q->emitter59.timer<=0)e59_schedule(e,E59_DELETE_TREE);}
 else if(s==E59_MUZZLE||s==E59_RELEASED||s==E59_SMOKE||s==E59_EXHAUST||s==E59_SPARK){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;if(e59_animate(w,e))e59_schedule(e,s==E59_RELEASED?E59_SELECT:E59_DELETE_TREE);}
 else return enemy_fault(w);
 if(e59_outside(w,a,bounds,1))e59_schedule(e,E59_DELETE_TREE);return !w->fault;
}
int e59_emit(struct WeaponWorld*w,struct NativeEntity*e,unsigned id){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*n;
 if(id==E59C_6342E){n=e59_child(w,e,E59_SPARK);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,tk_faced(a,-40),3);n->payload.enemy.actor.transform.priority++;n=e59_child(w,e,E59_SPARK);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,tk_faced(a,36),3);n->payload.enemy.actor.transform.facing^=1;n->payload.enemy.actor.transform.priority--;return 1;}
 if(id==E59C_6347E){if(!tk_sound(w,0x1067))return 0;unsigned angle=q->emitter59.angle&3;int16_t dx=tk_faced(a,emitter59_muzzles[angle][0]),dy=emitter59_muzzles[angle][1];n=e59_child(w,e,E59_MUZZLE);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,dx,dy);n->payload.enemy.emitter59.angle=q->emitter59.angle;n=e59_child(w,e,E59_SHELL);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,dx,dy);n->payload.enemy.emitter59.angle=q->emitter59.angle;id=E59C_634F6;}
 if(id==E59C_634F6){n=e59_child(w,e,E59_EXHAUST);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,24,0);n=e59_child(w,e,E59_EXHAUST);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,-16,0);return 1;}
 if(id==E59C_63524||id==E59C_6355C){if(id==E59C_63524)n=e59_child(w,e,E59_SMOKE);else{if(!tk_sound(w,0x1021))return 0;n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMOKE);}if(!n)return 0;for(unsigned i=0;i<17;i++){n=e59_child(w,e,id==E59C_63524?E59_FRAGMENT:E59_METAL);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,0,24);}return 1;}
 if(id==E59C_635A0){q->emitter59.releases--;if((int8_t)q->emitter59.releases<0)return 1;n=e59_child(w,e,E59_RELEASED);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,tk_faced(a,31),24);n->payload.enemy.actor.transform.facing^=1;return 1;}
 if(id==E59C_635D8){e59_attack(q,E59B_WRECK);hit_begin_attack(&a->hit);int r=enemy_attack(w,a);e59_attack(q,65535);if(!r)return 0;const struct Emitter59Stamp*s=&emitter59_stamp;return terrain_stamp(w->interactions->terrain,&a->transform,s->dx,s->dy,s->width,s->height,s->cells)?1:enemy_fault(w);}
 return enemy_fault(w);
}
/* A caller must supply the proven source selector; RNG D0 is NOT that input. */
int emitter59_set_release_choice(struct WeaponWorld*w,struct NativeEntity*e,unsigned choice){if(!e||e->kind!=ENTITY_ENEMY||choice>=4||(e->payload.enemy.actor.state!=E59_RELEASED&&e->payload.enemy.actor.state!=E59_SELECT))return enemy_fault(w);e->payload.enemy.emitter59.release_choice=choice;e->payload.enemy.emitter59.release_valid=1;return 1;}
