#include "carrier_internal.h"
/* $07DFC4..$080088: three constructors, staged body and typed scheduler.
 * No scene names are inferred from the source's descriptive labels. */
static void tick(struct EntityPool*,struct NativeEntity*,void*);
void cr_schedule(struct NativeEntity*e,unsigned state){e->tick=tick;enemy_schedule(&e->payload.enemy,state);}
uint16_t cr_random(struct WeaponWorld*w){if(!w->rng){enemy_fault(w);return 0;}return source_random_word(w->rng);}
void cr_reset(struct WeaponActor*a){a->vx=a->vy=a->ax=a->ay=0;transform_integer_copy(&a->transform,&a->transform);}
void cr_screen(struct WeaponWorld*w,struct WeaponActor*a,int x,int y){transform_integer_offset(&a->transform,cr_word(x-cr_sx(w,a)),cr_word(y-cr_sy(w,a)));}
int cr_hit_sound(struct WeaponWorld*w,struct WeaponActor*a){return (a->hit.flags&1)||!(a->hit.events&1)||a->hit.last_kind!=1||interaction_sound(w,0x108d);}
int16_t cr_rank(struct WeaponWorld*w,struct EnemyActor*q,unsigned n){
 struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid||n>=CRR_COUNT){enemy_fault(w);return 0;}
 const struct CarrierRankData*t=&carrier_rank[n];unsigned i=(interaction_rank(w,&q->actor,runtime_owner_carrier[n]))*8+(q->source_flags&3)*2;
 if(t->choices)i=t->choices[i+((uint8_t)(r->player_count_flag-1)&1)]*8+((cr_random(w)&14)>>1);else i+=(r->player_count_flag&2)>>1;return (int16_t)t->values[i];
}
void cr_start_entry(struct EnemyActor*q,uint16_t n){animation_begin(&q->actor.animation,n);q->actor.movement_flags|=32;}
void cr_start(struct EnemyActor*q,enum CarrierAnimation n){cr_start_entry(q,carrier_entries[n]);}
void cr_defense(struct EnemyActor*q,unsigned n){q->actor.defense=n==65535?0:carrier_bindings[n].defense;q->actor.defense_count=n==65535?0:carrier_bindings[n].count;}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read_field(void*ctx,unsigned f,unsigned width){
 struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;
 if(width==1){if(f==CRF_3B_1)return q->actor.animation.pose;if(f==CRF_3A_1)return q->actor.transform.facing;if(f==CRF_20_1)return q->actor.phase;}
 if(width==2){if(f==CRF_78_2)return (uint16_t)q->carrier_part.offset;if(f==CRF_70_2)return (uint16_t)q->carrier_crew.goal;}
 enemy_fault(c->w);return 0;
}
static int field(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){
 struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;
 if(op==AN_FIELD_XOR)v^=read_field(ctx,f,width);else if(op!=AN_FIELD_SET)return enemy_fault(c->w);
 if(width==1){if(f==CRF_3B_1)q->actor.animation.pose=(uint8_t)v;else if(f==CRF_3A_1)q->actor.transform.facing=(uint8_t)v;else if(f==CRF_20_1)q->actor.phase=(uint8_t)v;else return enemy_fault(c->w);}
 else if(width==2){if(f==CRF_78_2)q->carrier_part.offset=(int16_t)v;else if(f==CRF_70_2)q->carrier_crew.goal=(int16_t)v;else return enemy_fault(c->w);}
 else return enemy_fault(c->w);return 1;
}
static int delay(void*ctx,unsigned f,unsigned pose,uint8_t*value){struct Context*c=ctx;unsigned row=c->e->payload.enemy.carrier_part.delay_row;
 if(f!=CRF_5C_4||row>=4||pose>=6)return enemy_fault(c->w);*value=carrier_hatch_delays[row][pose];return 1;
}
static int bind(void*ctx,unsigned kind,uint32_t value){
 struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;unsigned n=value==0xffffffffu?65535:value;
 if(n!=65535&&n>=sizeof(carrier_bindings)/sizeof(*carrier_bindings))return enemy_fault(c->w);
 if(kind==AN_DEFENSE)cr_defense(q,n);
 else if(kind==AN_ATTACK){if(n==65535)q->actor.attack=0;else{q->attack_set=(struct HitAttackSet){carrier_bindings[n].attack,carrier_bindings[n].count};q->actor.attack=&q->attack_set;}hit_begin_attack(&q->actor.hit);}
 else return enemy_fault(c->w);return 1;
}
static int sound(void*ctx,uint16_t s,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,s,stop)?1:enemy_fault(c->w);}
static int call(void*ctx,unsigned n,struct Animation*anim){
 (void)anim;struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(n==CRC_4A110){a->palette=0x15c;return 1;}
 if(n==CRC_55BCE)return enemy_spawn_ordnance(c->w,entity_ref(c->e),&a->transform,ENEMY_GRENADE,a->variant,q->source_flags);
 if(n==CRC_29CB32){
  a->vx=a->transform.facing&1?256:-256;a->vy=510;a->ay=-34;a->ax=0;
  /* The source consumes RNG then MOVE.W #$7F, not AND.W: always +64. */
  cr_random(c->w);a->vx=cr_word(a->vx+64);return !c->w->fault;
 }return enemy_fault(c->w);
}
int cr_animate(struct WeaponWorld*w,struct NativeEntity*e){
 struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;
 struct Context c={w,e};struct AnimationOwner owner={.context=&c,.read=read_field,.write=field,.bind=bind,.call=call,.sound=sound,.delay=delay};
 int done=animation_step(&a->animation,&carrier_program,&owner);if(a->animation.fault)enemy_fault(w);return done;
}
struct EnemyActor*cr_parent(struct WeaponWorld*w,struct NativeEntity*e){
 struct NativeEntity*p=entity_get(w->entities,e->parent);if(!p||p->kind!=ENTITY_ENEMY){enemy_fault(w);return 0;}return &p->payload.enemy;
}
struct EnemyActor*cr_grandparent(struct WeaponWorld*w,struct NativeEntity*e){struct NativeEntity*p=entity_get(w->entities,e->parent);if(!p){enemy_fault(w);return 0;}return cr_parent(w,p);}
static void init(struct NativeEntity*e,unsigned state){
 /* Each semantic variant occupies the same scheduler payload; source work
  * words start at FF unless its constructor explicitly initializes them. */
 uint8_t*b=(uint8_t*)&e->payload.enemy.carrier;for(unsigned i=0;i<sizeof(e->payload.enemy.carrier);i++)b[i]=255;cr_schedule(e,state);
}
struct NativeEntity*cr_child(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct NativeEntity*n=enemy_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,tick);if(n)init(n,state);return n;
}
int cr_follow(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*p=cr_parent(w,e);if(!p)return 0;struct WeaponActor*a=&e->payload.enemy.actor;
 transform_integer_offset(&a->transform,cr_word((p->actor.transform.x>>8)-(a->transform.x>>8)),cr_word(transform_y_up(&p->actor.transform)-transform_y_up(&a->transform)));return 1;
}
int cr_parent_dead(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*p=cr_parent(w,e);return p&&p->dead==255;}
void cr_parent_tail(struct WeaponWorld*w,struct NativeEntity*e,int mark){if(cr_parent_dead(w,e))cr_schedule(e,mark?CR_MARK_DELETE:CR_DELETE);}
void cr_bob(struct EnemyActor*q){
 q->carrier.bob_angle=(q->carrier.bob_angle+2)&127;int16_t dy=(int16_t)((boss_trig[0][q->carrier.bob_angle]*4)>>8);
 transform_integer_offset(&q->actor.transform,0,cr_word(dy-q->carrier.bob_y));q->carrier.bob_y=dy;
}
int cr_platforms(struct WeaponWorld*w,struct WeaponActor*a,int cockpit){
 if(!w->interactions||!w->interactions->terrain)return enemy_fault(w);
 static const int8_t offsets[2][3][3]={{{-88,32,48},{56,32,48},{-48,8,112}},{{-40,48,40},{-56,0,88},{16,16,32}}};
 for(unsigned i=0;i<3;i++){const int8_t*r=offsets[cockpit][i];struct TerrainMotion m={.platform_ids=a->platform_ids};
  if(!terrain_platform_publish(w->interactions->terrain,&m,cr_word((a->transform.x>>8)+r[0]),cr_word(transform_y_up(&a->transform)+r[1]),r[2],1))return enemy_fault(w);a->platform_ids=m.platform_ids;
 }return 1;
}
static void body_ai(struct WeaponWorld*w,struct EnemyActor*q){
 struct WeaponActor*a=&q->actor;int direction=0;int16_t x=cr_sx(w,a);
 if(q->carrier.initial_hp!=-1){
  a->hit.hp=q->carrier.initial_hp;
  if(q->params[5]&&x<80)direction=1;else if(!q->params[5]&&x>240)direction=-1;
  else{q->carrier.initial_hp=-1;cr_defense(q,CRB_BODY);}
 }
 if(!direction){int i=enemy_nearest_x(w,a,0);if(i>=0){int16_t distance=cr_word((a->transform.x>>8)-(w->players[i]->actor.transform.x>>8));
   if(distance<96){if(q->params[1]*16>x)direction=1;}
   else if(distance>144&&q->params[0]*16<x)direction=-1;
  }}
 if(direction<0){a->vx=cr_word(a->vx-2);if(a->vx<=-288)a->vx=-288;q->carrier.period=24;q->carrier.direction=1;}
 else if(direction>0){a->vx=cr_word(a->vx+2);if(a->vx>=288)a->vx=288;q->carrier.period=48;q->carrier.direction=2;}
 else{q->carrier.period=36;if(a->vx<0){q->carrier.direction=1;a->vx=cr_word(a->vx+6);if(a->vx>=0){cr_reset(a);q->carrier.direction=0;}}
  else{q->carrier.direction=2;a->vx=cr_word(a->vx-6);if(a->vx<0){cr_reset(a);q->carrier.direction=0;}}}
}
static int body_enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;q->entry=0;struct NativeEntity*n;
 if(a->state==CR_ROOT){
  a->phase=q->dead=0;cr_reset(a);a->transform.priority=0x5000;a->hit.hp=cr_rank(w,q,q->carrier.variant?CRR_HP_B:CRR_HP);
  if(!cr_child(w,e,CR_INTRO_GATE))return 0;cr_schedule(e,CR_BODY);return 0;
 }
 if(a->state==CR_BODY){
  n=cr_child(w,e,CR_SPAWNER);if(!n)return 0;n->payload.enemy.carrier_spawner.guards=q->carrier.with_guards;
  q->carrier.period=36;q->carrier.clock=q->carrier.bob_angle=q->carrier.bob_y=q->carrier.cockpit_x=q->carrier.direction=0;
  q->carrier.cockpit_y=60;q->carrier.initial_hp=a->hit.hp;cr_defense(q,CRB_BODY_INTRO);
  for(unsigned i=0;i<3;i++){n=cr_child(w,e,CR_HULL);if(!n)return 0;n->payload.enemy.carrier_part.side=(uint8_t)i;}
  if(!cr_child(w,e,CR_HATCH)||!cr_child(w,e,CR_MARK_LEFT)||!cr_child(w,e,CR_MARK_RIGHT))return 0;
 }return !w->fault;
}
static void body_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(state==CR_INTRO_GATE){
  struct EnemyActor*p=cr_parent(w,e);if(!p)return;
  /* Native off-screen retirement can end the parent before its phase2
   * handshake. A retired attachment must not lock a later camera gate. */
  if(p->dead==255||entity_parent_deleting(w->entities,e)){entity_delete(w->entities,e);return;}
  w->enemies->intro_gate=0;if(p->dead==2)entity_delete(w->entities,e);return;
 }
 if(state==CR_BODY){
  body_ai(w,q);if(interaction_move(w,a,WMOVE_POINT)<0||!cr_platforms(w,a,0))return;
  q->carrier.clock=cr_word(q->carrier.clock+1);
  if(q->carrier.clock>=q->carrier.period){q->carrier.clock=carrier_cannon_clocks[cr_random(w)&3];struct NativeEntity*n=cr_child(w,e,CR_CANNON);if(!n)return;transform_integer_offset(&n->payload.enemy.actor.transform,56,72);}
  if(a->hit.flags&8){if(!cr_hit_sound(w,a))return;a->hit.flags&=247;}cr_bob(q);
  if(a->hit.flags&1){a->phase=3;q->carrier.cockpit_y=cr_word(q->carrier.cockpit_y-1);if(!q->carrier.cockpit_y){
    q->dead=1;q->carrier.bob_y=0;cr_defense(q,65535);
    if(!cr_child(w,e,CR_GUNNERS)||!cr_child(w,e,CR_COCKPIT))return;
    if(!item_spawn_drop(w,&a->transform,q->params[2],q->params[3],q->params[4],1,0))return;cr_schedule(e,CR_PHASE4);
   }}
 }else if(state==CR_PHASE4){
  if(interaction_move(w,a,WMOVE_SYNC)<0)return;if(a->phase==4){a->display_flags&=253;q->dead=2;cr_schedule(e,CR_PHASE5);}cr_bob(q);
 }else if(state==CR_PHASE5){
  if(a->phase==5){q->dead=3;cr_defense(q,65535);cr_schedule(e,CR_WAIT);}if(interaction_move(w,a,WMOVE_SYNC)<0)return;cr_bob(q);
 }else if(state==CR_WAIT){if(q->dead==255)cr_schedule(e,CR_DELETE);}
 else{enemy_fault(w);return;}
 if(interaction_outside(w,a,carrier_bounds[0],0))cr_schedule(e,CR_MARK_DELETE);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;
 for(unsigned budget=0;budget<8;budget++){
  unsigned state=q->actor.state;
  if(state==CR_MARK_DELETE||state==CR_DELETE){if(state==CR_MARK_DELETE)q->dead=255;entity_delete(p,e);return;}
  if(q->entry){int r=state<=CR_INTRO_GATE?body_enter(w,e):state<CR_SPAWNER?cr_parts_enter(w,e):cr_crew_enter(w,e);if(!r||w->fault)return;if(r==2)continue;}
  if(!e->active||e->deleting)return;state=q->actor.state;
  if(state<=CR_INTRO_GATE){body_frame(w,e,state);return;}
  int r=state<CR_SPAWNER?cr_parts_frame(w,e,state):cr_crew_frame(w,e,state);if(r!=2)return;
 }enemy_fault(w);
}
int carrier_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){
 if(id<129||id>131)return enemy_fault(w);struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;init(e,CR_ROOT);
 struct EnemyActor*q=&e->payload.enemy;q->carrier.variant=id==131?255:0;q->carrier.with_guards=id==130;
 if(params)for(unsigned i=0;i<8;i++)q->params[i]=params[i];return 1;
}
