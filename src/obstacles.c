#include "turret142_internal.h"
/* $06063A..$060CD4. Stage transitions publish actual terrain stamps. */
int t42_stamp(struct WeaponWorld*w,struct NativeEntity*e,unsigned i){if(i>=T42S_COUNT||!w->interactions||!w->interactions->terrain)return enemy_fault(w);const struct Turret142Stamp*s=&turret142_stamps[i];return terrain_stamp(w->interactions->terrain,&e->payload.enemy.actor.transform,s->dx,s->dy,s->width,s->height,s->cells)?1:enemy_fault(w);}
int obstacle_enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned v=q->turret142.variant;
 switch(a->state){
 case T42_A_NEW:if(v>2)return enemy_fault(w);a->palette=0x6c;interaction_priority(w,a,0x8000,12);t42_defense(q,T42B_A_BODY);a->hit.hp=t42_rank(w,q,T42R_A_HP);if(!t42_stamp(w,e,T42S_A_INIT))return 0;t42_start(q,T42A_A_IDLE);a->state=T42_A_IDLE;/* FALLTHROUGH */
 case T42_A_IDLE:a->hit.flags&=247;break;
 case T42_A_HURT:t42_start(q,T42A_A_HURT);break;
 case T42_A_DEAD:if(interaction_move(w,a,WMOVE_SYNC)<0||!t42_stamp(w,e,v?T42S_A_FIRST1:T42S_A_FIRST0))return 0;a->hit.flags&=254;t42_start(q,T42A_A_DEAD);break;
 case T42_A_RESET:a->hit.flags&=247;if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;a->hit.hp=t42_rank(w,q,T42R_A_HP2);a->state=T42_A_IDLE2;break;
 case T42_A_IDLE2:break;
 case T42_A_HURT2:t42_start(q,T42A_A_HURT2);break;
 case T42_A_DEAD2:a->hit.flags&=254;if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;t42_start(q,T42A_A_DEAD2);if(v>2||!t42_stamp(w,e,T42S_A_SECOND0+v))return enemy_fault(w);enemy_publish_death(w,a);break;
 case T42_A_WRECK:t42_defense(q,65535);a->hit.flags&=247;break;
 case T42_B_NEW:if(v>2)return enemy_fault(w);a->palette=0x82;if(!t42_stamp(w,e,T42S_B_INIT0+v))return 0;a->hit.hp=t42_rank(w,q,T42R_B_HP);interaction_priority(w,a,0x8000,12);t42_start(q,T42A_B_IDLE);a->state=T42_B_IDLE;break;
 case T42_B_DEAD:t42_start(q,T42A_B_DEAD);break;
 case T42_C_NEW:if(v>1)return enemy_fault(w);a->palette=turret142_prop_palettes[q->params[0]<3?q->params[0]:0];t42_start(q,T42A_C_IDLE);if(!t42_stamp(w,e,T42S_C_INIT0+v))return 0;a->hit.hp=100;interaction_priority(w,a,0x8000,12);a->state=T42_C_IDLE;return 0;/* Original installs handler and returns. */
 case T42_C_DEAD:break;/* Graph selected on the hit frame at $060C6A. */
 case T42_PROP_NEW:if(v>=5||!t42_stamp(w,e,T42S_PROP0+v))return enemy_fault(w);a->palette=14;animation_drawing(&a->animation,&turret142_program,turret142_prop_drawings[v]);a->state=T42_PROP;return 0;
 case T42_SMOKE:a->palette=0x6c;t42_start(q,T42A_SMOKE);a->transform.priority=0x2000;break;
 case T42_FLAME:a->palette=0x82;a->transform.priority=0xf000;t42_start(q,T42A_B_IDLE);break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static void collapse(struct WeaponWorld*w,struct NativeEntity*e){(void)w;struct EnemyActor*q=&e->payload.enemy;t42_start(q,tk_word((q->actor.transform.x>>8)-q->actor.hit.impact_x)<0?T42A_C_DEAD_B:T42A_C_DEAD_A);t42_schedule(e,T42_C_DEAD);}
int obstacle_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;int done;
 switch(s){
 case T42_A_IDLE:case T42_A_IDLE2:t42_animate(w,e);if(a->hit.flags&8)t42_schedule(e,s==T42_A_IDLE?T42_A_HURT:T42_A_HURT2);a->hit.flags&=247;if(a->hit.flags&1)t42_schedule(e,s==T42_A_IDLE?T42_A_DEAD:T42_A_DEAD2);break;
 case T42_A_HURT:case T42_A_HURT2:done=t42_animate(w,e);if(done)t42_schedule(e,s==T42_A_HURT?T42_A_IDLE:T42_A_IDLE2);a->hit.flags&=247;if(a->hit.flags&1)t42_schedule(e,s==T42_A_HURT?T42_A_DEAD:T42_A_DEAD2);break;
 case T42_A_DEAD:case T42_A_DEAD2:if(t42_animate(w,e))t42_schedule(e,s==T42_A_DEAD?T42_A_RESET:T42_A_WRECK);break;
 case T42_A_WRECK:t42_animate(w,e);break;
 case T42_B_IDLE:if(a->hit.flags&1)t42_schedule(e,T42_B_DEAD);else if(a->hit.flags&8)t42_start(q,T42A_B_HURT);t42_animate(w,e);break;
 case T42_B_DEAD:if(t42_animate(w,e)){entity_delete_children(w->entities,e);entity_delete(w->entities,e);return !w->fault;}break;
 case T42_C_IDLE:if(a->hit.flags&1)collapse(w,e);else if(a->hit.flags&8){if(a->hit.last_kind==4)collapse(w,e);else t42_start(q,T42A_C_HURT);}t42_animate(w,e);break;
 case T42_C_DEAD:if(t42_animate(w,e)){if(!tk_sound(w,0x109f))return 0;return boss_fx_transform(w,e,BFX_SMOKE);}break;
 case T42_PROP:if(!animation_output_direct(&a->animation,AN_OUTPUT_DIRECT))return enemy_fault(w);break;
 case T42_SMOKE:if(t42_animate(w,e))entity_delete(w->entities,e);return !w->fault;
 case T42_FLAME:t42_animate(w,e);a->hit.flags&=246;break;
 default:return enemy_fault(w);
 }if(t42_outside(w,a,2,0))entity_delete(w->entities,e);return !w->fault;
}
struct NativeEntity*obstacle_a_variant2(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t){struct NativeEntity*e=t42_allocate(w,p,o,t,T42_A_NEW);if(e)e->payload.enemy.turret142.variant=2;return e;}
struct NativeEntity*obstacle_flame(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t){return t42_allocate(w,p,o,t,T42_FLAME);}
int obstacle_has_children(struct WeaponWorld*w,const struct NativeEntity*e){return m4_has_children(w,e);}
