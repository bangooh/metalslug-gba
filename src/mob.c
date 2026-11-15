#include "mob.h"
#include "../generated/mob-programs.h"
/* The source frontend file also contains these ambient actors. They are
 * native entities, including their invulnerable-after-first-hit branches. */
enum State {NEW,A_INIT,A_RESUME,A_WALK,A_HIT,A_INTRO,FALL,RESUME,B_INIT,B_RESUME,B_WALK,B_HIT,B_TURN,B_VELOCITY,B_INTRO,B_STAGGER,DROP,C_RESUME,C_WALK,C_FALL,C_INTRO,SIT,HOP,D_INIT,D_REENTER,D_WALK,STATIC,WAIT_A,NEAR_A,WAIT_B,NEAR_B,DELETE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static int16_t word(int n){return (int16_t)(uint16_t)n;}
static int16_t sx(struct WeaponWorld*w,struct WeaponActor*a){return word((a->transform.x>>8)-w->camera_x);}
static void reset(struct WeaponActor*a){a->vx=a->vy=a->ax=a->ay=0;transform_integer_copy(&a->transform,&a->transform);}
static void facing(struct EnemyActor*q){q->params[0]&=1;q->actor.transform.facing=q->params[0];}
static void velocity(struct EnemyActor*q){q->actor.vx=word(q->params[1]*32);if(!q->actor.transform.facing)q->actor.vx=word(-q->actor.vx);}
static void start_entry(struct EnemyActor*q,uint16_t entry){animation_begin(&q->actor.animation,entry);q->actor.movement_flags|=32;}
static void start(struct EnemyActor*q,enum MobAnimation root){start_entry(q,mob_entries[root]);}
static int bind(void*user,unsigned kind,uint32_t id){struct NativeEntity*e=user;struct WeaponActor*a=&e->payload.enemy.actor;if(kind!=AN_DEFENSE)return 0;if(id==0xffffffffu){a->defense=0;a->defense_count=0;return 1;}if(id>=sizeof(mob_bindings)/sizeof(*mob_bindings))return 0;a->defense=mob_bindings[id].defense;a->defense_count=mob_bindings[id].count;return 1;}
static int animate(struct WeaponWorld*w,struct NativeEntity*e){struct AnimationOwner o={.context=e,.bind=bind};int r=animation_step(&e->payload.enemy.actor.animation,&mob_program,&o);if(e->payload.enemy.actor.animation.fault)enemy_fault(w);return r;}
static int settle(struct WeaponWorld*w,struct WeaponActor*a){
 int16_t y=word(transform_y_up(&a->transform)+w->camera_y);transform_integer_offset(&a->transform,0,word((y&1023)-y));
 for(unsigned budget=0;budget<2048;budget++){
  if(word(transform_y_up(&a->transform)+w->camera_y)<0){transform_integer_offset(&a->transform,word(160-sx(w,a)),word(384-transform_y_up(&a->transform)-w->camera_y));return 1;}
  int r=interaction_move(w,a,WMOVE_ITEM);if(r<0)return 0;if(r)return 1;
 }return enemy_fault(w);
}
static int foot(struct WeaponWorld*w,struct WeaponActor*a){int r=terrain_weapon_foot_contact(w->interactions->terrain,a);if(r<0)enemy_fault(w);return r;}
static int hit_direction(struct WeaponWorld*w,struct WeaponActor*a){
 if(!a->defense||!a->defense_count)return 0;
 const struct HitRect*r=&a->defense->rect;int16_t x=sx(w,a),center=word(word(r->left+x)+word(r->right+x))>>1;
 return (uint16_t)word(a->hit.impact_x-w->camera_x)<(uint16_t)center?0:1;
}
static void priority(struct WeaponWorld*w,struct WeaponActor*a,uint16_t v){interaction_priority(w,a,v,4);}
static int init(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned id=q->mob.variant;
 a->display_flags&=253;
 if(id==174||id==175){a->palette=0x125;if(id==175)reset(a);facing(q);q->mob.resume=0;enemy_schedule(q,id==174?A_INIT:A_INTRO);return 0;}
 if(id>=176&&id<=179){q->mob.choice=id&1;q->mob.retries=id&1?(id==177?16:3):-1;a->palette=0x124;q->mob.resume=1;q->mob.immune=0;enemy_schedule(q,id<=177?B_INIT:B_INTRO);return 0;}
 if(id>=195&&id<=206){a->palette=(uint16_t)(0x126+((id-(id<201?195:201))>>1));reset(a);if(!settle(w,a))return 0;facing(q);q->mob.choice=!(id&1)?0:1;priority(w,a,0x8000);enemy_schedule(q,id<201?C_RESUME:C_INTRO);return 0;}
 if(id==207){a->palette=0x125;if(!settle(w,a))return 0;reset(a);facing(q);priority(w,a,0);enemy_schedule(q,SIT);return 0;}
 if(id>=208&&id<=219){
  unsigned kind=(id-208)&3;a->palette=(uint16_t)(0x126+((id-208)>>2));
  if(!kind){reset(a);if(!settle(w,a))return 0;facing(q);priority(w,a,0x8000);a->hit.hp=1;start(q,MOA_STATIC_A);enemy_schedule(q,STATIC);return 0;}
  if(kind==1){start(q,MOA_WALK_D);priority(w,a,0x8000);if(!settle(w,a))return 0;}
  else{if(!settle(w,a))return 0;priority(w,a,0x8000);start(q,kind==2?MOA_WALK_E:MOA_WALK_F);}
  enemy_schedule(q,D_INIT);return 0;
 }
 if(id==220||id==221){a->palette=id==220?0x125:0x127;reset(a);if(!settle(w,a))return 0;facing(q);if(id==221)q->dead=0;priority(w,a,0x8000);enemy_schedule(q,id==220?WAIT_A:WAIT_B);return 0;}
 if(id>=222&&id<=226){a->palette=(uint16_t)(id<=224?0x126+id-222:0x129);start(q,id<=224?MOA_STATIC_B:id==225?MOA_STATIC_C:MOA_STATIC_D);reset(a);if(!settle(w,a))return 0;facing(q);priority(w,a,0x8000);enemy_schedule(q,STATIC);return 0;}
 return enemy_fault(w);
}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned s=a->state;q->entry=0;
 switch(s){
 case NEW:return init(w,e);
 case A_INIT:q->mob.immune=0;reset(a);if(!settle(w,a))return 0;priority(w,a,0x8000);/* FALLTHROUGH */
 case A_RESUME:a->hit.hp=1;velocity(q);a->hit.flags&=247;start(q,MOA_WALK_A);a->state=A_WALK;break;
 case A_INTRO:if(!settle(w,a))return 0;priority(w,a,0x8000);a->hit.hp=1;start(q,MOA_INTRO_A);break;
 case A_HIT:q->mob.immune=255;start(q,MOA_HIT_A);break;
 case FALL:case C_FALL:reset(a);a->vy=-32;break;
 case RESUME:enemy_schedule(q,q->mob.resume?B_VELOCITY:A_RESUME);return 2;
 case B_INIT:reset(a);if(!settle(w,a))return 0;facing(q);velocity(q);priority(w,a,0x8000);enemy_schedule(q,B_RESUME);return 2;
 case B_VELOCITY:velocity(q);enemy_schedule(q,B_RESUME);return 2;
 case B_TURN:a->transform.facing^=1;a->vx=word(-a->vx);transform_integer_offset(&a->transform,a->transform.facing&1?8:-8,0);enemy_schedule(q,B_RESUME);return 2;
 case B_RESUME:if((unsigned)q->mob.choice>=2)return enemy_fault(w);a->hit.hp=1;start_entry(q,mob_walk_b[q->mob.choice]);a->hit.flags&=247;a->state=B_WALK;break;
 case B_INTRO:reset(a);if(!settle(w,a))return 0;facing(q);velocity(q);priority(w,a,0x8000);start_entry(q,mob_intro_b[q->mob.choice]);break;
 case B_STAGGER:start(q,MOA_STAGGER);break;
 case B_HIT:
  q->mob.immune=255;if(q->mob.choice){struct NativeEntity*n=enemy_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,tick);if(!n)return 0;enemy_schedule(&n->payload.enemy,DROP);q->mob.choice=0;}start(q,MOA_HIT_B);break;
 case DROP:
  a->palette=0x124;priority(w,a,0x8000);reset(a);a->vx=a->transform.facing&1?-0x355:0x355;a->vy=0x690;a->ay=-56;
  if(!w->rng)return enemy_fault(w);source_random_word(w->rng);
  /* $098EE6 is MOVE.W #$7F, not AND.W: discard RNG, add the fixed 64. */
  a->vx=word(a->vx+64);if(a->transform.facing&1)a->vx=word(-a->vx);start(q,MOA_DROP);break;
 case C_RESUME:velocity(q);a->hit.hp=1;start_entry(q,mob_walk_c[q->mob.choice]);a->state=C_WALK;break;
 case C_INTRO:start_entry(q,mob_intro_c[q->mob.choice]);break;
 case SIT:q->mob.timer=60;a->hit.hp=1;start(q,MOA_SIT);break;
 case HOP:q->mob.timer=40;start(q,MOA_HOP);break;
 case D_REENTER:start(q,MOA_WALK_D);priority(w,a,0x8000);if(!settle(w,a))return 0;a->display_flags&=253;enemy_schedule(q,D_INIT);return 0;
 case D_INIT:reset(a);facing(q);velocity(q);a->hit.hp=1;a->state=D_WALK;break;
 case STATIC:break;
 case WAIT_A:a->hit.hp=1;start(q,MOA_WAIT_A);break;
 case WAIT_B:a->hit.hp=1;start(q,MOA_WAIT_B);break;
 case NEAR_A:start(q,MOA_NEAR_A);break;
 case NEAR_B:start(q,MOA_NEAR_B);break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static void tick(struct EntityPool*pool,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 for(unsigned budget=0;budget<4;budget++){
  if(a->state==DELETE){entity_delete(pool,e);return;}if(q->entry){int r=enter(w,e);if(!r)return;if(r==2)continue;}
  unsigned s=a->state;enum WeaponMove move=(s==A_WALK||s==B_WALK||s==C_WALK||s==D_WALK)?WMOVE_GROUND:(s==FALL||s==C_FALL)?WMOVE_ITEM:s==DROP?WMOVE_FALL:WMOVE_SYNC;
  if(interaction_move(w,a,move)<0)return;
  if(move==WMOVE_GROUND){
   if(s==B_WALK&&(a->hit.events&32))enemy_schedule(q,B_HIT);
   int ground=foot(w,a);if(ground<0)return;if(!ground){
    if(s==D_WALK)enemy_schedule(q,D_REENTER); /* Source returns to D even from E/F. */
    else enemy_schedule(q,s==C_WALK?C_FALL:FALL);
   }
  }else if(s==FALL||s==C_FALL){int ground=foot(w,a);if(ground<0)return;if(ground)enemy_schedule(q,s==C_FALL?C_RESUME:RESUME);}
  if(s==B_STAGGER)a->hit.flags&=247;
  int done=animate(w,e);
  if(s==A_WALK||s==B_WALK){
   if(q->mob.immune)a->hit.flags&=247;
   else if(a->hit.flags&8){
    if(s==A_WALK)enemy_schedule(q,A_HIT);
    else{
     int direction=hit_direction(w,a);
     if(q->mob.choice&&direction==(a->transform.facing&1)&&--q->mob.retries)enemy_schedule(q,B_STAGGER);else enemy_schedule(q,B_HIT);
    }
   }
  }else if(s==A_HIT&&done)enemy_schedule(q,A_RESUME);
  else if(s==A_INTRO&&done)enemy_schedule(q,A_INIT);
  else if(s==B_HIT&&done)enemy_schedule(q,B_TURN);
  else if(s==B_INTRO&&done)enemy_schedule(q,B_RESUME);
  else if(s==B_STAGGER){if(done)enemy_schedule(q,B_RESUME);}
  else if(s==C_INTRO&&done)enemy_schedule(q,C_RESUME);
  else if(s==SIT){if(q->mob.timer)q->mob.timer--;if(!q->mob.timer&&(a->hit.flags&8))enemy_schedule(q,HOP);a->hit.flags&=247;}
  else if(s==HOP){if(!--q->mob.timer)enemy_schedule(q,SIT);}
  else if(s==WAIT_A&&sx(w,a)<=176)enemy_schedule(q,NEAR_A);
  else if(s==WAIT_B&&!q->dead&&sx(w,a)<=176){q->dead=255;enemy_schedule(q,NEAR_B);}
  else if(s==NEAR_B&&done)enemy_schedule(q,WAIT_B);
  if(interaction_outside(w,a,mob_bounds,1))enemy_schedule(q,DELETE);return;
 }enemy_fault(w);
}
int mob_spawn_template(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){
 if((id<174||id>179)&&(id<195||id>226))return enemy_fault(w);struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;struct EnemyActor*q=&e->payload.enemy;q->mob=(__typeof__(q->mob)){.choice=-1,.retries=-1,.timer=-1,.resume=255,.immune=255,.variant=(uint8_t)id};enemy_schedule(q,NEW);if(params)for(unsigned i=0;i<8;i++)q->params[i]=params[i];return 1;
}
