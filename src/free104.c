#include "m4_internal.h"
/* Templates 104/105 ($0483E2/$0483D2). The source file's POW labels do
 * not make these item gifts: $048BA0 is an attacking projectile. */
enum State {NEW,FALL,LAND,WALK,JUMP,WAIT,THROW,THROW_REPEAT,LEAVE,STOP,STAND,FIRE,REPEAT,IDLE,EXIT,HAT,BULLET,ANGLE_BULLET,BLAST,FLASH,DELETE_TREE,DELETE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static uint16_t random_word(struct WeaponWorld*w){if(!w->rng){enemy_fault(w);return 0;}return source_random_word(w->rng);}
static void schedule(struct NativeEntity*e,unsigned state){e->tick=tick;enemy_schedule(&e->payload.enemy,state);}
static struct NativeEntity*child(struct WeaponWorld*w,struct NativeEntity*e,unsigned s,int dx,int dy){struct NativeEntity*n=enemy_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,tick);if(n){n->payload.enemy.free104=(__typeof__(n->payload.enemy.free104)){.timer=-1,.speed=-1,.life=-1,.delays=65535,.repeats=255,.can_fall=255,.fallen=255,.angle=255,.target=-1};schedule(n,s);transform_integer_offset(&n->payload.enemy.actor.transform,(int16_t)dx,(int16_t)dy);}return n;}
static int foot(struct WeaponWorld*w,struct WeaponActor*a){int r=terrain_weapon_foot_contact(w->interactions->terrain,a);if(r<0)enemy_fault(w);return r;}
static int fall_check(struct WeaponWorld*w,struct EnemyActor*q){int r=foot(w,&q->actor);if(r<0)return -1;if(r)q->free104.fallen=0;return !r;}
int free104_move(struct WeaponWorld*w,struct WeaponActor*a,int walk){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;int ground=foot(w,a);if(ground<0)return 0;if(!ground)return interaction_move(w,a,WMOVE_ITEM)>=0;return !walk||interaction_move(w,a,WMOVE_GROUND)>=0;}
static int settle(struct WeaponWorld*w,struct WeaponActor*a){int16_t y=m4_sy(w,a);m4_screen(w,a,m4_sx(w,a),y&1023);for(unsigned i=0;i<2048;i++){if(m4_sy(w,a)<0){m4_screen(w,a,160,384);return 1;}int r=interaction_move(w,a,WMOVE_ITEM);if(r<0)return 0;if(r)return 1;}return enemy_fault(w);}
static int near(struct WeaponWorld*w,struct WeaponActor*a){int slot=enemy_nearest_x(w,a,0);if(slot<0)return 0;struct PlayerTransform*p=&w->players[slot]->actor.transform;int16_t dy=m4_word(transform_y_up(p)-transform_y_up(&a->transform)),dx=m4_word((p->x>>8)-(a->transform.x>>8));if(dx<0)dx=m4_word(-dx);return dy>=-16&&dy<=24&&dx<=48;}
static int retarget(struct WeaponWorld*w,int target){if(target>=0&&target<2&&enemy_player_live(w->players[target]))return target;int live=(enemy_player_live(w->players[0])?1:0)|(enemy_player_live(w->players[1])?2:0);return live==3?(random_word(w)&1):live==2?1:0;}
int free104_callback(struct WeaponWorld*w,struct NativeEntity*e,unsigned n){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*v;
 if(n==M4C_283CA){hit_begin_attack(&a->hit);return 1;}
 if(n==M4C_4951E){q->free104.timer=m4_rank_value(w,q,M4R_REPEAT_WAIT);return !w->fault;}
 if(n==M4C_49472){int dx=a->transform.facing&1?32:-32;return child(w,e,FLASH,dx,14)&&child(w,e,BULLET,dx,14);}
 if(n==M4C_493FA){q->free104.angle=a->transform.facing&1?0:8;const int16_t*off=m4_free_offsets[q->free104.angle];
  /* $5C is reused as the offset record before either child is allocated. */
  q->free104.delays=65535;
  if(!child(w,e,FLASH,off[0],off[1]))return 0;v=child(w,e,ANGLE_BULLET,off[0],off[1]);if(!v)return 0;v->payload.enemy.free104.angle=q->free104.angle;return 1;
 }
 return enemy_fault(w);
}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 switch(a->state){
 case NEW:
  if(!settle(w,a))return 0;m4_reset(a);if(m4_sx(w,a)<=160)a->transform.facing|=1;a->palette=0x38;a->hit.hp=1;m4_defense(q,M4B_FREE);q->free104.delays=3;q->free104.timer=m4_rank_value(w,q,M4R_WAIT);interaction_priority(w,a,0x8000,24);
  if(!q->params[0]){schedule(e,q->params[3]?WAIT:STAND);return 2;}if(q->params[4])q->free104.timer=q->params[4];
  {int r=fall_check(w,q);if(r<0)return 0;schedule(e,r?FALL:WALK);return 2;}
 case FALL:a->vx>>=1;m4_start(q,M4A_FREE_FALL);break;
 case LAND:a->vx=0;m4_start(q,M4A_FREE_LAND);break;
 case WALK:q->free104.speed=m4_rank_value(w,q,M4R_SPEED);a->vx=a->transform.facing&1?q->free104.speed:m4_word(-q->free104.speed);q->free104.delays=((uint16_t)q->free104.speed>>9)&3;m4_start(q,M4A_FREE_WALK);break;
 case JUMP:if(!q->params[3]){schedule(e,STOP);return 2;}a->vx=a->transform.facing&1?819:-819;a->vy=3275;a->ay=-1310;a->ax=0;m4_start(q,M4A_FREE_JUMP);break;
 case WAIT:a->vx=0;m4_defense(q,M4B_FREE_WAIT);m4_start(q,M4A_FREE_WAIT);break;
 case THROW:a->vx=0;q->free104.repeats=(uint8_t)m4_rank_value(w,q,M4R_REPEATS);/* FALLTHROUGH */
 case THROW_REPEAT:q->free104.timer=m4_rank_value(w,q,M4R_REPEAT_WAIT);m4_defense(q,M4B_FREE_THROW);m4_start(q,M4A_FREE_THROW);a->state=THROW;break;
 case LEAVE:a->vx=0;m4_defense(q,M4B_FREE);m4_start(q,M4A_FREE_LEAVE);break;
 case STOP:a->vx=0;m4_start(q,M4A_FREE_STOP);break;
 case STAND:a->vx=0;m4_defense(q,M4B_FREE_STAND);m4_start(q,M4A_FREE_STAND);break;
 case FIRE:a->vx=0;m4_defense(q,M4B_FREE);q->free104.repeats=(uint8_t)m4_rank_value(w,q,M4R_REPEATS);m4_start(q,M4A_FREE_FIRE);break;
 case REPEAT:m4_start(q,M4A_FREE_REPEAT);a->state=FIRE;break;
 case IDLE:a->vx=0;m4_defense(q,M4B_FREE_STAND);m4_start(q,M4A_FREE_IDLE);break;
 case EXIT:a->vx=0;m4_defense(q,M4B_FREE);m4_start(q,M4A_FREE_EXIT);break;
 case HAT:a->vx=a->transform.facing&1?68:-68;a->vy=810;a->ay=-54;a->ax=0;a->palette=0x38;m4_start(q,M4A_FREE_HAT);break;
 case FLASH:a->palette=4;m4_start(q,M4A_FREE_FLASH);break;
 case BULLET:case ANGLE_BULLET:{
  int16_t speed=m4_rank_value(w,q,M4R_BULLET);if(a->state==BULLET){a->vx=a->transform.facing&1?speed:m4_word(-speed);a->vy=0;}else{unsigned angle=(q->free104.angle&15)*16;a->vx=m4_word(((int32_t)boss_trig[1][angle]*speed)>>8);a->vy=m4_word(((int32_t)boss_trig[0][angle]*speed)>>8);}
  q->free104.life=150;a->palette=0x17b;interaction_priority(w,a,0xd000,20);a->hit.type_flags|=16;if(!interaction_sound(w,0x1064))return 0;m4_start(q,M4A_FREE_BULLET);break;
 }
 case BLAST:interaction_priority(w,a,0x2000,20);boss_fx_transform(w,e,BFX_FIRE);return 0;
 default:return enemy_fault(w);
 }return !w->fault;
}
int free104_tail(struct WeaponWorld*w,struct NativeEntity*e,int alternate){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;q->free104.target=(int8_t)retarget(w,q->free104.target);q->free104.timer=m4_word(q->free104.timer-1);
 if(a->hit.flags&8){if(!child(w,e,HAT,0,alternate?16:24))return 0;if(!(alternate?human_death_hit_tail_alternate(w,e):human_death_hit_tail(w,e)))return 0;}
 if(interaction_outside(w,a,m4_bounds[2],0))schedule(e,DELETE);return !w->fault;
}
static void escape(struct WeaponWorld*w,struct NativeEntity*e,int alternate){
 /* $058FE2 runs the existing escape controller before this frame's common
  * tail. Keep the word timer's high byte alias through that one final tail. */
 struct EnemyActor*q=&e->payload.enemy;__typeof__(q->free104) saved=q->free104;
 uint8_t flags=(uint8_t)(((uint16_t)saved.timer>>8)&239);infantry_force_escape(e,flags);q->actor.display_flags|=2;q->params[0]=1;
 int ground=terrain_weapon_ground_test(w->interactions->terrain,&q->actor);if(ground<0){enemy_fault(w);return;}q->grounded=ground?255:0;
 infantry_escape(w,e,INFANTRY_EXIT_FACE);if(w->fault||!e->active||e->deleting)return;
 if(!child(w,e,HAT,0,24))return;int airborne=foot(w,&q->actor);if(airborne<0)return;
 if(!airborne){q->free104=saved;schedule(e,FALL);free104_tail(w,e,alternate);return;}
 int target=retarget(w,q->infantry.target);q->infantry.target=(int8_t)target;uint16_t timer=((uint16_t)q->infantry.flags<<8)|((uint16_t)saved.timer&255);timer--;q->infantry.flags=(uint8_t)(timer>>8);
 if(q->actor.hit.flags&8){if(!child(w,e,HAT,0,alternate?16:24))return;if(!(alternate?human_death_hit_tail_alternate(w,e):human_death_hit_tail(w,e)))return;}
 if(interaction_outside(w,&q->actor,m4_bounds[2],0))schedule(e,DELETE);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 for(unsigned budget=0;budget<8;budget++){
  if(a->state==DELETE||a->state==DELETE_TREE){if(a->state==DELETE_TREE)entity_delete_children(p,e);entity_delete(p,e);return;}
  if(q->entry){q->entry=0;int r=enter(w,e);if(!r||w->fault)return;if(r==2)continue;}
  unsigned s=a->state;int r=0,done=0;
  if(s==HAT||s==BULLET||s==ANGLE_BULLET||s==FLASH){
   r=interaction_move(w,a,s==HAT?WMOVE_BODY:s==FLASH?WMOVE_SYNC:WMOVE_POINT);if(r<0)return;if(r&&s!=FLASH)schedule(e,s==HAT?DELETE_TREE:BLAST);done=m4_animate(w,e,1);
   if(s==FLASH&&done)schedule(e,DELETE_TREE);
   if(s==BULLET||s==ANGLE_BULLET){if(--q->free104.life<=0)schedule(e,BLAST);if(!enemy_attack(w,a))return;if(a->hit.flags&2)schedule(e,BLAST);}
   if(interaction_outside(w,a,boss_fx_bounds[8],1))schedule(e,DELETE_TREE);return;
  }
  if(s==FALL){r=interaction_move(w,a,WMOVE_ITEM);if(r<0)return;if(r)schedule(e,LAND);}
  else if(s==JUMP){r=interaction_move(w,a,WMOVE_BODY);if(r<0)return;if(r)a->vx=0;}
  else if(!free104_move(w,a,s==WALK))return;
  done=m4_animate(w,e,1);if(w->fault)return;
  switch(s){
  case FALL:break;
  case LAND:if(done)schedule(e,WALK);break;
  /* $0484D6/$0484E2: finish walking only after crossing the target.
   * Facing zero moves left (target > current); facing one moves right
   * (target <= current). Reversing these tests strands M3's grenadier
   * outside the screen and prevents the enemy-clear gate from opening. */
  case WALK:{int16_t x=m4_word(q->params[0]*16);if(a->transform.facing&1?x<=m4_sx(w,a):x>m4_sx(w,a))schedule(e,JUMP);break;}
  case JUMP:if(done&&!a->vx)schedule(e,WAIT);break;
  case WAIT:if(near(w,a))schedule(e,LEAVE);if(m4_sx(w,a)>=24&&m4_sx(w,a)<=296&&q->free104.timer<=0)schedule(e,THROW);break;
  case THROW:if(done&&q->free104.timer<=0){schedule(e,THROW_REPEAT);q->free104.repeats--;if((int8_t)q->free104.repeats<=0){schedule(e,WAIT);q->free104.timer=m4_rank_value(w,q,M4R_WAIT);}}break;
  case LEAVE:case EXIT:if(done){escape(w,e,s==LEAVE);return;}break;
  case STOP:if(done)schedule(e,STAND);break;
  case STAND:if(near(w,a))schedule(e,EXIT);if(q->free104.timer<=0)schedule(e,FIRE);break;
  case FIRE:if(done&&q->free104.timer<=0){schedule(e,REPEAT);q->free104.repeats--;if((int8_t)q->free104.repeats<=0){schedule(e,IDLE);q->free104.timer=m4_rank_value(w,q,M4R_WAIT);}}break;
  case IDLE:if(done)schedule(e,STAND);break;
  default:enemy_fault(w);return;
  }
  if(s!=FALL&&(s!=JUMP||q->free104.can_fall)){r=fall_check(w,q);if(r<0)return;if(r)schedule(e,FALL);}
  free104_tail(w,e,s==WAIT||s==THROW||s==LEAVE);return;
 }enemy_fault(w);
}
struct NativeEntity*free104_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){
 if(id!=104&&id!=105){enemy_fault(w);return 0;}struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;struct EnemyActor*q=&e->payload.enemy;q->free104=(__typeof__(q->free104)){.timer=-1,.speed=-1,.life=-1,.delays=65535,.repeats=255,.can_fall=id==105,.fallen=255,.angle=255,.target=-1};if(params)for(unsigned i=0;i<8;i++)q->params[i]=params[i];if(id==105)q->params[3]=1;schedule(e,NEW);return e;
}

