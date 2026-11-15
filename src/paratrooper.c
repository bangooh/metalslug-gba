#include "shore.h"
enum State {SPAWNER,STOP,TROOP,IDLE,PICK,A,B,LEAVE,DIE,BULLET,SMOKE,EXPLODE,DELETE,DELETE_TREE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static void init(struct NativeEntity*e,unsigned s){uint8_t*p=(uint8_t*)&e->payload.enemy.para;for(unsigned i=0;i<sizeof(e->payload.enemy.para);i++)p[i]=255;enemy_schedule(&e->payload.enemy,s);}
static struct NativeEntity*child(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct NativeEntity*n=enemy_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,tick);if(n)init(n,s);return n;}
static int callback(struct WeaponWorld*w,struct NativeEntity*e,unsigned n){
 if(n==SC_66CC0){struct NativeEntity*v=shore_splash(w,e,1);if(!v)return 0;v->payload.enemy.actor.transform.priority--;return 1;}
 if(n!=SC_66C40&&n!=SC_66C80)return enemy_fault(w);int dx=n==SC_66C40?-8:-20,dy=n==SC_66C40?32:28;
 struct NativeEntity*v=child(w,e,SMOKE);if(!v)return 0;transform_integer_offset(&v->payload.enemy.actor.transform,dx,dy);v=child(w,e,BULLET);if(!v)return 0;transform_integer_offset(&v->payload.enemy.actor.transform,dx,dy);v->payload.enemy.para.angle=n==SC_66C40?64:80;return 1;
}
static int target(struct WeaponWorld*w){int p0=enemy_player_live(w->players[0]),p1=enemy_player_live(w->players[1]);if(!p0)return p1?1:-1;if(!p1)return 0;if(!w->rng){enemy_fault(w);return -1;}return source_random_word(w->rng)&1;}
static int choose_a(struct WeaponWorld*w,struct WeaponActor*a){
 int slot=enemy_nearest_x(w,a,0);if(slot<0)slot=1;int16_t px=w->players[slot]?m4_word((w->players[slot]->actor.transform.x>>8)-w->camera_x):-1,dx=m4_word(px-m4_sx(w,a));if(dx<0)dx=m4_word(-dx);if(dx<32)return 1;
 /* $05E5A8 clears C for a live player in front, including equality. */
 int16_t x=m4_word(m4_sx(w,a)-24);for(unsigned i=0;i<2;i++)if(enemy_player_live(w->players[i])){dx=m4_word(x-m4_word((w->players[i]->actor.transform.x>>8)-w->camera_x));if(a->transform.facing&1?dx<=0:dx>=0)return 0;}return 1;
}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 switch(a->state){
 case SPAWNER:if(!q->params[0])q->params[0]=255;if(!q->params[3])q->params[3]=255;q->para.limit=q->params[1]*32;q->dead=0;a->phase=0;break;
 case STOP:a->phase=1;break;
 case TROOP:a->palette=0x183;m4_reset(a);interaction_priority(w,a,0x4000,20);q->para.timer=q->params[4]?q->params[4]:shore_rank_value(w,q,SR_IDLE);if(q->para.variant){m4_screen(w,a,160,m4_sy(w,a));int i=target(w);if(i>=0){int16_t x=m4_word((w->players[i]->actor.transform.x>>8)-w->camera_x);if(x>=32&&x<=288)m4_screen(w,a,x,m4_sy(w,a));}}shore_start(q,SA_PARA_ENTER);break;
 case IDLE:shore_defense(q,SB_PARA);shore_start(q,SA_PARA_IDLE);break;
 case PICK:if(q->params[3]!=255)q->params[3]--;q->para.repeats=shore_rank_value(w,q,SR_REPEATS);q->para.period=shore_rank_value(w,q,SR_PERIOD);enemy_schedule(q,q->params[2]&&!choose_a(w,a)?B:A);return 2;
 case A:case B:q->para.timer=q->para.period;shore_start(q,a->state==A?SA_PARA_A:SA_PARA_B);break;
 case LEAVE:shore_start(q,SA_PARA_LEAVE);break;
 case DIE:a->palette=w->enemies->blood_enabled?0x183:0x1d7;if(!interaction_sound(w,0x1043))return 0;shore_start(q,SA_PARA_DIE);break;
 case BULLET:{int16_t speed=shore_rank_value(w,q,SR_BULLET);a->vx=m4_word(((int32_t)boss_trig[1][q->para.angle]*speed)>>8);a->vy=m4_word(((int32_t)boss_trig[0][q->para.angle]*speed)>>8);interaction_priority(w,a,0xd000,20);a->palette=0x17b;shore_start(q,SA_PARA_BULLET);break;}
 case SMOKE:interaction_priority(w,a,0xc000,20);a->palette=4;shore_start(q,SA_PARA_SMOKE);break;
 case EXPLODE:interaction_priority(w,a,0xc000,20);return boss_fx_transform(w,e,BFX_FIRE)?3:0;
 default:return enemy_fault(w);
 }return !w->fault;
}
static void heartbeat(struct WeaponWorld*w,struct NativeEntity*e,int mode){struct EnemyActor*q=&e->payload.enemy;struct EnemyActor*p=m4_parent(w,e);if(!p)return;
 if(mode==2&&(p->actor.phase||!q->params[3]))enemy_schedule(q,LEAVE);if(mode&&(q->actor.hit.flags&8))enemy_schedule(q,DIE);p->dead++;if(m4_sx(w,&q->actor)<=0)enemy_schedule(q,DELETE);
}
static int timer(struct WeaponWorld*w,struct EnemyActor*q){q->para.timer=m4_word(q->para.timer-1);int x=m4_sx(w,&q->actor);return x>=32&&x<=288&&q->para.timer<=0;}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 for(unsigned budget=0;budget<3;budget++){
  if(a->state==DELETE||a->state==DELETE_TREE){if(a->state==DELETE_TREE)entity_delete_children(p,e);entity_delete(p,e);return;}
  if(q->entry){q->entry=0;int r=enter(w,e);if(!r||r==3||w->fault)return;if(r==2)continue;}
  unsigned s=a->state;
  if(s==STOP){if(!q->dead)enemy_schedule(q,DELETE);q->dead=0;return;}
  if(interaction_move(w,a,s==BULLET?WMOVE_POINT:WMOVE_SYNC)<0)return;
  if(s==SPAWNER){
   if(q->para.limit<w->secondary_x_offset){enemy_schedule(q,STOP);return;}
   if(!q->dead){if(!q->params[0]){enemy_schedule(q,STOP);return;}q->para.timer=m4_word(q->para.timer-1);if(q->para.timer<=0){q->para.timer=shore_rank_value(w,q,SR_SPAWN);struct NativeEntity*n=child(w,e,TROOP);if(!n)return;n->payload.enemy.para.variant=q->para.variant;for(unsigned i=2;i<=4;i++)n->payload.enemy.params[i]=q->params[i];if(q->params[0]!=255)q->params[0]--;}}
   q->dead=0;if(m4_sx(w,a)<=16||interaction_outside(w,a,shore_bounds[0],0))enemy_schedule(q,STOP);return;
  }
  int done=shore_animate(w,e,callback);
  if(s==BULLET||s==SMOKE){if(s==BULLET){if(!enemy_attack(w,a))return;if((a->hit.flags&2)||m4_sy(w,a)>=456)enemy_schedule(q,EXPLODE);}else if(done)enemy_schedule(q,DELETE_TREE);if(interaction_outside(w,a,boss_fx_bounds[8],1))enemy_schedule(q,DELETE_TREE);return;}
  if(s==TROOP){if(done)enemy_schedule(q,IDLE);heartbeat(w,e,0);return;}
  if(s==IDLE){if(timer(w,q))enemy_schedule(q,PICK);heartbeat(w,e,2);return;}
  if(s==A||s==B){if(done)shore_start(q,SA_PARA_IDLE);if(timer(w,q)){enemy_schedule(q,s);q->para.repeats=m4_word(q->para.repeats-1);if(q->para.repeats<=0){enemy_schedule(q,IDLE);q->para.timer=shore_rank_value(w,q,SR_IDLE);}}heartbeat(w,e,2);return;}
  if(s==LEAVE||s==DIE){if(done)enemy_schedule(q,DELETE);heartbeat(w,e,s==LEAVE);return;}enemy_fault(w);return;
 }enemy_fault(w);
}
int paratrooper_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){if(id!=94&&id!=95)return enemy_fault(w);struct NativeEntity*n=enemy_allocate(w,parent,order,t,tick);if(!n)return 0;init(n,SPAWNER);n->payload.enemy.para.variant=id==95;if(params)for(unsigned i=0;i<8;i++)n->payload.enemy.params[i]=params[i];return 1;}
