#include "tank_internal.h"
/* $072FC8 tank body, $073FCA missile, and the connected child scheduler.
 * Original byte/word wrap and immediate entry versus next-frame transitions
 * are explicit; no original CPU handler addresses are retained at runtime. */
static void tick(struct EntityPool*,struct NativeEntity*,void*);
uint16_t tk_random(struct WeaponWorld*w){if(!w->rng){enemy_fault(w);return 0;}return source_random_word(w->rng);}
int tk_sound(struct WeaponWorld*w,unsigned s){return interaction_sound(w,(uint16_t)s);}
int tk_hit_sound(struct WeaponWorld*w,struct WeaponActor*a){return (a->hit.flags&1)||!(a->hit.events&1)||a->hit.last_kind!=1||tk_sound(w,0x108d);}
int16_t tk_rank(struct WeaponWorld*w,struct EnemyActor*q,unsigned n){
 struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid||n>=TKR_COUNT){enemy_fault(w);return 0;}
 unsigned i=(interaction_rank(w,&q->actor,runtime_owner_tank[n]))*8+(q->source_flags&3)*2;const struct TankRankData*t=&tank_rank[n];
 if(t->choices)i=t->choices[i+((uint8_t)(r->player_count_flag-1)&1)]*8+((tk_random(w)&14)>>1);else i+=(r->player_count_flag&2)>>1;return (int16_t)t->values[i];
}
void tk_start_entry(struct EnemyActor*q,uint16_t n){if(n==65535)return;animation_begin(&q->actor.animation,n);q->actor.movement_flags|=32;}
void tk_start(struct EnemyActor*q,enum TankAnimation n){tk_start_entry(q,tank_entries[n]);}
void tk_defense(struct EnemyActor*q,unsigned n){q->actor.defense=n==65535?0:tank_bindings[n].defense;q->actor.defense_count=n==65535?0:tank_bindings[n].count;}
void tk_attack(struct EnemyActor*q,unsigned n){if(n==65535)q->actor.attack=0;else{q->attack_set=(struct HitAttackSet){tank_bindings[n].attack,tank_bindings[n].count};q->actor.attack=&q->attack_set;}}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read_field(void*ctx,unsigned f,unsigned width){
 struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;
 if(width==2){if(f==TKF_34_2||f==TKF_7E_2)return q->selector;if(f==TKF_7C_2)return (uint16_t)q->tank.priority;}
 if(width==1){switch(f){
 case TKF_76_1:return q->family?q->bazooka.offset_x:q->tank.pose;
 case TKF_77_1:return q->family?q->bazooka.offset_y:(uint8_t)q->tank.offset_x;
 case TKF_74_1:return q->family?q->bazooka.count:q->tank.drive_pose;
 case TKF_75_1:return q->family?q->bazooka.direction:q->tank.stage;
 case TKF_7A_1:return q->family?q->bazooka.variant:q->tank.height_a;
 case TKF_7B_1:return q->family?q->bazooka.target_clock:q->tank.height_b;
 case TKF_9B_1:return q->params[3];default:break;}}
 enemy_fault(c->w);return 0;
}
static int write_field(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){
 struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;uint32_t old=read_field(ctx,f,width);if(c->w->fault)return 0;
 if(op==AN_FIELD_AND)v=old&v;else if(op==AN_FIELD_OR)v=old|v;else if(op==AN_FIELD_XOR)v=old^v;else if(op!=AN_FIELD_SET)return enemy_fault(c->w);
 if(width==2){if(f==TKF_34_2||f==TKF_7E_2)q->selector=(uint16_t)v;else if(f==TKF_7C_2)q->tank.priority=(int16_t)v;else return enemy_fault(c->w);return 1;}
 switch(f){
 case TKF_76_1:if(q->family)q->bazooka.offset_x=(uint8_t)v;else q->tank.pose=(uint8_t)v;break;
 case TKF_77_1:if(q->family)q->bazooka.offset_y=(uint8_t)v;else q->tank.offset_x=(int8_t)v;break;
 case TKF_74_1:if(q->family)q->bazooka.count=(uint8_t)v;else q->tank.drive_pose=(uint8_t)v;break;
 case TKF_75_1:if(q->family)q->bazooka.direction=(uint8_t)v;else q->tank.stage=(uint8_t)v;break;
 case TKF_7A_1:if(q->family)q->bazooka.variant=(uint8_t)v;else q->tank.height_a=(uint8_t)v;break;
 case TKF_7B_1:if(q->family)q->bazooka.target_clock=(uint8_t)v;else q->tank.height_b=(uint8_t)v;break;
 case TKF_9B_1:q->params[3]=(uint8_t)v;break;default:return enemy_fault(c->w);
 }return 1;
}
static int bind(void*ctx,unsigned kind,uint32_t value){struct Context*c=ctx;unsigned n=value==0xffffffffu?65535:value;
 if(n!=65535&&n>=sizeof(tank_bindings)/sizeof(*tank_bindings))return enemy_fault(c->w);
 if(kind==AN_DEFENSE)tk_defense(&c->e->payload.enemy,n);else if(kind==AN_ATTACK){tk_attack(&c->e->payload.enemy,n);hit_begin_attack(&c->e->payload.enemy.actor.hit);}else return enemy_fault(c->w);return 1;
}
static int sound(void*ctx,uint16_t s,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,s,stop)?1:enemy_fault(c->w);}
static int call(void*ctx,unsigned n,struct Animation*a){(void)a;struct Context*c=ctx;return tk_emit(c->w,c->e,n);}
int tk_animate(struct WeaponWorld*w,struct NativeEntity*e){
 struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;struct Context c={w,e};
 struct AnimationOwner owner={.context=&c,.read=read_field,.write=write_field,.bind=bind,.sound=sound,.call=call};
 int done=animation_step(&a->animation,&tank_program,&owner);if(a->animation.fault)enemy_fault(w);return done;
}
static void initialize(struct EnemyActor*q,unsigned state){
 q->family=(state>=TK_CREW&&state<=TK_BZ_SPAWNER);uint8_t*p=(uint8_t*)&q->tank;
 for(unsigned i=0;i<sizeof(q->tank);i++)p[i]=255;
 enemy_schedule(q,state);
}
struct EnemyActor*tk_parent(struct WeaponWorld*w,struct NativeEntity*e){struct NativeEntity*p=entity_get(w->entities,e->parent);if(!p||p->kind!=ENTITY_ENEMY){enemy_fault(w);return 0;}return &p->payload.enemy;}
struct NativeEntity*tk_child(struct WeaponWorld*w,struct NativeEntity*e,unsigned state,enum EntityInsert order){
 struct NativeEntity*n=enemy_allocate(w,entity_ref(e),order,&e->payload.enemy.actor.transform,tick);if(!n)return 0;initialize(&n->payload.enemy,state);return n;
}
int tk_follow(struct WeaponWorld*w,struct NativeEntity*e,int select){
 struct EnemyActor*q=&e->payload.enemy,*p=tk_parent(w,e);if(!p)return 0;
 q->actor.transform=p->actor.transform;q->actor.transform.priority=(uint16_t)(p->actor.transform.priority+q->tank.priority+(p->tank.stage>=3?2:0));
 q->tank.offset_x=p->tank.offset_x;q->tank.offset_y=p->tank.offset_y;transform_integer_offset(&q->actor.transform,q->tank.offset_x,q->tank.offset_y);
 if(select&&q->tank.pose!=p->tank.pose){
  q->tank.pose=p->tank.pose;unsigned i=q->tank.pose&31;if(i>=7||q->tank.table>=4)return enemy_fault(w);
  const uint16_t*rows[]={tank_hull,tank_cabin,tank_launcher,tank_gun};tk_start_entry(q,rows[q->tank.table][i]);
 }return 1;
}
int tk_settle(struct WeaponWorld*w,struct WeaponActor*a){
 transform_integer_offset(&a->transform,0,tk_word((tk_sy(w,a)&1023)-tk_sy(w,a)));
 for(unsigned i=0;i<2048;i++){
  if(tk_sy(w,a)<0){transform_integer_offset(&a->transform,tk_word(160-tk_sx(w,a)),tk_word(384-tk_sy(w,a)));return 1;}
  int r=interaction_move(w,a,WMOVE_ITEM);if(r<0)return 0;if(r)return 1;
 }return enemy_fault(w);
}
int tk_stamp(struct WeaponWorld*w,struct NativeEntity*e,unsigned n){
 if(n>=2||!w->interactions||!w->interactions->terrain)return enemy_fault(w);const struct TankTerrainStamp*t=&tank_stamps[n];
 return terrain_stamp(w->interactions->terrain,&e->payload.enemy.actor.transform,t->dx,t->dy,t->width,t->height,t->cells)?1:enemy_fault(w);
}
static int boundary(struct WeaponWorld*w,struct EnemyActor*q){
 int16_t x=tk_sx(w,&q->actor);if(!(q->tank.direction&1))return x<160;
 if(!w->secondary_view_valid){enemy_fault(w);return 0;}return x>280||tk_word(0x1050-w->secondary_x_offset)<x;
}
static int near(struct WeaponWorld*w,struct EnemyActor*q){
 if(q->tank.direction&1)return 0;int i=enemy_nearest_x(w,&q->actor,0);return i>=0&&tk_word((q->actor.transform.x>>8)-(w->players[i]->actor.transform.x>>8))<152;
}
static int body_enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned state=a->state;struct NativeEntity*n;q->entry=0;
 switch(state){
 case TK_ROOT:
  if(!tk_sound(w,0x28))return 0;a->palette=0xec;a->hit_palette=0x8a;tk_attack(q,TKB_IDLE_ATK);tk_defense(q,TKB_BODY_DEF);q->tank.contact=0;interaction_priority(w,a,0x4000,20);
  a->hit.hp=tk_rank(w,q,TKR_HP);q->start_y=tk_rank(w,q,TKR_THRESHOLD);if(!tk_settle(w,a))return 0;
  n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_DAMAGE_SMOKE_BODY);if(!n)return 0;n->payload.enemy.actor.attached_offset=60;
  n=tk_child(w,e,TK_HULL,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.tank.priority=0;
  n=tk_child(w,e,TK_CABIN,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.tank.priority=-1;
  n=tk_child(w,e,TK_LAUNCHER,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.tank.priority=-1;
  if(!tk_child(w,e,TK_GUN,ENTITY_FIRST_CHILD))return 0;q->tank.direction=q->tank.stage=0;q->tank.speed=320;
  if(!tk_sound(w,0x1073))return 0;a->state=TK_DRIVE;/* FALLTHROUGH */
 case TK_DRIVE:
  if(q->tank.stage){enemy_schedule(q,TK_RETREAT);return 2;}if(boundary(w,q)){enemy_schedule(q,TK_TURN);return 2;}
  a->vx=q->tank.direction&1?q->tank.speed:tk_word(-q->tank.speed);
  tk_start_entry(q,tank_drive[(q->tank.drive_pose&3)+((q->tank.direction&1)?4:0)]);tk_attack(q,TKB_MOVE_ATK);break;
 case TK_TURN:q->tank.direction^=1;a->vx=0;q->tank.wait=60;tk_start_entry(q,tank_turn[q->tank.drive_pose&7]);tk_attack(q,TKB_IDLE_ATK);break;
 case TK_RETREAT:
  if(!w->secondary_view_valid)return enemy_fault(w);a->effect_x=tk_word(0x1050-w->secondary_x_offset);q->tank.direction=1;a->vx=q->tank.speed>>1;
  if(a->effect_x<tk_sx(w,a)){q->tank.direction=0;a->vx=tk_word(-a->vx);}
  tk_start_entry(q,tank_retreat[(q->tank.drive_pose&3)+(q->tank.direction?4:0)]);tk_attack(q,TKB_MOVE_ATK);q->tank.wait=180;break;
 case TK_PAUSE:q->tank.wait=20;q->tank.stage=2;tk_attack(q,TKB_IDLE_ATK);break;
 case TK_PHASE2:if(!tk_sound(w,0x10a7))return 0;tk_start(q,TKA_PHASE2);break;
 case TK_PHASE3:
  a->timer=(uint16_t)tk_rank(w,q,TKR_MINIMUM);q->tank.watch=0;
  if(!mission_event_spawn(w,entity_ref(e),0xee1b4,tk_watch_stop))return 0;break;
 case TK_DIE:
  if(!tk_stamp(w,e,1))return 0;w->enemies->damage_enabled=0;if(!tk_sound(w,0x1033))return 0;q->tank.stage=6;tk_start(q,TKA_DIE);break;
 case TK_SINK:tk_start(q,TKA_SINK);break;
 case TK_EXPLODE:
  transform_integer_offset(&a->transform,0,32);if(!tk_sound(w,0x102f))return 0;
  for(int x=-56;x<=56;x+=56){n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMOKE);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,(int16_t)x,0);}
  for(unsigned i=0;i<16;i++){n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_CRASH_FRAGMENT);if(!n)return 0;n->payload.enemy.params[3]=q->params[3];}
  for(unsigned i=0;i<17;i++){n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_METAL_HEAVY);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,0,24);}
  transform_integer_offset(&a->transform,0,-32);tk_animate(w,e);entity_delete(w->entities,e);return 0;
 default:return enemy_fault(w);
 }return !w->fault;
}
static int prevent_death(struct WeaponWorld*w,struct WeaponActor*a){if(a->hit.flags&1){a->hit.hp=20;a->hit.flags&=254;a->hit.events|=1;return tk_hit_sound(w,a);}return 1;}
static void body_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(state==TK_DIE||state==TK_SINK){
  if(interaction_move(w,a,WMOVE_SYNC)<0)return;if(tk_animate(w,e)){if(state==TK_DIE)w->enemies->scripted_player_lock=0;enemy_schedule(q,state==TK_DIE?TK_SINK:TK_EXPLODE);}
  if(state==TK_SINK)q->tank.watch=255;return;
 }
 if(state==TK_PAUSE&&--q->tank.wait<=0)enemy_schedule(q,TK_PHASE2);
 if(q->tank.contact<3)interaction_body_contact(w,a,&tank_contacts[q->tank.contact]);
 int r=interaction_move(w,a,state==TK_DRIVE||state==TK_RETREAT?WMOVE_FLAT:WMOVE_SYNC);if(r<0)return;
 if(state==TK_DRIVE&&(r||boundary(w,q)||near(w,q)))enemy_schedule(q,TK_TURN);
 if(state==TK_RETREAT&&tk_word(a->effect_x-16)<tk_sx(w,a)&&tk_word(a->effect_x+16)>tk_sx(w,a))enemy_schedule(q,TK_PAUSE);
 int done=tk_animate(w,e);
 if(state==TK_TURN&&--q->tank.wait<=0)enemy_schedule(q,TK_DRIVE);
 if(state==TK_RETREAT&&--q->tank.wait<=0)enemy_schedule(q,TK_PAUSE);
 if((state==TK_PAUSE||state==TK_PHASE2)&&done)enemy_schedule(q,TK_PHASE3);
 if(a->hit.flags&8){
  a->hit.flags&=247;if(!tk_hit_sound(w,a))return;
  if(state<=TK_RETREAT&&!q->tank.stage&&a->hit.hp<=q->start_y){q->tank.stage=1;
   if(!boss_fx_pattern(w,entity_ref(e),&a->transform,BP_TANK_HIT)||!tk_sound(w,0x1032))return;enemy_schedule(q,TK_RETREAT);}
 }
 if(state==TK_PHASE3){
  int16_t wait=tk_word(a->timer-1);if(wait< -1)wait=-1;a->timer=(uint16_t)wait;
  if(a->hit.flags&1){if(wait<=0)enemy_schedule(q,TK_DIE);else{a->hit.flags&=254;a->hit.hp=20;}}
 }else if(!prevent_death(w,a))return;
 hit_begin_attack(&a->hit);enemy_attack(w,a);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;
 for(unsigned budget=0;budget<8;budget++){
  unsigned state=q->actor.state;
  if(state==TK_DELETE||state==TK_DELETE_TREE){if(state==TK_DELETE_TREE)entity_delete_children(p,e);entity_delete(p,e);return;}
  if(q->entry){int r=state<=TK_EXPLODE?body_enter(w,e):state>=TK_BZ_A_NEW?bz_enter(w,e):state>=TK_CREW?tk_crew_enter(w,e):tk_parts_enter(w,e);if(!r||w->fault)return;if(r==2)continue;}
  if(!e->active||e->deleting)return;state=q->actor.state;
  if(state<=TK_EXPLODE){body_frame(w,e,state);return;}
  int r=state>=TK_BZ_A_NEW?bz_frame(w,e,state):state>=TK_CREW?tk_crew_frame(w,e,state):tk_parts_frame(w,e,state);if(r!=2)return;
 }enemy_fault(w);
}
int tank_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){
 if(id!=118&&id!=119)return enemy_fault(w);struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;initialize(&e->payload.enemy,id==118?TK_ROOT:TK_MISSILE);
 if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];return 1;
}

