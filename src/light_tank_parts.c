#include "light_tank.h"
#include "items.h"
/* Attached actors keep their own animation cursor. Parent animation commands
 * change that cursor without replacing the part's semantic controller. */
static int16_t mirrored(struct WeaponActor*a,int16_t n){return a->transform.facing==1?n:m4_word(-n);}
static int follow(struct WeaponWorld*w,struct NativeEntity*e,int driver){struct EnemyActor*q=&e->payload.enemy,*p=m4_parent(w,e);if(!p)return 0;struct WeaponActor*a=&q->actor,*b=&p->actor;a->transform.x=b->transform.x;a->transform.y=b->transform.y;a->transform.priority=b->transform.priority;a->transform.facing=b->transform.facing;a->animation.freeze=b->animation.freeze;
 if(driver){a->transform.priority=m4_word(a->transform.priority+q->light_tank.priority_offset);transform_integer_offset(&a->transform,mirrored(a,-16),24);}else{q->light_tank.delay_mode=p->light_tank.delay_mode;q->light_tank.delay_row=p->light_tank.delay_row;}return 1;
}
static void human_palette(struct WeaponWorld*w,struct WeaponActor*a){a->palette=w->enemies->blood_enabled?14:47;}
static int missile_velocity(struct EnemyActor*q){unsigned angle=(q->light_tank.angle&31)*8;q->actor.vx=m4_word(((int32_t)boss_trig[1][angle]*q->light_tank.speed)>>8);q->actor.vy=m4_word(((int32_t)boss_trig[0][angle]*q->light_tank.speed)>>8);return 1;}
static int random_target(struct WeaponWorld*w){int a=enemy_player_live(w->players[0]),b=enemy_player_live(w->players[1]);return a&&b?(lt_random(w)&1):b?1:0;}
static int emit(struct WeaponWorld*w,struct NativeEntity*e,unsigned state,int dx,int dy){struct NativeEntity*n=lt_child(w,e,state,ENTITY_FIRST_CHILD);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,mirrored(&e->payload.enemy.actor,(int16_t)dx),(int16_t)dy);return 1;}
int lt_callback(struct WeaponWorld*w,struct NativeEntity*e,unsigned n){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*v;
 switch(n){
 case LTC_283CA:hit_begin_attack(&a->hit);return 1;
 case LTC_4A0D4:human_palette(w,a);return 1;
 case LTC_4A1BA:return interaction_sound(w,(uint16_t)q->light_tank.timer);
 case LTC_6A050:a->palette=26;return 1;
 case LTC_6A044:a->palette=14;return 1;
 case LTC_69FB8:return interaction_sound(w,0x1064)&&emit(w,e,LT_SMOKE,0,0)&&emit(w,e,LT_SHELL,32,40);
 case LTC_69F68:return interaction_sound(w,0x1064)&&emit(w,e,LT_SMOKE_BACK,40,48)&&emit(w,e,LT_MISSILE,40,62);
 case LTC_6E4CC:v=lt_child(w,e,LT_TRAIL,ENTITY_LAST_CHILD);if(!v)return 0;v->payload.enemy.params[6]=q->params[6];v->payload.enemy.actor.transform.priority--;q->light_tank.palette_cycle=(q->light_tank.palette_cycle+1)&3;if(q->light_tank.palette_cycle>=3)q->light_tank.palette_cycle=0;a->palette=0x157+q->light_tank.palette_cycle;return 1;
 case LTC_69EF8:v=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMOKE);if(!v)return 0;v->payload.enemy.actor.transform.priority=0x4000;return 1;
 case LTC_69F58:return item_spawn_drop(w,&a->transform,q->params[5],q->params[6],q->params[7],1,0);
 case LTC_69FF6:interaction_priority(w,a,0x4000,20);if(!q->light_tank.temporary){lt_attack(q,LTB_WRECK);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;const struct LightTankStamp*s=&light_tank_stamps[q->light_tank.stamp];if(!terrain_stamp(w->interactions->terrain,&a->transform,s->dx,s->dy,s->width,s->height,s->cells))return enemy_fault(w);}return 1;
 default:return enemy_fault(w);
 }
}
int lt_parts_enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned s=a->state;
 switch(s){
 case LT_DRIVER:a->palette=29;break;
 case LT_DRIVER_OUT:if((int8_t)q->params[0]>=2)q->params[0]=(uint8_t)lt_random(w);q->params[0]&=1;lt_start(q,q->params[0]?LTA_DRIVER_OUT1:LTA_DRIVER_OUT0);break;
 case LT_DRIVER_HIT:q->light_tank.priority_offset=0;lt_start(q,LTA_DRIVER_HIT);break;
 case LT_DRIVER_DIE:human_palette(w,a);m4_reset(a);lt_start(q,LTA_DRIVER_DIE);break;
 case LT_DRIVER_EXIT:interaction_priority(w,a,0x8000,24);transform_integer_offset(&a->transform,0,8);if(!q->params[0]){a->transform.facing^=1;a->vx=a->transform.facing&1?136:-136;a->vy=540;a->ay=-72;a->ax=0;human_palette(w,a);infantry_force_escape(e,255);e->tick(w->entities,e,w);return 3;}human_palette(w,a);transform_integer_offset(&a->transform,0,4);return soldier_tank_eject(w,e)?3:0;
 case LT_DRIVER_CORPSE:if(!follow(w,e,1))return 0;lt_animate(w,e);if(!human_corpse_transform(w,e,0))return 0;a->transform.priority+=2;return 3;
 case LT_TURRET:a->palette=q->params[3]?28:27;q->light_tank.impact_palette=a->hit_palette=0x77;a->hit.hp=lt_rank(w,q,LTR_TURRET_HP);break;
 case LT_TURRET_RELEASE:{struct EnemyActor*p=m4_parent(w,e);if(!p)return 0;p->light_tank.turret_index=p->light_tank.turret_generation=65535;entity_delete_children(w->entities,e);entity_delete(w->entities,e);return 0;}
 case LT_SMOKE:case LT_SMOKE_BACK:if(s==LT_SMOKE_BACK)a->transform.facing^=1;a->palette=8;lt_start(q,s==LT_SMOKE?LTA_SMOKE:LTA_SMOKE_BACK);interaction_priority(w,a,0x4000,28);break;
 case LT_SHELL:{unsigned i=lt_rank(w,q,LTR_SHELL)&3;const int16_t*v=light_tank_shell[0][i];a->vx=v[0];a->ay=v[1];a->vy=v[2];a->hit.type_flags|=16;a->palette=0x157;interaction_priority(w,a,0xd000,20);lt_start(q,LTA_SHELL);break;}
 case LT_TRAIL:a->palette=0x157;lt_start(q,LTA_TRAIL);break;
 case LT_MISSILE:a->hit.type_flags|=16;a->palette=35;q->light_tank.angle=12;a->hit.hp=5;interaction_priority(w,a,0xd000,20);q->light_tank.speed=lt_rank(w,q,LTR_MISSILE);{unsigned i=(uint16_t)q->light_tank.speed>>8;if(i>=16)return enemy_fault(w);q->light_tank.turn_delay=light_tank_turn_delays[i];q->light_tank.cooldown=light_tank_homing_delays[i];}q->light_tank.timer=90;lt_start(q,LTA_MISSILE);missile_velocity(q);break;
 case LT_HOME:q->light_tank.cooldown=600;q->light_tank.target=(int8_t)random_target(w);break;
 case LT_MISSILE_COAST:break;
 case LT_FIRE_BLAST:interaction_priority(w,a,0x4000,0);lt_defense(q,65535);return boss_fx_transform(w,e,BFX_FIRE)?3:0;
 case LT_SHELL_BLAST:case LT_SHELL_IMPACT:interaction_priority(w,a,0x4000,s==LT_SHELL_BLAST?28:0);lt_defense(q,65535);return boss_fx_transform(w,e,BFX_SMOKE)?3:0;
 default:return enemy_fault(w);
 }return !w->fault;
}
static int driver_score(struct WeaponWorld*w,struct WeaponActor*a){if((a->display_flags&2)||(int8_t)a->score_owner<0)return 1;return w->items&&item_score_add(w->items,a->score_owner,enemy_hit_scores[a->hit.last_kind&31])?1:enemy_fault(w);}
static int home(struct WeaponWorld*w,struct EnemyActor*q){q->light_tank.cooldown=m4_word(q->light_tank.cooldown+1);if((int8_t)q->light_tank.cooldown<=(int8_t)q->light_tank.turn_delay)return 1;q->light_tank.cooldown=0;unsigned i=(unsigned)(uint8_t)q->light_tank.target;if(i>=2)return enemy_fault(w);struct PlayerBodyState*p=w->players[i];int16_t x=p?m4_word((p->actor.transform.x>>8)-w->camera_x):-1,y=p?m4_word(transform_y_up(&p->actor.transform)+w->camera_y):-1;
 unsigned angle=((enemy_angle(m4_word(x-m4_sx(w,&q->actor)),m4_word(y-m4_sy(w,&q->actor)))+8)&255)>>3;if(angle!=(uint16_t)q->light_tank.angle)q->light_tank.angle=m4_word(q->light_tank.angle+(angle<(uint16_t)q->light_tank.angle?-1:1));q->light_tank.angle&=31;return missile_velocity(q);
}
int lt_parts_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;
 if(s==LT_DRIVER||s==LT_DRIVER_OUT||s==LT_DRIVER_HIT){if(!follow(w,e,1))return 0;p=m4_parent(w,e);if(!p)return 0;if(s==LT_DRIVER){q->light_tank.delay_mode=p->light_tank.delay_mode;q->light_tank.delay_row=p->light_tank.delay_row;lt_animate(w,e);return !w->fault;}
  a->animation.freeze=0;if(s==LT_DRIVER_HIT){if(!p->light_tank.freeze)lt_schedule(e,LT_DRIVER_DIE);lt_animate(w,e);return !w->fault;}
  if(lt_animate(w,e))lt_schedule(e,LT_DRIVER_EXIT);if(a->hit.flags&8){lt_schedule(e,LT_DRIVER_HIT);if(!driver_score(w,a))return 0;}return !w->fault;
 }
 if(s==LT_TURRET){if(!follow(w,e,0))return 0;lt_animate(w,e);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;if(a->hit.flags&8){if(!lt_hurt(w,a))return 0;a->hit.flags&=246;}
  if(a->phase!=0x33&&a->phase!=0x77&&a->hit.hp<=0){a->phase=0x33;if(!interaction_sound(w,0x102d))return 0;struct NativeEntity*n=entity_get(w->entities,e->parent);if(!n)return enemy_fault(w);lt_schedule(n,LT_TURRET_HIT);}if(a->phase==0x77)lt_schedule(e,LT_TURRET_RELEASE);if(entity_parent_deleting(w->entities,e))lt_schedule(e,LT_DELETE_TREE);return !w->fault;
 }
 if(s==LT_HOME){if(!home(w,q))return 0;}
 if(s==LT_MISSILE||s==LT_HOME||s==LT_MISSILE_COAST){if(s==LT_MISSILE){q->light_tank.cooldown=m4_word(q->light_tank.cooldown-1);if(q->light_tank.cooldown<=0)lt_schedule(e,LT_HOME);}q->light_tank.timer=m4_word(q->light_tank.timer-1);if(q->light_tank.timer<=0)lt_schedule(e,LT_MISSILE_COAST);int r=interaction_move(w,a,WMOVE_POINT);if(r<0)return 0;if(r)lt_schedule(e,LT_FIRE_BLAST);lt_animate(w,e);if(!enemy_attack(w,a))return 0;if(a->hit.flags&2)lt_schedule(e,LT_FIRE_BLAST);a->hit.flags&=247;if(a->hit.flags&1)lt_schedule(e,LT_FIRE_BLAST);if(interaction_outside(w,a,light_tank_bounds[1],1))lt_schedule(e,LT_DELETE_TREE);return !w->fault;}
 if(s==LT_TRAIL){p=m4_parent(w,e);if(!p)return 0;a->palette=p->actor.palette;}
 int r=interaction_move(w,a,s==LT_SHELL?WMOVE_FALL:WMOVE_SYNC);if(r<0)return 0;if(s==LT_SHELL&&r)lt_schedule(e,LT_SHELL_IMPACT);int done=lt_animate(w,e);
 if(s==LT_SHELL){if(!enemy_attack(w,a))return 0;if(a->hit.flags&2)lt_schedule(e,LT_SHELL_BLAST);if(interaction_outside(w,a,light_tank_bounds[2],1))lt_schedule(e,LT_DELETE_TREE);}
 else if(s==LT_TRAIL){if(done)lt_schedule(e,LT_DELETE_TREE);}
 else if(s==LT_SMOKE||s==LT_SMOKE_BACK){if(done)lt_schedule(e,LT_DELETE_TREE);}
 else if(s==LT_DRIVER_DIE){if(done||interaction_outside(w,a,boss_fx_bounds[8],1))lt_schedule(e,LT_DELETE_TREE);}
 else return enemy_fault(w);return !w->fault;
}
