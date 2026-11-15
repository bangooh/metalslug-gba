#include "shore.h"
enum State {NEW,WAIT,RISE,FALL,FIRE,SINK,HIT,HIT_FALL,SLIDE,MINE_A,MINE_B,FRAGMENT};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static void init(struct NativeEntity*e,unsigned s){uint8_t*p=(uint8_t*)&e->payload.enemy.barrel;for(unsigned i=0;i<sizeof(e->payload.enemy.barrel);i++)p[i]=255;enemy_schedule(&e->payload.enemy,s);}
static struct NativeEntity*child(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct NativeEntity*n=enemy_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,tick);if(n)init(n,s);return n;}
static int callback(struct WeaponWorld*w,struct NativeEntity*e,unsigned n){if(n==SC_66680)return child(w,e,MINE_A)!=0;return enemy_fault(w);}
static int splash(struct WeaponWorld*w,struct NativeEntity*e,int offset){struct NativeEntity*n=shore_splash(w,e,0);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,0,(int16_t)offset);if(offset)n->payload.enemy.actor.display_flags|=64;return 1;}
static int16_t player_x(struct WeaponWorld*w,struct WeaponActor*a,int*slot){*slot=enemy_nearest_x(w,a,0);if(*slot<0)*slot=1;return w->players[*slot]?m4_word((w->players[*slot]->actor.transform.x>>8)-w->camera_x):-1;}
static void face(struct WeaponWorld*w,struct WeaponActor*a){int slot;int16_t x=player_x(w,a,&slot);a->transform.facing=m4_word(m4_sx(w,a)-x)<0?1:0;}
static int near(struct WeaponWorld*w,struct EnemyActor*q){int slot;int16_t d=m4_word(m4_sx(w,&q->actor)-player_x(w,&q->actor,&slot));if(d<0)d=m4_word(-d);q->barrel.target=(uint8_t)slot;return q->params[3]*16>=d;}
static int16_t attacker_speed(struct WeaponWorld*w,struct WeaponActor*a){unsigned i=a->hit.last_attacker;if(i<ENTITY_CAPACITY){struct NativeEntity*n=&w->entities->nodes[i];if(n->kind==ENTITY_PLAYER)return n->payload.player.vx;if(n->kind==ENTITY_PLAYER_ARM)return 0;/* $05DC34 clears arm velocity; arm follow never changes it. */}struct WeaponActor*b=i<ENTITY_CAPACITY?entity_weapon_actor(&w->entities->nodes[i]):0;if(!b){enemy_fault(w);return 0;}return b->vx;}
static int probe(struct WeaponWorld*w,struct WeaponActor*a,struct WeaponContact*c){if(!w->probe||!w->probe(w->scene,a,c))return enemy_fault(w);return 1;}
static int foot(struct WeaponWorld*w,struct WeaponActor*a,uint8_t*raw){int r=terrain_weapon_foot_contact(w->interactions->terrain,a);if(r<0){enemy_fault(w);return -1;}if(raw){struct WeaponContact c;if(!terrain_weapon_point_sample(w->interactions->terrain,a,(int16_t)(a->transform.x>>8),m4_word(transform_y_up(&a->transform)-1),&c)){enemy_fault(w);return -1;}*raw=c.raw;}return r;}
static int mine_move(struct WeaponWorld*w,struct EnemyActor*q){struct WeaponActor*a=&q->actor;a->ay=m4_word(q->barrel.bob-a->vy)>>3;if(interaction_move(w,a,WMOVE_POINT)<0)return -1;q->barrel.bob=m4_word(q->barrel.bob-a->vy);int16_t d=q->barrel.bob;if(d<0)d=m4_word(-d);return d<=1536;}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned s=a->state;
 switch(s){
 case NEW:a->palette=0x179;a->display_flags|=64;a->transform.priority=0x8000;m4_reset(a);if(interaction_move(w,a,WMOVE_POINT)<0)return 0;a->hit.hp=1;shore_start(q,SA_BARREL_IDLE);break;
 case WAIT:shore_start(q,SA_BARREL_WAIT);q->barrel.repeats=q->params[0];break;
 case RISE:a->vx=a->ax=0;a->vy=2560;a->ay=-128+(q->params[1]&7)*8;shore_start(q,SA_BARREL_RISE);if(!splash(w,e,8))return 0;break;
 case FALL:a->ay>>=1;face(w,a);shore_start(q,SA_BARREL_FALL);break;
 case FIRE:shore_start(q,SA_BARREL_FIRE);break;
 case SINK:if(!interaction_outside(w,a,shore_bounds[1],1)&&(!interaction_sound(w,0x1052)||!splash(w,e,8)))return 0;break;
 case HIT:if(!interaction_sound(w,0x103e))return 0;a->display_flags&=191;m4_reset(a);a->vx=attacker_speed(w,a)>>2;shore_start(q,SA_BARREL_HIT);a->state=HIT_FALL;/* fall through */
 case HIT_FALL:a->vy=0;a->ay=-48;break;
 case SLIDE:{uint8_t raw=0;int r=foot(w,a,&raw);if(r<0)return 0;if(r&&raw==0x41){if(!interaction_sound(w,0x1052)||!splash(w,e,8))return 0;entity_delete(w->entities,e);return 0;}a->ax=m4_word(-a->vx)>>5;a->vy=a->ay=0;shore_start(q,SA_BARREL_SLIDE);break;}
 case MINE_A:a->palette=0x1da;q->barrel.palettes[0]=0x179;q->barrel.palettes[1]=0x1da;a->transform.priority=0x8000;m4_reset(a);a->vx=shore_rank_value(w,q,SR_MINE);if(!(a->transform.facing&1))a->vx=m4_word(-a->vx);transform_integer_offset(&a->transform,a->transform.facing&1?24:-24,36);shore_start(q,SA_MINE_A);shore_attack(q,SB_MINE_ATTACK);hit_begin_attack(&a->hit);shore_defense(q,SB_MINE);a->hit.hp=100;q->barrel.bob=-3072;break;
 case MINE_B:shore_start(q,SA_MINE_B);break;
 case FRAGMENT:{a->palette=0x179;shore_start(q,SA_FRAGMENT);m4_reset(a);a->vy=1792;a->ay=-128;struct EnemyActor*p=m4_parent(w,e);if(!p)return 0;int16_t speed=attacker_speed(w,&p->actor);if(!speed)speed=2048;a->vx=speed>>1;break;}
 default:return enemy_fault(w);
 }return !w->fault;
}
static int hit(struct WeaponWorld*w,struct NativeEntity*e,int fragment){if(!(e->payload.enemy.actor.hit.flags&8))return 1;if(fragment&&!child(w,e,FRAGMENT))return 0;enemy_schedule(&e->payload.enemy,HIT);return 1;}
static int box(struct WeaponWorld*w,struct WeaponActor*a,unsigned n){const int16_t*b=shore_boxes[n];struct HitRect r={b[0],b[1],b[2],b[3]};return enemy_nearest_x(w,a,&r)>=0;}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(q->entry){q->entry=0;if(!enter(w,e))return;}unsigned s=a->state;
 if(s==MINE_A||s==MINE_B){int r=mine_move(w,q);if(r<0)return;if(r&&s==MINE_A)enemy_schedule(q,MINE_B);if(!(w->frame_phase&1)){uint16_t old=q->barrel.palettes[0];q->barrel.palettes[0]=q->barrel.palettes[1];q->barrel.palettes[1]=old;a->palette=old;}shore_animate(w,e,callback);
  if(a->hit.flags&8){a->hit.flags&=247;if(!interaction_sound(w,0x108d))return;}int explode=!!(a->hit.flags&1);if(!explode){struct WeaponContact c;if(!probe(w,a,&c))return;explode=c.solid;if(!explode){if(!enemy_attack(w,a))return;explode=(a->hit.flags&2)||interaction_outside(w,a,shore_bounds[2],1);}}
  if(explode){if(!interaction_sound(w,0x1022)||!boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_FIRE))return;entity_delete(p,e);}return;
 }
 if(s==SINK){shore_animate(w,e,callback);entity_delete(p,e);return;}
 if(s==FRAGMENT){if(interaction_move(w,a,WMOVE_POINT)<0)return;struct WeaponContact c;if(!probe(w,a,&c))return;if(c.solid){if(c.raw==0x41&&!splash(w,e,0))return;entity_delete(p,e);return;}shore_animate(w,e,callback);if(interaction_outside(w,a,shore_bounds[2],1))entity_delete(p,e);return;}
 int16_t old=a->vx;enum WeaponMove move=s==NEW||s==WAIT?WMOVE_SYNC:s==HIT_FALL||s==SLIDE?WMOVE_FALL:WMOVE_POINT;if(interaction_move(w,a,move)<0)return;
 if(s==SLIDE){if(((uint16_t)old^(uint16_t)a->vx)&32768)a->vx=a->ax=0;int r=foot(w,a,0);if(r<0)return;if(!r)enemy_schedule(q,HIT_FALL);}
 if(s==HIT_FALL){int r=foot(w,a,0);if(r<0)return;if(r)enemy_schedule(q,SLIDE);}
 int done=shore_animate(w,e,callback);
 switch(s){
 case NEW:face(w,a);if(near(w,q))enemy_schedule(q,WAIT);break;
 case WAIT:if(done&&!--q->barrel.repeats)enemy_schedule(q,RISE);break;
 case RISE:if(a->vy<0)enemy_schedule(q,FALL);if(!hit(w,e,1))return;break;
 case FALL:{if(box(w,a,0)&&!box(w,a,1))enemy_schedule(q,FIRE);struct WeaponContact c;if(!probe(w,a,&c))return;if(c.solid&&c.raw==0x41)enemy_schedule(q,SINK);if(!hit(w,e,1))return;break;}
 case FIRE:{uint8_t raw=0;int r=foot(w,a,&raw);if(r<0)return;if(r&&raw==0x41)enemy_schedule(q,SINK);if(!hit(w,e,0))return;break;}
 case SLIDE:if(done){entity_delete(p,e);return;}break;
 case HIT_FALL:break;
 default:enemy_fault(w);return;
 }
 if(interaction_outside(w,a,shore_bounds[1],0))entity_delete(p,e);
}
int barrel_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,const struct PlayerTransform*t,const uint8_t params[8]){struct NativeEntity*n=enemy_allocate(w,parent,order,t,tick);if(!n)return 0;init(n,NEW);if(params)for(unsigned i=0;i<8;i++)n->payload.enemy.params[i]=params[i];return 1;}
