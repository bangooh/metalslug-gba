#include "bosses.h"
/* Template 75 / $06E52A..$06F162. Authored flight paths, crew, detached
 * low/high weapons, dive, wreck pieces and the scripted arc entry. */
enum State {ROOT,FLIGHT,DIVE,DIVE_FRAME,WATER,DESTROY,RETIRE,CREW,GUN,AIM,
 DROP,DECEL,FLY,DROP_B,RELOAD,GROUND,EXPLODE,EXPLODE_OFFSET,SHELL,
 ARC_WAIT,ARC,ARC_FALL,DELETE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static int callback(struct WeaponWorld*,struct NativeEntity*,unsigned);
static int16_t word(int n){return (int16_t)(uint16_t)n;}
static uint16_t random_word(struct WeaponWorld*w){if(!w->rng){enemy_fault(w);return 0;}return source_random_word(w->rng);}
static int16_t sx(struct WeaponWorld*w,struct WeaponActor*a){return word((a->transform.x>>8)-w->camera_x);}
static int16_t sy(struct WeaponWorld*w,struct WeaponActor*a){return word(transform_y_up(&a->transform)+w->camera_y);}
static int16_t flip(struct WeaponActor*a,int16_t v){return a->transform.facing&1?word(-v):v;}
static int move(struct WeaponWorld*w,struct WeaponActor*a,enum WeaponMove m){return interaction_move(w,a,m);}
static int animate(struct WeaponWorld*w,struct NativeEntity*e){return boss_animate(w,e,callback);}
static int16_t rank(struct WeaponWorld*w,struct EnemyActor*q,unsigned i){
 if(!w->interactions->rank_valid){enemy_fault(w);return 0;}
 return (int16_t)gunship_rank[i][interaction_rank(w,&q->actor,runtime_owner_gunship[i])][q->source_flags&3][(w->interactions->player_count_flag&2)>>1];
}
static struct EnemyActor*parent(struct WeaponWorld*w,struct NativeEntity*e){
 struct NativeEntity*p=entity_get(w->entities,e->parent);if(!p||p->kind!=ENTITY_ENEMY){enemy_fault(w);return 0;}return &p->payload.enemy;
}
static struct NativeEntity*child(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct NativeEntity*n=enemy_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,tick);if(!n)return 0;
 n->payload.enemy.gunship=(__typeof__(n->payload.enemy.gunship)){.pose=255,.hold=255,.path={.angle=65535}};enemy_schedule(&n->payload.enemy,state);return n;
}
static int follow(struct WeaponWorld*w,struct NativeEntity*e,int offset){
 struct EnemyActor*q=&e->payload.enemy,*p=parent(w,e);if(!p)return 0;
 struct PlayerTransform*t=&q->actor.transform,*s=&p->actor.transform;
 t->x=s->x;t->y=s->y;t->facing=s->facing;t->priority=s->priority;
 if(offset){const int16_t*r=gunship_offsets[p->gunship.pose&7];t->priority=(uint16_t)(t->priority+r[2]);transform_integer_offset(t,flip(&q->actor,r[0]),r[1]);}
 return 1;
}
static int hit(struct WeaponWorld*w,struct WeaponActor*a){
 if(!(a->hit.flags&8))return 1;a->hit.flags&=247;
 return (a->hit.flags&1)||!(a->hit.events&1)||a->hit.last_kind!=1||interaction_sound(w,0x108d);
}
static void outside(struct WeaponWorld*w,struct EnemyActor*q,int force,unsigned next){if(interaction_outside(w,&q->actor,gunship_bounds,force))enemy_schedule(q,next);}
struct PathContext {struct WeaponWorld*w;struct NativeEntity*e;};
static int path_field(void*context,unsigned f,unsigned width,uint32_t v){
 struct PathContext*c=context;struct EnemyActor*q=&c->e->payload.enemy;if(width!=1)return enemy_fault(c->w);
 if(f==0)q->actor.transform.facing=(uint8_t)v;else if(f==1)q->gunship.hold=(uint8_t)v;else return enemy_fault(c->w);return 1;
}
static int path_sound(void*context,uint16_t sound){struct PathContext*c=context;return interaction_sound(c->w,sound);}
static int path_call(void*context,unsigned id){
 struct PathContext*c=context;struct EnemyActor*q=&c->e->payload.enemy;struct WeaponActor*a=&q->actor;
 switch(id){
 case GPC_6EECE:{unsigned i=sy(c->w,a)>416?2:sy(c->w,a)>352?1:0;if(a->transform.facing&1)i+=3;return interaction_sound(c->w,gunship_sounds[i]);}
 case GPC_6F0B8:boss_start(q,BA_GS_A);return 1;
 case GPC_6F0C6:boss_start(q,BA_GS_B);return 1;
 case GPC_6F0E2:boss_start(q,BA_GS_D);return 1;
 case GPC_6F0F0:boss_start(q,BA_GS_E);return 1;
 default:return enemy_fault(c->w);
 }
}
static int callback(struct WeaponWorld*w,struct NativeEntity*e,unsigned id){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(id==BC_6F10C||id==BC_6F124){a->ax=flip(a,word(-(q->gunship.speed>>(id==BC_6F10C?7:4))));return 1;}
 if(id==BC_6F13C){struct NativeEntity*n=child(w,e,SHELL);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,flip(a,16),0);return 1;}
 return enemy_fault(w);
}
static int clearance(struct WeaponWorld*w,struct EnemyActor*q,int second){
 struct WeaponActor*a=&q->actor;int high=q->params[1]==0;
 int16_t floor=second?(high?44:16):(high?68:40);
 if(q->params[3]){int16_t y=second?(high?376:336):(high?400:360);if(q->params[3]!=1)y-=16;return y>sy(w,a);}
 int16_t y=sy(w,a),probe=y;
 for(unsigned count=0;count<8193;count++){
  if(probe<256)break;uint8_t shape;
  struct PlayerTransform t=a->transform;transform_integer_offset(&t,0,word(probe-y));
  if(!terrain_weapon_point_shape(w->interactions->terrain,a,word(t.x>>8),transform_y_up(&t),&shape))return enemy_fault(w);
  if(shape&&((shape&48)==0||(shape&48)==32))break;
  probe=word(probe-8);if(count==8192)return enemy_fault(w);
 }
 return word(y-probe)<=floor;
}
static void speed_limit(struct EnemyActor*q){
 struct WeaponActor*a=&q->actor;uint16_t vx=(uint16_t)flip(a,a->vx);
 if(vx>=(uint16_t)q->gunship.speed){a->vx=flip(a,word(-q->gunship.speed));a->ax=0;}
 if(a->vy>=0)a->ay=a->vy=0;
}
static void gun_tail(struct WeaponWorld*w,struct EnemyActor*q,int alternate){
 struct WeaponActor*a=&q->actor;
 if(!enemy_attack(w,a))return;if(a->hit.flags&2)enemy_schedule(q,alternate?EXPLODE_OFFSET:EXPLODE);
 if(alternate){if(a->hit.flags&8){if(!interaction_sound(w,0x1022))return;enemy_schedule(q,EXPLODE_OFFSET);}}
 else{if(!hit(w,a))return;if(a->hit.flags&1){if(!interaction_sound(w,0x1022))return;enemy_schedule(q,EXPLODE);}}
 outside(w,q,1,DELETE);
}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned state=a->state;q->entry=0;
 switch(state){
 case ROOT:{
  a->palette=0x3b;a->hit_palette=0x7d;q->gunship.base_speed=rank(w,q,3+(q->params[2]&3));a->hit.hp=rank(w,q,0);
  interaction_priority(w,a,0xc000,20);boss_defense(q,BB_GS_BODY);a->phase=0;
  q->gunship.path.cursor=gunship_paths[q->params[0]&7];q->gunship.path.distance=0;
  struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_DAMAGE_SMOKE);if(!n)return 0;n->payload.enemy.actor.attached_offset=-32;
  if(!child(w,e,CREW))return 0;n=child(w,e,GUN);if(!n)return 0;
  for(unsigned i=0;i<8;i++)n->payload.enemy.params[i]=q->params[i];a->state=FLIGHT;return !w->fault;
 }
 case DIVE:
  if(q->params[4]==2)q->params[4]=random_word(w)&1;
  if(q->params[4]){enemy_schedule(q,DESTROY);return enter(w,e);}
  if(!interaction_sound(w,0x10b8))return 0;interaction_priority(w,a,0xc000,28);boss_attack_binding(q,BB_GS_CRASH);hit_begin_attack(&a->hit);
  a->vy=0;a->ay=-32;a->vx=flip(a,word(-(q->gunship.speed>1536?1536:q->gunship.speed)));
  boss_start(q,BA_GS_D);a->state=DIVE_FRAME;return 1;
 case WATER:{
  transform_integer_offset(&a->transform,0,word(288-sy(w,a)));a->transform.priority=65535;
  if(!boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SURFACE20))return 0;
  a->state=DESTROY;
 }
 /* FALLTHROUGH */
 case DESTROY:{
  interaction_priority(w,a,0x4000,0);if(!interaction_sound(w,0x1023))return 0;
  if(!boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMOKE)||!boss_fx_pattern(w,entity_ref(e),&a->transform,BP_GS_DEATH))return 0;
  for(unsigned i=0;i<4;i++){
   struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,i<2?BFX_WRECK0:i==2?BFX_WRECK1:BFX_WRECK2);if(!n)return 0;
   if(i==1)n->payload.enemy.actor.transform.facing^=1;
  }
  a->phase=1;entity_delete(w->entities,e);return 2;
 }
 case RETIRE:a->phase=1;/* FALLTHROUGH */
 case DELETE:entity_delete(w->entities,e);return 2;
 case CREW:a->palette=0x3b;a->hit_palette=0x7d;boss_start(q,BA_GS_CREW);return 1;
 case GUN:
  a->hit.type_flags|=16;if(q->params[1]==2)q->params[1]=(uint8_t)random_word(w);q->params[1]&=1;
  a->palette=0x3c;a->hit_palette=0x7a;q->gunship.speed=rank(w,q,2);interaction_priority(w,a,0xd000,28);boss_defense(q,BB_GS_GUN);a->hit.hp=rank(w,q,1);boss_start(q,BA_GS_GUN);return !w->fault;
 case AIM:boss_start(q,BA_GS_AIM);return 1;
 case DROP:interaction_priority(w,a,0xd000,28);a->ay=-128;boss_start(q,BA_GS_DROP);return 1;
 case DECEL:a->ay=word(-(a->vy>>4));boss_start(q,BA_GS_FLY);return 1;
 case FLY:a->vy=a->ay=0;return 1;
 case DROP_B:
  interaction_priority(w,a,0xd000,28);a->ay=-128;boss_attack_binding(q,BB_GS_B_HIGH_HIT);boss_defense(q,BB_GS_B_HIGH);
  q->gunship.offset=40;transform_integer_offset(&a->transform,0,-40);boss_start(q,BA_GS_DROP_B);return 1;
 case RELOAD:
  transform_integer_offset(&a->transform,0,q->gunship.offset);boss_attack_binding(q,BB_GS_B_LOW_HIT);boss_defense(q,BB_GS_B_LOW);
  q->gunship.offset=8;transform_integer_offset(&a->transform,0,-8);a->ay=-128;boss_start(q,BA_GS_FLY);return 1;
 case GROUND:a->ay=a->vy=0;return 1;
 case EXPLODE_OFFSET:transform_integer_offset(&a->transform,0,q->gunship.offset);/* FALLTHROUGH */
 case EXPLODE:interaction_priority(w,a,0x4000,28);boss_defense(q,65535);return boss_fx_transform(w,e,BFX_FIRE)?2:0;
 case SHELL:a->palette=4;boss_start(q,BA_GS_SHELL);return 1;
 case ARC_WAIT:
  a->palette=0x3b;a->phase=0;if(!interaction_sound(w,0x10b8))return 0;interaction_priority(w,a,0xd000,24);a->display_flags|=64;
  q->gunship.path.angle=0xd000;boss_start(q,BA_GS_A);q->selector=5;return 1;
 case ARC:{struct NativeEntity*n=child(w,e,CREW);if(!n)return 0;n->payload.enemy.actor.display_flags|=64;return 1;}
 case ARC_FALL:{struct EnemyActor*p=parent(w,e);if(!p)return 0;p->dead=2;a->vx=-512;return 1;}
 default:return enemy_fault(w);
 }
}
static void tick(struct EntityPool*pool,struct NativeEntity*e,void*world){
 (void)pool;struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(q->entry&&enter(w,e)!=1)return;if(w->fault||e->deleting)return;unsigned state=a->state;
 switch(state){
 case FLIGHT:{
  if(q->params[5])a->hit.flags|=64;
  q->gunship.speed=word(((int32_t)gunship_speeds[(q->gunship.path.angle>>11)&31]*q->gunship.base_speed)>>8);
  if(move(w,a,WMOVE_SYNC)<0)return;struct PathContext c={w,e};struct FlightOwner o={&c,path_field,path_call,path_sound};
  int r=flight_step(&a->transform,&q->gunship.path,q->gunship.speed,&gunship_flight_program,&o);if(r<0){enemy_fault(w);return;}if(r)enemy_schedule(q,DESTROY);
  animate(w,e);if(a->hit.flags&8){a->hit.flags&=247;boss_start(q,BA_GS_HIT);if(!(a->hit.flags&1)&&(a->hit.events&1)&&a->hit.last_kind==1&&!interaction_sound(w,0x108d))return;}
  if(a->hit.flags&1)enemy_schedule(q,DIVE);outside(w,q,0,RETIRE);return;
 }
 case DIVE_FRAME:{
  int r=move(w,a,WMOVE_FALL);if(r<0)return;if(r)enemy_schedule(q,DESTROY);animate(w,e);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return;
  if(q->params[3]&&sy(w,a)<=288)enemy_schedule(q,WATER);outside(w,q,0,RETIRE);return;
 }
 case CREW:{
  if(!follow(w,e,0))return;struct EnemyActor*p=parent(w,e);if(!p)return;if(p->actor.hit.events&1)a->hit.events|=1;
  if(p->actor.phase==1)enemy_schedule(q,DELETE);animate(w,e);return;
 }
 case GUN:case AIM:{
  if(!follow(w,e,1))return;int done=animate(w,e);struct EnemyActor*p=parent(w,e);if(!p)return;
  if(state==GUN&&!p->gunship.hold)enemy_schedule(q,AIM);if(state==AIM&&done)enemy_schedule(q,q->params[6]?DROP_B:DROP);
  if(p->actor.phase==1)enemy_schedule(q,DELETE);outside(w,q,0,DELETE);return;
 }
 case DROP:
  if(move(w,a,WMOVE_FALL)<0)return;if(clearance(w,q,0))enemy_schedule(q,DECEL);animate(w,e);gun_tail(w,q,0);return;
 case DECEL:case FLY:
  if(state==DECEL&&q->params[5])a->hit.flags|=64;if(move(w,a,WMOVE_POINT)<0)return;interaction_body_contact(w,a,&gunship_contact);
  if(state==DECEL&&clearance(w,q,1))enemy_schedule(q,FLY);speed_limit(q);animate(w,e);gun_tail(w,q,0);return;
 case DROP_B:{
  int16_t vy=a->vy;int32_t y=a->transform.y;int r=move(w,a,WMOVE_BODY);if(r<0)return;
  if(r){a->ay=-128;a->vy=vy;a->transform.y=y;if(move(w,a,WMOVE_POINT)<0)return;enemy_schedule(q,RELOAD);}
  transform_integer_offset(&a->transform,0,q->gunship.offset);animate(w,e);transform_integer_offset(&a->transform,0,word(-q->gunship.offset));gun_tail(w,q,1);return;
 }
 case RELOAD:case GROUND:{
  if(q->params[5])a->hit.flags|=64;int r=move(w,a,state==RELOAD?WMOVE_BODY:WMOVE_GROUND);if(r<0)return;if(state==RELOAD&&r)enemy_schedule(q,GROUND);
  speed_limit(q);transform_integer_offset(&a->transform,0,q->gunship.offset);animate(w,e);transform_integer_offset(&a->transform,0,word(-q->gunship.offset));gun_tail(w,q,1);return;
 }
 case SHELL:if(move(w,a,WMOVE_SYNC)<0)return;if(animate(w,e))enemy_schedule(q,DELETE);return;
 case ARC_WAIT:q->selector--;if((int16_t)q->selector<=0)enemy_schedule(q,ARC);return;
 case ARC:{
  struct EnemyActor*p=parent(w,e);if(!p)return;unsigned angle=q->gunship.path.angle>>8;
  transform_integer_offset(&a->transform,word((p->actor.transform.x>>8)+boss_trig[1][angle]-(a->transform.x>>8)),word(transform_y_up(&p->actor.transform)+256+boss_trig[0][angle]-transform_y_up(&a->transform)));
  if(sx(w,a)<=320)animate(w,e);q->gunship.path.angle=(uint16_t)(q->gunship.path.angle-128);
  if(q->gunship.path.angle>>8==192)p->dead=1;if((q->gunship.path.angle>>8)<=128||sx(w,a)<=-32)enemy_schedule(q,ARC_FALL);return;
 }
 case ARC_FALL:if(move(w,a,WMOVE_POINT)<0)return;animate(w,e);if(sx(w,a)<=-32)enemy_schedule(q,DELETE);return;
 default:enemy_fault(w);return;
 }
}
int gunship_spawn(struct WeaponWorld*w,struct EntityRef parent_ref,enum EntityInsert order,const struct PlayerTransform*t,const uint8_t params[8]){
 struct NativeEntity*e=enemy_allocate(w,parent_ref,order,t,tick);if(!e)return 0;
 struct EnemyActor*q=&e->payload.enemy;q->gunship=(__typeof__(q->gunship)){.pose=255,.hold=255,.path={.angle=65535}};
 if(params)for(unsigned i=0;i<8;i++)q->params[i]=params[i];enemy_schedule(q,ROOT);return 1;
}
struct NativeEntity*gunship_arc(struct WeaponWorld*w,struct EntityRef parent_ref,const struct PlayerTransform*t){
 struct NativeEntity*e=enemy_allocate(w,parent_ref,ENTITY_FIRST_CHILD,t,tick);if(!e)return 0;
 e->payload.enemy.gunship=(__typeof__(e->payload.enemy.gunship)){.pose=255,.hold=255};enemy_schedule(&e->payload.enemy,ARC_WAIT);return e;
}
