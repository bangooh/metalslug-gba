#include "tank_internal.h"
/* $06A07E..$06A7D6, $06AC5C and the grounded motion/decision helpers.
 * Body and weapon keep independent command/ack clocks. */
static void reset(struct WeaponActor*a){a->vx=a->vy=a->ax=a->ay=0;a->transform.x=(a->transform.x>>8)*256;a->transform.y=(a->transform.y>>8)*256;}
static uint8_t nearest(struct WeaponWorld*w,struct WeaponActor*a){int i=enemy_nearest_x(w,a,0);return (uint8_t)(i<0?1:i);}
static int16_t player_x(struct WeaponWorld*w,unsigned i){struct PlayerBodyState*p=w->players[i&1];return p?tk_word((p->actor.transform.x>>8)-w->camera_x):-1;}
static int16_t player_y(struct WeaponWorld*w,unsigned i){struct PlayerBodyState*p=w->players[i&1];return p?tk_word(transform_y_up(&p->actor.transform)+w->camera_y):-1;}
static int close_x(struct WeaponWorld*w,struct EnemyActor*q,unsigned i){int16_t dx=tk_word(tk_sx(w,&q->actor)-player_x(w,i));return dx>=-32&&dx<=32;}
static void reacquire(struct WeaponWorld*w,struct EnemyActor*q){
 if(q->bazooka.target<2&&enemy_player_live(w->players[q->bazooka.target]))return;
 q->bazooka.target=nearest(w,&q->actor);
 if(enemy_player_live(w->players[0])&&close_x(w,q,0)){q->bazooka.target=0;if(enemy_player_live(w->players[1])&&close_x(w,q,1))q->bazooka.target=nearest(w,&q->actor);}
 else if(enemy_player_live(w->players[1])&&close_x(w,q,1))q->bazooka.target=1;
}
static int mismatch(struct EnemyActor*q){return (q->bazooka.direction^q->actor.transform.facing)&1;}
static int target_turn(struct WeaponWorld*w,struct EnemyActor*q){
 if(!q->params[6]||q->bazooka.target==255)return 0;
 if(q->bazooka.target>=2)return enemy_fault(w);int16_t x=player_x(w,q->bazooka.target),own=tk_sx(w,&q->actor);
 if(q->bazooka.direction&1?tk_word(x-8)<=own:tk_word(x+8)>=own){q->bazooka.direction^=1;return 1;}return 0;
}
static int turn(struct WeaponWorld*w,struct EnemyActor*q){
 struct WeaponActor*a=&q->actor;
 if((a->vx<0&&(a->movement_flags&2))||(a->vx>0&&(a->movement_flags&1))){q->bazooka.direction^=1;return 1;}
 if(tk_sx(w,a)<=48&&!(q->bazooka.direction&1)){q->bazooka.direction|=1;return 1;}
 if(tk_sx(w,a)>=272&&(q->bazooka.direction&1)){q->bazooka.direction&=254;return 1;}
 return target_turn(w,q);
}
static int move(struct WeaponWorld*w,struct EnemyActor*q,int dying){
 struct WeaponActor*a=&q->actor;if(!dying&&(a->hit.flags&64))return 0;
 if(!dying)interaction_body_contact(w,a,&tank_bazooka_contact);
 if(interaction_move(w,a,WMOVE_SYNC)<0)return -1;
 int foot=terrain_weapon_foot_contact(w->interactions->terrain,a);if(foot<0){enemy_fault(w);return -1;}
 if(!foot){int r=interaction_move(w,a,WMOVE_ITEM);if(r<0)return -1;if(r)a->vy=a->ay=0;return 0;}
 return dying?0:interaction_move(w,a,WMOVE_FLAT);
}
static int gate_a(struct WeaponWorld*w,struct EnemyActor*q){
 q->bazooka.wait=tk_word(q->bazooka.wait-1);if(q->bazooka.wait>0)return 0;
 unsigned i=nearest(w,&q->actor);int16_t dy=tk_word(player_y(w,i)-tk_sy(w,&q->actor));if(dy<=0)dy=tk_word(-dy);if(dy>128)dy=128;
 unsigned index=(uint16_t)dy>>5;if(index>=5)return enemy_fault(w);
 int range=tank_bazooka_ranges[q->params[0]&3][index];int16_t dx=tk_word(player_x(w,i)-tk_sx(w,&q->actor));return dx>=-range&&dx<=range;
}
static int gate_b(struct WeaponWorld*w,struct EnemyActor*q){
 if(q->bazooka.wait>0)return 0;if(!q->params[6])return 1;if(q->bazooka.target==255)return 0;if(q->bazooka.target>=2)return enemy_fault(w);
 int16_t x=player_x(w,q->bazooka.target),own=tk_sx(w,&q->actor);return tk_word(x-8)<=own&&tk_word(x+8)>=own;
}
static int outside(struct WeaponWorld*w,struct EnemyActor*q){return interaction_outside(w,&q->actor,tank_bounds[2],0)||tk_sx(w,&q->actor)<-128;}
static int prepare(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(!tk_settle(w,a))return 0;
 a->transform.facing=q->bazooka.direction=q->params[1]&1;q->bazooka.variant=q->params[2];a->palette=tank_crew_palettes[q->bazooka.variant&3];a->hit_palette=0x7b;
 interaction_priority(w,a,0x8000,20);tk_defense(q,TKB_CREW_DEF);
 struct NativeEntity*n=tk_child(w,e,TK_WEAPON,ENTITY_FIRST_CHILD);if(!n)return 0;struct EnemyActor*p=&n->payload.enemy;
 p->params[0]=q->params[0];p->params[4]=q->params[4];p->params[5]=q->params[5];p->params[6]=q->params[6];p->params[7]=q->params[7];p->bazooka.variant=q->params[2];a->hit.type_flags|=4;return 1;
}
int bz_enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned state=a->state;q->entry=0;
 switch(state){
 case TK_BZ_A_NEW:
  tk_attack(q,q->bazooka.attack_kind?TKB_BZ_ATTACK_B:TKB_BZ_ATTACK_A);hit_begin_attack(&a->hit);q->params[5]=q->params[6]=0;if(!prepare(w,e))return 0;
  q->bazooka.speed=tk_rank(w,q,TKR_BZ_SPEED);q->bazooka.wait=tk_rank(w,q,TKR_CREW_WAIT);a->hit.hp=tk_rank(w,q,TKR_CREW_HP);enemy_schedule(q,TK_BZ_A_IDLE);return 2;
 case TK_BZ_A_IDLE:q->bazooka.command=0;q->bazooka.count=(uint8_t)tk_rank(w,q,TKR_BZ_IDLE);a->vx=0;tk_start(q,TKA_BZ_IDLE);break;
 case TK_BZ_A_WALK:case TK_BZ_B_WALK:
  if(state==TK_BZ_B_WALK)target_turn(w,q);q->bazooka.command=0;a->vx=q->bazooka.direction&1?q->bazooka.speed:tk_word(-q->bazooka.speed);tk_start(q,mismatch(q)?TKA_BZ_BACK:TKA_BZ_WALK);break;
 case TK_BZ_A_STOP:a->vx=0;q->bazooka.command=0;tk_start(q,mismatch(q)?TKA_BZ_STOP_BACK:TKA_BZ_STOP);break;
 case TK_BZ_A_TURN:q->bazooka.command=0;tk_start(q,mismatch(q)?TKA_BZ_TURN_BACK:TKA_BZ_TURN);break;
 case TK_BZ_A_ALERT:reset(a);q->bazooka.target=nearest(w,a);q->bazooka.command=1;q->bazooka.count=q->bazooka.ack=0;tk_start(q,TKA_BZ_IDLE);break;
 case TK_BZ_A_CHOOSE:q->bazooka.count=(uint8_t)tk_rank(w,q,TKR_CREW_COUNT);enemy_schedule(q,TK_BZ_A_FIRE);return 2;
 case TK_BZ_A_NEXT:q->bazooka.count--;enemy_schedule(q,(int8_t)q->bazooka.count<=0?TK_BZ_A_RECOVER:TK_BZ_A_FIRE);return 2;
 case TK_BZ_A_FIRE:q->bazooka.ack=0;q->bazooka.command=2;q->bazooka.wait=tk_rank(w,q,TKR_CREW_REPEAT);tk_start(q,TKA_CREW_FIRE);break;
 case TK_BZ_A_RECOVER:q->bazooka.wait=tk_word(tk_rank(w,q,TKR_CREW_WAIT)+30);q->bazooka.count=q->bazooka.ack=0;q->bazooka.command=3;tk_start(q,TKA_BZ_IDLE);break;
 case TK_BZ_B_NEW:
  q->params[5]=1;if(!prepare(w,e))return 0;q->bazooka.speed=tk_rank(w,q,TKR_BZ_B_SPEED);q->bazooka.wait=tk_rank(w,q,TKR_BZ_B_WAIT);a->hit.hp=tk_rank(w,q,TKR_BZ_B_HP);reacquire(w,q);tk_attack(q,TKB_BZ_ATTACK_B);enemy_schedule(q,TK_BZ_B_WALK);return 2;
 case TK_BZ_B_IDLE:q->bazooka.target_clock=(q->bazooka.target_clock+1)&3;if(!q->bazooka.target_clock)q->bazooka.target=nearest(w,a);q->bazooka.command=0;q->bazooka.count=(uint8_t)tk_rank(w,q,TKR_BZ_B_IDLE);a->vx=0;tk_start(q,TKA_BZ_IDLE);break;
 case TK_BZ_B_ALERT:if(!q->params[6]){enemy_schedule(q,TK_BZ_B_CHOOSE);return 2;}reset(a);q->bazooka.wait=tk_rank(w,q,TKR_BZ_B_ALERT);tk_start(q,TKA_BZ_IDLE);break;
 case TK_BZ_B_CHOOSE_MOVE:case TK_BZ_B_CHOOSE:
  if(state==TK_BZ_B_CHOOSE)reset(a);q->bazooka.ack=0;q->bazooka.count=(uint8_t)tk_rank(w,q,TKR_BZ_B_COUNT);enemy_schedule(q,state==TK_BZ_B_CHOOSE?TK_BZ_B_FIRE:TK_BZ_B_FIRE_MOVE);return 2;
 case TK_BZ_B_NEXT:case TK_BZ_B_NEXT_MOVE:
  q->bazooka.count--;enemy_schedule(q,(int8_t)q->bazooka.count<=0?(state==TK_BZ_B_NEXT?TK_BZ_B_RECOVER:TK_BZ_B_WALK):(state==TK_BZ_B_NEXT?TK_BZ_B_FIRE:TK_BZ_B_FIRE_MOVE));return 2;
 case TK_BZ_B_FIRE:case TK_BZ_B_FIRE_MOVE:q->bazooka.ack=0;q->bazooka.command=2;q->bazooka.wait=tk_rank(w,q,TKR_BZ_B_REPEAT);tk_start(q,state==TK_BZ_B_FIRE?TKA_CREW_FIRE:TKA_BZ_FIRE_MOVE);break;
 case TK_BZ_B_RECOVER:q->bazooka.wait=tk_rank(w,q,TKR_BZ_B_WAIT);q->bazooka.target=255;reacquire(w,q);enemy_schedule(q,TK_BZ_B_IDLE);return 2;
 case TK_BZ_DIE:a->vx=0;a->display_flags&=253;q->bazooka.command=6;tk_start(q,TKA_CREW_DIE);break;
 case TK_BZ_FADE:q->bazooka.wait=60;tk_defense(q,65535);a->display_flags&=253;q->bazooka.command=6;break;
 case TK_BZ_SPAWNER:{
  a->phase=0;struct NativeEntity*n=tk_child(w,e,TK_ALLY,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.bazooka.ally_defense=1;n->payload.enemy.params[0]=18;q->bazooka.child_index=n->index;q->bazooka.child_generation=n->generation;break;
 }
 default:return enemy_fault(w);
 }return !w->fault;
}
int bz_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(state==TK_BZ_SPAWNER){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;struct NativeEntity*n=entity_get(w->entities,(struct EntityRef){q->bazooka.child_index,q->bazooka.child_generation});if(!n)return enemy_fault(w);if((n->payload.enemy.actor.hit.flags&1)||outside(w,q))enemy_schedule(q,TK_DELETE);return !w->fault;}
 if(state==TK_BZ_FADE){q->bazooka.wait=tk_word(q->bazooka.wait-1);if(q->bazooka.wait<=0)enemy_schedule(q,TK_DELETE_TREE);return 1;}
 if(state==TK_BZ_B_FIRE_MOVE){q->bazooka.wait=tk_word(q->bazooka.wait-1);if(q->bazooka.wait<=0)enemy_schedule(q,TK_BZ_B_NEXT_MOVE);}
 int r=move(w,q,state==TK_BZ_DIE);if(r<0)return 0;
 /* The moving-fire alias decrements before motion in source; it has no
  * incoming edge from these six constructors, but retains its full state. */
 if(state==TK_BZ_A_WALK||state==TK_BZ_A_TURN||state==TK_BZ_B_WALK||state==TK_BZ_B_FIRE_MOVE){if(r){q->bazooka.direction^=1;enemy_schedule(q,state>=TK_BZ_B_NEW?TK_BZ_B_IDLE:TK_BZ_A_STOP);}}
 int done=tk_animate(w,e);
 switch(state){
 case TK_BZ_A_IDLE:q->bazooka.count--;if((int8_t)q->bazooka.count<=0)enemy_schedule(q,TK_BZ_A_TURN);if(gate_a(w,q))enemy_schedule(q,TK_BZ_A_ALERT);break;
 case TK_BZ_A_WALK:if(turn(w,q))enemy_schedule(q,TK_BZ_A_STOP);if(gate_a(w,q))enemy_schedule(q,TK_BZ_A_STOP);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;break;
 case TK_BZ_A_STOP:if(done)enemy_schedule(q,TK_BZ_A_IDLE);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;break;
 case TK_BZ_A_TURN:if(done)enemy_schedule(q,TK_BZ_A_WALK);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;break;
 case TK_BZ_A_ALERT:if(q->bazooka.ack)enemy_schedule(q,TK_BZ_A_CHOOSE);break;
 case TK_BZ_A_FIRE:if(q->bazooka.ack){q->bazooka.ack=0;q->bazooka.command=4;}q->bazooka.wait=tk_word(q->bazooka.wait-1);if(q->bazooka.wait<=0)enemy_schedule(q,TK_BZ_A_NEXT);break;
 case TK_BZ_A_RECOVER:if(q->bazooka.ack)enemy_schedule(q,TK_BZ_A_TURN);break;
 case TK_BZ_B_WALK:if(turn(w,q))enemy_schedule(q,TK_BZ_B_IDLE);q->bazooka.wait=tk_word(q->bazooka.wait-1);if(gate_b(w,q))enemy_schedule(q,TK_BZ_B_ALERT);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;break;
 case TK_BZ_B_IDLE:q->bazooka.count--;if((int8_t)q->bazooka.count<=0)enemy_schedule(q,TK_BZ_B_WALK);q->bazooka.wait=tk_word(q->bazooka.wait-1);if(gate_b(w,q))enemy_schedule(q,TK_BZ_B_ALERT);break;
 case TK_BZ_B_ALERT:q->bazooka.wait=tk_word(q->bazooka.wait-1);if(q->bazooka.wait<=0)enemy_schedule(q,TK_BZ_B_CHOOSE);break;
 case TK_BZ_B_FIRE:if(done&&q->bazooka.ack){q->bazooka.ack=0;q->bazooka.command=0;}q->bazooka.wait=tk_word(q->bazooka.wait-1);if(q->bazooka.wait<=0)enemy_schedule(q,TK_BZ_B_NEXT);break;
 case TK_BZ_B_FIRE_MOVE:if(q->bazooka.ack){q->bazooka.ack=0;q->bazooka.command=0;}if(turn(w,q))enemy_schedule(q,TK_BZ_B_IDLE);break;
 case TK_BZ_DIE:if(done||outside(w,q))enemy_schedule(q,TK_BZ_FADE);return !w->fault;
 default:return enemy_fault(w);
 }
 if(state>=TK_BZ_B_NEW)reacquire(w,q);q->bazooka.hurt=0;
 if(a->hit.flags&8){if(!tk_hit_sound(w,a))return 0;a->hit.flags&=247;q->bazooka.hurt=1;}
 if(a->hit.flags&1)enemy_schedule(q,TK_BZ_DIE);if(outside(w,q))enemy_schedule(q,TK_BZ_FADE);return !w->fault;
}
