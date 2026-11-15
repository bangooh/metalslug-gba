#include "enemies.h"
#include "slug_surface.h"
/* $056ACC..$058F1C. Labels in the disassembly call foot-player tackles
 * "SlugApproach" and throws "surrender"; names here follow actual behavior. */
enum State {NEW,WALK,AIR_DISPATCH,GRAB_APPROACH,FOOT_APPROACH,GRAB_LEAP,
 GRAB_TOUCH,GRAB_ALIGN,GRAB_CANNON,GRAB_RIDER,GRAB_TREAD,GRAB_CANNON_RESUME,
 GRAB_RIDER_RESUME,GRAB_TREAD_RESUME,GRAB_HIT,GRAB_SHAKE,GRAB_THROWN,GRAB_RELEASE,
 FOOT_LEAP,FOOT_HIT,FOOT_LAND,STOP,STOP_BACK,IDLE,LOOK,FALL,LAND,STEP_FRONT,STEP_BACK,
 MELEE,BRAKE,CROUCH_START,CROUCH_IDLE,CROUCH_FRONT,CROUCH_BACK,LOWER,JUMP,THROW,
 CROUCH_THROW,LEAP,BOUNCE,CLIMB_UP_START,CLIMB_UP,CLIMB_UP_END,CLIMB_DISMOUNT,
 CLIMB_DOWN_START,CLIMB_DOWN,CLIMB_DOWN_END,CLIMB_THROW,TAUNT,TAUNT_REPEAT,TAUNT_END,
 ESCAPE_FACE,ESCAPE_LOWER,JUMP_IN,GRAB_JUMP_IN};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static int16_t word(int n){return (int16_t)(uint16_t)n;}
static int16_t sx(struct WeaponWorld*w,const struct WeaponActor*a){return word((a->transform.x>>8)-w->camera_x);}
static int16_t sy(struct WeaponWorld*w,const struct WeaponActor*a){return word(transform_y_up(&a->transform)+w->camera_y);}
static uint16_t random_word(struct WeaponWorld*w){if(!w->rng){enemy_fault(w);return 0;}return source_random_word(w->rng);}
static int chance(struct WeaponWorld*w,uint16_t threshold){uint16_t r=random_word(w);return (r&255)<threshold&&(r>>8)<threshold;}
static uint16_t maximum(uint16_t x,uint16_t y){return x>y?x:y;}
static uint16_t square(uint16_t x){return (uint16_t)(((uint32_t)x*x)>>8);}
static uint16_t magnitude(int16_t x){return x<0?(uint16_t)word(-x):(uint16_t)x;}
static int16_t damp(int16_t v,unsigned shift,int minimum){int16_t d=v>>shift;if(minimum&&v&&!d)d=1;return word(v-d);}
static void resume(struct EnemyActor*q,unsigned s){q->actor.state=(uint8_t)s;q->entry=0;}
static int16_t faced(struct EnemyActor*q,int16_t v){return q->actor.transform.facing&1?word(-v):v;}
static void speed(struct EnemyActor*q,int16_t v){q->scale=(uint16_t)v;q->actor.vx=q->actor.transform.facing&1?v:word(-v);}
static void position(struct WeaponActor*a,int16_t x,int16_t y){transform_integer_offset(&a->transform,word(x-(a->transform.x>>8)),word(y-transform_y_up(&a->transform)));}
static struct PlayerBodyState*target(struct WeaponWorld*w,struct EnemyActor*q){int t=q->infantry.target;if(t<0||t>1)return 0;return w->players[t];}
static int physics(struct WeaponWorld*w,struct EnemyActor*q){
 if(!enemy_ground_status(w,q,0))return 0;
 int r=enemy_swapped_move(w,q,q->grounded?WMOVE_GROUND:WMOVE_ITEM);if(r<0)return 0;
 if(!q->grounded)q->grounded=r?255:0;return 1;
}
/* Full source Think. Six WORD weights, byte flags, and call order remain
 * distinct. The low and high bytes of source $90 each contribute five bits. */
