#include "final_internal.h"
#include "slug_support.h"
/* Templates 114..116 / $071068..$072FC8. The source's FinalBoss label is
 * retained as provenance, without inferring an in-game name from it. */
static void tick(struct EntityPool*,struct NativeEntity*,void*);
uint16_t fb_random(struct WeaponWorld*w){if(!w->rng){enemy_fault(w);return 0;}return source_random_word(w->rng);}
int fb_sound(struct WeaponWorld*w,unsigned s,int stop){return w->sound&&w->sound(w->scene,(uint16_t)s,stop)?1:enemy_fault(w);}
int fb_hit_sound(struct WeaponWorld*w,struct WeaponActor*a){return (a->hit.flags&1)||!(a->hit.events&1)||a->hit.last_kind!=1||fb_sound(w,0x108d,0);}
void fb_screen(struct WeaponWorld*w,struct WeaponActor*a,int16_t x,int16_t y){transform_integer_offset(&a->transform,fb_word(x-fb_sx(w,a)),fb_word(y-fb_sy(w,a)));}
int16_t fb_rank(struct WeaponWorld*w,struct EnemyActor*q,enum FinalRank n){
 struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid||n>=FBR_COUNT){enemy_fault(w);return 0;}
 return (int16_t)final_rank[n][(interaction_rank(w,&q->actor,runtime_owner_final[n]))*8+(q->source_flags&3)*2+((r->player_count_flag&2)>>1)];
}
void fb_start_entry(struct EnemyActor*q,uint16_t n){if(n==65535)return;animation_begin(&q->actor.animation,n);q->actor.movement_flags|=32;}
void fb_start(struct EnemyActor*q,enum FinalAnimation n){fb_start_entry(q,final_entries[n]);}
void fb_defense(struct EnemyActor*q,unsigned n){q->actor.defense=n==65535?0:final_bindings[n].defense;q->actor.defense_count=n==65535?0:final_bindings[n].count;}
void fb_attack(struct EnemyActor*q,unsigned n){if(n==65535)q->actor.attack=0;else{q->attack_set=(struct HitAttackSet){final_bindings[n].attack,final_bindings[n].count};q->actor.attack=&q->attack_set;}}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read(void*ctx,unsigned n,unsigned width){struct Context*c=ctx;if(n==FBF_34_2&&width==2)return c->e->payload.enemy.selector;enemy_fault(c->w);return 0;}
static int write(void*ctx,unsigned n,unsigned width,enum AnimationFieldOperation op,uint32_t v){
 struct Context*c=ctx;if(n!=FBF_82_1||width!=1||op!=AN_FIELD_SET)return enemy_fault(c->w);c->e->payload.enemy.final.pose=(uint8_t)v;return 1;
}
static int bind(void*ctx,unsigned kind,uint32_t value){
 struct Context*c=ctx;unsigned n=value==0xffffffffu?65535:value;if(n!=65535&&n>=sizeof(final_bindings)/sizeof(*final_bindings))return enemy_fault(c->w);
 if(kind==AN_DEFENSE)fb_defense(&c->e->payload.enemy,n);else if(kind==AN_ATTACK){fb_attack(&c->e->payload.enemy,n);hit_begin_attack(&c->e->payload.enemy.actor.hit);}else return enemy_fault(c->w);return 1;
}
static int sound(void*ctx,uint16_t s,int stop){struct Context*c=ctx;return fb_sound(c->w,s,stop);}
static int call(void*ctx,unsigned id,struct Animation*a){(void)a;struct Context*c=ctx;return fb_emit(c->w,c->e,id);}
int fb_animate(struct WeaponWorld*w,struct NativeEntity*e){
 struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;struct Context c={w,e};
 struct AnimationOwner owner={.context=&c,.read=read,.write=write,.bind=bind,.sound=sound,.call=call};
 int done=animation_step(&a->animation,&final_program,&owner);if(a->animation.fault)enemy_fault(w);return done;
}
static void initialize(struct EnemyActor*q){uint8_t*p=(uint8_t*)&q->final;for(unsigned i=0;i<sizeof(q->final);i++)p[i]=255;}
struct EnemyActor*fb_parent(struct WeaponWorld*w,struct NativeEntity*e){struct NativeEntity*p=entity_get(w->entities,e->parent);if(!p||p->kind!=ENTITY_ENEMY){enemy_fault(w);return 0;}return &p->payload.enemy;}
struct NativeEntity*fb_child(struct WeaponWorld*w,struct NativeEntity*e,enum FinalState state){
 struct NativeEntity*n=enemy_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,tick);if(!n)return 0;initialize(&n->payload.enemy);enemy_schedule(&n->payload.enemy,state);return n;
}
int fb_follow(struct WeaponWorld*w,struct NativeEntity*e,int pose_offset){
 struct EnemyActor*q=&e->payload.enemy,*p=fb_parent(w,e);if(!p)return 0;
 /* $05E506 includes fractions, facing and priority. */
 q->actor.transform.x=p->actor.transform.x;q->actor.transform.y=p->actor.transform.y;q->actor.transform.facing=p->actor.transform.facing;
 q->actor.transform.priority=(uint16_t)(p->actor.transform.priority+q->final.priority);
 if(pose_offset){if(q->final.part>5)return enemy_fault(w);unsigned table=q->final.part<2?0:q->final.part==2?1:q->final.part<5?2:3;
  const int16_t*d=final_offsets[table][(uint8_t)(p->final.pose-1)&15];transform_integer_offset(&q->actor.transform,fb_faced(&q->actor,d[0]),d[1]);}
 return 1;
}
static int settle(struct WeaponWorld*w,struct WeaponActor*a){
 fb_screen(w,a,fb_sx(w,a),fb_sy(w,a)&1023);
 for(unsigned n=0;n<2048;n++){if(fb_sy(w,a)<0){fb_screen(w,a,160,384);return 1;}int r=interaction_move(w,a,WMOVE_ITEM);if(r<0)return 0;if(r)return 1;}return enemy_fault(w);
}
static int heartbeat(struct WeaponWorld*w,struct NativeEntity*e,int alive){
 struct EnemyActor*q=&e->payload.enemy;if(!(q->final.attached&1)||e->parent.index==ENTITY_NONE)return 1;
 struct EnemyActor*p=fb_parent(w,e);if(!p)return 0;p->final.heartbeat++;if(alive)p->final.alive++;q->actor.phase=p->actor.phase;return 1;
}
static void commands(struct EnemyActor*q,unsigned a,unsigned b,unsigned c,unsigned d,unsigned head){q->final.commands[1]=a;q->final.commands[2]=b;q->final.commands[3]=c;q->final.commands[4]=d;q->final.commands[0]=head;}
/* $0727EA: the two attack clocks run in order, including overwrites of the
 * same part command bytes when both expire during one frame. */
