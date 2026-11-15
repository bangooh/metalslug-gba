#include "boss117_internal.h"
/* Template 117 / $081908..$0832CE. Typed states retain the original order
 * of body/part signals; source function addresses are not runtime state. */
static void tick(struct EntityPool*,struct NativeEntity*,void*);
void b117_schedule(struct NativeEntity*e,unsigned state){e->tick=tick;enemy_schedule(&e->payload.enemy,state);}
uint16_t b117_random(struct WeaponWorld*w){if(!w->rng){enemy_fault(w);return 0;}return source_random_word(w->rng);}
void b117_screen(struct WeaponWorld*w,struct WeaponActor*a,int x,int y){transform_integer_offset(&a->transform,b117_word(x-b117_sx(w,a)),b117_word(y-b117_sy(w,a)));}
int16_t b117_rank(struct WeaponWorld*w,struct EnemyActor*q,unsigned n){
 struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid||n>=B117R_COUNT){enemy_fault(w);return 0;}
 const struct Boss117RankData*t=&boss117_rank[n];unsigned i=(interaction_rank(w,&q->actor,runtime_owner_boss117[n]))*8+(q->source_flags&3)*2;
 if(t->choices)i=t->choices[i+((uint8_t)(r->player_count_flag-1)&1)]*8+((b117_random(w)&14)>>1);else i+=(r->player_count_flag&2)>>1;return (int16_t)t->values[i];
}
void b117_start_entry(struct EnemyActor*q,uint16_t n){if(n==65535)return;animation_begin(&q->actor.animation,n);q->actor.movement_flags|=32;}
void b117_start(struct EnemyActor*q,enum Boss117Animation n){b117_start_entry(q,boss117_entries[n]);}
void b117_defense(struct EnemyActor*q,unsigned n){q->actor.defense=n==65535?0:boss117_bindings[n].defense;q->actor.defense_count=n==65535?0:boss117_bindings[n].count;}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read_field(void*ctx,unsigned f,unsigned width){
 struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;
 if(width==2){if(f==B117F_5C_2)return (uint16_t)q->boss117.selector;if(f==B117F_5E_2)return (uint16_t)q->boss117.shot;}
 enemy_fault(c->w);return 0;
}
static int field(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){
 struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;
 if(op==AN_FIELD_XOR)v^=read_field(ctx,f,width);else if(op!=AN_FIELD_SET)return enemy_fault(c->w);
 if(width!=2)return enemy_fault(c->w);
 if(f==B117F_5C_2)q->boss117.selector=(int16_t)v;else if(f==B117F_5E_2)q->boss117.shot=(int16_t)v;else return enemy_fault(c->w);return 1;
}
static int bind(void*ctx,unsigned kind,uint32_t value){
 struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;unsigned n=value==0xffffffffu?65535:value;
 if(n!=65535&&n>=sizeof(boss117_bindings)/sizeof(*boss117_bindings))return enemy_fault(c->w);
 if(kind==AN_DEFENSE)b117_defense(q,n);
 else if(kind==AN_ATTACK){if(n==65535)q->actor.attack=0;else{q->attack_set=(struct HitAttackSet){boss117_bindings[n].attack,boss117_bindings[n].count};q->actor.attack=&q->attack_set;}hit_begin_attack(&q->actor.hit);}
 else return enemy_fault(c->w);return 1;
}
static int sound(void*ctx,uint16_t s,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,s,stop)?1:enemy_fault(c->w);}
static int call(void*ctx,unsigned n,struct Animation*anim){(void)anim;struct Context*c=ctx;return b117_callback(c->w,c->e,n);}
int b117_animate(struct WeaponWorld*w,struct NativeEntity*e){
 struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;struct Context c={w,e};
 struct AnimationOwner owner={.context=&c,.read=read_field,.write=field,.bind=bind,.call=call,.sound=sound};
 int done=animation_step(&a->animation,&boss117_program,&owner);if(a->animation.fault)enemy_fault(w);return done;
}
struct EnemyActor*b117_parent(struct WeaponWorld*w,struct NativeEntity*e){struct NativeEntity*p=entity_get(w->entities,e->parent);if(!p||p->kind!=ENTITY_ENEMY){enemy_fault(w);return 0;}return &p->payload.enemy;}
static void init(struct NativeEntity*e,unsigned state){
 struct EnemyActor*q=&e->payload.enemy;uint8_t*p=(uint8_t*)&q->boss117;for(unsigned i=0;i<sizeof(q->boss117);i++)p[i]=255;b117_schedule(e,state);
}
struct NativeEntity*b117_child(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct NativeEntity*n=enemy_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,tick);if(n)init(n,state);return n;
}
struct NativeEntity*b117_pattern(struct WeaponWorld*w,struct NativeEntity*e,unsigned pattern){
 if(pattern>=B117P_COUNT){enemy_fault(w);return 0;}
 struct NativeEntity*n=enemy_allocate(w,entity_ref(e),ENTITY_LAST_CHILD,&e->payload.enemy.actor.transform,tick);
 if(n){init(n,B117_PATTERN);n->payload.enemy.boss117.pattern=(uint16_t)pattern;}return n;
}
int b117_patterns(struct WeaponWorld*w,struct NativeEntity*e,unsigned first,unsigned second){
 struct NativeEntity*n=b117_pattern(w,e,first);if(!n)return 0;n->payload.enemy.actor.transform.priority=0xc000;
 n=b117_pattern(w,e,second);if(!n)return 0;n->payload.enemy.actor.transform.priority=0xc000;return 1;
}
int b117_follow(struct WeaponWorld*w,struct NativeEntity*e,int offset){
 struct EnemyActor*q=&e->payload.enemy,*p=b117_parent(w,e);if(!p)return 0;
 q->actor.transform.priority=p->actor.transform.priority;q->boss117.upgraded=p->boss117.upgraded;
 if(offset)q->boss117.target=p->boss117.target;
 b117_screen(w,&q->actor,p->boss117.anchor_x+(offset?q->boss117.anchor_x:0),p->boss117.anchor_y+(offset?q->boss117.anchor_y:0));return 1;
}
int b117_full_follow(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*p=b117_parent(w,e);if(!p)return 0;e->payload.enemy.actor.transform=p->actor.transform;return 1;}
int b117_relay(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy,*p=b117_parent(w,e);if(!p)return 0;
 if(q->actor.hit.events&1){q->actor.hit.events&=254;p->boss117.flags|=64;}if(p->boss117.flags&128)q->actor.hit.events|=1;return 1;
}
int b117_parent_dead(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*p=b117_parent(w,e);return p&&p->actor.phase;}
void b117_target(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy,*p=q->boss117.slot==255?q:b117_parent(w,e);if(!p)return;
 if(p->boss117.target<2&&enemy_player_live(w->players[p->boss117.target]))return;
 int i=enemy_nearest_x(w,&q->actor,0);p->boss117.target=i<0?1:(uint8_t)i;
}
static int16_t target_x(struct WeaponWorld*w,struct EnemyActor*q){
 struct PlayerBodyState*p=q->boss117.target<2?w->players[q->boss117.target]:0;
 return p?b117_word((p->actor.transform.x>>8)-w->camera_x):-1;
}
int b117_special_gate(struct WeaponWorld*w,struct EnemyActor*q){
 /* $05E42A probes the caller's $6B, despite taking a player index. */
 return (q->actor.hit.type_flags&1)&&(enemy_player_live(w->players[0])||enemy_player_live(w->players[1]));
}
int b117_aim(struct WeaponWorld*w,struct NativeEntity*e,int*delta){
 struct EnemyActor*q=&e->payload.enemy,*p=b117_parent(w,e);if(!p)return 0;
 struct PlayerBodyState*t=q->boss117.target<2?w->players[q->boss117.target]:0;
 int16_t tx=t?b117_word((t->actor.transform.x>>8)-w->camera_x):-1,ty=t?b117_word(transform_y_up(&t->actor.transform)+w->camera_y):-1;
 int goal=b117_word(enemy_angle(b117_word(tx-b117_sx(w,&q->actor)),b117_word(ty-b117_sy(w,&q->actor)-56))-36)>>3;
 *delta=0;if(goal==q->boss117.selector){p->boss117.aimed=1;return 0;}
 *delta=goal>q->boss117.selector?1:-1;int next=q->boss117.selector+*delta;if(next<0||next>6)*delta=0;p->boss117.aimed=0;return 1;
}
static void body_relay(struct EnemyActor*q){if(q->boss117.flags&64)q->actor.hit.events|=1;q->boss117.flags&=63;if(q->actor.hit.events&1)q->boss117.flags|=128;}
static int body_animation(struct WeaponWorld*w,struct EnemyActor*q){if((uint16_t)q->boss117.selector>=4)return enemy_fault(w);b117_start_entry(q,boss117_body[q->boss117.selector]);return 1;}
static void anchor(struct WeaponWorld*w,struct EnemyActor*q){q->boss117.bob=0;q->boss117.anchor_x=b117_sx(w,&q->actor);q->boss117.anchor_y=b117_sy(w,&q->actor);}
static int bob(struct WeaponWorld*w,struct EnemyActor*q,unsigned row){unsigned old=q->boss117.bob&15;q->boss117.bob=(q->boss117.bob+1)&15;q->boss117.anchor_x=b117_sx(w,&q->actor);q->boss117.anchor_y=b117_word(b117_sy(w,&q->actor)+boss117_bob[row][old]);return !q->boss117.bob;}
static int start_moving(struct WeaponWorld*w,struct EnemyActor*q){
 if(q->boss117.flags&32)return 0;int x=b117_sx(w,&q->actor);
 if(x<32){q->boss117.direction=0;return 1;}if(x>288){q->boss117.direction=1;return 1;}
 if(q->boss117.aimed)return 0;q->boss117.direction=target_x(w,q)>x?0:1;return 1;
}
static int stop_moving(struct WeaponWorld*w,struct EnemyActor*q){
 if(q->boss117.flags&32)return 1;int x=b117_sx(w,&q->actor);if(x>=32&&x<=288&&q->boss117.aimed)return 1;
 return q->boss117.direction?(x<48||target_x(w,q)>x):(x>288||target_x(w,q)<x);
}
static void body_tail(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(!(q->boss117.flags&7)){
  if((q->boss117.flags&16)&&q->boss117.aimed)q->boss117.flags|=4;
  else if(q->boss117.flags&8)q->boss117.flags|=b117_special_gate(w,q)||(b117_random(w)&7)?1:2;
 }
 if(!q->boss117.upgraded){unsigned sides=q->boss117.sides&3;
  if(!sides||b117_rank(w,q,sides==3?B117R_THRESHOLD_BOTH:B117R_THRESHOLD_ONE)>=a->hit.hp)q->boss117.upgraded=255;
 }
 a->hit.flags&=247;if(a->hit.flags&1)b117_schedule(e,B117_DIE);
}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;q->entry=0;struct NativeEntity*n;
 switch(a->state){
 case B117_ROOT:
  if(!interaction_sound(w,0x28))return 0;a->vx=a->vy=a->ax=a->ay=0;transform_integer_copy(&a->transform,&a->transform);
  n=b117_child(w,e,B117_GUARD);if(!n)return 0;n->payload.enemy.actor.transform=(struct PlayerTransform){0};
  if(!b117_child(w,e,B117_CENTRAL)||!b117_child(w,e,B117_SIDE_A)||!b117_child(w,e,B117_SIDE_B))return 0;
  for(int i=5;i>=0;i--){n=b117_child(w,e,B117_PLATE);if(!n)return 0;n->payload.enemy.boss117.slot=(uint8_t)i;}
  q->boss117.aimed=q->boss117.selector=q->boss117.flags=q->boss117.upgraded=a->phase=0;q->boss117.sides=3;
  b117_defense(q,B117B_BODY);interaction_priority(w,a,0x8000,20);a->palette=0x72;a->hit_palette=0x83;a->hit.hp=b117_rank(w,q,B117R_HP);
  {int i=enemy_nearest_x(w,a,0);q->boss117.target=i<0?1:(uint8_t)i;}a->state=B117_SELECT;
  /* Source initialization enters its first frame immediately. */
  /* fall through */
 case B117_SELECT:b117_target(w,e);return body_animation(w,q);
 case B117_PREPARE:q->boss117.bob=0;return body_animation(w,q);
 case B117_ACCEL:
  b117_target(w,e);q->boss117.speed=b117_rank(w,q,q->boss117.upgraded?B117R_SPEED_LATE:B117R_SPEED);a->vx=0;
  if(q->boss117.direction&1){q->boss117.speed=b117_word(-q->boss117.speed);a->ax=-32;b117_start(q,B117A_MOVE_LEFT);}
  else{a->ax=32;b117_start(q,B117A_MOVE_RIGHT);}return !w->fault;
 case B117_CRUISE:a->ax=0;a->vx=q->boss117.speed;return 1;
 case B117_BRAKE:a->ax=q->boss117.direction&1?96:-96;return 1;
 case B117_STOP:q->boss117.bob=0;a->ax=a->vx=0;return body_animation(w,q);
 case B117_DIE:
  if(!w->sound||!w->sound(w->scene,0x1071,1))return enemy_fault(w);
  q->boss117.flags=0;w->enemies->damage_enabled=0;a->phase=1;q->boss117.timer=180;b117_defense(q,65535);w->enemies->scene_flags[1]=1;
  if(!b117_child(w,e,B117_DEATH_OVERLAY)||!body_animation(w,q))return 0;return interaction_sound(w,0x1033);
 case B117_GUARD:return 1;
 default:return enemy_fault(w);
 }
}
static void frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(state==B117_GUARD){struct EnemyActor*p=b117_parent(w,e);if(!p)return;
  if(b117_sx(w,&p->actor)<=288){p->actor.hit.invulnerable=0;entity_delete(w->entities,e);}else{p->actor.hit.invulnerable=2;w->enemies->intro_gate=0;}return;
 }
 if(state==B117_DIE){
  if(interaction_move(w,a,WMOVE_SYNC)<0)return;anchor(w,q);b117_animate(w,e);q->boss117.timer=b117_word(q->boss117.timer-1);
  if(q->boss117.timer==60&&!b117_pattern(w,e,B117P_PARACHUTE))return;if(q->boss117.timer==110&&!interaction_sound(w,0x1032))return;
  if(!q->boss117.timer)w->enemies->scripted_player_lock=0;return;
 }
 if(state==B117_SELECT||state==B117_PREPARE||state==B117_STOP){
  if(interaction_move(w,a,WMOVE_SYNC)<0)return;body_relay(q);
  if(state==B117_SELECT){anchor(w,q);b117_animate(w,e);if(start_moving(w,q))b117_schedule(e,B117_PREPARE);}
  else{if(bob(w,q,state==B117_PREPARE?1:2))b117_schedule(e,state==B117_PREPARE?B117_ACCEL:B117_SELECT);b117_animate(w,e);}
 }else{
  /* Both original acceleration branches use the same signed comparison. */
  if(state==B117_ACCEL&&q->boss117.speed>=a->vx)b117_schedule(e,B117_CRUISE);
  if(state==B117_BRAKE){if(a->vx>=0)b117_schedule(e,B117_STOP);}
  else if(stop_moving(w,q))b117_schedule(e,B117_BRAKE);
  if(interaction_move(w,a,WMOVE_POINT)<0)return;body_relay(q);bob(w,q,0);b117_animate(w,e);
 }
 body_tail(w,e);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;
 for(unsigned budget=0;budget<8;budget++){
  unsigned s=q->actor.state;
  if(s==B117_DELETE||s==B117_DELETE_TREE){if(s==B117_DELETE_TREE)entity_delete_children(p,e);entity_delete(p,e);return;}
  if(q->entry){int r=s<=B117_GUARD?enter(w,e):s<B117_TROOP?b117_parts_enter(w,e):b117_effects_enter(w,e);if(!r||w->fault)return;if(r==2)continue;}
  if(!e->active||e->deleting)return;s=q->actor.state;
  if(s<=B117_GUARD){frame(w,e,s);return;}
  int r=s<B117_TROOP?b117_parts_frame(w,e,s):b117_effects_frame(w,e,s);if(r!=2)return;
 }enemy_fault(w);
}
int boss117_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,const struct PlayerTransform*t,const uint8_t params[8]){
 struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;init(e,B117_ROOT);
 if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];return 1;
}
