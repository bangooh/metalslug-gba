#include "light_tank.h"
#include "slug_surface.h"
/* $068C1E..$069880. Entry aliases execute now; state writes take effect next tick. */
static void tick(struct EntityPool*,struct NativeEntity*,void*);
void lt_schedule(struct NativeEntity*e,unsigned s){e->tick=tick;enemy_schedule(&e->payload.enemy,s);}
static void init(struct NativeEntity*e,unsigned s){uint8_t*p=(uint8_t*)&e->payload.enemy.light_tank;for(unsigned i=0;i<sizeof(e->payload.enemy.light_tank);i++)p[i]=255;lt_schedule(e,s);}
struct NativeEntity*lt_child(struct WeaponWorld*w,struct NativeEntity*e,unsigned s,enum EntityInsert o){struct NativeEntity*n=enemy_allocate(w,entity_ref(e),o,&e->payload.enemy.actor.transform,tick);if(n)init(n,s);return n;}
uint16_t lt_random(struct WeaponWorld*w){if(!w->rng){enemy_fault(w);return 0;}return source_random_word(w->rng);}
int16_t lt_rank(struct WeaponWorld*w,struct EnemyActor*q,unsigned n){struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid||n>=LTR_COUNT){enemy_fault(w);return 0;}return (int16_t)light_tank_rank[n][(interaction_rank(w,&q->actor,runtime_owner_light_tank[n]))*8+(q->source_flags&3)*2+((r->player_count_flag&2)>>1)];}
void lt_start_entry(struct EnemyActor*q,uint16_t n){if(n==65535)return;animation_begin(&q->actor.animation,n);q->actor.movement_flags|=32;}
void lt_start(struct EnemyActor*q,enum LightTankAnimation n){lt_start_entry(q,light_tank_entries[n]);}
void lt_defense(struct EnemyActor*q,unsigned n){q->actor.defense=n==65535?0:light_tank_bindings[n].defense;q->actor.defense_count=n==65535?0:light_tank_bindings[n].count;}
void lt_attack(struct EnemyActor*q,unsigned n){if(n==65535)q->actor.attack=0;else{q->attack_set=(struct HitAttackSet){light_tank_bindings[n].attack,light_tank_bindings[n].count};q->actor.attack=&q->attack_set;}}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read(void*ctx,unsigned f,unsigned width){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;if(width==1){if(f==LTF_3B)return q->actor.animation.pose;if(f==LTF_20)return q->actor.phase;}if(width==2){if(f==LTF_94)return (uint16_t)q->light_tank.freeze;if(f==LTF_74)return (uint16_t)q->light_tank.timer;if(f==LTF_82)return (uint16_t)q->light_tank.angle;}enemy_fault(c->w);return 0;}
static int write(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;if(op!=AN_FIELD_SET)return enemy_fault(c->w);if(width==1){if(f==LTF_3B)q->actor.animation.pose=(uint8_t)v;else if(f==LTF_20)q->actor.phase=(uint8_t)v;else return enemy_fault(c->w);}else if(width==2){if(f==LTF_94)q->light_tank.freeze=(int16_t)v;else if(f==LTF_74)q->light_tank.timer=(int16_t)v;else return enemy_fault(c->w);}else return enemy_fault(c->w);return 1;}
static int delay(void*ctx,unsigned f,unsigned pose,uint8_t*out){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;if(f!=LTF_5C||pose>=8||q->light_tank.delay_mode>=2||q->light_tank.delay_row>=4)return enemy_fault(c->w);*out=light_tank_delays[q->light_tank.delay_mode][q->light_tank.delay_row][pose];return 1;}
static int bind(void*ctx,unsigned kind,uint32_t v){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;unsigned n=v==0xffffffffu?65535:v;if(n!=65535&&n>=sizeof(light_tank_bindings)/sizeof(*light_tank_bindings))return enemy_fault(c->w);if(kind==AN_DEFENSE)lt_defense(q,n);else if(kind==AN_ATTACK){lt_attack(q,n);hit_begin_attack(&q->actor.hit);}else return enemy_fault(c->w);return 1;}
static int sound(void*ctx,uint16_t s,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,s,stop)?1:enemy_fault(c->w);}
static int callback(void*ctx,unsigned n,struct Animation*a){(void)a;struct Context*c=ctx;return lt_callback(c->w,c->e,n);}
int lt_animate(struct WeaponWorld*w,struct NativeEntity*e){struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;struct Context c={w,e};struct AnimationOwner o={.context=&c,.read=read,.write=write,.delay=delay,.bind=bind,.sound=sound,.call=callback};int done=animation_step(&a->animation,&light_tank_program,&o);if(a->animation.fault)enemy_fault(w);return done;}
int lt_settle(struct WeaponWorld*w,struct WeaponActor*a){m4_screen(w,a,m4_sx(w,a),m4_sy(w,a)&1023);for(unsigned i=0;i<2048;i++){if(m4_sy(w,a)<0){m4_screen(w,a,160,384);return 1;}int r=interaction_move(w,a,WMOVE_ITEM);if(r<0)return 0;if(r)return 1;}return enemy_fault(w);}
int lt_hurt(struct WeaponWorld*w,struct WeaponActor*a){return !(a->hit.flags&1)&&(a->hit.events&1)&&a->hit.last_kind==1?interaction_sound(w,0x108d):1;}
static struct NativeEntity*part(struct WeaponWorld*w,struct EnemyActor*q,int turret){uint16_t id=turret?q->light_tank.turret_index:q->light_tank.driver_index,g=turret?q->light_tank.turret_generation:q->light_tank.driver_generation;if(id==65535)return 0;struct NativeEntity*n=entity_get(w->entities,(struct EntityRef){id,g});if(!n)enemy_fault(w);return n;}
static int commands(struct WeaponWorld*w,struct EnemyActor*q,unsigned turret,unsigned driver){struct NativeEntity*n=part(w,q,1);if(n)lt_start_entry(&n->payload.enemy,light_tank_turret[turret]);n=part(w,q,0);if(n)lt_start_entry(&n->payload.enemy,light_tank_driver[driver]);return !w->fault;}
static int special_hit(struct WeaponActor*a){unsigned n=a->hit.last_kind;return n==2||n==3||n==21||n==22||n==23||n==24||n==30;}
static int gate(struct WeaponWorld*w,struct EnemyActor*q){q->light_tank.cooldown=m4_word(q->light_tank.cooldown-1);if(q->light_tank.cooldown>0)return 0;q->light_tank.cooldown=0;return m4_sx(w,&q->actor)<=288;}
static int mismatch(struct EnemyActor*q){return q->actor.transform.facing!=q->light_tank.direction;}
static int past(struct WeaponWorld*w,struct EnemyActor*q){uint16_t x=(uint16_t)m4_sx(w,&q->actor),limit=(uint16_t)q->light_tank.limit;return q->light_tank.direction==1?limit<x:x<limit;}
static void delay_row(struct EnemyActor*q){unsigned n=q->light_tank.clock++>>4;q->light_tank.delay_row=n>3?3:n;}
static int stop_sound(struct WeaponWorld*w){return w->sound&&w->sound(w->scene,0x109e,1)?1:enemy_fault(w);}
static void signal(struct WeaponWorld*w,struct EnemyActor*q){if(q->params[4])w->enemies->intro_gate=0;}
static int foot(struct WeaponWorld*w,struct WeaponActor*a){int r=terrain_weapon_foot_contact(w->interactions->terrain,a);if(r<0)enemy_fault(w);return r;}
static int body_enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*n;unsigned s=a->state;
 switch(s){
 case LT_NEW:
  a->palette=q->params[3]?28:27;q->light_tank.impact_palette=a->hit_palette=0x77;if(!lt_settle(w,a))return 0;m4_reset(a);q->light_tank.cooldown=lt_rank(w,q,LTR_COOLDOWN);q->light_tank.period=lt_rank(w,q,LTR_PERIOD);interaction_priority(w,a,0x8000,20);a->hit.hp=100;lt_defense(q,LTB_BODY);q->light_tank.repeats=0;
  n=lt_child(w,e,LT_DRIVER,ENTITY_FIRST_CHILD);if(!n)return 0;q->light_tank.driver_index=n->index;q->light_tank.driver_generation=n->generation;n->payload.enemy.params[0]=q->params[0];
  if(!q->params[1]){n=lt_child(w,e,LT_TURRET,ENTITY_FIRST_CHILD);if(!n)return 0;q->light_tank.turret_index=n->index;q->light_tank.turret_generation=n->generation;n->payload.enemy.params[3]=q->params[3];}
  a->transform.facing=q->light_tank.direction=m4_sx(w,a)>256?0:1;q->light_tank.limit=a->transform.facing?96:224;
  n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_DAMAGE_SMOKE_BODY);if(!n)return 0;n->payload.enemy.actor.attached_offset=0;
  if(!vehicle_spawn_surface(w,entity_ref(e),&a->transform))return 0;
  lt_attack(q,LTB_DRIVE);lt_schedule(e,q->params[2]?LT_IDLE:LT_DRIVE);return 2;
 case LT_FALL:m4_reset(a);if(!commands(w,q,0,0))return 0;lt_start(q,LTA_IDLE);break;
 case LT_DRIVE:
  if(q->params[2]){lt_schedule(e,LT_IDLE);return 2;}if(!interaction_sound(w,0x109e))return 0;m4_reset(a);a->vx=q->light_tank.direction==1?768:-768;q->light_tank.clock=0;q->light_tank.delay_mode=mismatch(q);if(!commands(w,q,q->light_tank.delay_mode?2:1,1))return 0;lt_start(q,q->light_tank.delay_mode?LTA_REVERSE:LTA_DRIVE);delay_row(q);break;
 case LT_IDLE:q->light_tank.timer=15;if(!commands(w,q,0,0))return 0;lt_start(q,LTA_IDLE);break;
 case LT_TURN:case LT_BRAKE:q->light_tank.delay_mode=mismatch(q);if(s==LT_TURN)q->light_tank.direction^=1;a->ax=m4_word(-(a->vx>>4));q->light_tank.clock=0;if(!stop_sound(w))return 0;a->state=LT_BRAKE;break;
 case LT_RESUME:if(q->params[2]){lt_schedule(e,LT_IDLE);return 2;}if(gate(w,q)){lt_schedule(e,LT_AIM);return 2;}if(!commands(w,q,4,3))return 0;lt_start(q,LTA_RESUME);break;
 case LT_AIM:case LT_AIM_NEAR:
  if(s==LT_AIM&&q->light_tank.repeats){lt_schedule(e,LT_FIRE);return 2;}q->light_tank.aim=1;
  if(s==LT_AIM_NEAR){int slot=enemy_nearest_x(w,a,0);q->light_tank.aim=slot>=0&&m4_word((a->transform.x>>8)-(w->players[slot]->actor.transform.x>>8))<=160;}
  if(!commands(w,q,q->light_tank.aim?5:6,5))return 0;lt_start(q,q->light_tank.aim?LTA_AIM_HIGH:LTA_AIM_LOW);q->light_tank.timer=50;a->state=LT_AIM;break;
 case LT_FIRE_FIRST:q->light_tank.repeats=(uint8_t)lt_rank(w,q,LTR_REPEATS);/* fall through */
 case LT_FIRE:q->light_tank.period=lt_rank(w,q,LTR_PERIOD);if(!commands(w,q,q->light_tank.aim?9:8,6))return 0;lt_start(q,q->light_tank.aim?LTA_FIRE_HIGH:LTA_FIRE_LOW);a->state=LT_FIRE;break;
 case LT_CLOSE:q->light_tank.repeats=0;q->light_tank.cooldown=lt_rank(w,q,LTR_COOLDOWN);if(!q->light_tank.aim){lt_schedule(e,LT_IDLE);return 2;}q->light_tank.aim=0;if(!commands(w,q,7,0))return 0;lt_start(q,LTA_CLOSE);break;
 case LT_TURRET_HIT:n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMOKE);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,a->transform.facing==1?32:-32,0);/* fall through */
 case LT_HIT:case LT_HIT_BIG:if(!commands(w,q,s==LT_HIT?10:s==LT_HIT_BIG?11:13,8))return 0;lt_start(q,s==LT_HIT?LTA_HIT:LTA_HIT_BIG);a->hit.flags&=247;if(!stop_sound(w))return 0;a->state=LT_HIT;break;
 case LT_DEAD:case LT_DEAD_BIG:n=part(w,q,0);if(n)lt_schedule(n,s==LT_DEAD?LT_DRIVER_OUT:LT_DRIVER_CORPSE);if(!commands(w,q,s==LT_DEAD?12:13,10))return 0;lt_start(q,s==LT_DEAD?LTA_DEAD:LTA_DEAD_BIG);q->light_tank.freeze=0;a->display_flags&=253;a->state=LT_DEAD;break;
 case LT_REMOVE:if(!interaction_sound(w,0x1033)||!boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMOKE))return 0;for(unsigned i=0;i<17;i++){n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_METAL_HEAVY);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,0,24);}entity_delete(w->entities,e);return 0;
 default:return enemy_fault(w);
 }return !w->fault;
}
static void body_tail(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(s!=LT_FALL){int r=foot(w,a);if(r<0)return;if(!r)lt_schedule(e,LT_FALL);}signal(w,q);
 if(a->hit.flags&8){if(s==LT_HIT){a->hit.flags&=247;if(!lt_hurt(w,a))return;}else{if(!lt_hurt(w,a))return;a->hit.flags&=247;if(!lt_hurt(w,a))return;lt_schedule(e,special_hit(a)?LT_HIT_BIG:LT_HIT);}}
 if(a->hit.flags&1)lt_schedule(e,special_hit(a)?LT_DEAD_BIG:LT_DEAD);
 if(interaction_outside(w,a,light_tank_bounds[0],0))lt_schedule(e,LT_DELETE);
}
static void body_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(s==LT_DEAD){if(interaction_move(w,a,WMOVE_SYNC)<0)return;if(q->light_tank.freeze>0){q->light_tank.freeze--;a->animation.freeze++;}int done=lt_animate(w,e);if(done&&q->light_tank.temporary)lt_schedule(e,LT_REMOVE);if(interaction_outside(w,a,light_tank_bounds[0],0))lt_schedule(e,LT_DELETE);return;}
 interaction_body_contact(w,a,&light_tank_contact);int r=interaction_move(w,a,s==LT_FALL?WMOVE_ITEM:s==LT_DRIVE||s==LT_BRAKE?WMOVE_FLAT:WMOVE_SYNC);if(r<0)return;
 if(s==LT_BRAKE){if(a->ax&&!a->vx){a->ax=0;if(!commands(w,q,3,2))return;lt_start(q,LTA_BRAKE);}delay_row(q);}
 if(s==LT_FALL&&r)lt_schedule(e,LT_IDLE);if(s==LT_DRIVE&&r)lt_schedule(e,LT_BRAKE);
 int done=lt_animate(w,e);
 switch(s){
 case LT_DRIVE:if(past(w,q))lt_schedule(e,LT_TURN);if(gate(w,q))lt_schedule(e,LT_BRAKE);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return;break;
 case LT_IDLE:if(!q->params[2]){q->light_tank.timer=m4_word(q->light_tank.timer-1);if(!q->light_tank.timer)lt_schedule(e,LT_RESUME);}if(gate(w,q))lt_schedule(e,LT_AIM);break;
 case LT_BRAKE:if(done)lt_schedule(e,LT_IDLE);gate(w,q);break;
 case LT_RESUME:if(done)lt_schedule(e,LT_DRIVE);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return;break;
 case LT_AIM:q->light_tank.timer=m4_word(q->light_tank.timer-1);if(!q->light_tank.timer)lt_schedule(e,LT_FIRE_FIRST);a->hit.flags&=247;break;
 case LT_FIRE:if(done&&(int8_t)q->light_tank.repeats<=0)lt_schedule(e,LT_CLOSE);if((int8_t)q->light_tank.repeats>0){q->light_tank.period=m4_word(q->light_tank.period-1);if(q->light_tank.period<=0){q->light_tank.repeats--;if((int8_t)q->light_tank.repeats>0)lt_schedule(e,LT_FIRE);}}a->hit.flags&=247;break;
 case LT_CLOSE:if(done)lt_schedule(e,LT_IDLE);break;
 case LT_HIT:if(done)lt_schedule(e,LT_RESUME);signal(w,q);gate(w,q);break;
 case LT_FALL:break;
 default:enemy_fault(w);return;
 }body_tail(w,e,s);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 for(unsigned i=0;i<8;i++){
  if(a->state==LT_DELETE||a->state==LT_DELETE_TREE){if(a->state==LT_DELETE_TREE)entity_delete_children(p,e);entity_delete(p,e);return;}
  if(q->entry){q->entry=0;int r=a->state<LT_DRIVER?body_enter(w,e):lt_parts_enter(w,e);if(!r||r==3||w->fault)return;if(r==2)continue;}
  unsigned s=a->state;if(s<LT_DRIVER)body_frame(w,e,s);else lt_parts_frame(w,e,s);return;
 }enemy_fault(w);
}
int light_tank_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){if(id<53||id>55)return enemy_fault(w);struct NativeEntity*n=enemy_allocate(w,parent,order,t,tick);if(!n)return 0;init(n,LT_NEW);n->payload.enemy.light_tank.temporary=id==55;n->payload.enemy.light_tank.stamp=id==54;if(params)for(unsigned i=0;i<8;i++)n->payload.enemy.params[i]=params[i];return 1;}
