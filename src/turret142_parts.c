#include "turret142_internal.h"
static int follow(struct WeaponWorld*w,struct NativeEntity*e,int facing){struct EnemyActor*p=tk_parent(w,e);if(!p)return 0;struct WeaponActor*a=&e->payload.enemy.actor,*owner=&p->actor;/* $05E4EE copies integer XY and priority, retaining fractions. */transform_integer_offset(&a->transform,tk_word((owner->transform.x>>8)-(a->transform.x>>8)),tk_word(transform_y_up(&owner->transform)-transform_y_up(&a->transform)));a->transform.priority=owner->transform.priority;if(facing)a->transform.facing=owner->transform.facing;return 1;}
int t42_parts_enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;
 switch(a->state){
 case T42_GUN:a->palette=0x189;q->turret142.delay=0;q->turret142.pose=0;break;
 case T42_BARREL:a->palette=0x189;if(!follow(w,e,1))return 0;a->transform.priority--;t42_start(q,T42A_T_BARREL);break;
 case T42_SHELL:if(!tk_sound(w,0x109b))return 0;a->palette=0x187;t42_start(q,T42A_T_SHELL);m4_reset(a);a->ay=tk_word(-t42_rank(w,q,T42R_GRAVITY));p=tk_parent(w,e);if(!p)return 0;a->vx=p->actor.vx;transform_integer_offset(&a->transform,tk_word((p->actor.transform.x>>8)-(a->transform.x>>8)),tk_word(transform_y_up(&p->actor.transform)-transform_y_up(&a->transform)-16));a->transform.priority--;a->hit.hp=60;t42_defense(q,T42B_SHELL);t42_attack(q,T42B_SHELL_HIT);hit_begin_attack(&a->hit);break;
 case T42_BLAST:if(!tk_sound(w,0x1022)||!boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_FIRE))return 0;break;
 case T42_CRASH:q->turret142.timer=4;break;
 default:return enemy_fault(w);
 }return !w->fault;
}
int t42_parts_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(s==T42_GUN){if(!follow(w,e,1))return 0;struct EnemyActor*p=tk_parent(w,e);if(!p)return 0;a->palette=(p->turret142.hits&3)==1?0x18b:0x189;unsigned bank=p->turret142.pose<=6?p->turret142.pose:0;q->turret142.delay--;if((int8_t)q->turret142.delay<=0){q->turret142.pose++;if(q->turret142.pose==16)q->turret142.pose=0;if(q->turret142.pose>=16)return enemy_fault(w);q->turret142.delay=turret142_guns[bank][q->turret142.pose].delay;}if(q->turret142.pose>=16)return enemy_fault(w);animation_drawing(&a->animation,&turret142_program,turret142_guns[bank][q->turret142.pose].drawing);a->animation.drawing_flags=0;if(!animation_output_direct(&a->animation,AN_OUTPUT_DIRECT))return enemy_fault(w);if(p->actor.phase==255)entity_delete(w->entities,e);return !w->fault;}
 if(s==T42_CRASH){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;t42_attack(q,T42B_CRASH);hit_begin_attack(&a->hit);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;q->turret142.timer=tk_word(q->turret142.timer-1);if(q->turret142.timer<0)entity_delete(w->entities,e);return !w->fault;}
 if(s==T42_BLAST){t42_animate(w,e);entity_delete(w->entities,e);return !w->fault;}
 if(s==T42_BARREL){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;if(t42_animate(w,e)||t42_outside(w,a,1,1))entity_delete(w->entities,e);return !w->fault;}
 if(s!=T42_SHELL)return enemy_fault(w);int r=interaction_move(w,a,WMOVE_FALL);if(r<0)return 0;if(r)t42_schedule(e,T42_BLAST);t42_animate(w,e);struct EnemyActor*p=tk_parent(w,e);if(!p)return 0;if(p->actor.phase==255)t42_schedule(e,T42_BLAST);if(a->hit.flags&8){a->hit.flags&=247;if(!tk_sound(w,0x108d))return 0;}if(a->hit.flags&1)t42_schedule(e,T42_BLAST);else{if(!enemy_attack(w,a))return 0;if(a->hit.flags&2)t42_schedule(e,T42_BLAST);}a->ax=tk_word(-a->vx)>>4;if(t42_outside(w,a,1,1))entity_delete(w->entities,e);return !w->fault;
}
