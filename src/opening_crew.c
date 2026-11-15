#include "opening_internal.h"
#include "slug_surface.h"
/*5F384/5F38A use the same block with different parent-release gating. */
static int outside(struct WeaponWorld*w,struct NativeEntity*e,unsigned n,int force){return interaction_outside(w,&e->payload.enemy.actor,opening_bounds[n],force);}
static int puller_bounds(struct WeaponWorld*w,struct NativeEntity*e){if(outside(w,e,0,1)){struct EnemyActor*p=op_parent(w,e);if(!p)return 0;p->dead=0xee;entity_delete(w->entities,e);}return !w->fault;}
static int escape(struct WeaponWorld*w,struct NativeEntity*e){/*Delay adoption: source writes a next-task entry only. A later same-frame approach/death may override it. */op_schedule(e,OP_PULL_ESCAPE);return pow_spawn_surface(w,entity_ref(e),&e->payload.enemy.actor.transform);}
static int platform(struct WeaponWorld*w,struct WeaponActor*a){if(!w->interactions)return enemy_fault(w);struct TerrainMotion m={.platform_ids=a->platform_ids};if(!terrain_platform_publish(w->interactions->terrain,&m,op_word((a->transform.x>>8)-32),op_word(transform_y_up(&a->transform)+16),64,0))return enemy_fault(w);a->platform_ids=m.platform_ids;return 1;}
static int settle(struct WeaponWorld*w,struct WeaponActor*a){for(unsigned i=0;i<65536;i++){transform_integer_offset(&a->transform,0,-1);uint8_t shape;if(!w->point_shape||!w->point_shape(w->scene,a,(int16_t)(a->transform.x>>8),transform_y_up(&a->transform),&shape))return enemy_fault(w);if(shape)return 1;}return enemy_fault(w);}
int op_crew_enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;struct NativeEntity*n;
 switch(a->state){
 case OP_BLOCK:a->palette=0x27;q->scale_x=q->scale_y=255;a->transform.priority=0x4001;vg_reset(a);op_start(q,OPA_BLOCK);q->opening.cooldown=0;a->state=OP_BLOCK_WAIT;break;
 case OP_BLOCK_WAIT:break;
 case OP_BLOCK_IDLE:q->opening.signal=0;break;
 case OP_BLOCK_HOLD:q->dead=0;break;
 case OP_BLOCK_FALL:if(!op_child(w,e,OP_BLOCK_BACK,0,0,0))return 0;vg_reset(a);a->ay=-192;q->opening.signal=255;op_start(q,OPA_BLOCK_FALL);op_attack(q,OPB_BLOCK_ATTACK);hit_begin_attack(&a->hit);break;
 case OP_BLOCK_HIT:a->transform.priority=0xd000;a->ay>>=1;a->vy=0;op_attack(q,65535);op_start(q,OPA_BLOCK_HIT);break;
 case OP_BLOCK_LAND:if(!op_sync(w,a)||!interaction_sound(w,0x1053))return 0;a->transform.priority=0x8000;n=op_child(w,e,OP_LAND_FX,0,0,0);if(!n)return 0;n->payload.enemy.actor.transform.priority++;if(!op_stamp(w,a,OPS_S2C0F06))return 0;vg_reset(a);op_start(q,OPA_BLOCK_FLAT);break;
 case OP_BLOCK_BACK:a->palette=0x27;q->scale_x=q->scale_y=255;vg_reset(a);op_start(q,OPA_BLOCK_BACK);break;
 case OP_PULLER:a->palette=0x1d;transform_integer_offset(&a->transform,a->transform.facing&1?-60:74,48);vg_reset(a);op_start(q,OPA_PULL_INTRO);a->vy=-128;if(!settle(w,a))return 0;q->opening.signal=0;break;
 case OP_PULL_RUN:vg_reset(a);op_start(q,OPA_PULL_RUN);a->vx=a->transform.facing&1?512:-512;break;
 case OP_PULL_ACTION:p=op_parent(w,e);if(!p)return 0;/*5E4DC changes integerXY only. */transform_integer_offset(&a->transform,op_word((p->actor.transform.x>>8)-(a->transform.x>>8)),op_word(transform_y_up(&p->actor.transform)-transform_y_up(&a->transform)));transform_integer_offset(&a->transform,a->transform.facing&1?-24:24,16);vg_reset(a);op_start(q,OPA_PULL_ACTION);break;
 case OP_PULL_END:vg_reset(a);op_start(q,OPA_PULL_END);break;
 case OP_PULL_ESCAPE:infantry_force_escape(e,255);e->tick(w->entities,e,w);return 0;
 case OP_LAND_FX:a->palette=10;a->display_flags|=64;q->scale_x=q->scale_y=255;op_start(q,OPA_LAND_FX);if(outside(w,e,3,1)){entity_delete(w->entities,e);return 0;}op_defense(q,65535);break;
 default:return enemy_fault(w);
 }return !w->fault;
}
int op_crew_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;int done,r;
 if(s==OP_BLOCK_WAIT){if(!op_sync(w,a))return 0;if(op_sx(w,a)<=352)op_schedule(e,a->phase==0x88?OP_BLOCK_HOLD:OP_BLOCK_IDLE);return !w->fault;}
 if(s==OP_BLOCK_IDLE||s==OP_BLOCK_HOLD){if(!op_sync(w,a))return 0;op_animate(w,e);a->hit.flags&=247;p=op_parent(w,e);if(!p)return 0;if(s==OP_BLOCK_IDLE){if(enemy_nearest_x(w,a,&opening_targets[a->transform.facing&1])>=0){if((int8_t)q->opening.cooldown<=0){if(!op_child(w,e,OP_PULLER,0,0,0))return 0;op_schedule(e,OP_BLOCK_HOLD);}else q->opening.cooldown--;}}
  else{if((a->phase==0x88&&p->dead==255)||q->dead==255)op_schedule(e,OP_BLOCK_FALL);if(q->dead==0xee){q->opening.cooldown=30;op_schedule(e,OP_BLOCK_IDLE);}}if(p->dead==255)op_schedule(e,OP_BLOCK_FALL);if(outside(w,e,1,0))entity_delete(w->entities,e);return !w->fault;
 }
 if(s==OP_BLOCK_FALL){r=interaction_move(w,a,WMOVE_FALL);if(r<0)return 0;if(r)op_schedule(e,OP_BLOCK_LAND);op_animate(w,e);if(!platform(w,a))return 0;a->hit.flags&=247;if(!enemy_attack(w,a))return 0;if(a->hit.flags&2)op_schedule(e,OP_BLOCK_HIT);if(outside(w,e,1,0))entity_delete(w->entities,e);return !w->fault;}
 if(s==OP_BLOCK_HIT||s==OP_BLOCK_LAND){if(interaction_move(w,a,s==OP_BLOCK_HIT?WMOVE_FALL:WMOVE_FLAT)<0)return 0;done=op_animate(w,e);if((s==OP_BLOCK_HIT&&done)||outside(w,e,1,0))entity_delete(w->entities,e);return !w->fault;}
 if(s==OP_BLOCK_BACK){if(!op_sync(w,a))return 0;op_animate(w,e);struct NativeEntity*n=entity_get(w->entities,e->parent);if(!n)return enemy_fault(w);p=op_parent(w,n);if(!p)return 0;if(p->dead==255||outside(w,e,2,1))entity_delete(w->entities,e);return !w->fault;}
 if(s==OP_LAND_FX){/*44048 is the unconditional scroll conversion already represented by native world coordinates. */if(op_animate(w,e)||outside(w,e,3,1))entity_delete(w->entities,e);return !w->fault;}
 if(s==OP_PULLER){if(!op_sync(w,a))return 0;if(op_animate(w,e))op_schedule(e,OP_PULL_RUN);return puller_bounds(w,e);}
 if(s!=OP_PULL_RUN&&s!=OP_PULL_ACTION&&s!=OP_PULL_END)return enemy_fault(w);
 if(interaction_move(w,a,s==OP_PULL_RUN?WMOVE_POINT:WMOVE_SYNC)<0)return 0;done=op_animate(w,e);p=op_parent(w,e);if(!p)return 0;
 if(s==OP_PULL_ACTION&&done)op_schedule(e,OP_PULL_END);else if(s==OP_PULL_END&&done&&!escape(w,e))return 0;
 if(a->hit.flags&8)p->dead=0xee;
 if(s==OP_PULL_RUN){if(p->opening.signal==255&&!escape(w,e))return 0;int16_t x=op_word((p->actor.transform.x>>8)-(a->transform.x>>8)),y=op_word(transform_y_up(&p->actor.transform)-transform_y_up(&a->transform));if(x<0)x=op_word(-x);if(y<0)y=op_word(-y);if(x<=10&&y<=48)op_schedule(e,OP_PULL_ACTION);}
 else if(s==OP_PULL_ACTION&&p->opening.signal==255&&q->opening.signal!=255&&!escape(w,e))return 0;
 if(!human_death_hit_tail(w,e))return 0;return puller_bounds(w,e);
}