static void attack_clocks(struct WeaponWorld*w,struct EnemyActor*q){
 q->final.attack_wait=fb_word(q->final.attack_wait-1);
 if(q->final.attack_wait<=0){
  if(q->final.attack_stage==0){
   if(q->actor.phase==1){q->final.attack_count=127;q->final.attack_wait=fb_word(fb_rank(w,q,FBR_ATTACK_WAIT)+16);commands(q,2,2,0,0,2);}
   else{q->final.attack_count=(uint8_t)fb_rank(w,q,FBR_ATTACK_COUNT);q->final.attack_wait=fb_word(fb_rank(w,q,FBR_ATTACK_WAIT)+30);commands(q,2,2,2,0,2);}q->final.attack_stage=1;
  }else if(q->final.attack_stage==1){
   if(q->final.attack_count&1)commands(q,0,0,3,0,0);else commands(q,3,3,0,0,0);
   q->final.attack_wait=fb_word(fb_rank(w,q,FBR_ATTACK_WAIT)+30+(q->actor.phase==1?0:-14));
   if((int8_t)--q->final.attack_count<0)q->final.attack_stage=2;
  }else{q->final.attack_stage=0;q->final.attack_wait=fb_rank(w,q,FBR_ATTACK_REST);q->final.attack_wait=fb_word(q->final.attack_wait+(fb_random(w)&63));commands(q,4,4,4,0,0);}
 }
 q->final.second_wait=fb_word(q->final.second_wait-1);
 if(q->final.second_wait<=0){
  if(q->actor.phase==1){
   if(q->final.second_stage==0){q->final.second_wait=fb_word((fb_rank(w,q,FBR_SECOND_WAIT)>>1)+20);q->final.second_stage=1;commands(q,0,0,0,2,0);}
   else if(q->final.second_stage==1){commands(q,0,0,0,3,0);q->final.second_wait=fb_word((fb_rank(w,q,FBR_SECOND_WAIT)>>1)+4);q->final.second_stage=2;}
   else{q->final.second_wait=fb_word((fb_rank(w,q,FBR_SECOND_WAIT)>>1)+10);q->final.second_stage=1;commands(q,0,0,0,6,0);}
  }else if(q->final.second_stage==0){q->final.second_count=(uint8_t)fb_rank(w,q,FBR_SECOND_COUNT);q->final.second_wait=fb_word(fb_rank(w,q,FBR_SECOND_WAIT)+30);q->final.second_stage=1;commands(q,0,0,0,2,3);}
  else if(q->final.second_stage==1){commands(q,0,0,0,3,0);q->final.second_wait=fb_word(fb_rank(w,q,FBR_SECOND_WAIT)+30);q->final.second_stage=2;}
  else{q->final.second_wait=fb_word(fb_rank(w,q,FBR_SECOND_WAIT)+30);if((int8_t)--q->final.second_count<0){q->final.second_wait=fb_rank(w,q,FBR_SECOND_REST);q->final.second_wait=fb_word(q->final.second_wait+(fb_random(w)&63));}q->final.second_stage=0;commands(q,0,0,0,4,0);}
 }
}
static void chassis_contact(struct WeaponWorld*w,struct EnemyActor*q){
 /* $05E42A tests bit 0 on the calling actor (A6), not the returned
  * player slot. Preserve that source gate instead of 'correcting' it. */
 struct SlugSharedState*s=w->enemies->slug;if(!(q->actor.hit.type_flags&1)||!s||!s->present)return;
 for(unsigned player=0;player<2;player++)if(slug_ridden_by(s,player)){
  struct SlugContact*c=&s->contacts[q->actor.transform.facing&1?1:0];const int16_t*b=final_slug_box;
  int16_t left=b[0],right=b[1],x=fb_word((c->transform.x>>8)-(q->actor.transform.x>>8)),y=fb_word(transform_y_up(&c->transform)-transform_y_up(&q->actor.transform));
  if(q->actor.transform.facing&1){left=fb_word(-b[1]);right=fb_word(-b[0]);}
  if(x<left||x>right||y<b[2]||y>b[3])continue;
  int16_t delta=q->actor.transform.facing&1?fb_word(x+b[0]):fb_word(b[0]-x);c->correction=fb_word(-delta-y);
 }
}
static int body_enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned state=a->state;q->entry=0;struct NativeEntity*n;
 switch(state){
 case FB_ROOT:
  if(!fb_sound(w,0x28,0))return 0;
  n=fb_child(w,e,FB_LEFT);if(!n)return 0;fb_screen(w,&n->payload.enemy.actor,-64,320);n->payload.enemy.actor.transform.facing=1;n->payload.enemy.final.attached=n->payload.enemy.final.front=1;
  n=fb_child(w,e,FB_RIGHT);if(!n)return 0;fb_screen(w,&n->payload.enemy.actor,384,432);n->payload.enemy.final.attached=1;n->payload.enemy.final.front=0;
  a->phase=0;q->final.heartbeat=q->final.alive=0;a->state=FB_WAIT_CHILDREN;return 2;
 case FB_END:if(!fb_sound(w,0x1071,1))return 0;q->final.wait=30;break;
 case FB_LEFT:case FB_RIGHT:case FB_LEFT_ALONE:case FB_RIGHT_ALONE:{
  unsigned right=state==FB_RIGHT||state==FB_RIGHT_ALONE;a->hit_palette=0x80;
  if(state==FB_LEFT_ALONE||state==FB_RIGHT_ALONE){q->final.attached=0;if(right)a->transform.facing=1;a->palette=0x1e5;}
  else{if(!w->secondary_view_valid)return enemy_fault(w);a->display_flags|=2;fb_screen(w,a,fb_sx(w,a),fb_word(512-(right?0x97:0xea)+w->secondary_y_offset));a->palette=right?0x5c:0x5b;}
  q->final.side=(uint8_t)right;if(!fb_child(w,e,FB_HEAD))return 0;
  for(unsigned i=0;i<4;i++){n=fb_child(w,e,FB_LIMB_NEW);if(!n)return 0;n->payload.enemy.final.part=(uint16_t)(i==3&&right?4:i);}
  commands(q,1,1,1,1,0);a->hit.hp=fb_rank(w,q,right?FBR_RIGHT_HP:FBR_LEFT_HP);
  n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_DAMAGE_SMOKE_BODY);if(!n)return 0;n->payload.enemy.actor.attached_offset=0;
  interaction_priority(w,a,q->final.front?0x8001:0x4001,20);
  q->final.attack_wait=fb_random(w)&63;q->final.second_wait=fb_random(w)&63;q->final.attack_stage=q->final.second_stage=0;
  fb_defense(q,FBB_BODY_DEF);fb_attack(q,FBB_BODY_ATK);q->final.wait=1;a->vx=fb_faced(a,-192);a->state=FB_ENTRANCE;break;
 }
 case FB_SNAP:if(!settle(w,a))return 0;a->state=FB_IDLE;/* FALLTHROUGH */
 case FB_IDLE:fb_start(q,fb_faced(a,a->vx)>0?FBA_REVERSE:FBA_IDLE);if(!fb_sound(w,0x1071,0))return 0;break;
 case FB_TURN:case FB_TURN_SHORT:q->final.wait=state==FB_TURN?60:1;a->vx=fb_word(-a->vx);if(!fb_sound(w,0x1071,1))return 0;fb_start(q,FBA_TURN);a->state=FB_TURN;break;
 case FB_PAUSE_RESET:q->final.wait=60;a->state=FB_PAUSE;/* FALLTHROUGH */
 case FB_PAUSE:fb_start(q,FBA_WAIT);break;
 case FB_RESUME:fb_start(q,FBA_RESUME);break;
 case FB_STAND:case FB_STAND_B:a->hit.flags&=247;fb_start(q,state==FB_STAND?FBA_STAND:FBA_STAND_B);if(!fb_sound(w,0x1071,1))return 0;break;
 case FB_DIE:if(!fb_sound(w,0x1033,0))return 0;a->display_flags&=253;fb_start(q,FBA_DIE);commands(q,5,5,5,5,5);break;
 case FB_EXPLODE:
  a->display_flags&=253;if(!fb_sound(w,0x102f,0))return 0;
  for(int x=-56;x<=56;x+=56){n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMOKE);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,(int16_t)x,0);}
  if(!fb_crash_fragments(w,e))return 0;
  /* $072DB8's counter uses BCC: 17 heavy fragments. $06E412 uses BNE: 16. */
  for(unsigned i=0;i<17;i++){n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_METAL_HEAVY);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,0,24);}
  fb_animate(w,e);entity_delete(w->entities,e);return 2;
 case FB_RESET:a->display_flags&=253;commands(q,5,5,5,5,5);a->state=FB_DELETE;return 2;
 default:return 1;
 }return !w->fault;
}
static void body_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(state==FB_WAIT_CHILDREN||state==FB_WAIT_END){
  if(state==FB_WAIT_CHILDREN&&(int8_t)q->final.alive<1){w->enemies->damage_enabled=0;a->state=FB_WAIT_END;}
  if((int8_t)q->final.heartbeat<2){a->phase=1;if(!q->final.heartbeat)enemy_schedule(q,FB_END);}q->final.heartbeat=q->final.alive=0;return;
 }
 if(state==FB_END){q->final.wait=fb_word(q->final.wait-1);if(q->final.wait<=0){w->enemies->scripted_player_lock=0;entity_delete(w->entities,e);}return;}
 if(state==FB_ENTRANCE){if(!heartbeat(w,e,1))return;if(--q->final.wait<=0)enemy_schedule(q,FB_SNAP);return;}
 if(state==FB_DIE){interaction_move(w,a,WMOVE_SYNC);if(fb_animate(w,e))enemy_schedule(q,FB_EXPLODE);if(!heartbeat(w,e,0))return;if(interaction_outside(w,a,final_bounds[0],0))enemy_schedule(q,FB_RESET);return;}
 interaction_body_contact(w,a,&final_contact);chassis_contact(w,q);
 int r=interaction_move(w,a,state==FB_IDLE?WMOVE_FLAT:WMOVE_SYNC);if(r<0)return;
 if(state==FB_IDLE&&r)enemy_schedule(q,FB_TURN);int done=fb_animate(w,e);
 if(state==FB_IDLE){int16_t x=fb_sx(w,a);if((a->vx<=0&&x<80)||(a->vx>0&&x>240)||(a->vx<0&&(a->movement_flags&2))||(a->vx>0&&(a->movement_flags&1)))enemy_schedule(q,FB_TURN);}
 else if(state==FB_TURN&&done)enemy_schedule(q,FB_PAUSE);
 else if(state==FB_PAUSE){q->final.wait=fb_word(q->final.wait-1);if(!q->final.wait)enemy_schedule(q,FB_RESUME);}
 else if(state==FB_RESUME&&done)enemy_schedule(q,FB_IDLE);
 else if((state==FB_STAND||state==FB_STAND_B)&&done)enemy_schedule(q,FB_RESUME);
 if(state==FB_IDLE||state==FB_TURN||state==FB_RESUME){hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return;}
 if(!heartbeat(w,e,1))return;attack_clocks(w,q);
 if(a->hit.flags&8){if(!fb_hit_sound(w,a))return;a->hit.flags&=247;}
 if(a->hit.flags&1)enemy_schedule(q,FB_DIE);
 if(interaction_outside(w,a,final_bounds[0],0))enemy_schedule(q,FB_RESET);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;
 /* Immediate part dispatch can reenter a second part entry during the same
  * tick; bounded by the five source command bytes, never an instruction VM. */
 for(unsigned budget=0;budget<8;budget++){
  unsigned state=q->actor.state;
  if(state==FB_DELETE||state==FB_DELETE_TREE){if(state==FB_DELETE_TREE)entity_delete_children(p,e);entity_delete(p,e);return;}
  if(q->entry){int r=state<=FB_RESET?body_enter(w,e):fb_parts_enter(w,e);if(r!=1||w->fault)return;}
  if(!e->active||e->deleting)return;state=q->actor.state;
  if(state<=FB_RESET){body_frame(w,e,state);return;}
  int r=fb_parts_frame(w,e,state);if(r!=2)return;
 }enemy_fault(w);
}
int final_boss_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){
 if(id<114||id>116)return enemy_fault(w);struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;
 initialize(&e->payload.enemy);if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];
 enemy_schedule(&e->payload.enemy,id==114?FB_ROOT:id==115?FB_LEFT_ALONE:FB_RIGHT_ALONE);return 1;
}