int free104_prepare(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;m4_reset(a);if(m4_sx(w,a)<=160)a->transform.facing|=1;a->palette=0x38;a->hit.hp=1;m4_defense(q,M4B_FREE);q->free104.delays=3;q->free104.timer=m4_rank_value(w,q,M4R_WAIT);interaction_priority(w,a,0x8000,24);return !w->fault;}
int free104_shot(struct WeaponWorld*w,struct NativeEntity*e,unsigned angle,int air){if(angle>=16)return enemy_fault(w);struct EnemyActor*q=&e->payload.enemy;const int16_t*off=(air?m4_free_air_offsets:m4_free_offsets)[angle];q->free104.delays=65535;if(!child(w,e,FLASH,off[0],off[1]))return 0;struct NativeEntity*n=child(w,e,ANGLE_BULLET,off[0],off[1]);if(!n)return 0;n->payload.enemy.free104.angle=(uint8_t)angle;return 1;}
struct NativeEntity*free104_hat(struct WeaponWorld*w,struct NativeEntity*e,int dy){return child(w,e,HAT,0,dy);}

/* $049472/$0494C8: shared horizontal shot without changing owner storage. */
int free104_level_shot(struct WeaponWorld*w,struct NativeEntity*e,int high){int dx=high?30:32;if(!(e->payload.enemy.actor.transform.facing&1))dx=-dx;return child(w,e,FLASH,dx,high?34:14)&&child(w,e,BULLET,dx,high?34:14);}
