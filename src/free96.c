#include "m4_internal.h"
#include "light_tank.h"
/* $0478FC..$0483D2 and airborne 106/107. These are armed soldiers; source
 * comments calling the aiming tables gifts do not describe their behavior. */
enum State {NEW,INIT_RELEASE,INTRO,RETARGET,IDLE,WALK,RUN,BACK,RETREAT,WAIT,APPROACH,STOP,TURN,FALL,LAND,AIM,ROTATE,FIRE_FIRST,FIRE,RETURN,RECOVER,LOWER,THROW_FIRST,THROW,RAISE,AIR_NEW,AIR_IDLE,AIR_FIRE,AIR_LAND,CHUTE_NEW,CHUTE_IDLE,CHUTE_FIRE,CHUTE_LAND,CHUTE_HIT,RUNNER,CART,DELETE,DELETE_TREE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static void schedule(struct NativeEntity*e,unsigned s){e->tick=tick;enemy_schedule(&e->payload.enemy,s);}
static void init(struct NativeEntity*e,unsigned s){uint8_t*p=(uint8_t*)&e->payload.enemy.free104;for(unsigned i=0;i<sizeof(e->payload.enemy.free104);i++)p[i]=255;schedule(e,s);}
static struct NativeEntity*child(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct NativeEntity*n=enemy_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,tick);if(n)init(n,s);return n;}
static int fixed_target(struct WeaponWorld*w,struct EnemyActor*q){int i=q->free104.target;if(i<0||i>1){enemy_fault(w);return 1;}return i;}
static int16_t px(struct WeaponWorld*w,int i){return w->players[i]?m4_word((w->players[i]->actor.transform.x>>8)-w->camera_x):-1;}
static int16_t py(struct WeaponWorld*w,int i){return w->players[i]?m4_word(transform_y_up(&w->players[i]->actor.transform)+w->camera_y):-1;}
static int random_target(struct WeaponWorld*w){int a=enemy_player_live(w->players[0]),b=enemy_player_live(w->players[1]);return a&&b?(lt_random(w)&1):b?1:0;}
static int nearest(struct WeaponWorld*w,struct WeaponActor*a,unsigned box){if(box>=7)return enemy_nearest_x(w,a,0);const int16_t*b=m4_free_boxes[box];struct HitRect r={b[0],b[1],b[2],b[3]};return enemy_nearest_x(w,a,&r);}
static int in_box(struct WeaponWorld*w,struct EnemyActor*q){int i=fixed_target(w,q);const int16_t*b=m4_free_boxes[3+(q->params[1]&3)];int16_t dx=m4_word(px(w,i)-m4_sx(w,&q->actor)),dy=m4_word(py(w,i)-m4_sy(w,&q->actor));if(q->actor.transform.facing&1)dx=m4_word(-dx);if(q->actor.transform.facing&2)dy=m4_word(-dy);return dx>=b[0]&&dx<=b[1]&&dy>=b[2]&&dy<=b[3];}
static int16_t distance(struct WeaponWorld*w,struct WeaponActor*a,int i){int16_t d=m4_word(m4_sx(w,a)-px(w,i));return d<0?m4_word(-d):d;}
static int high(struct WeaponWorld*w,struct WeaponActor*a,int i){int16_t d=m4_word(py(w,i)-m4_sy(w,a));return d>24||d<-16;}
static uint8_t angle(struct WeaponWorld*w,struct WeaponActor*a,int i){return enemy_angle(m4_word(px(w,i)-m4_sx(w,a)),m4_word(py(w,i)-m4_sy(w,a)));}
static int aligned(struct WeaponWorld*w,struct EnemyActor*q){uint8_t a=angle(w,&q->actor,fixed_target(w,q));return (((uint8_t)(a+q->free104.step))&q->free104.mask)==a;}
static int edge(struct WeaponWorld*w,struct EnemyActor*q){int16_t x=m4_sx(w,&q->actor);return (x<=32&&!(q->free104.direction&1))||(x>=288&&(q->free104.direction&1));}
static int wall(struct EnemyActor*q){if(q->actor.hit.events&32){q->free104.wall_gate++;if((int8_t)q->free104.wall_gate>=30)return 1;}q->free104.wall_gate=0;return 0;}
static int should_stop(struct WeaponWorld*w,struct EnemyActor*q){if(nearest(w,&q->actor,7)<0)return 1;if(!in_box(w,q))return 0;return !high(w,&q->actor,fixed_target(w,q))||aligned(w,q);}
static int shoot(struct WeaponWorld*w,struct EnemyActor*q){if((int8_t)q->free104.turns<=5){if(q->free104.timer>0)return 0;q->free104.timer=0;if(!in_box(w,q))return 0;}if(q->params[0]&&high(w,&q->actor,fixed_target(w,q))&&!aligned(w,q))return 0;return 1;}
static int should_walk(struct WeaponWorld*w,struct EnemyActor*q){return !in_box(w,q)&&distance(w,&q->actor,fixed_target(w,q))>192;}
static int range(struct WeaponWorld*w,struct EnemyActor*q,unsigned col){if(col==0&&edge(w,q))return 0;int i=nearest(w,&q->actor,2);return i>=0&&(col==2||!high(w,&q->actor,i))&&distance(w,&q->actor,i)<m4_free_distances[q->params[2]&3][col];}
static int behind(struct WeaponWorld*w,struct EnemyActor*q){if(nearest(w,&q->actor,0)>=0)return 0;if(nearest(w,&q->actor,1)>=0)return 1;int16_t d=m4_word(m4_sx(w,&q->actor)-px(w,fixed_target(w,q)));return q->actor.transform.facing&1?d>0:d<0;}
static int turn(struct WeaponWorld*w,struct EnemyActor*q){q->free104.turn_gate++;if((int8_t)q->free104.turn_gate<30)return 0;q->free104.turn_gate=30;return behind(w,q);}
static int fall_check(struct WeaponWorld*w,struct EnemyActor*q){int r=terrain_weapon_foot_contact(w->interactions->terrain,&q->actor);if(r<0){enemy_fault(w);return -1;}if(r)q->free104.fallen=0;return !r;}
static void speed(struct WeaponWorld*w,struct EnemyActor*q,int run,int reverse){q->free104.speed=m4_rank_value(w,q,run?M4R_FS_RUN:M4R_SPEED);q->actor.vx=q->actor.transform.facing&1?q->free104.speed:m4_word(-q->free104.speed);if(reverse)q->actor.vx=m4_word(-q->actor.vx);q->free104.delays=((uint16_t)q->free104.speed>>9)&3;}
static int air_move(struct WeaponWorld*w,struct EnemyActor*q){struct WeaponActor*a=&q->actor;q->free104.air_angle-=2;unsigned d=q->free104.air_angle;a->vx=m4_word((((int32_t)boss_trig[1][d]*176)>>8)+q->free104.drift);int16_t vy=m4_word(((int32_t)boss_trig[0][d]*176)>>8)>>1;if(vy>0)vy=m4_word(-vy);a->vy=m4_word(vy+32-q->free104.descent);if(interaction_move(w,a,WMOVE_POINT)<0)return -1;if(q->free104.air_mode){struct WeaponContact c;if(!w->probe||!w->probe(w->scene,a,&c)){enemy_fault(w);return -1;}return c.solid;}int r=terrain_weapon_foot_contact(w->interactions->terrain,a);if(r<0)enemy_fault(w);return r;}
static int air_aim(struct WeaponWorld*w,struct EnemyActor*q){unsigned a=((angle(w,&q->actor,fixed_target(w,q))+8)>>4)&15;int16_t phase=q->free104.phase;int step;
 if(a>0&&a<4){if(phase==16)return 0;step=1;}
 else if(a>=4&&a<8){if(!phase)return 0;step=-1;}
 else{unsigned wanted=(a? a-8:8)*2;if(wanted==(uint16_t)phase){if(q->free104.timer>0)return 0;q->free104.timer=0;return 1;}step=wanted<(uint16_t)phase?-1:1;if(step<0?!phase:phase==16)return 0;}
 if(!(w->frame_phase&3))q->free104.phase=m4_word(phase+step);return 0;
}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned s=a->state;struct NativeEntity*n;
 switch(s){
 case NEW:case INIT_RELEASE:
  if(s==NEW&&!lt_settle(w,a))return 0;if(s==INIT_RELEASE){q->free104.mask=240;q->free104.step=8;q->free104.variant=0;q->free104.form=0;}if(!free104_prepare(w,e))return 0;q->free104.target=(int8_t)nearest(w,a,7);if(q->free104.target<0)q->free104.target=1;schedule(e,q->free104.form?INTRO:q->params[0]?WALK:IDLE);return 2;
 case INTRO:m4_start(q,M4A_FS_INTRO);break;
 case RETARGET:q->free104.target=(int8_t)random_target(w);schedule(e,IDLE);return 2;
 case IDLE:a->vx=0;q->free104.direction=a->transform.facing^1;m4_start(q,M4A_FS_IDLE);break;
 case WALK:q->free104.direction=a->transform.facing;speed(w,q,0,0);m4_start(q,M4A_FS_WALK);break;
 case RUN:case BACK:q->free104.direction=a->transform.facing^(s==BACK);if(edge(w,q)){schedule(e,s==RUN?BACK:RUN);return 2;}speed(w,q,1,s==BACK);m4_start(q,s==RUN?M4A_FS_RUN:M4A_FS_BACK);break;
 case RETREAT:q->free104.direction=a->transform.facing^1;if(edge(w,q)){schedule(e,IDLE);return 2;}q->free104.target=(int8_t)nearest(w,a,2);if(q->free104.target<0)q->free104.target=1;speed(w,q,1,1);m4_start(q,M4A_FS_BACK);break;
 case WAIT:a->vx=0;q->free104.direction=a->transform.facing&1;if(edge(w,q)){schedule(e,TURN);return 2;}if(behind(w,q)){schedule(e,APPROACH);return 2;}q->free104.target=(int8_t)nearest(w,a,2);if(q->free104.target<0)q->free104.target=1;m4_start(q,M4A_FS_WAIT);break;
 case APPROACH:q->free104.direction=a->transform.facing;speed(w,q,0,0);m4_start(q,M4A_FS_WALK);break;
 case STOP:if(!enemy_player_live(w->players[fixed_target(w,q)])){schedule(e,RETARGET);return 2;}a->vx=0;m4_start(q,M4A_FS_STOP);break;
 case TURN:q->free104.turns++;a->vx=0;m4_start(q,M4A_FS_TURN);break;
 case FALL:a->vx>>=1;m4_start(q,M4A_FS_FALL);break;
 case LAND:a->vx=0;m4_start(q,M4A_FS_LAND);break;
 case AIM:{int i=fixed_target(w,q);if(q->free104.variant&&w->players[i]&&(w->players[i]->status_flags&64)&&py(w,i)==m4_sy(w,a)){schedule(e,LOWER);return 2;}q->free104.turns=0;a->vx=0;uint8_t d=angle(w,a,i)+q->free104.step;unsigned mask=high(w,a,i)?q->free104.mask:128;q->free104.life=(d&mask)>>4;q->free104.angle=(uint8_t)q->free104.life;q->free104.low=q->free104.life>8;m4_start(q,q->free104.low?M4A_FS_AIM1:M4A_FS_AIM0);break;}
 case ROTATE:a->vx=0;q->free104.life&=15;m4_start(q,q->free104.low?M4A_FS_ROTATE1:M4A_FS_ROTATE0);q->free104.phase=0;if(!q->free104.low){if(!(a->transform.facing&1))q->free104.life=(uint8_t)(8-q->free104.life);}else{q->free104.life=(uint8_t)(q->free104.life-9);if(a->transform.facing&1)q->free104.life=(uint8_t)(6-q->free104.life);}q->free104.life=(uint8_t)(q->free104.life*2);break;
 case FIRE_FIRST:a->vx=0;q->free104.phase=m4_word(q->free104.phase-1);q->free104.repeats=(uint8_t)m4_rank_value(w,q,M4R_REPEATS);/* fall through */
 case FIRE:m4_start_entry(q,(a->transform.facing&1?m4_fs_fire1:m4_fs_fire0)[q->free104.angle&15]);if(!free104_shot(w,e,q->free104.angle&15,0))return 0;q->free104.timer=m4_rank_value(w,q,M4R_REPEAT_WAIT);a->state=FIRE;break;
 case RETURN:a->vx=0;m4_start(q,q->free104.low?M4A_FS_ROTATE1:M4A_FS_ROTATE0);if(q->free104.phase>=(q->free104.low?6:8)){q->free104.phase=m4_word((q->free104.low?12:16)-q->free104.phase);a->transform.facing^=1;}break;
 case RECOVER:a->vx=0;q->free104.timer=m4_rank_value(w,q,M4R_WAIT);m4_start(q,q->free104.low?M4A_FS_RECOVER1:M4A_FS_RECOVER0);break;
 case LOWER:m4_defense(q,M4B_FREE_WAIT);m4_start(q,M4A_FS_LOWER);break;
 case THROW_FIRST:a->vx=0;q->free104.repeats=(uint8_t)m4_rank_value(w,q,M4R_REPEATS);/* fall through */
 case THROW:q->free104.timer=m4_rank_value(w,q,M4R_REPEAT_WAIT);m4_defense(q,M4B_FREE_THROW);m4_start(q,M4A_FS_THROW);a->state=THROW;break;
 case RAISE:m4_start(q,M4A_FS_RAISE);break;
 case AIR_NEW:a->palette=56;a->hit.hp=1;interaction_priority(w,a,0x8000,24);m4_defense(q,M4B_FS_AIR);q->free104.drift=(int8_t)q->params[6]*16;q->free104.descent=m4_rank_value(w,q,M4R_FS_AIR_SPEED);q->free104.target=(int8_t)nearest(w,a,7);if(q->free104.target<0)q->free104.target=1;q->free104.phase=0;q->free104.step=8;q->free104.mask=240;if(!child(w,e,CHUTE_NEW))return 0;schedule(e,AIR_IDLE);return 2;
 case AIR_IDLE:q->free104.timer=m4_rank_value(w,q,M4R_FS_AIR_WAIT);q->free104.air_state=0;m4_start(q,M4A_FS_AIR_IDLE);break;
 case AIR_FIRE:q->free104.air_state=1;q->free104.angle=(q->free104.phase>>1)+8;if(!free104_shot(w,e,q->free104.angle&15,1))return 0;if((uint16_t)q->free104.phase>16)return enemy_fault(w);m4_start_entry(q,m4_fs_air_fire[((uint16_t)q->free104.phase>>1)&15]);break;
 case AIR_LAND:q->free104.air_state=3;m4_start(q,M4A_FS_AIR_LAND);break;
 case CHUTE_NEW:a->palette=0x18d;m4_defense(q,M4B_FS_CHUTE);a->hit.hp=m4_rank_value(w,q,M4R_FS_CHUTE_HP);/* fall through */
 case CHUTE_IDLE:m4_start(q,M4A_FS_PARACHUTE);a->state=CHUTE_IDLE;break;
 case CHUTE_FIRE:{struct EnemyActor*p=m4_parent(w,e);if(!p)return 0;if((uint16_t)p->free104.phase>16)return enemy_fault(w);m4_start_entry(q,m4_fs_chute_fire[((uint16_t)p->free104.phase>>1)&15]);break;}
 case CHUTE_LAND:case CHUTE_HIT:m4_start(q,s==CHUTE_LAND?M4A_FS_CHUTE_LAND:M4A_FS_CHUTE_HIT);m4_defense(q,65535);break;
 case RUNNER:if(!lt_settle(w,a))return 0;a->palette=14;a->transform.priority=0x8000;m4_reset(a);a->hit.hp=1;m4_start(q,M4A_FS_RUNNER);a->vx=m4_rank_value(w,q,M4R_FS_RUNNER);a->transform.facing=q->params[0];if(!(a->transform.facing&1))a->vx=m4_word(-a->vx);a->ay=-96;if((int16_t)w->camera_y>=208&&!(lt_random(w)&7)){int16_t d=a->vx>>1;a->vx=m4_word(a->vx+d);d>>=1;a->vx=m4_word(a->vx+d);}a->phase=0;n=child(w,e,CART);if(!n)return 0;n->payload.enemy.actor.vx=a->vx;n->payload.enemy.actor.vy=a->vy;break;
 case CART:if(!lt_settle(w,a))return 0;a->palette=0x144;a->transform.priority--;m4_start(q,M4A_FS_CART);m4_attack(q,M4B_FS_CART_ATTACK);hit_begin_attack(&a->hit);m4_defense(q,M4B_FS_CART);break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static int retarget(struct WeaponWorld*w,struct EnemyActor*q){if(!enemy_player_live(w->players[fixed_target(w,q)]))q->free104.target=(int8_t)random_target(w);return !w->fault;}
static int16_t attacker_vx(struct WeaponWorld*w,struct WeaponActor*a){unsigned i=a->hit.last_attacker;if(i>=ENTITY_CAPACITY){enemy_fault(w);return 0;}struct NativeEntity*n=&w->entities->nodes[i];if(n->kind==ENTITY_PLAYER)return n->payload.player.vx;if(n->kind==ENTITY_PLAYER_ARM)return 0;struct WeaponActor*b=entity_weapon_actor(n);if(!b){enemy_fault(w);return 0;}return b->vx;}
static void air_tail(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(!retarget(w,q))return;q->free104.timer=m4_word(q->free104.timer-1);if(a->hit.flags&8){struct NativeEntity*n=free104_hat(w,e,24);if(!n)return;n->payload.enemy.actor.vx=attacker_vx(w,a)>>4;if(w->fault||!human_death_hit_tail(w,e))return;q->free104.air_state=2;}
 int16_t x=m4_sx(w,a);if((int8_t)q->params[6]<0?x<=-32:x>=352)schedule(e,DELETE);if(interaction_outside(w,a,m4_bounds[3],0))schedule(e,DELETE);
}
static void chute(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(s==CHUTE_LAND||s==CHUTE_HIT){if(interaction_move(w,a,WMOVE_SYNC)<0)return;if((w->frame_phase&1)&&m4_animate(w,e,1))schedule(e,DELETE_TREE);return;}
 if(entity_parent_deleting(w->entities,e)){entity_delete_children(w->entities,e);entity_delete(w->entities,e);return;}struct EnemyActor*p=m4_parent(w,e);if(!p)return;struct WeaponActor*b=&p->actor;a->transform.x=b->transform.x;a->transform.y=b->transform.y;a->transform.facing=b->transform.facing;a->transform.priority=b->transform.priority-1;
 if(s==CHUTE_IDLE)q->free104.phase=p->free104.phase;m4_animate(w,e,1);if(s==CHUTE_IDLE&&p->free104.air_state==1)schedule(e,CHUTE_FIRE);if(s==CHUTE_FIRE&&!p->free104.air_state)schedule(e,CHUTE_IDLE);
 if(p->free104.air_state==3){schedule(e,CHUTE_LAND);return;}if(p->free104.air_state==2){schedule(e,CHUTE_HIT);return;}a->hit.flags&=247;if(a->hit.flags&1){struct NativeEntity*n=entity_get(w->entities,e->parent);if(!n){enemy_fault(w);return;}b->hit.last_attacker=n->index;b->hit.last_kind=a->hit.last_kind;b->hit.impact_x=(int16_t)(b->transform.x>>8);b->hit.impact_y=transform_y_up(&b->transform);b->hit.flags|=8;schedule(e,CHUTE_HIT);}if(entity_parent_deleting(w->entities,e))schedule(e,DELETE_TREE);
}
static void runner(struct WeaponWorld*w,struct NativeEntity*e,int cart){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(cart){struct EnemyActor*p=m4_parent(w,e);if(!p)return;if(p->actor.phase==255){if(interaction_move(w,a,WMOVE_GROUND)<0)return;}else{int16_t x=m4_word((p->actor.transform.x>>8)+(a->transform.facing&1?32:-32));transform_integer_offset(&a->transform,m4_word(x-(a->transform.x>>8)),0);int16_t vx=a->vx;a->vx=0;if(!lt_settle(w,a))return;a->vx=vx;}m4_animate(w,e,1);if(!enemy_attack(w,a))return;a->hit.hp=32767;a->hit.flags&=247;if(interaction_outside(w,a,m4_bounds[5],0))entity_delete(w->entities,e);return;}
 if(interaction_move(w,a,WMOVE_GROUND)<0)return;m4_animate(w,e,1);if(a->hit.flags&8){a->phase=255;if(!human_death_hit_tail(w,e))return;}if(interaction_outside(w,a,m4_bounds[4],0)){a->phase=255;entity_delete(w->entities,e);}
}
static void frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(s>=CHUTE_IDLE&&s<=CHUTE_HIT){chute(w,e,s);return;}if(s==RUNNER||s==CART){runner(w,e,s==CART);return;}
 if(s==AIR_IDLE||s==AIR_FIRE||s==AIR_LAND){if(s==AIR_LAND){if(interaction_move(w,a,WMOVE_SYNC)<0)return;}else{int r=air_move(w,q);if(r<0)return;if(r)schedule(e,AIR_LAND);}int done=m4_animate(w,e,1);if(s==AIR_IDLE){if(air_aim(w,q)){q->free104.air_state=1;schedule(e,AIR_FIRE);}}else if(done)schedule(e,s==AIR_FIRE?AIR_IDLE:INIT_RELEASE);air_tail(w,e);return;}
 if(s==FALL){int r=interaction_move(w,a,WMOVE_ITEM);if(r<0)return;if(r)schedule(e,LAND);}else if(!free104_move(w,a,s==WALK||s==RUN||s==BACK||s==RETREAT||s==APPROACH))return;
 int done=m4_animate(w,e,1);if(w->fault)return;
 switch(s){
 case INTRO:if(done)schedule(e,IDLE);break;
 case IDLE:
  if(q->params[0]){if(enemy_player_live(w->players[fixed_target(w,q)])){
   if(!should_stop(w,q)){unsigned choice=1;if(q->free104.mask!=192&&in_box(w,q)&&(lt_random(w)&1))choice=2;schedule(e,choice==1?RUN:BACK);}
   if(should_walk(w,q))schedule(e,WALK);if(turn(w,q))schedule(e,TURN);if(range(w,q,0))schedule(e,RETREAT);if(range(w,q,1))schedule(e,WAIT);
  }else q->free104.target=(int8_t)random_target(w);}
  if(m4_sx(w,a)>=24&&m4_sx(w,a)<=296&&shoot(w,q))schedule(e,AIM);break;
 case WALK:if(wall(q))schedule(e,STOP);if(should_stop(w,q))schedule(e,STOP);if(turn(w,q))schedule(e,STOP);if(range(w,q,0))schedule(e,STOP);if(range(w,q,1))schedule(e,STOP);break;
 case RUN:case BACK:if(wall(q))schedule(e,s==RUN?BACK:RUN);if(should_stop(w,q))schedule(e,IDLE);if(turn(w,q))schedule(e,TURN);if(range(w,q,0))schedule(e,RETREAT);if(edge(w,q))schedule(e,s==RUN?BACK:RUN);if(range(w,q,1))schedule(e,WAIT);break;
 case RETREAT:if(wall(q))schedule(e,AIM);if(shoot(w,q))schedule(e,IDLE);if(turn(w,q))schedule(e,TURN);if(!range(w,q,0))schedule(e,IDLE);if(edge(w,q))schedule(e,IDLE);if(range(w,q,1))schedule(e,WAIT);break;
 case WAIT:if(done)schedule(e,APPROACH);break;
 case APPROACH:if(!range(w,q,2))schedule(e,STOP);if(edge(w,q))schedule(e,STOP);break;
 case STOP:case TURN:case LAND:if(done)schedule(e,IDLE);break;
 case FALL:break;
 case AIM:if(done)schedule(e,ROTATE);break;
 case ROTATE:if(((uint16_t)q->free104.life&31)==(uint16_t)q->free104.phase)schedule(e,FIRE_FIRST);q->free104.phase=m4_word(q->free104.phase+1);break;
 case FIRE:if((done||(int8_t)q->free104.repeats>1)&&q->free104.timer<=0){schedule(e,FIRE);q->free104.repeats--;if((int8_t)q->free104.repeats<=0)schedule(e,RETURN);}break;
 case RETURN:if(!q->free104.phase)schedule(e,RECOVER);if(!(w->frame_phase&1))q->free104.phase=m4_word(q->free104.phase-1);break;
 case RECOVER:if(done)schedule(e,RETARGET);break;
 case LOWER:if(done)schedule(e,THROW_FIRST);break;
 case THROW:if(done&&q->free104.timer<=0){schedule(e,THROW);q->free104.repeats--;if((int8_t)q->free104.repeats<=0){schedule(e,RAISE);q->free104.timer=m4_rank_value(w,q,M4R_WAIT);}}break;
 case RAISE:if(done){m4_defense(q,M4B_FREE);schedule(e,RETARGET);}break;
 default:enemy_fault(w);return;
 }
 if(s!=FALL){int r=fall_check(w,q);if(r<0)return;if(r)schedule(e,FALL);}free104_tail(w,e,s==THROW||s==RAISE);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;for(unsigned i=0;i<8;i++){if(q->actor.state==DELETE||q->actor.state==DELETE_TREE){if(q->actor.state==DELETE_TREE)entity_delete_children(p,e);entity_delete(p,e);return;}if(q->entry){q->entry=0;int r=enter(w,e);if(!r||w->fault)return;if(r==2)continue;}frame(w,e,q->actor.state);return;}enemy_fault(w);}
int free96_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){if(!((id>=96&&id<=103)||id==106||id==107||id==110))return enemy_fault(w);struct NativeEntity*n=enemy_allocate(w,parent,order,t,tick);if(!n)return 0;init(n,id==110?RUNNER:id>=106?AIR_NEW:NEW);struct EnemyActor*q=&n->payload.enemy;if(params)for(unsigned i=0;i<8;i++)q->params[i]=params[i];if(id<=103){unsigned k=(id-96)%3;if(id>=102)k=2;q->free104.mask=k==0?192:k==1?224:240;q->free104.step=k==0?32:k==1?16:8;q->free104.variant=id>=102;q->free104.form=(id>=99&&id<=101)||id==103;}else q->free104.air_mode=id==107;return 1;}