int soldier_think(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(q->infantry.cooldown)q->infantry.cooldown--;if(q->infantry.lifetime)q->infantry.lifetime--;
 q->infantry.flags&=(uint8_t)~96u;
 if(a->hit.events&32){uint8_t shape;int16_t x=word((a->transform.x>>8)+((a->transform.facing&1)?3:-3));
  if(!w->point_shape||!w->point_shape(w->scene,a,x,word(transform_y_up(&a->transform)+24),&shape))return enemy_fault(w);
  q->infantry.flags|=shape?32:64;
 }
 q->infantry.target=(int8_t)enemy_nearest_player(w,a,&q->infantry.distance);q->infantry.missing=q->infantry.target<0;
 struct PlayerBodyState*p=target(w,q);
 if(!p){q->infantry.mood|=1;q->infantry.flags|=1;
  q->infantry.probability=q->infantry.retreat=q->infantry.advance=q->infantry.fear=q->infantry.leap=q->infantry.crouch=32;return 1;}
 q->infantry.mood&=254;
 int left=word((a->transform.x>>8)-(p->actor.transform.x>>8))<0;
 q->infantry.flags=(uint8_t)((q->infantry.flags&252)|(left?2:0)|(left==!!(a->transform.facing&1)?1:0));
 q->infantry.advance=q->infantry.fear=q->infantry.leap=q->infantry.crouch=0;
 int behind;if(enemy_near_death(w,a,&behind)){
  if(behind){q->infantry.advance=1024;q->infantry.mood=(q->infantry.mood|4)&247;}
  else{uint16_t r=random_word(w);if((int8_t)r>=0){q->infantry.fear=1024;q->infantry.mood=(q->infantry.mood|8)&251;}
   else{q->infantry.advance=r&1023;q->infantry.fear=random_word(w)&1023;q->infantry.leap=q->infantry.crouch=128;}}
  q->infantry.mood|=2;
 }else q->infantry.mood&=241;
 unsigned mode=q->params[0];
 if(mode==2)a->hit.flags=(uint8_t)((a->hit.flags&207)|((int8_t)((uint16_t)q->infantry.origin>>8)<0?32:16));
 else{int branch=mode==1||(mode>2&&(random_word(w)&1));a->hit.flags=(uint8_t)((a->hit.flags&207)|(branch?48:0));}
 unsigned bias=(q->infantry.radius&31)+((q->infantry.radius>>8)&31)+16;
 int16_t d=word(q->infantry.distance-bias);if(d<0)d=0;
 uint16_t close=(uint16_t)(d*4);q->infantry.probability=(uint16_t)(256-(close>=256?256:close));
 uint32_t far=(uint32_t)(uint16_t)d<<(q->params[4]&7);far<<=1;
 q->infantry.retreat=(uint16_t)(far>=256?256:far);
 int16_t x=sx(w,a);if(x<48||x>=288){q->infantry.retreat=256;q->infantry.probability=q->infantry.advance=q->infantry.fear=0;}
 return !w->fault;
}
static int rank(struct WeaponWorld*w,struct EnemyActor*q,unsigned table){
 struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid){enemy_fault(w);return 0;}
 unsigned level=interaction_rank(w,&q->actor,runtime_owner_infantry[table<2?table:4]),flags=q->source_flags&3;
 if(table<2)return infantry_rank[table][level][flags][(r->player_count_flag&2)>>1];
 unsigned row=infantry_speed_choices[2][level][flags][(uint8_t)(r->player_count_flag-1)&1];
 return infantry_speeds[2][row][(random_word(w)&14)>>1];
}
static int pick_anchor(struct WeaponWorld*w,struct EnemyActor*q){
 if(!(q->infantry.abilities&7)||!(q->infantry.flags&1)||!target(w,q))return -1;
 unsigned bits=(~enemy_anchor_mask(w,q))&q->infantry.abilities&7,values[3],count=0;
 for(unsigned i=0;i<3;i++)if(bits&(1u<<i))values[count++]=i;
 if(!count)return -1;if(!w->rng){enemy_fault(w);return -1;}
 return (int)values[count-1-source_random(w->rng,(uint16_t)count)];
}
static int foot_target(struct WeaponWorld*w,struct EnemyActor*q){
 if(!(q->infantry.abilities&7)||!(q->infantry.flags&1)||!target(w,q))return 0;
 struct SlugSharedState*s=w->enemies->slug;int p=q->infantry.target;
 if(s&&s->present&&s->rider_heartbeat&&slug_ridden_by(s,(unsigned)p)){q->actor.owner=w->players[p]->actor.owner;return 0;}
 return 1; /* $056F00 rejects a mounted player, despite the source label. */
}
static int throw_gate(struct WeaponWorld*w,struct EnemyActor*q){
 if(!(q->infantry.abilities&8)||sx(w,&q->actor)<48||sx(w,&q->actor)>=288||q->infantry.cooldown||!(q->infantry.flags&1)||enemy_zone_contains(w,&q->actor))return 0;
 q->infantry.cooldown=(q->infantry.cooldown&0xff00)|q->params[2];return 1;
}
static void edge(struct WeaponWorld*w,struct EnemyActor*q){
 if((!(q->actor.transform.facing&1))!=(q->actor.vx<0))return;
 int ascending;if(!climb_target_crossing(&w->interactions->climb,&q->actor,&q->infantry.key,&ascending))return;
 if((q->actor.hit.flags&16)&&ascending)enemy_schedule(q,CLIMB_UP_START);
 else if((q->actor.hit.flags&32)&&!ascending)enemy_schedule(q,CLIMB_DOWN_START);
}
static int melee(struct WeaponWorld*w,struct EnemyActor*q){
 int16_t x=sx(w,&q->actor);if(x<32||x>=768)return 0;
 enemy_attack_binding(q,ENB_MELEE_PROBE);hit_begin_attack(&q->actor.hit);hit_begin_attack(&q->actor.hit);
 if(!enemy_attack(w,&q->actor))return 0;return !!(q->actor.hit.flags&2);
}
static void trajectory(struct WeaponWorld*w,struct EnemyActor*q){
 struct WeaponActor*a=&q->actor;struct PlayerBodyState*p=target(w,q);
 unsigned low=a->transform.facing&1?17:4,high=a->transform.facing&1?18:5,down=a->transform.facing&1?19:16;
 if((uint16_t)sy(w,a)>=384)low=down;
 else if(p){int16_t dy=word(transform_y_up(&p->actor.transform)-transform_y_up(&a->transform));if(dy>96)low=high;else if(dy<-64)low=down;}
 a->variant=(uint8_t)low;
}
static int common_init(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 int r=terrain_weapon_ground_test(w->interactions->terrain,a);if(r<0)return enemy_fault(w);q->grounded=r?255:0;a->hit.type_flags|=8;
 unsigned n=(unsigned)rank(w,q,0);if(!q->params[1])n=65535;q->infantry.cooldown=(uint16_t)((n*q->params[1])>>8);
 n=(unsigned)rank(w,q,1)*(q->params[2]?q->params[2]:30)>>8;q->params[2]=(uint8_t)((uint16_t)n>=256?255:n);
 q->infantry.lifetime=(uint16_t)((q->params[3]?q->params[3]:10)<<6);a->palette=14;a->ax=a->ay=0;interaction_priority(w,a,0x8000,24);
 if(!pow_spawn_surface(w,entity_ref(e),&a->transform))return 0;q->infantry.advance=q->infantry.fear=0;
 if(!soldier_think(w,e))return 0;q->infantry.abilities=0;q->infantry.origin=(int16_t)random_word(w);q->infantry.radius=random_word(w);return !w->fault;
}
static void gun_mode(struct WeaponWorld*w,struct EnemyActor*q){
 if((int16_t)random_word(w)<0)q->infantry.flags&=239;else q->infantry.flags|=16;
 q->infantry.abilities|=8;q->infantry.weapon=0;
}
static int initialize(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;unsigned id=q->infantry.engaged;
 if(id==4)q->actor.transform.facing&=254;if(id==5)q->actor.transform.facing|=1;
 if(!common_init(w,e))return 0;
 if(id<=5||id==7||(id>=12&&id<=15)||(id>=17&&id<=19))gun_mode(w,q);
 if(id==6||id==7||(id>=17&&id<=19)){q->infantry.flags|=16;q->infantry.abilities|=16;}
 if(id>=8&&id<=19){static const uint8_t masks[12]={1,2,4,7,1,2,4,7,1,2,4,7};q->infantry.abilities|=masks[id-8];}
 if(id>=20&&id<=22){q->infantry.flags|=16;q->infantry.abilities|=8;q->infantry.weapon=(uint8_t)(id-19);}
 if(id==2||id==3){int16_t x;
  if(!climb_target_nearest(&w->interactions->climb,&q->actor,&q->infantry.key,&x)){entity_delete_children(w->entities,e);entity_delete(w->entities,e);return 0;}
  transform_integer_offset(&q->actor.transform,word(x-(q->actor.transform.x>>8)),0);enemy_schedule(q,id==2?CLIMB_UP_START:CLIMB_DOWN);return 2;
 }
 if(id==4||id==5){enemy_schedule(q,TAUNT);return 1;}
 if(sx(w,&q->actor)<160)q->actor.transform.facing|=1;enemy_schedule(q,q->grounded?WALK:FALL);return 2;
}
static int face_fixed_target(struct WeaponWorld*w,struct EnemyActor*q){
 struct PlayerBodyState*p=target(w,q);if(!p)return enemy_fault(w);
 if(word((p->actor.transform.x>>8)-(q->actor.transform.x>>8))<0)q->actor.transform.facing&=254;else q->actor.transform.facing|=1;return 1;
}
/* Source entry aliases can run a different entry/frame immediately. Entry
 * returns 2 for next-tick-only constructors or a controller handoff. */
