#include "shield.h"
#include "m4_internal.h"
#include "items.h"
/* Two full entry families, one shared shield/ordnance lifetime. */
enum State {NEW,IDLE,GUARD,FALL,WALK,STOP,BACK,CROUCH,CROUCH_FIRE,RISE,AIM,FIRE,RECOVER,HIT_SMALL,HIT,LOST,ESCAPE,SHIELD,DETACH,BOUNCE,FADE,BULLET,MUZZLE,DELETE,DELETE_TREE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static void schedule(struct NativeEntity*e,unsigned s){e->tick=tick;enemy_schedule(&e->payload.enemy,s);}
static uint16_t random_word(struct WeaponWorld*w){if(!w->rng){enemy_fault(w);return 0;}return source_random_word(w->rng);}
static int16_t rank_value(struct WeaponWorld*w,struct EnemyActor*q,unsigned n){struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid||n>=SHR_COUNT){enemy_fault(w);return 0;}const struct ShieldRankData*t=&shield_rank[n];unsigned i=(interaction_rank(w,&q->actor,runtime_owner_shield[n]))*8+(q->source_flags&3)*2;if(t->choices)i=t->choices[i+((uint8_t)(r->player_count_flag-1)&1)]*8+((random_word(w)&14)>>1);else i+=(r->player_count_flag&2)>>1;return (int16_t)t->values[i];}
void shield_start_entry(struct EnemyActor*q,uint16_t n){if(n==65535)return;animation_begin(&q->actor.animation,n);q->actor.movement_flags|=32;}
void shield_start(struct EnemyActor*q,enum ShieldAnimation n){shield_start_entry(q,shield_entries[n]);}
void shield_defense(struct EnemyActor*q,unsigned n){q->actor.defense=n==65535?0:shield_bindings[n].defense;q->actor.defense_count=n==65535?0:shield_bindings[n].count;}
int shield_hurt(struct WeaponWorld*w,struct WeaponActor*a){return !(a->hit.flags&1)&&(a->hit.events&1)&&a->hit.last_kind==1?interaction_sound(w,0x108d):1;}
static void init(struct NativeEntity*e,unsigned s){uint8_t*p=(uint8_t*)&e->payload.enemy.shield;for(unsigned i=0;i<sizeof(e->payload.enemy.shield);i++)p[i]=255;schedule(e,s);}
static struct NativeEntity*child(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct NativeEntity*n=enemy_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,tick);if(n)init(n,s);return n;}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read(void*ctx,unsigned f,unsigned width){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;if(width==1){if(f==SHF_80)return q->shield.pose;if(f==SHF_81)return q->shield.forward_disabled;if(f==SHF_3B)return q->actor.animation.pose;if(f==SHF_3A)return q->actor.transform.facing;}if(width==2){if(f==SHF_16)return q->shield.palettes[0];if(f==SHF_18)return q->shield.palettes[1];if(f==SHF_1A)return q->shield.palettes[2];}enemy_fault(c->w);return 0;}
static int write(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;if(op==AN_FIELD_XOR)v^=read(ctx,f,width);else if(op!=AN_FIELD_SET)return enemy_fault(c->w);if(width!=1)return enemy_fault(c->w);if(f==SHF_80)q->shield.pose=(uint8_t)v;else if(f==SHF_81)q->shield.forward_disabled=(uint8_t)v;else if(f==SHF_3B)q->actor.animation.pose=(uint8_t)v;else if(f==SHF_3A)q->actor.transform.facing=(uint8_t)v;else return enemy_fault(c->w);return 1;}
static int delay(void*ctx,unsigned f,unsigned pose,uint8_t*out){struct Context*c=ctx;unsigned row=c->e->payload.enemy.shield.delays;if(f!=SHF_94||row>=4||pose>=12)return enemy_fault(c->w);*out=shield_delays[row][pose];return 1;}
static int bind(void*ctx,unsigned kind,uint32_t v){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;if(kind==AN_PALETTE){if(v>65535)return enemy_fault(c->w);q->actor.palette=(uint16_t)v;return 1;}unsigned n=v==0xffffffffu?65535:v;if(n!=65535&&n>=sizeof(shield_bindings)/sizeof(*shield_bindings))return enemy_fault(c->w);if(kind==AN_DEFENSE)shield_defense(q,n);else if(kind==AN_ATTACK){if(n==65535)q->actor.attack=0;else{q->attack_set=(struct HitAttackSet){shield_bindings[n].attack,shield_bindings[n].count};q->actor.attack=&q->attack_set;}hit_begin_attack(&q->actor.hit);}else return enemy_fault(c->w);return 1;}
static int sound(void*ctx,uint16_t s,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,s,stop)?1:enemy_fault(c->w);}
static int call(void*ctx,unsigned n,struct Animation*a){(void)a;struct Context*c=ctx;struct WeaponActor*b=&c->e->payload.enemy.actor;if(n==SHC_283CA){hit_begin_attack(&b->hit);return 1;}if(n==SHC_4A120){b->palette=14;return 1;}if(n==SHC_68206){if(!interaction_sound(c->w,0x10ff)||!child(c->w,c->e,MUZZLE))return 0;struct NativeEntity*v=child(c->w,c->e,BULLET);if(!v)return 0;transform_integer_offset(&v->payload.enemy.actor.transform,b->transform.facing&1?30:-30,28);return 1;}return enemy_fault(c->w);}
int shield_animate(struct WeaponWorld*w,struct NativeEntity*e){struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;struct Context c={w,e};struct AnimationOwner o={.context=&c,.read=read,.write=write,.delay=delay,.bind=bind,.sound=sound,.call=call};int done=animation_step(&a->animation,&shield_program,&o);if(a->animation.fault)enemy_fault(w);return done;}
static int settle(struct WeaponWorld*w,struct WeaponActor*a){m4_screen(w,a,m4_sx(w,a),m4_sy(w,a)&1023);for(unsigned i=0;i<2048;i++){if(m4_sy(w,a)<0){m4_screen(w,a,160,384);return 1;}int r=interaction_move(w,a,WMOVE_ITEM);if(r<0)return 0;if(r)return 1;}return enemy_fault(w);}
static int foot(struct WeaponWorld*w,struct WeaponActor*a){int r=terrain_weapon_foot_contact(w->interactions->terrain,a);if(r<0)enemy_fault(w);return r;}
static int move(struct WeaponWorld*w,struct WeaponActor*a,int walk){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;int r=foot(w,a);if(r<0)return 0;if(!r)return interaction_move(w,a,WMOVE_ITEM)>=0;return !walk||interaction_move(w,a,WMOVE_GROUND)>=0;}
static int fall_check(struct WeaponWorld*w,struct EnemyActor*q){int r=foot(w,&q->actor);if(r<0)return 0;if(!r){q->shield.air_count++;if((int8_t)q->shield.air_count>=3)return 1;}q->shield.air_count=0;return 0;}
static int edge(struct WeaponWorld*w,struct WeaponActor*a){return (m4_sx(w,a)<=32&&(a->transform.facing&1))||(m4_sx(w,a)>=288&&!(a->transform.facing&1));}
static int nearest(struct WeaponWorld*w,struct WeaponActor*a,unsigned box,int16_t*dist){int slot;if(box<4){struct HitRect r={shield_boxes[box][0],shield_boxes[box][1],shield_boxes[box][2],shield_boxes[box][3]};slot=enemy_nearest_x(w,a,&r);}else slot=enemy_nearest_x(w,a,0);if(slot<0)return 0;int16_t d=m4_word((a->transform.x>>8)-(w->players[slot]->actor.transform.x>>8));*dist=d<0?m4_word(-d):d;return 1;}
static int guard(struct WeaponWorld*w,struct EnemyActor*q){q->shield.guard=m4_word(q->shield.guard+1);if(q->shield.guard<15)return 0;q->shield.guard=15;int16_t d=0;return !nearest(w,&q->actor,2,&d)&&nearest(w,&q->actor,3,&d);}
static int can_fire(struct WeaponWorld*w,struct EnemyActor*q,int melee,int screen,int distance){struct WeaponActor*a=&q->actor;if(screen&&(m4_sx(w,a)<24||m4_sx(w,a)>296))return 0;if(q->params[0]!=(melee?1:0)||q->shield.timer>0)return 0;int16_t d=0;return nearest(w,a,melee?1:0,&d)&&(!distance||d<=q->shield.distance);}
static int prepare(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct NativeEntity*n=entity_get(w->entities,(struct EntityRef){q->shield.child_index,q->shield.child_generation});if(!n)return enemy_fault(w);struct WeaponActor*a=&q->actor,*b=&n->payload.enemy.actor;if(!q->shield.forward_disabled&&(b->hit.events&1)){a->hit.flags|=8;a->hit.events|=1;a->hit.last_kind=b->hit.last_kind;}if(b->defense)interaction_body_contact(w,a,&shield_contact);return !w->fault;}
static int drop(struct WeaponWorld*w,struct NativeEntity*e,int thrown){struct EnemyActor*q=&e->payload.enemy;if(q->shield.dropped||!q->params[2])return 1;struct PlayerTransform t=q->actor.transform;transform_integer_offset(&t,0,thrown?16:4);if(!item_spawn_drop(w,&t,q->params[2],q->params[3],q->params[4],1,0))return 0;struct NativeEntity*g=entity_get(w->entities,w->projectile_group);if(!g||!g->next)return enemy_fault(w);struct NativeEntity*n=&w->entities->nodes[g->next];if(n->kind!=ENTITY_ITEM)return enemy_fault(w);if(thrown)n->payload.item.actor.vy=2048;q->shield.dropped=1;return 1;}
static int death(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;q->actor.phase=0;q->actor.hit.flags|=8;if(!drop(w,e,0))return 0;return q->params[2]?human_smoke_transform(w,e):human_death_hit_tail(w,e);}
static int tail(struct WeaponWorld*w,struct NativeEntity*e,int idle){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;q->shield.timer=m4_word(q->shield.timer-1);if(a->hit.flags&8){a->hit.flags&=247;if(!shield_hurt(w,a))return 0;if(idle)schedule(e,a->hit.last_kind==1?HIT_SMALL:HIT);}if(a->hit.flags&1)return death(w,e);if(interaction_outside(w,a,shield_bounds[0],0))schedule(e,DELETE);return !w->fault;}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*n;struct EnemyActor*p;
 switch(a->state){
 case NEW:q->shield.dropped=0;if(m4_sx(w,a)<=256)a->transform.facing^=1;if(!settle(w,a))return 0;if(q->params[0]==2)q->params[0]=random_word(w)&1;a->palette=q->params[2]?0x1c7:0x38;q->shield.palettes[0]=a->palette;q->shield.impact_palette=a->hit_palette=q->params[2]?0x93:0x8e;interaction_priority(w,a,0x8000,24);q->shield.forward_disabled=0;a->hit.hp=1;shield_defense(q,SHB_BODY);q->shield.delays=0;n=child(w,e,SHIELD);if(!n)return 0;q->shield.child_index=n->index;q->shield.child_generation=n->generation;q->shield.timer=rank_value(w,q,SHR_CROUCH_COOLDOWN);if(!q->shield.variant){q->shield.distance=rank_value(w,q,SHR_DISTANCE);if(q->params[0])q->shield.distance=54;}schedule(e,q->shield.variant?(q->params[1]?WALK:IDLE):(q->params[1]?IDLE:WALK));return 2;
 case IDLE:shield_start(q,SHA_IDLE);break;
 case GUARD:q->shield.guard=0;shield_start(q,SHA_GUARD);break;
 case FALL:shield_start(q,SHA_FALL);break;
 case WALK:case BACK:{int16_t speed=rank_value(w,q,SHR_SPEED);a->vx=a->transform.facing&1?speed:m4_word(-speed);q->shield.delays=((uint16_t)speed>>9)&3;if(a->state==BACK){a->vx=m4_word(-a->vx);shield_start(q,SHA_BACK);}else{shield_defense(q,SHB_WALK);shield_start(q,SHA_WALK);}break;}
 case STOP:m4_reset(a);shield_start(q,SHA_STOP);break;
 case CROUCH:q->shield.life=(uint8_t)rank_value(w,q,SHR_CROUCH_COUNT);shield_start(q,SHA_CROUCH);break;
 case CROUCH_FIRE:q->shield.timer=rank_value(w,q,SHR_CROUCH_WAIT);shield_start(q,SHA_CROUCH_FIRE);break;
 case RISE:q->shield.timer=rank_value(w,q,SHR_CROUCH_COOLDOWN);shield_start(q,SHA_RISE);break;
 case AIM:q->shield.life=(uint8_t)rank_value(w,q,SHR_AIM_COUNT);shield_start(q,SHA_AIM);break;
 case FIRE:q->shield.timer=rank_value(w,q,SHR_AIM_WAIT);shield_start(q,SHA_FIRE);break;
 case RECOVER:q->shield.timer=rank_value(w,q,SHR_AIM_COOLDOWN);shield_start(q,SHA_RECOVER);break;
 case HIT_SMALL:case HIT:shield_start(q,a->state==HIT_SMALL?SHA_HIT_SMALL:SHA_HIT);break;
 case LOST:if(!drop(w,e,1))return 0;shield_start(q,SHA_LOST);break;
 case ESCAPE:infantry_force_escape(e,255);return 2;
 case SHIELD:p=m4_parent(w,e);if(!p)return 0;a->palette=p->params[2]?0x1c7:0x38;q->shield.palettes[0]=a->palette;q->shield.impact_palette=a->hit_palette=p->params[2]?0x93:0x8e;a->hit.hp=rank_value(w,q,SHR_SHIELD_HP);shield_defense(q,SHB_SHIELD);break;
 case DETACH:transform_integer_offset(&a->transform,-8,18);a->vx=a->transform.facing&1?-204:204;a->vy=1080;a->ay=-36;a->ax=0;shield_start(q,SHA_SHIELD_FALL);break;
 case BOUNCE:a->vx=a->transform.facing&1?-204:204;a->vy=1630;a->ay=-163;a->ax=0;shield_start(q,SHA_SHIELD_FALL);break;
 case FADE:q->shield.life=30;break;
 case BULLET:a->hit.type_flags|=16;interaction_priority(w,a,0xd000,20);a->vx=rank_value(w,q,SHR_BULLET);if(!(a->transform.facing&1))a->vx=m4_word(-a->vx);q->shield.palettes[0]=0x157;q->shield.palettes[1]=0x158;q->shield.palettes[2]=0x159;a->palette=0x159;shield_start(q,SHA_BULLET);break;
 case MUZZLE:interaction_priority(w,a,0x2000,20);a->palette=0x17a;shield_start(q,SHA_MUZZLE);break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static int body(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned v=q->shield.variant;int16_t dist=0;
 if(s!=STOP&&s!=LOST&&!prepare(w,e))return 0;
 if(s==FALL){int r=interaction_move(w,a,WMOVE_ITEM);if(r<0)return 0;if(r)schedule(e,v?WALK:IDLE);}
 else if(s==STOP&&v){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;}
 else if(!move(w,a,s==WALK||s==BACK))return 0;
 int done=shield_animate(w,e);unsigned fall_variant=v;
 switch(s){
 case IDLE:
  if(!v&&!q->params[1]){if(nearest(w,a,4,&dist)&&!edge(w,a)&&m4_word(dist+8)<=q->shield.distance)schedule(e,BACK);if(nearest(w,a,4,&dist)&&m4_word(dist-16)>q->shield.distance)schedule(e,WALK);}
  if(guard(w,q))schedule(e,GUARD);if(can_fire(w,q,0,1,!v))schedule(e,CROUCH);if(can_fire(w,q,1,1,0))schedule(e,AIM);break;
 case WALK:
  if(v){int16_t target=q->params[1]*16;if(a->transform.facing&1?target<=m4_sx(w,a):target>m4_sx(w,a))schedule(e,STOP);if(a->state!=WALK)shield_defense(q,SHB_BODY);}
  else{if(can_fire(w,q,0,0,1)||can_fire(w,q,1,0,0))schedule(e,STOP);if(guard(w,q))schedule(e,STOP);if(!nearest(w,a,4,&dist)||dist<=q->shield.distance)schedule(e,STOP);}break;
 case BACK:
  if(!nearest(w,a,4,&dist)||dist>q->shield.distance||edge(w,a))schedule(e,IDLE);if(can_fire(w,q,0,1,1))schedule(e,CROUCH);if(can_fire(w,q,1,1,0))schedule(e,AIM);if(guard(w,q))schedule(e,GUARD);break;
 case GUARD:case STOP:case RISE:case RECOVER:case HIT_SMALL:case HIT:if(done)schedule(e,IDLE);break;
 case CROUCH:if(done)schedule(e,CROUCH_FIRE);break;
 case AIM:if(done)schedule(e,FIRE);break;
 case CROUCH_FIRE:case FIRE:
  if(done){if(q->shield.timer<=0){schedule(e,s);q->shield.life=(uint8_t)(q->shield.life-1);if((int8_t)q->shield.life<=0)schedule(e,s==FIRE?RECOVER:RISE);}else if(v)fall_variant=0;/* source B jumps into A's shared tail */}
  if(s==FIRE){hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;}break;
 case LOST:if(done)schedule(e,ESCAPE);shield_defense(q,65535);break;
 case FALL:break;
 default:return enemy_fault(w);
 }
 if(s!=FALL&&s!=LOST&&fall_check(w,q)){q->shield.variant=(uint8_t)fall_variant;schedule(e,FALL);}
 if(s==WALK&&!v&&a->state!=WALK)shield_defense(q,SHB_BODY);
 return tail(w,e,s==IDLE);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 for(unsigned budget=0;budget<6;budget++){
  if(a->state==DELETE||a->state==DELETE_TREE){if(a->state==DELETE_TREE)entity_delete_children(p,e);entity_delete(p,e);return;}
  if(q->entry){q->entry=0;unsigned before=a->state;int r=enter(w,e);if(!r||w->fault)return;if(before==ESCAPE){e->tick(p,e,w);return;}if(r==2)continue;}
  unsigned s=a->state;if(s<SHIELD){body(w,e,s);return;}
  if(s==SHIELD){struct EnemyActor*parent=m4_parent(w,e);if(!parent)return;a->transform.x=parent->actor.transform.x;a->transform.y=parent->actor.transform.y;a->transform.priority=parent->actor.transform.priority;a->transform.facing=parent->actor.transform.facing;q->shield.delays=parent->shield.delays;
   if(q->shield.pose!=parent->shield.pose){q->shield.pose=parent->shield.pose;shield_start_entry(q,shield_poses[q->shield.pose&7]);}shield_animate(w,e);a->hit.flags&=247;if(a->hit.flags&1){struct NativeEntity*n=entity_get(p,e->parent);if(!n){enemy_fault(w);return;}schedule(n,LOST);schedule(e,DETACH);}if(!parent->actor.phase)schedule(e,DETACH);if(entity_parent_deleting(p,e))schedule(e,DELETE_TREE);return;
  }
  int r=interaction_move(w,a,s==DETACH||s==BOUNCE?WMOVE_ITEM:s==BULLET?WMOVE_POINT:WMOVE_SYNC);if(r<0)return;
  if(s==DETACH||s==BOUNCE){if(r)schedule(e,s==DETACH?BOUNCE:FADE);shield_animate(w,e);}
  else if(s==FADE){if(w->frame_phase&1)shield_animate(w,e);if(--q->shield.life<=0)schedule(e,DELETE_TREE);}
  else if(s==BULLET){if(r)schedule(e,DELETE_TREE);shield_animate(w,e);if(!enemy_attack(w,a))return;if(a->hit.flags&2)schedule(e,DELETE_TREE);}
  else if(s==MUZZLE){if(shield_animate(w,e))schedule(e,DELETE_TREE);}
  else{enemy_fault(w);return;}
  if(interaction_outside(w,a,boss_fx_bounds[8],1))schedule(e,DELETE_TREE);return;
 }enemy_fault(w);
}
struct NativeEntity*shield_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){if(id!=108&&id!=109){enemy_fault(w);return 0;}struct NativeEntity*n=enemy_allocate(w,parent,order,t,tick);if(!n)return 0;init(n,NEW);n->payload.enemy.shield.variant=id==109;if(params)for(unsigned i=0;i<8;i++)n->payload.enemy.params[i]=params[i];return n;}
