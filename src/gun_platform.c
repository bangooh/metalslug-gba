#include "tank_internal.h"
#include "m4_internal.h"
#include "items.h"
#include "../generated/gun-platform-programs.h"
/* $04BB9A..$04CAA8. Four constructors share the exact parent phase protocol;
 * each shield retains its own HP and each shooter its own death controller. */
enum State {NEW,SPAWN,IDLE,OPEN,AIM,REARM,FIRE,DESTROY,DESTROY_WRECK,WRECK_ANIM,WRECK,
 HATCH,HATCH_WAIT,HATCH_OPEN,HATCH_FIRE,HATCH_HOLD,HATCH_CLOSE,SHIELD,SHIELD_FIRE,
 RIDER_A,RIDER_B,RIDER_IDLE,RIDER_ALERT,RIDER_FIRE,RIDER_HOLD,RIDER_RETURN,
 GUN,GUN_DAMAGED,GUN_B,SHOT,SHOT_SLOW,SMOKE,BLAST,DELETE_TREE,DELETE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static void schedule(struct NativeEntity*e,unsigned s){e->tick=tick;enemy_schedule(&e->payload.enemy,s);}
static void init(struct NativeEntity*e,unsigned s){uint8_t*p=(uint8_t*)&e->payload.enemy.gun_platform;for(unsigned i=0;i<sizeof(e->payload.enemy.gun_platform);i++)p[i]=255;schedule(e,s);}
static struct NativeEntity*child(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct NativeEntity*n=enemy_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,tick);if(n)init(n,s);return n;}
static struct EnemyActor*parent(struct WeaponWorld*w,struct NativeEntity*e){return tk_parent(w,e);}
static void start_entry(struct EnemyActor*q,uint16_t n){if(n==65535)return;animation_begin(&q->actor.animation,n);q->actor.movement_flags|=32;}
static void start(struct EnemyActor*q,unsigned n){start_entry(q,gun_platform_entries[n]);}
static void defense(struct EnemyActor*q,unsigned n){q->actor.defense=n==65535?0:gun_platform_bindings[n].defense;q->actor.defense_count=n==65535?0:gun_platform_bindings[n].count;}
static void attack(struct EnemyActor*q,unsigned n){if(n==65535)q->actor.attack=0;else{q->attack_set=(struct HitAttackSet){gun_platform_bindings[n].attack,gun_platform_bindings[n].count};q->actor.attack=&q->attack_set;}}
static int16_t rank(struct WeaponWorld*w,struct EnemyActor*q,unsigned n){struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid||n>=GPR_COUNT){enemy_fault(w);return 0;}return (int16_t)gun_platform_rank[n][(interaction_rank(w,&q->actor,runtime_owner_gun_platform[n]))*8+(q->source_flags&3)*2+((r->player_count_flag&2)>>1)];}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;};
static int bind(void*ctx,unsigned kind,uint32_t v){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;unsigned n=v==0xffffffffu?65535:v;if(n!=65535&&n>=sizeof(gun_platform_bindings)/sizeof(*gun_platform_bindings))return enemy_fault(c->w);if(kind==AN_DEFENSE)defense(q,n);else if(kind==AN_ATTACK){attack(q,n);hit_begin_attack(&q->actor.hit);}else return enemy_fault(c->w);return 1;}
static int sound(void*ctx,uint16_t s,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,s,stop)?1:enemy_fault(c->w);}
static int call(void*ctx,unsigned n,struct Animation*a){(void)a;struct Context*c=ctx;struct NativeEntity*v;switch(n){
 case GPC_283CA:hit_begin_attack(&c->e->payload.enemy.actor.hit);return 1;
 case GPC_49472:case GPC_494C8:return free104_level_shot(c->w,c->e,n==GPC_494C8);
 case GPC_4CA58:if(!interaction_sound(c->w,0x1067))return 0;v=child(c->w,c->e,SMOKE);if(!v)return 0;transform_integer_offset(&v->payload.enemy.actor.transform,0,-32);return 1;
 case GPC_4CA7C:v=child(c->w,c->e,SHOT_SLOW);if(!v)return 0;transform_integer_offset(&v->payload.enemy.actor.transform,-32,4);return 1;
 default:return enemy_fault(c->w);
}}
static int animate(struct WeaponWorld*w,struct NativeEntity*e){struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;struct Context c={w,e};struct AnimationOwner o={.context=&c,.bind=bind,.sound=sound,.call=call};int done=animation_step(&a->animation,&gun_platform_program,&o);if(a->animation.fault)enemy_fault(w);return done;}
static int stamp(struct WeaponWorld*w,struct EnemyActor*q,unsigned i){if(i>=5)return enemy_fault(w);const struct GunPlatformStamp*t=&gun_platform_stamps[i];return terrain_stamp(w->interactions->terrain,&q->actor.transform,t->dx,t->dy,t->width,t->height,t->cells)?1:enemy_fault(w);}
static int outside(struct WeaponWorld*w,struct EnemyActor*q){return interaction_outside(w,&q->actor,gun_platform_bounds,0);}
static int pattern(struct WeaponWorld*w,struct NativeEntity*e,unsigned n){return boss_fx_pattern(w,entity_ref(e),&e->payload.enemy.actor.transform,(enum BossPattern)n)!=0;}
static int drop(struct WeaponWorld*w,struct EnemyActor*q){return item_spawn_drop(w,&q->actor.transform,q->params[0],q->params[1],q->params[2],1,0);}
static void palette(struct EnemyActor*q){q->actor.palette=0x84;q->actor.hit_palette=0x8d;}
static void integer_follow(struct EnemyActor*q,struct EnemyActor*p,int dy){struct WeaponActor*a=&q->actor;transform_integer_offset(&a->transform,tk_word((p->actor.transform.x>>8)-(a->transform.x>>8)),tk_word(transform_y_up(&p->actor.transform)+dy-transform_y_up(&a->transform)));}
static void inherit_hit(struct EnemyActor*q,struct EnemyActor*p){if(p->actor.hit.events&128)q->actor.hit.events|=1;}
static void parent_tail(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*p=parent(w,e);if(!p)return;if(p->actor.phase==255)schedule(e,DELETE);if(outside(w,&e->payload.enemy))schedule(e,DELETE);}
static int body_hit(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(a->phase==255){if(!stamp(w,q,4))return 0;schedule(e,DESTROY_WRECK);return 1;}
 q->gun_platform.hit=0;
 if(a->hit.flags&8){q->gun_platform.hit=1;a->hit.flags&=247;if(q->dead!=1&&a->hit.hp<50){q->dead=1;if(!interaction_sound(w,0x1027)||!pattern(w,e,a->hit.last_kind==1?BP_PLATFORM_HIT_A:BP_PLATFORM_HIT_B)||!pattern(w,e,BP_PLATFORM_HIT_C))return 0;}}
 if(a->hit.flags&1)schedule(e,DESTROY);return !w->fault;
}
static int select_body(struct WeaponWorld*w,struct EnemyActor*q,int shield,unsigned damaged){if(damaged>=2||q->gun_platform.variant>=4)return enemy_fault(w);const uint16_t*row=shield?(damaged?gun_platform_shield1:gun_platform_shield0):(damaged?gun_platform_body1:gun_platform_body0);start_entry(q,row[q->gun_platform.variant]);return 1;}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*n;unsigned s=a->state;int16_t speed;
 switch(s){
 case NEW:
  palette(q);interaction_priority(w,a,0x4000,8);a->phase=q->dead=0;a->hit.hp=rank(w,q,GPR_HP);m4_reset(a);if(!tk_settle(w,a)||!stamp(w,q,q->gun_platform.variant))return 0;
  n=child(w,e,GUN);if(!n)return 0;n->payload.enemy.gun_platform.variant=q->gun_platform.variant;schedule(e,SPAWN);return 0;
 case SPAWN:{unsigned v=q->gun_platform.variant;
  if(gun_platform_parts[2][v]){n=child(w,e,gun_platform_parts[2][v]==1?RIDER_A:RIDER_B);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,0,24);}
  if(gun_platform_parts[0][v]){n=child(w,e,HATCH);if(!n)return 0;n->payload.enemy.gun_platform.variant=v;n->payload.enemy.gun_platform.offset_y=gun_platform_hatch_y[v];}
  if(gun_platform_parts[1][v]){n=child(w,e,SHIELD);if(!n)return 0;n->payload.enemy.gun_platform.variant=v;}schedule(e,IDLE);return 2;
 }
 case IDLE:q->gun_platform.idle=rank(w,q,GPR_IDLE);q->gun_platform.serial=0;a->phase=0;q->gun_platform.wait=0;break;
 case OPEN:q->gun_platform.repeats=(uint8_t)rank(w,q,GPR_REPEATS);q->gun_platform.interval=rank(w,q,GPR_INTERVAL);q->gun_platform.clock=0;q->gun_platform.wait=30;a->phase=1;break;
 case REARM:q->gun_platform.wait=0;a->state=AIM;break;
 case AIM:break;
 case FIRE:q->gun_platform.wait=27;break;
 case DESTROY:
  q->gun_platform.hit=0;if(!drop(w,q)||!interaction_sound(w,0x1023)||!pattern(w,e,BP_PLATFORM_DEATH_A)||!pattern(w,e,BP_PLATFORM_DEATH_B))return 0;q->gun_platform.wait=10;break;
 case DESTROY_WRECK:
  q->gun_platform.hit=0;if(!drop(w,q))return 0;a->display_flags&=253;
  if(gun_platform_wreck[q->gun_platform.variant]==65535){schedule(e,DESTROY);return 2;}start_entry(q,gun_platform_wreck[q->gun_platform.variant]);
  n=child(w,e,WRECK);if(!n)return 0;n->payload.enemy.actor.transform.priority=a->transform.priority-8;a->state=WRECK_ANIM;break;
 case WRECK:q->gun_platform.hit=0;a->palette=0x84;start(q,GPA_WRECK);break;
 case HATCH:palette(q);interaction_priority(w,a,0x8000,8);a->phase=q->dead=0;m4_reset(a);a->state=HATCH_WAIT;break;
 case HATCH_WAIT:break;
 case HATCH_OPEN:case HATCH_FIRE:case HATCH_CLOSE:
  if(q->dead>=2)return enemy_fault(w);start_entry(q,(s==HATCH_OPEN?gun_platform_hatch_open:s==HATCH_FIRE?gun_platform_hatch_fire:gun_platform_hatch_close)[q->dead]);break;
 case HATCH_HOLD:{struct EnemyActor*p=parent(w,e);if(!p)return 0;integer_follow(q,p,q->gun_platform.offset_y);interaction_priority(w,a,a->transform.priority,a->transform.priority&28);if(q->dead>=2)return enemy_fault(w);start_entry(q,gun_platform_hatch_hold[q->dead]);break;}
 case SHIELD:palette(q);a->display_flags|=2;interaction_priority(w,a,0,8);a->phase=q->dead=0;a->hit.hp=rank(w,q,GPR_SHIELD_HP);m4_reset(a);break;
 case SHIELD_FIRE:break;
 case RIDER_A:case RIDER_B:
  a->phase=q->dead=0;a->hit.hp=1;a->palette=0x38;if(!tk_settle(w,a))return 0;m4_reset(a);q->gun_platform.variant=s==RIDER_B;if(s==RIDER_B)transform_integer_offset(&a->transform,10,-11);defense(q,s==RIDER_A?GPB_RIDER_A:GPB_RIDER_B);interaction_priority(w,a,0x2000,8);a->state=RIDER_IDLE;/* FALLTHROUGH */
 case RIDER_IDLE:start(q,q->gun_platform.variant?GPA_RIDER_B_IDLE:GPA_RIDER_A_IDLE);break;
 case RIDER_ALERT:start(q,q->gun_platform.variant?GPA_RIDER_B_ALERT:GPA_RIDER_A_ALERT);break;
 case RIDER_FIRE:start(q,q->gun_platform.variant?GPA_RIDER_B_FIRE:GPA_RIDER_A_FIRE);break;
 case RIDER_HOLD:a->state=RIDER_ALERT;break; /* Source resumes frame, retains fire drawing. */
 case RIDER_RETURN:start(q,GPA_RIDER_B_RETURN);break;
 case GUN:palette(q);attack(q,GPB_GUN);start(q,GPA_GUN);break;
 case GUN_DAMAGED:start(q,GPA_GUN_DAMAGED);a->state=GUN_B;break;
 case SHOT:case SHOT_SLOW:
  speed=q->gun_platform.speed;if(s==SHOT){start(q,GPA_SHOT);transform_integer_offset(&a->transform,0,-8);}
  if(s==SHOT_SLOW||speed<=0){if(speed<=0)speed=rank(w,q,GPR_SHOT_SPEED);start(q,GPA_SHOT_SLOW);}a->vx=a->transform.facing&1?speed:tk_word(-speed);a->palette=0x1cf;a->hit.type_flags|=16;interaction_priority(w,a,0xd000,20);a->state=SHOT;break;
 case SMOKE:a->palette=8;a->transform.facing^=1;interaction_priority(w,a,0x4000,28);start(q,GPA_SMOKE);break;
 case BLAST:interaction_priority(w,a,0x4000,0);defense(q,65535);boss_fx_transform(w,e,BFX_FIRE);return 0;
 default:return enemy_fault(w);
 }return !w->fault;
}
static void frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;int done,r;
 if(s<=FIRE){
  if(s==OPEN){q->gun_platform.wait=tk_word(q->gun_platform.wait-1);if(!q->gun_platform.wait)schedule(e,AIM);}
  if(interaction_move(w,a,WMOVE_SYNC)<0||!select_body(w,q,0,q->dead))return;animate(w,e);
  if(s==IDLE){q->gun_platform.idle=tk_word(q->gun_platform.idle-1);if(q->gun_platform.idle<=0){q->gun_platform.idle=0;a->phase=1;schedule(e,OPEN);}}
  else if(s==FIRE){q->gun_platform.wait=tk_word(q->gun_platform.wait-1);if(!q->gun_platform.wait){q->gun_platform.serial++;q->gun_platform.repeats--;schedule(e,q->gun_platform.repeats?REARM:IDLE);}}
  else if(!q->gun_platform.wait){q->gun_platform.clock=tk_word(q->gun_platform.clock+1);if(q->gun_platform.clock>=q->gun_platform.interval){q->gun_platform.clock=0;a->phase=q->gun_platform.serial&1?3:2;schedule(e,FIRE);}}
  w->enemies->intro_gate=0;if(!body_hit(w,e))return;if(outside(w,q))schedule(e,DELETE_TREE);return;
 }
 if(s==DESTROY||s==WRECK_ANIM||s==WRECK||s==SMOKE){
  if(interaction_move(w,a,WMOVE_SYNC)<0)return;done=animate(w,e);
  if(s==DESTROY){q->gun_platform.wait=tk_word(q->gun_platform.wait-1);if(!q->gun_platform.wait){a->phase=255;if(!stamp(w,q,4))return;a->display_flags&=253;schedule(e,WRECK);}return;}
  if(s==SMOKE){if(done)schedule(e,DELETE_TREE);return;}if(s==WRECK_ANIM&&done)schedule(e,WRECK);if(outside(w,q))schedule(e,DELETE);return;
 }
 if(s==SHOT){r=interaction_move(w,a,WMOVE_POINT);if(r<0)return;if(!r){r=interaction_move(w,a,WMOVE_POINT);if(r<0)return;}if(r)schedule(e,BLAST);animate(w,e);if(!enemy_attack(w,a))return;if(a->hit.flags&2)schedule(e,BLAST);if(interaction_outside(w,a,boss_fx_bounds[8],1))schedule(e,DELETE_TREE);return;}
 p=parent(w,e);if(!p)return;
 if(s==GUN||s==GUN_B){
  if(s==GUN&&p->dead==1)schedule(e,GUN_DAMAGED);a->transform=p->actor.transform;if(p->gun_platform.hit)a->hit.events|=1;const int16_t*off=gun_platform_offsets[q->gun_platform.variant];transform_integer_offset(&a->transform,off[0],off[1]);animate(w,e);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return;
  if(p->actor.phase==255||entity_parent_deleting(w->entities,e))schedule(e,DELETE_TREE);return;
 }
 if(s>=HATCH_WAIT&&s<=HATCH_CLOSE){
  if(s==HATCH_WAIT){if(p->actor.phase==1)schedule(e,HATCH_OPEN);}
  else{if(s!=HATCH_HOLD)integer_follow(q,p,q->gun_platform.offset_y);if(s==HATCH_OPEN||s==HATCH_FIRE)interaction_priority(w,a,a->transform.priority,a->transform.priority&28);inherit_hit(q,p);done=animate(w,e);
   if(s==HATCH_OPEN&&p->actor.phase==2)schedule(e,HATCH_FIRE);
   if(s==HATCH_FIRE&&p->actor.phase!=2)schedule(e,HATCH_HOLD);
   if(s==HATCH_HOLD){if(!p->actor.phase)schedule(e,HATCH_CLOSE);if(p->actor.phase==2)schedule(e,HATCH_FIRE);}
   if(s==HATCH_CLOSE&&done)schedule(e,HATCH_WAIT);
  }parent_tail(w,e);return;
 }
 if(s==SHIELD||s==SHIELD_FIRE){
  integer_follow(q,p,0);if(!select_body(w,q,1,p->dead))return;inherit_hit(q,p);animate(w,e);if(s==SHIELD&&p->actor.phase==2)schedule(e,SHIELD_FIRE);if(s==SHIELD_FIRE&&p->actor.phase!=2){a->state=SHIELD;q->entry=0;}
  parent_tail(w,e);if(a->hit.flags&8){if(!tk_hit_sound(w,a))return;a->hit.flags&=247;}if(a->hit.flags&1)p->actor.phase=255;return;
 }
 if(s>=RIDER_IDLE&&s<=RIDER_RETURN){
  if(interaction_move(w,a,WMOVE_SYNC)<0)return;done=animate(w,e);
  if(s==RIDER_IDLE&&p->actor.phase)schedule(e,RIDER_ALERT);
  if(s==RIDER_ALERT&&done){if(!p->actor.phase)schedule(e,q->gun_platform.variant?RIDER_RETURN:RIDER_IDLE);if(p->actor.phase==3)schedule(e,RIDER_FIRE);}
  if(s==RIDER_FIRE&&p->actor.phase!=3)schedule(e,RIDER_HOLD);if(s==RIDER_RETURN&&done)schedule(e,RIDER_IDLE);
  if(a->hit.flags&8&&!human_platform_death(w,e,!q->gun_platform.variant))return;parent_tail(w,e);return;
 }
 enemy_fault(w);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;for(unsigned i=0;i<8;i++){
 if(q->actor.state==DELETE||q->actor.state==DELETE_TREE){if(q->actor.state==DELETE_TREE){q->actor.phase=255;entity_delete_children(p,e);}entity_delete(p,e);return;}
 if(q->entry){q->entry=0;int r=enter(w,e);if(!r||w->fault)return;if(r==2)continue;}frame(w,e,q->actor.state);return;
 }enemy_fault(w);}
int gun_platform_spawn(struct WeaponWorld*w,struct EntityRef parent_ref,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){if(id<133||id>136)return enemy_fault(w);struct NativeEntity*e=enemy_allocate(w,parent_ref,order,t,tick);if(!e)return 0;init(e,NEW);e->payload.enemy.gun_platform.variant=(uint8_t)(id-133);if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];return 1;}

/* $04C846 versus $04C862: preserve a positive caller-supplied speed. */
void gun_platform_projectile(struct WeaponWorld*w,struct NativeEntity*e,int16_t speed,int full){init(e,full?SHOT:SHOT_SLOW);e->payload.enemy.gun_platform.speed=speed;tick(w->entities,e,w);}