static int enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 for(unsigned budget=0;budget<16;budget++){
  unsigned state=a->state;q->entry=0;int r;int16_t x,y;
  switch(state){
  case NEW:r=initialize(w,e);if(r!=1)return r;continue;
  case WALK:enemy_start(q,EAN_RUN);speed(q,(int16_t)rank(w,q,2));a->vy=0;break;
  case AIR_DISPATCH:{static const uint8_t next[4]={LEAP,CROUCH_START,FALL,LEAP};enemy_schedule(q,next[(random_word(w)&12)>>2]);continue;}
  case GRAB_APPROACH:case FOOT_APPROACH:break;
  case GRAB_LEAP:
   enemy_start(q,EAN_GRAB_LEAP);a->vx=faced(q,-546);a->vy=1635;a->ay=-109;a->ax=0;
   if(target(w,q)){enemy_attack_binding(q,q->infantry.target==0?ENB_GRAB_P1:ENB_GRAB_P2);hit_begin_attack(&a->hit);}break;
  case FOOT_LEAP:
   enemy_start(q,EAN_SLUG_LEAP);a->ax=0;
   if(q->params[5]==255){a->vx=faced(q,-546);a->vy=1080;a->ay=-72;}
   else if(q->params[5]==1){a->vx=faced(q,-409);a->vy=2040;a->ay=-102;}
   else if(q->params[5]==2){a->vx=faced(q,-292);a->vy=2044;a->ay=-73;}
   else{a->vx=faced(q,-682);a->vy=2040;a->ay=-170;}
   hit_begin_attack(&a->hit);break;
  case FOOT_HIT:enemy_start(q,EAN_SLUG_HIT);hit_begin_attack(&a->hit);break;
  case GRAB_TOUCH:{
   enemy_start(q,EAN_GRAB_TOUCH);enemy_defense(q,ENB_GRAB_TOUCH_DEF);a->hit.flags&=247;enemy_attack_binding(q,65535);hit_begin_attack(&a->hit);q->infantry.probability=0;
   if(pick_anchor(w,q)<0){enemy_schedule(q,GRAB_RELEASE);continue;}
   struct PlayerBodyState*p=target(w,q);if(!p)return enemy_fault(w);
   q->infantry.leap=(uint16_t)word((a->transform.x>>8)-(p->actor.transform.x>>8));q->infantry.crouch=(uint16_t)word(transform_y_up(&a->transform)-transform_y_up(&p->actor.transform));break;
  }
  case GRAB_ALIGN:
   enemy_start(q,EAN_GRAB_ALIGN);a->transform.facing&=254;r=pick_anchor(w,q);
   if(r<0){enemy_schedule(q,GRAB_RELEASE);continue;}q->infantry.anchor=(uint8_t)r;
   if(!enemy_anchor(w,q,(unsigned)r,0,&x,&y)){enemy_schedule(q,GRAB_RELEASE);continue;}
   q->infantry.leap=(uint16_t)word((a->transform.x>>8)-x);q->infantry.crouch=(uint16_t)word(transform_y_up(&a->transform)-y);break;
  case GRAB_CANNON:case GRAB_RIDER:case GRAB_TREAD:
   a->transform.facing&=254;q->infantry.anchor=(uint8_t)(state-GRAB_CANNON);
   if(!enemy_anchor(w,q,q->infantry.anchor,1,&x,&y)){enemy_schedule(q,GRAB_RELEASE);continue;}
   if(state==GRAB_TREAD)q->infantry.grabs=0;
   enemy_schedule(q,GRAB_CANNON_RESUME+(state-GRAB_CANNON));continue;
  case GRAB_CANNON_RESUME:enemy_start(q,EAN_GRAB_CANNON);q->infantry.signal=0;break;
  case GRAB_RIDER_RESUME:
   enemy_start(q,EAN_GRAB_RIDER);a->palette=q->infantry.target==0?0x20:0x14b;
   enemy_attack_binding(q,q->infantry.target==0?ENB_RIDER_P1:ENB_RIDER_P2);hit_begin_attack(&a->hit);break;
  case GRAB_TREAD_RESUME:enemy_start(q,EAN_GRAB_TREAD);a->hit.flags&=246;break;
  case GRAB_HIT:
   if(++q->infantry.grabs>=3){a->hit.flags|=8;enemy_schedule(q,GRAB_THROWN);continue;}
   a->hit.flags&=246;enemy_start(q,EAN_GRAB_HIT);break;
  case GRAB_SHAKE:enemy_start(q,q->infantry.anchor==2?EAN_GRAB_SHAKE_TREAD:EAN_GRAB_SHAKE);break;
  case GRAB_THROWN:case GRAB_RELEASE:
   if(state==GRAB_RELEASE){enemy_attack_binding(q,65535);hit_begin_attack(&a->hit);}
   if(!face_fixed_target(w,q))return 0;enemy_start(q,state==GRAB_THROWN?EAN_GRAB_THROWN:EAN_GRAB_RELEASE);
   a->vx=faced(q,state==GRAB_THROWN?819:614);a->vy=state==GRAB_THROWN?810:1630;a->ay=state==GRAB_THROWN?-81:-163;a->ax=0;break;
  case STOP:case CROUCH_START:
   if(q->infantry.abilities&16){enemy_schedule(q,BOUNCE);continue;}
   if(state==STOP){enemy_start(q,EAN_EXIT_LAND_HORIZONTAL);speed(q,256);}
   else{enemy_start(q,EAN_RAISE);a->vx=word(a->vx*2);a->vy=0;}break;
  case STOP_BACK:enemy_start(q,EAN_STOP_BACK);speed(q,word(-rank(w,q,2)));break;
  case IDLE:enemy_start(q,a->ground_slope?EAN_SLOPE_IDLE:EAN_IDLE);a->vx=a->vy=0;break;
  case LOOK:if(random_word(w)&15){enemy_schedule(q,BRAKE);continue;}enemy_start(q,EAN_LOOK);break;
  case FALL:enemy_start(q,EAN_EXIT_FALL);break;
  case LAND:
   if(magnitude(a->vy)<magnitude(a->vx)){enemy_schedule(q,STOP);continue;}
   enemy_start(q,EAN_EXIT_LAND_VERTICAL);a->vx=0;break;
  case FOOT_LAND:enemy_start(q,EAN_SLUG_LAND);a->vx=0;break;
  case STEP_FRONT:case STEP_BACK:
   enemy_start(q,state==STEP_FRONT?(a->ground_slope?EAN_STEP_FRONT_SLOPE:EAN_STEP_FRONT):(a->ground_slope?EAN_STEP_BACK_SLOPE:EAN_STEP_BACK));
   speed(q,state==STEP_FRONT?128:-128);a->vy=0;break;
  case MELEE:a->vx=0;enemy_start(q,EAN_MELEE);enemy_attack_binding(q,ENB_MELEE);hit_begin_attack(&a->hit);hit_begin_attack(&a->hit);break;
  case BRAKE:enemy_start(q,EAN_EXIT_BRAKE);break;
  case CROUCH_IDLE:enemy_start(q,EAN_IDLE_ARMED);a->vx=0;break;
  case CROUCH_FRONT:case CROUCH_BACK:{
   int forwards=state==CROUCH_FRONT?!!(q->infantry.flags&1):!(q->infantry.flags&1);
   enemy_start(q,EAN_CROUCH_STEP);speed(q,forwards?((q->infantry.flags&1)?256:512):-256);break;
  }
  case LOWER:enemy_start(q,EAN_LOWER);a->vx=0;break;
  case JUMP:{
   int16_t v=word(q->infantry.advance+q->infantry.retreat-q->infantry.fear-q->infantry.probability);v>>=1;
   a->vx=(int16_t)((a->transform.facing&1?0:65535)^(uint16_t)v);
   enemy_start(q,v<0?EAN_JUMP_BACK:EAN_EXIT_LEAP);a->vy=word(q->infantry.leap<<3);if(v<0)speed(q,-256);
   resume(q,FALL);return 1; /* $058924 jumps to the fall FRAME, preserving its stream. */
  }
  case THROW:{
   unsigned type=q->infantry.weapon;if(type==3)type=(int16_t)random_word(w)<0?1:2;
   enemy_start(q,type==1?EAN_MORTAR_THROW:type==2?EAN_ROLLER_THROW:EAN_THROW);trajectory(w,q);a->vx=0;break;
  }
  case CROUCH_THROW:if(q->infantry.weapon){enemy_schedule(q,CROUCH_BACK);continue;}enemy_start(q,EAN_GUARD_ATTACK);a->variant=6;a->vx=0;break;
  case LEAP:enemy_start(q,EAN_EXIT_LEAP);a->vx=faced(q,-409);a->vy=1630;a->ay=-163;a->ax=0;break;
  case BOUNCE:
   if(!soldier_think(w,e))return 0;if(!(q->infantry.flags&1)){enemy_schedule(q,BRAKE);continue;}
   speed(q,400);enemy_start(q,EAN_BOUNCE_THROW);break;
  case CLIMB_UP_START:enemy_start(q,EAN_CLIMB_UP_START);break;
  case CLIMB_UP:enemy_start(q,EAN_CLIMB_UP);a->ay=0;a->vy=340;break;
  case CLIMB_UP_END:enemy_start(q,EAN_CLIMB_UP_END);break;
  case CLIMB_DISMOUNT:q->grounded=0;enemy_start(q,EAN_CLIMB_DISMOUNT);a->vx=faced(q,-409);a->vy=810;a->ay=-81;a->ax=0;break;
  case CLIMB_DOWN_START:enemy_start(q,EAN_CLIMB_DOWN_START);a->vy=a->ay=0;break;
  case CLIMB_DOWN:enemy_start(q,EAN_CLIMB_DOWN);a->ay=0;a->vy=-340;break;
  case CLIMB_DOWN_END:enemy_start(q,EAN_CLIMB_DOWN_END);break;
  case CLIMB_THROW:enemy_start(q,q->infantry.flags&1?EAN_GUARD_THROW_A:EAN_GUARD_THROW_B);a->variant=q->infantry.flags&1?7:8;break;
  case TAUNT:q->infantry.taunt=0;a->vx=a->vy=0;enemy_schedule(q,TAUNT_REPEAT);continue;
  case TAUNT_REPEAT:enemy_start(q,(enum EnemyAnimation)(EAN_TAUNT_0+((random_word(w)&12)>>2)));break;
  case TAUNT_END:enemy_start(q,EAN_TAUNT_END);break;
  case ESCAPE_FACE:case ESCAPE_LOWER:infantry_escape(w,e,state==ESCAPE_FACE?INFANTRY_EXIT_FACE:INFANTRY_EXIT_LOWER);return 2;
  /* Non-template child entries $05783E/$057880 are exposed by the spawn
   * API for source actors that create jump-in soldiers. */
  case JUMP_IN:case GRAB_JUMP_IN:
   if(!common_init(w,e))return 0;
   if(state==JUMP_IN)gun_mode(w,q);else q->infantry.abilities|=7;
   enemy_start(q,state==JUMP_IN?EAN_JUMP_IN:EAN_GRAB_JUMP_IN);a->vx=faced(q,-409);a->vy=0;a->ay=-40;a->ax=0;
   if(state==JUMP_IN){resume(q,LEAP);return 1;}
   if(!soldier_think(w,e))return 0;
   if(foot_target(w,q)){enemy_start(q,EAN_SLUG_JUMP_IN);resume(q,FOOT_LEAP);return 1;}
   if(target(w,q)){enemy_attack_binding(q,q->infantry.target==0?ENB_GRAB_P1:ENB_GRAB_P2);hit_begin_attack(&a->hit);}
   resume(q,GRAB_LEAP);return 1;
  default:return enemy_fault(w);
  }return !w->fault;
 }return enemy_fault(w);
}
static void tail(struct WeaponWorld*w,struct NativeEntity*e,int alternate){
 if(w->fault)return;
 if(alternate? !human_death_hit_tail_alternate(w,e):!human_death_hit_tail(w,e))return;
 if(interaction_outside(w,&e->payload.enemy.actor,infantry_bounds[1],1))entity_delete(w->entities,e);
}
static void attached_damage(struct WeaponWorld*w,struct NativeEntity*e){
 struct WeaponActor*a=&e->payload.enemy.actor;
 if(a->hit.flags&8){if(a->hit.last_kind==28)(void)human_death_hit_tail(w,e);a->hit.flags&=247;}
}
static int grab_break(struct EnemyActor*q){return q->infantry.probability>=768||(q->infantry.probability&&!q->infantry.retreat);}
static void grab_follow(struct WeaponWorld*w,struct EnemyActor*q){
 if(!w->enemies->damage_enabled){q->actor.hit.flags|=8;enemy_schedule(q,GRAB_THROWN);return;}
 int16_t x,y;if(enemy_anchor(w,q,q->infantry.anchor,2,&x,&y))position(&q->actor,x,y);else enemy_schedule(q,GRAB_RELEASE);
 q->infantry.retreat=q->infantry.probability;q->infantry.probability=(uint16_t)damp((int16_t)q->infantry.probability,6,1);
 int slot=q->infantry.target;if(slot>=0&&slot<2&&w->inputs[slot].actions){q->infantry.probability=(uint16_t)(q->infantry.probability+256);if(q->infantry.probability>=2816)enemy_schedule(q,GRAB_THROWN);}
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(q->entry&&enter(w,e)!=1)return;unsigned state=a->state;int r;uint16_t d;int16_t x,y;int alternate=0;
 switch(state){
 case WALK:
  if(!physics(w,q)||!soldier_think(w,e))return;
  if(pick_anchor(w,q)>=0){enemy_schedule(q,GRAB_APPROACH);tick(p,e,w);return;}
  if(foot_target(w,q)){enemy_schedule(q,FOOT_APPROACH);tick(p,e,w);return;}
  enemy_animate(w,e);edge(w,q);
  if(q->infantry.flags&1){
   if(chance(w,(uint16_t)(256-maximum(q->infantry.retreat,q->infantry.advance))))enemy_schedule(q,STOP);
   if(chance(w,q->infantry.crouch))enemy_schedule(q,CROUCH_START);
   if(chance(w,maximum(q->infantry.fear,q->infantry.probability)))enemy_schedule(q,BRAKE);
   if(chance(w,q->infantry.leap))enemy_schedule(q,JUMP);
  }else if(chance(w,q->infantry.retreat))enemy_schedule(q,STOP);
  if(q->infantry.flags&32)enemy_schedule(q,BRAKE);if(q->infantry.flags&64)enemy_schedule(q,LEAP);
  if(!q->infantry.lifetime)enemy_schedule(q,ESCAPE_FACE);if(throw_gate(w,q))enemy_schedule(q,THROW);
  if(!q->grounded)enemy_schedule(q,AIR_DISPATCH);if(melee(w,q))enemy_schedule(q,MELEE);break;
 case GRAB_APPROACH:case FOOT_APPROACH:
  if(!physics(w,q)||!soldier_think(w,e))return;enemy_animate(w,e);
  if(state==GRAB_APPROACH&&pick_anchor(w,q)<0){resume(q,WALK);break;}
  if(chance(w,square(q->infantry.probability)))enemy_schedule(q,state==GRAB_APPROACH?GRAB_LEAP:FOOT_LEAP);
  if(!(q->infantry.flags&1))enemy_schedule(q,BRAKE);
  if(!q->grounded)enemy_schedule(q,state==GRAB_APPROACH?GRAB_LEAP:FOOT_LEAP);
  if(q->infantry.flags&64)enemy_schedule(q,LEAP);if(q->infantry.flags&32)enemy_schedule(q,STOP);break;
 case GRAB_LEAP:
  r=interaction_move(w,a,WMOVE_FALL);if(r<0)return;if(r)enemy_schedule(q,LAND);enemy_animate(w,e);
  if(pick_anchor(w,q)>=0&&a->vy<0){if(!enemy_attack(w,a))return;struct PlayerBodyState*b=target(w,q);
   if((a->hit.flags&2)&&b&&word(transform_y_up(&b->actor.transform)+16-transform_y_up(&a->transform))<0)enemy_schedule(q,GRAB_TOUCH);}
  break;
 case FOOT_LEAP:
  r=interaction_move(w,a,WMOVE_FALL);if(r<0)return;if(r)enemy_schedule(q,FOOT_LAND);enemy_animate(w,e);
  if(a->vy<0&&!enemy_attack(w,a))return;if(a->hit.flags&2)enemy_schedule(q,FOOT_HIT);break;
 case FOOT_HIT:
  r=interaction_move(w,a,WMOVE_FALL);if(r<0)return;q->grounded=r?255:0;enemy_animate(w,e);
  if(!enemy_attack(w,a))return;if((a->hit.events&2)&&!interaction_sound(w,0x10fa))return;
  if(q->grounded)enemy_schedule(q,FOOT_LAND);break;
 case GRAB_TOUCH:{
  if(pick_anchor(w,q)<0)enemy_schedule(q,GRAB_RELEASE);struct PlayerBodyState*b=target(w,q);if(!b){enemy_fault(w);return;}
  a->transform.priority=(b->actor.transform.priority&0xffe3)|24;
  position(a,word((b->actor.transform.x>>8)+(int16_t)q->infantry.leap),word(transform_y_up(&b->actor.transform)+(int16_t)q->infantry.crouch));
  if(enemy_animate(w,e))enemy_schedule(q,GRAB_ALIGN);attached_damage(w,e);return;
 }
 case GRAB_ALIGN:
  q->infantry.leap=(uint16_t)damp((int16_t)q->infantry.leap,5,1);q->infantry.crouch=(uint16_t)damp((int16_t)q->infantry.crouch,5,1);
  if(enemy_anchor(w,q,q->infantry.anchor,0,&x,&y))position(a,word(x+(int16_t)q->infantry.leap),word(y+(int16_t)q->infantry.crouch));else enemy_schedule(q,GRAB_RELEASE);
  enemy_animate(w,e);
  if(!(q->infantry.leap|q->infantry.crouch))enemy_schedule(q,q->infantry.anchor<3?GRAB_CANNON+q->infantry.anchor:GRAB_RELEASE);
  attached_damage(w,e);return;
 case GRAB_CANNON_RESUME:case GRAB_RIDER_RESUME:
  grab_follow(w,q);if(grab_break(q))enemy_schedule(q,GRAB_SHAKE);
  if(enemy_animate(w,e))enemy_schedule(q,GRAB_RELEASE);
  if(state==GRAB_CANNON_RESUME&&q->infantry.signal)enemy_anchor_detach_cannon(w,q);
  attached_damage(w,e);return;
 case GRAB_TREAD_RESUME:
  grab_follow(w,q);enemy_animate(w,e);if(grab_break(q))enemy_schedule(q,GRAB_SHAKE);
  if((a->hit.flags&8)&&a->hit.last_kind==2)enemy_schedule(q,GRAB_HIT);attached_damage(w,e);return;
 case GRAB_HIT:
  grab_follow(w,q);if(enemy_animate(w,e))enemy_schedule(q,GRAB_TREAD_RESUME);attached_damage(w,e);return;
 case GRAB_SHAKE:
  grab_follow(w,q);if(enemy_animate(w,e))enemy_schedule(q,grab_break(q)?GRAB_SHAKE:GRAB_CANNON_RESUME+q->infantry.anchor);
  if(a->hit.flags&8)enemy_schedule(q,GRAB_HIT);attached_damage(w,e);return;
 case GRAB_THROWN:case GRAB_RELEASE:case LEAP:
  r=enemy_swapped_move(w,q,WMOVE_BODY);if(r<0)return;q->grounded=r?255:0;
  if(r)enemy_schedule(q,state==GRAB_THROWN?LOWER:LAND);enemy_animate(w,e);break;
 case STOP:case STOP_BACK:
  if(!physics(w,q)||!soldier_think(w,e))return;a->vx=damp(a->vx,4,0);if(enemy_animate(w,e))enemy_schedule(q,IDLE);edge(w,q);
  if(chance(w,q->infantry.crouch))enemy_schedule(q,CROUCH_START);
  if(!q->infantry.lifetime)enemy_schedule(q,ESCAPE_FACE);if(!q->grounded)enemy_schedule(q,FALL);
  if(chance(w,q->infantry.leap))enemy_schedule(q,JUMP);if(melee(w,q))enemy_schedule(q,MELEE);break;
 case IDLE:
  if(!physics(w,q)||!soldier_think(w,e))return;enemy_animate(w,e);edge(w,q);
  if(q->infantry.flags&1){
   d=maximum(q->infantry.probability,q->infantry.fear);if(chance(w,d))enemy_schedule(q,STEP_BACK);
   d=square(d);if(chance(w,d))enemy_schedule(q,STOP_BACK);d=square(d);if(chance(w,d))enemy_schedule(q,BRAKE);
   d=maximum(q->infantry.retreat,q->infantry.advance);if(chance(w,d))enemy_schedule(q,STEP_FRONT);
   d=square(d);if(chance(w,d))enemy_schedule(q,WALK);
  }else{if(chance(w,q->infantry.probability))enemy_schedule(q,WALK);if(chance(w,q->infantry.retreat))enemy_schedule(q,LOOK);}
  if(chance(w,q->infantry.fear))enemy_schedule(q,BRAKE);if(chance(w,q->infantry.advance))enemy_schedule(q,WALK);
  if(!a->ground_slope&&chance(w,q->infantry.crouch))enemy_schedule(q,CROUCH_START);
  if(!q->infantry.lifetime)enemy_schedule(q,ESCAPE_FACE);if(throw_gate(w,q))enemy_schedule(q,THROW);
  if(q->infantry.mood&1)enemy_schedule(q,TAUNT);if(chance(w,q->infantry.leap))enemy_schedule(q,JUMP);
  if(melee(w,q))enemy_schedule(q,MELEE);if(!q->grounded)enemy_schedule(q,FALL);break;
 case LOOK:case TAUNT_END:
  if(interaction_move(w,a,WMOVE_SYNC)<0||!soldier_think(w,e))return;
  if(enemy_animate(w,e))enemy_schedule(q,state==LOOK?IDLE:q->infantry.flags&1?BRAKE:WALK);break;
 case FALL:
  a->vx=damp(a->vx,6,0);r=enemy_swapped_move(w,q,WMOVE_ITEM);if(r<0)return;q->grounded=r?255:0;
  if(r)enemy_schedule(q,LAND);enemy_animate(w,e);break;
 case LAND:case FOOT_LAND:
  if(!physics(w,q))return;if(enemy_animate(w,e))enemy_schedule(q,q->grounded?WALK:FALL);break;
 case STEP_FRONT:case STEP_BACK:
  if(!physics(w,q)||!soldier_think(w,e))return;if(enemy_animate(w,e))enemy_schedule(q,IDLE);edge(w,q);
  if(chance(w,q->infantry.leap))enemy_schedule(q,JUMP);if(!q->grounded)enemy_schedule(q,FALL);
  if(throw_gate(w,q))enemy_schedule(q,THROW);if(melee(w,q))enemy_schedule(q,MELEE);break;
 case MELEE:
  if(!physics(w,q))return;if(enemy_animate(w,e))enemy_schedule(q,IDLE);break;
 case BRAKE:
  if(!soldier_think(w,e))return;a->vx=damp(a->vx,4,0);if(!physics(w,q))return;
  if(enemy_animate(w,e))enemy_schedule(q,q->infantry.flags&1?STOP:WALK);if(!q->grounded)enemy_schedule(q,FALL);break;
 case CROUCH_START:
  a->vx=damp(a->vx,5,0);if(!physics(w,q)||!soldier_think(w,e))return;
  if(enemy_animate(w,e)&&q->grounded)enemy_schedule(q,CROUCH_IDLE);break;
 case CROUCH_IDLE:
  if(!physics(w,q)||!soldier_think(w,e))return;enemy_animate(w,e);
  if(chance(w,(uint16_t)(256-q->infantry.crouch)))enemy_schedule(q,LOWER);
  if(q->infantry.flags&1){if(chance(w,q->infantry.retreat))enemy_schedule(q,CROUCH_FRONT);if(chance(w,q->infantry.probability))enemy_schedule(q,CROUCH_BACK);}
  else enemy_schedule(q,CROUCH_BACK);
  if(!q->infantry.lifetime)enemy_schedule(q,ESCAPE_LOWER);if(throw_gate(w,q))enemy_schedule(q,CROUCH_THROW);
  if(!q->grounded)enemy_schedule(q,FALL);alternate=1;break;
 case CROUCH_FRONT:case CROUCH_BACK:
  r=enemy_swapped_move(w,q,WMOVE_FLAT);if(r<0)return;if(r)enemy_schedule(q,LOWER);
  r=terrain_weapon_foot_contact(w->interactions->terrain,a);if(r<0){enemy_fault(w);return;}q->grounded=r?255:0;
  if(!soldier_think(w,e))return;if(a->ground_slope)enemy_schedule(q,LOWER);
  if(enemy_animate(w,e))enemy_schedule(q,CROUCH_IDLE);if(chance(w,(uint16_t)(256-q->infantry.crouch)))enemy_schedule(q,LOWER);
  if(a->hit.events&32)enemy_schedule(q,LOWER);if(!q->infantry.lifetime)enemy_schedule(q,ESCAPE_LOWER);
  if(throw_gate(w,q))enemy_schedule(q,CROUCH_THROW);if(!q->grounded)enemy_schedule(q,FALL);alternate=1;break;
 case LOWER:
  if(!physics(w,q)||!soldier_think(w,e))return;if(enemy_animate(w,e))enemy_schedule(q,WALK);if(!q->grounded)enemy_schedule(q,FALL);break;
 case THROW:case CROUCH_THROW:
  (void)throw_gate(w,q);if(!physics(w,q))return;if(enemy_animate(w,e))enemy_schedule(q,state==THROW?IDLE:CROUCH_IDLE);
  if(!q->grounded)enemy_schedule(q,FALL);alternate=state==CROUCH_THROW;break;
 case BOUNCE:
  a->vx=damp(a->vx,4,1);if(!physics(w,q))return;
  if(enemy_animate(w,e))enemy_schedule(q,q->infantry.abilities&7?LOWER:ESCAPE_LOWER);break;
 case CLIMB_UP_START:case CLIMB_DOWN_START:
  if(state==CLIMB_UP_START)a->ay=a->vy=0;
  if(!climb_target_follow(w,a,q->infantry.key,&y))return;
  if(enemy_animate(w,e))enemy_schedule(q,state==CLIMB_UP_START?CLIMB_UP:CLIMB_DOWN);break;
 case CLIMB_UP:case CLIMB_DOWN:
  if(!soldier_think(w,e)||!climb_target_follow(w,a,q->infantry.key,&y))return;
  if(enemy_animate(w,e)){enemy_schedule(q,state);if(throw_gate(w,q))enemy_schedule(q,CLIMB_THROW);}
  if(y<0)enemy_schedule(q,FALL);
  if(state==CLIMB_UP){if(y>=0&&(uint16_t)word(y-36)<(uint16_t)sy(w,a))enemy_schedule(q,CLIMB_UP_END);}
  else{r=terrain_weapon_foot_contact(w->interactions->terrain,a);if(r<0){enemy_fault(w);return;}q->grounded=r?255:0;if(r)enemy_schedule(q,CLIMB_DOWN_END);}break;
 case CLIMB_UP_END:case CLIMB_DOWN_END:
  if(interaction_move(w,a,WMOVE_SYNC)<0)return;if(state==CLIMB_UP_END&&!soldier_think(w,e))return;
  if(enemy_animate(w,e))enemy_schedule(q,state==CLIMB_UP_END?CLIMB_DISMOUNT:IDLE);break;
 case CLIMB_DISMOUNT:
  a->vx=damp(a->vx,4,1);if(!physics(w,q)||!soldier_think(w,e))return;
  if(enemy_animate(w,e))enemy_schedule(q,q->grounded?IDLE:FALL);break;
 case CLIMB_THROW:{
  int16_t old=a->vy;a->vy=a->ay=0;r=climb_target_follow(w,a,q->infantry.key,&y);a->vy=old;if(!r)return;
  if(enemy_animate(w,e))enemy_schedule(q,old<0?CLIMB_DOWN:CLIMB_UP);break;
 }
 case TAUNT_REPEAT:
  if(!physics(w,q)||!soldier_think(w,e))return;if(enemy_animate(w,e))enemy_schedule(q,TAUNT_REPEAT);
  if(q->infantry.mood&1){q->infantry.taunt=1;break;}
  if(q->infantry.taunt)enemy_schedule(q,TAUNT_END);
  if(chance(w,q->infantry.probability))enemy_schedule(q,q->infantry.flags&1?TAUNT_END:BRAKE);
  if(q->infantry.mood&8)enemy_schedule(q,TAUNT_END);if(q->infantry.mood&4)enemy_schedule(q,BRAKE);break;
 default:enemy_fault(w);return;
 }
 tail(w,e,alternate);
}
int soldier_spawn_template(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8],uint8_t flags){
 if(id<1||id>22)return 0;struct NativeEntity*e=enemy_allocate(w,p,o,t,tick);if(!e)return 0;struct EnemyActor*q=&e->payload.enemy;
 q->infantry.kind=5;q->infantry.engaged=(uint8_t)id;q->source_flags=flags;
 if(params)for(unsigned i=0;i<8;i++)q->params[i]=params[i];return 1;
}
struct NativeEntity*soldier_spawn_jump_entity(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,int grab){
 struct NativeEntity*e=enemy_allocate(w,p,o,t,tick);if(!e)return 0;e->payload.enemy.infantry.kind=5;
 enemy_schedule(&e->payload.enemy,grab?GRAB_JUMP_IN:JUMP_IN);return e;
}

int soldier_spawn_jump(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,int grab){return soldier_spawn_jump_entity(w,p,o,t,grab)!=0;}

/* $05724E enters the existing template-9 constructor with forced parameters. */
static int fixed_eject(struct WeaponWorld*w,struct NativeEntity*e,unsigned id){
 struct EnemyActor*q=&e->payload.enemy;uint8_t*b=(uint8_t*)&q->infantry;for(unsigned i=0;i<sizeof(q->infantry);i++)b[i]=255;
 q->infantry.kind=5;q->infantry.engaged=(uint8_t)id;q->infantry.target=-2;q->actor.display_flags|=2;
 q->params[0]=2;q->params[1]=10;q->params[2]=30;q->params[3]=10;e->tick=tick;enemy_schedule(q,NEW);tick(w->entities,e,w);return !w->fault;
}

int soldier_tank_eject(struct WeaponWorld*w,struct NativeEntity*e){return fixed_eject(w,e,9);}
int soldier_mortar_eject(struct WeaponWorld*w,struct NativeEntity*e){return fixed_eject(w,e,1);}
