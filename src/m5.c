#include "m5_internal.h"
#include "slug_support.h"
/* $06F17E..$071068: semantic states, not source addresses. Animation and
 * authored flight programs contain bounded native indices only. */
static void tick(struct EntityPool*,struct NativeEntity*,void*);
uint16_t m5_random(struct WeaponWorld*w){if(!w->rng){enemy_fault(w);return 0;}return source_random_word(w->rng);}
int m5_sound(struct WeaponWorld*w,unsigned s){return interaction_sound(w,(uint16_t)s);}
int16_t m5_rank_value(struct WeaponWorld*w,struct EnemyActor*q,unsigned i){
 struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid||i>=M5R_COUNT){enemy_fault(w);return 0;}
 const struct M5Rank*t=&m5_rank[i];unsigned index=(interaction_rank(w,&q->actor,runtime_owner_m5[i]))*8+(q->source_flags&3)*2;
 if(t->choices){index+=((uint8_t)(r->player_count_flag-1)&1);return (int16_t)t->values[t->choices[index]*8+((m5_random(w)&14)>>1)];}
 return (int16_t)t->values[index+((r->player_count_flag&2)>>1)];
}
void m5_start_entry(struct EnemyActor*q,uint16_t v){if(v==65535)return;animation_begin(&q->actor.animation,v);q->actor.movement_flags|=32;}
void m5_start(struct EnemyActor*q,enum M5Animation v){m5_start_entry(q,m5_entries[v]);}
void m5_defense(struct EnemyActor*q,unsigned v){q->actor.defense=v==65535?0:m5_bindings[v].defense;q->actor.defense_count=v==65535?0:m5_bindings[v].count;}
void m5_reset(struct WeaponActor*a){a->vx=a->vy=a->ax=a->ay=0;a->transform.x=(a->transform.x>>8)*256;a->transform.y=(512-(int32_t)transform_y_up(&a->transform))*256;}
struct EnemyActor*m5_parent(struct WeaponWorld*w,struct NativeEntity*e){struct NativeEntity*p=entity_get(w->entities,e->parent);if(!p||p->kind!=ENTITY_ENEMY){enemy_fault(w);return 0;}return &p->payload.enemy;}
static void initialize_storage(struct EnemyActor*q){uint8_t*b=(uint8_t*)&q->m5;for(unsigned i=0;i<sizeof(q->m5);i++)b[i]=255;}
struct NativeEntity*m5_child(struct WeaponWorld*w,struct NativeEntity*e,unsigned state,enum EntityInsert order){
 struct NativeEntity*n=enemy_allocate(w,entity_ref(e),order,&e->payload.enemy.actor.transform,tick);if(!n)return 0;
 initialize_storage(&n->payload.enemy);enemy_schedule(&n->payload.enemy,state);return n;
}
void m5_follow(struct EnemyActor*q,const struct EnemyActor*p,int full,int facing,int dx,int dy,int priority){
 struct PlayerTransform*t=&q->actor.transform;const struct PlayerTransform*s=&p->actor.transform;
 if(full){t->x=s->x;t->y=s->y;}else transform_integer_offset(t,m5_word((s->x>>8)-(t->x>>8)),m5_word(transform_y_up(s)-transform_y_up(t)));
 if(facing)t->facing=s->facing;t->priority=(uint16_t)(s->priority+priority);transform_integer_offset(t,(int16_t)dx,(int16_t)dy);
}
int m5_random_target(struct WeaponWorld*w){int a=enemy_player_live(w->players[0]),b=enemy_player_live(w->players[1]);return a&&b?(m5_random(w)&1):b?1:0;}
int16_t m5_player_x(struct WeaponWorld*w,unsigned i){return i<2&&w->players[i]?m5_word((w->players[i]->actor.transform.x>>8)-w->camera_x):-1;}
int16_t m5_player_y(struct WeaponWorld*w,unsigned i){return i<2&&w->players[i]?m5_word(transform_y_up(&w->players[i]->actor.transform)+w->camera_y):-1;}
int m5_hit_sound(struct WeaponWorld*w,struct WeaponActor*a){return (a->hit.flags&1)||!(a->hit.events&1)||a->hit.last_kind!=1||m5_sound(w,0x108d);}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read(void*ctx,unsigned f,unsigned width){
 struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;
 if((f==M5F_76_2||f==M5F_34_2)&&width==2)return q->selector;
 if(f==M5F_3B_1&&width==1)return q->actor.animation.pose;
 if(f==M5F_3A_1&&width==1)return q->actor.transform.facing;enemy_fault(c->w);return 0;
}
static int write(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){
 struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;
 if(f==M5F_3A_1&&width==1){
  if(op==AN_FIELD_SET)q->actor.transform.facing=(uint8_t)v;else if(op==AN_FIELD_AND)q->actor.transform.facing&=(uint8_t)v;else if(op==AN_FIELD_OR)q->actor.transform.facing|=(uint8_t)v;else if(op==AN_FIELD_XOR)q->actor.transform.facing^=(uint8_t)v;else return enemy_fault(c->w);return 1;}
 if(f==M5F_3B_1&&width==1&&op==AN_FIELD_SET){q->actor.animation.pose=(uint8_t)v;return 1;}
 if(op!=AN_FIELD_SET)return enemy_fault(c->w);
 if(f==M5F_34_1&&width==1)q->m5.path.angle=(uint16_t)((q->m5.path.angle&255)|((v&255)<<8));
 else if(f==M5F_85_1&&width==1)q->m5.offset_y=(int8_t)v;
 else if(f==M5F_84_1&&width==1)q->m5.offset_x=(int8_t)v;
 else if(f==M5F_8A_1&&width==1)q->m5.crew=(uint8_t)v;
 else if(f==M5F_20_1&&width==1)q->actor.phase=(uint8_t)v;
 else if(f==M5F_76_2&&width==2)q->selector=(uint16_t)v;
 else return enemy_fault(c->w);return 1;
}
static int bind(void*ctx,unsigned kind,uint32_t value){
 struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;unsigned b=value==0xffffffffu?65535:value;
 if(b!=65535&&b>=sizeof(m5_bindings)/sizeof(*m5_bindings))return enemy_fault(c->w);
 if(kind==AN_DEFENSE)m5_defense(q,b);
 else if(kind==AN_CONTACT)q->m5.contact=(uint16_t)b;
 else if(kind==AN_ATTACK){if(b==65535)q->actor.attack=0;else{q->attack_set=(struct HitAttackSet){m5_bindings[b].attack,m5_bindings[b].count};q->actor.attack=&q->attack_set;}hit_begin_attack(&q->actor.hit);}
 else return enemy_fault(c->w);return 1;
}
static int sound(void*ctx,uint16_t s,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,s,stop)?1:enemy_fault(c->w);}
static int call(void*ctx,unsigned id,struct Animation*a){(void)a;struct Context*c=ctx;return m5_emit(c->w,c->e,id);}
int m5_animate(struct WeaponWorld*w,struct NativeEntity*e){
 struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;
 struct Context c={w,e};struct AnimationOwner o={.context=&c,.read=read,.write=write,.bind=bind,.call=call,.sound=sound};
 int done=animation_step(&a->animation,&m5_program,&o);if(a->animation.fault)enemy_fault(w);return done;
}
static int body_animation(struct WeaponWorld*w,struct NativeEntity*e){struct WeaponActor*a=&e->payload.enemy.actor;transform_integer_offset(&a->transform,0,44);int r=m5_animate(w,e);transform_integer_offset(&a->transform,0,-44);return r;}
static void contact(struct WeaponWorld*w,struct EnemyActor*q){
 if(q->m5.contact==65535)return;if(q->m5.contact>=sizeof(m5_bindings)/sizeof(*m5_bindings)||!m5_bindings[q->m5.contact].contact){enemy_fault(w);return;}
 interaction_body_contact(w,&q->actor,m5_bindings[q->m5.contact].contact);
}
static void target_position(struct WeaponWorld*w,struct EnemyActor*q,unsigned table){unsigned i=m5_random(w)&(table==4?3:7);q->m5.target_x=m5_targets[table][i][0];q->m5.target_y=m5_targets[table][i][1];}
static void motion_reset(struct EnemyActor*q,unsigned mode){q->m5.motion=(uint8_t)mode;q->m5.timeout=600;}
static int motion(struct WeaponWorld*w,struct EnemyActor*q){
 struct WeaponActor*a=&q->actor;unsigned mode=m5_motion[q->m5.motion&7];if(mode==3)return 0;
 if(mode>=4){unsigned phase=q->m5.sine;q->m5.sine+=8;a->vy=boss_trig[0][phase];if(mode==5)a->vy>>=1;a->vx=0;a->hit.flags|=64;
  if(interaction_move(w,a,WMOVE_FALL)>0)a->vy=a->ay=0;return 0;}
 int16_t dx=m5_word(q->m5.target_x-m5_sx(w,a)),dy=m5_word(q->m5.target_y-m5_sy(w,a)),sum=m5_word(dx+dy);
 if(sum<0)sum=m5_word(-sum);int threshold=mode==0?16:mode==1?8:1;
 if(sum<=threshold){q->m5.sine=0;return 1;}
 weapon_vector(enemy_angle(dx,dy),q->m5.speed,&a->vx,&a->vy);a->hit.flags|=64;interaction_move(w,a,WMOVE_FALL);
 q->m5.timeout=m5_word(q->m5.timeout-1);if(q->m5.timeout<=0){q->m5.sine=0;return 1;}return 0;
}
static void hit_tail(struct WeaponWorld*w,struct NativeEntity*e,int lethal){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(a->hit.flags&8){a->hit.flags&=247;if(!m5_hit_sound(w,a))return;
  if(a->hit.hp<=q->m5.threshold){if(q->m5.stage!=q->m5.previous){q->m5.previous=q->m5.stage;if(!m5_sound(w,0x1023)||!boss_fx_pattern(w,entity_ref(e),&a->transform,BP_M5_HIT))return;}
   if(q->m5.threshold==q->m5.segment){q->m5.threshold=0;q->m5.stage=2;}else{q->m5.threshold=q->m5.segment;q->m5.stage=1;}}}
 if(a->hit.flags&1){if(lethal)enemy_schedule(q,M5_DIE);else{a->hit.flags&=254;a->hit.hp=1;q->m5.stage=2;}}
}
static int has_slug(struct WeaponWorld*w){struct SlugSharedState*s=w->enemies->slug;return s&&s->present&&s->rider_heartbeat&&(slug_ridden_by(s,0)||slug_ridden_by(s,1));}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned state=a->state;q->entry=0;
 switch(state){
 case M5_ROOT:{
  a->palette=0x59;a->hit_palette=0x7f;m5_defense(q,M5_BODY_DEFENSE);interaction_priority(w,a,0x4000,20);
  unsigned row=(uint16_t)m5_rank_value(w,q,M5R_HP);if(row>=16)return enemy_fault(w);
  a->hit.hp=m5_hp[row][0];q->m5.threshold=m5_hp[row][1];q->m5.segment=m5_hp[row][2];q->m5.stage=0;q->m5.previous=255;q->m5.rotor=0;
  if(!m5_child(w,e,M5_ROTOR,ENTITY_FIRST_CHILD))return 0;
  for(unsigned i=0;i<3;i++){struct NativeEntity*n=m5_child(w,e,M5_PART,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.params[0]=(uint8_t)i;}
  q->m5.upgraded=0;if(!m5_stage_spawn(w,e))return 0;
  q->m5.speed=512;q->dead=0;m5_screen(w,a,0,352);q->m5.rotor=1;a->vx=1536;q->m5.gate=255;
  if(!m5_sound(w,0x10b9))return 0;target_position(w,q,2);m5_start(q,M5A_ENTRANCE);a->state=M5_ENTRANCE;break;
 }
 case M5_INTRO_LEFT:case M5_INTRO_RIGHT:
  if(!m5_sound(w,state==M5_INTRO_LEFT?0x10b5:0x10c3))return 0;
  if(state==M5_INTRO_LEFT){a->transform.facing|=1;a->vx=-1024;}else{a->transform.facing=0;a->vx=(int16_t)m5_intro_speed;}
  a->transform.priority=0;a->palette=0x12b;m5_start(q,M5A_INTRO);break;
 case M5_WAIT:q->m5.gate=0;q->m5.timer=120;m5_screen(w,a,408,320);q->m5.rotor=1;m5_reset(a);q->m5.crew=2;break;
 case M5_MUSIC:if(!m5_sound(w,0x10ba))return 0;a->state=M5_IDLE;/* FALLTHROUGH */
 case M5_IDLE:q->m5.target_x=160;q->m5.target_y=400;q->m5.timer=m5_rank_value(w,q,M5R_IDLE0+(q->m5.stage&3));q->m5.speed=512;motion_reset(q,0);m5_start(q,M5A_IDLE);break;
 case M5_PICK:case M5_PICK_SPECIAL:{
  q->m5.choice=state==M5_PICK_SPECIAL?4:m5_random(w)&3;q->m5.target=(uint8_t)m5_random_target(w);
  q->m5.side=m5_player_x(w,q->m5.target)>=m5_sx(w,a);unsigned choice=q->m5.choice;
  if(choice!=3&&choice!=4&&m5_player_y(w,q->m5.target)>=356)choice+=5;
  q->m5.target_x=m5_targets[q->m5.side][choice&7][0];q->m5.target_y=m5_targets[q->m5.side][choice&7][1];
  if(q->m5.choice!=3&&q->m5.choice!=4&&(m5_random(w)&1))q->m5.target_y=m5_word(q->m5.target_y+40);
  if(!m5_sound(w,0x1074))return 0;motion_reset(q,1);q->m5.speed=512;m5_start(q,M5A_PICK);
  if(q->m5.upgraded){q->m5.side+=2;q->m5.speed=1024;m5_start(q,M5A_PICK_UP);}break;
 }
 case M5_HOVER:q->m5.speed=256;motion_reset(q,2);m5_start_entry(q,m5_hover[(q->m5.side&1)+(q->m5.upgraded?2:0)]);break;
 case M5_ATTACK_A:a->phase=255;/* FALLTHROUGH */
 case M5_ATTACK_B:case M5_ATTACK_C:
  target_position(w,q,state==M5_ATTACK_C?2:3);motion_reset(q,0);if(!m5_sound(w,0x1075))return 0;
  m5_start(q,state==M5_ATTACK_A?M5A_ATTACK_A:state==M5_ATTACK_B?M5A_ATTACK_B:M5A_ATTACK_C);break;
 case M5_HOLD_A:q->m5.timer=180;break;
 case M5_END_A:case M5_END_B:if(!m5_sound(w,0x1075))return 0;m5_start(q,state==M5_END_A?M5A_END_A:M5A_END_B);break;
 case M5_CHARGE:target_position(w,q,4);q->m5.timer=300;q->m5.speed=512;motion_reset(q,0);m5_start(q,M5A_IDLE);break;
 case M5_BARRAGE:{int slug=has_slug(w);q->m5.count=(int8_t)(m5_rank_value(w,q,slug?M5R_SLUG_COUNT:M5R_ROCKET_COUNT)&127);q->m5.timer=m5_rank_value(w,q,slug?M5R_SLUG_WAIT:M5R_ROCKET_WAIT);if(!q->m5.count)q->m5.count=1;q->m5.motion=3;break;}
 case M5_BARRAGE_END:q->m5.motion=4;q->m5.timer=30;break;
 case M5_LOOP_B:q->m5.count=(int8_t)m5_rank_value(w,q,M5R_CREW_COUNT);q->m5.timer=45;break;
 case M5_LOOP_C:q->m5.count=127;q->m5.timer=45;motion_reset(q,0);break;
 case M5_DIE:{w->enemies->damage_enabled=0;a->vx=512;a->vy=-128;q->m5.stage=3;struct NativeEntity*n=boss_fx_pattern(w,entity_ref(e),&a->transform,BP_M5_DEATH);if(!n)return 0;n->payload.enemy.actor.transform.priority=0xc000;m5_start(q,M5A_DIE);break;}
 case M5_EXPLODE:{
  int16_t x=m5_sx(w,a);m5_screen(w,a,288,m5_sy(w,a));if(!m5_sound(w,0x1033))return 0;w->enemies->scene_flags[0]=4;w->enemies->scene_flags[1]=0;
  struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMOKE);if(!n)return 0;n->payload.enemy.actor.transform.priority++;
  for(unsigned i=0;i<32;i++){n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_METAL);if(!n)return 0;n->payload.enemy.actor.transform.priority++;}
  m5_screen(w,a,x,m5_sy(w,a));body_animation(w,e);enemy_schedule(q,M5_DELETE);return 2;
 }
 case M5_DELETE_TREE:entity_delete_children(w->entities,e);/* FALLTHROUGH */
 case M5_DELETE:entity_delete(w->entities,e);return 2;
 default:return m5_children_enter(w,e);
 }return !w->fault;
}
static void frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(state==M5_INTRO_LEFT||state==M5_INTRO_RIGHT){interaction_move(w,a,WMOVE_POINT);m5_animate(w,e);if(interaction_outside(w,a,m5_bounds[1],0))enemy_schedule(q,M5_DELETE_TREE);return;}
 if(state==M5_ENTRANCE){interaction_move(w,a,WMOVE_POINT);interaction_move(w,a,WMOVE_POINT);body_animation(w,e);if(m5_sx(w,a)>=408)enemy_schedule(q,M5_WAIT);return;}
 if(state==M5_WAIT){if(q->dead){q->m5.timer=m5_word(q->m5.timer-1);if(q->m5.timer<=0)enemy_schedule(q,M5_MUSIC);}return;}
 if(state==M5_DIE){a->hit.flags|=64;if(interaction_move(w,a,WMOVE_POINT)>0)a->vy=0;if(m5_sx(w,a)>=352){m5_reset(a);enemy_schedule(q,M5_EXPLODE);}if(m5_sy(w,a)<=320)a->vy=64;if(m5_sy(w,a)>=368)a->vy=-64;if(body_animation(w,e))enemy_schedule(q,M5_EXPLODE);return;}
 if(state<M5_IDLE||state>M5_LOOP_C){m5_children_frame(w,e,state);return;}
 contact(w,q);int reached=motion(w,q);
 if(reached){if(state==M5_CHARGE)enemy_schedule(q,M5_BARRAGE);else if(state==M5_PICK||state==M5_PICK_SPECIAL||state==M5_HOVER)q->m5.motion=3;else if(state==M5_LOOP_C){motion_reset(q,0);target_position(w,q,2);}else if(state!=M5_BARRAGE&&state!=M5_BARRAGE_END)q->m5.motion=4;}
 int done=body_animation(w,e);
 switch(state){
 case M5_IDLE:{
  q->m5.timer=m5_word(q->m5.timer-1);unsigned choice=0;if(q->m5.timer<=0){if(q->m5.stage>=2)choice=2;else{choice=(uint16_t)m5_rank_value(w,q,M5R_CHOICE1);if(q->m5.stage!=1)choice=(uint16_t)m5_rank_value(w,q,M5R_CHOICE0);}}
  if(choice>=8){enemy_fault(w);return;}static const uint8_t actions[]={M5_IDLE,M5_PICK,M5_ATTACK_C,M5_ATTACK_A,M5_CHARGE,M5_ATTACK_B,M5_PICK_SPECIAL};
  if(m5_actions[choice])enemy_schedule(q,actions[m5_actions[choice]]);break;
 }
 case M5_PICK:case M5_PICK_SPECIAL:if(done)enemy_schedule(q,M5_HOVER);break;
 case M5_HOVER:if(done){q->m5.upgraded=1;enemy_schedule(q,M5_IDLE);}break;
 case M5_ATTACK_A:if(done)enemy_schedule(q,M5_HOLD_A);break;
 case M5_HOLD_A:q->m5.timer=m5_word(q->m5.timer-1);if(q->m5.timer<=0||!a->phase)enemy_schedule(q,M5_END_A);break;
 case M5_END_A:case M5_END_B:if(done)enemy_schedule(q,M5_IDLE);break;
 case M5_CHARGE:q->m5.timer=m5_word(q->m5.timer-1);if(q->m5.timer<=0)enemy_schedule(q,M5_BARRAGE);break;
 case M5_BARRAGE:
  q->m5.timer=m5_word(q->m5.timer-1);if(q->m5.timer<=0){q->m5.timer=m5_rank_value(w,q,M5R_ROCKET_WAIT);if(has_slug(w))q->m5.timer=m5_rank_value(w,q,M5R_SLUG_WAIT);
   struct NativeEntity*n=m5_child(w,e,M5_ROCKET,ENTITY_FIRST_CHILD);if(!n)return;transform_integer_offset(&n->payload.enemy.actor.transform,-59,8);q->m5.count=(int8_t)(q->m5.count-1);if(q->m5.count<=0)enemy_schedule(q,M5_BARRAGE_END);}
  if(a->hit.flags&1)enemy_schedule(q,M5_BARRAGE_END);break;
 case M5_BARRAGE_END:q->m5.timer=m5_word(q->m5.timer-1);if(q->m5.timer<=0)enemy_schedule(q,M5_IDLE);break;
 case M5_ATTACK_B:if(done)enemy_schedule(q,M5_LOOP_B);break;
 case M5_LOOP_B:
  if(q->m5.crew==1&&q->m5.count>0){q->m5.crew=0;struct NativeEntity*n=m5_child(w,e,M5_CREW_ENTER,ENTITY_FIRST_CHILD);if(!n)return;n->payload.enemy.actor.display_flags|=2;}
  if(q->m5.count<=0){q->m5.timer=m5_word(q->m5.timer-1);if(q->m5.timer<=0)enemy_schedule(q,M5_END_B);}if(a->hit.flags&1)enemy_schedule(q,M5_END_B);break;
 case M5_ATTACK_C:if(done)enemy_schedule(q,M5_LOOP_C);break;
 case M5_LOOP_C:if(q->m5.stage>=2)m5_defense(q,M5_BODY_DEFENSE);else if(!q->m5.count){q->m5.timer=m5_word(q->m5.timer-1);if(q->m5.timer<=0)enemy_schedule(q,M5_END_B);}break;
 default:enemy_fault(w);return;
 }hit_tail(w,e,state==M5_LOOP_C);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 (void)p;struct WeaponWorld*w=world;struct EnemyActor*q=&e->payload.enemy;if(w->fault)return;
 /* $070B46/$070BA4/$070BE4 and $06FA06 bypass source camera
  * subtraction. The native actor uses world coordinates, so carry it by
  * the camera delta before testing screen-space targets, including the
  * stationary WAIT/HOLD paths. Freshly allocated actors are already synced. */
 if(q->actor.state>=M5_WAIT&&q->actor.state<=M5_EXPLODE&&!scene_preserve_screen(w,&q->actor))return;
 if(q->entry&&enter(w,e)!=1)return;frame(w,e,q->actor.state);
}
int m5_spawn(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){
 if(id<111||id>113)return 0;struct NativeEntity*e=enemy_allocate(w,p,order,t,tick);if(!e)return 0;initialize_storage(&e->payload.enemy);
 if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];enemy_schedule(&e->payload.enemy,id==111?M5_ROOT:id==112?M5_INTRO_LEFT:M5_INTRO_RIGHT);return 1;
}