/* $06AA14: standalone scene ally; uses its own body loop and shared weapon. */
struct NativeEntity*tank_ally_spawn(struct WeaponWorld*w,struct EntityRef parent,const struct PlayerTransform*t){
 struct NativeEntity*e=enemy_allocate(w,parent,ENTITY_FIRST_CHILD,t,tick);if(!e)return 0;
 initialize(&e->payload.enemy,TK_ALLY);e->payload.enemy.bazooka.ally_defense=0;return e;
}

/* Eight independently callable templates; scene allies retain parent phase. */
int bazooka_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8],uint8_t flags){
 if(id!=64&&id!=65&&id!=86&&id!=87&&(id<90||id>93))return enemy_fault(w);
 unsigned state=id==64?TK_ALLY:id==65?TK_BZ_SPAWNER:id<90?TK_BZ_A_NEW:TK_BZ_B_NEW;
 struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;initialize(&e->payload.enemy,state);struct EnemyActor*q=&e->payload.enemy;
 q->source_flags=flags;q->bazooka.attack_kind=id==87?0:1;q->bazooka.ally_defense=0;
 if(params)for(unsigned i=0;i<8;i++)q->params[i]=params[i];
 if(id>=90){q->params[4]=(id==90||id==92);q->params[6]=id<92;}
 return 1;
}

/* Shared tail restores this controller after an ejection override. */
void tk_schedule_crew_death(struct NativeEntity*e){e->tick=tick;enemy_schedule(&e->payload.enemy,TK_CREW_DIE);}
