#include "m4_internal.h"
/* $085190..$085EE8. All actors share typed animation/hit services. */
static void tick(struct EntityPool*,struct NativeEntity*,void*);
void m4_reset(struct WeaponActor*a){a->vx=a->vy=a->ax=a->ay=0;transform_integer_copy(&a->transform,&a->transform);}
void m4_screen(struct WeaponWorld*w,struct WeaponActor*a,int16_t x,int16_t y){transform_integer_offset(&a->transform,m4_word(x-m4_sx(w,a)),m4_word(y-m4_sy(w,a)));}
/* $044022/$04400E use background106F70/74, not camera106F50/54. */
int m4_project(struct WeaponWorld*w,struct WeaponActor*a,int16_t x,int16_t y){if(!w->background_view_valid)return enemy_fault(w);m4_screen(w,a,m4_word(x-w->background_x_offset),m4_word(512-y+w->background_y_offset));return 1;}
int m4_secondary_y(struct WeaponWorld*w,struct WeaponActor*a,int16_t*out){if(!w->background_view_valid)return enemy_fault(w);*out=m4_word(512-m4_sy(w,a)+w->background_y_offset);return 1;}
int16_t m4_rank_value(struct WeaponWorld*w,struct EnemyActor*q,unsigned n){struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid||n>=M4R_COUNT){enemy_fault(w);return 0;}return (int16_t)m4_rank[n][(interaction_rank(w,&q->actor,runtime_owner_m4[n]))*8+(q->source_flags&3)*2+((r->player_count_flag&2)>>1)];}
void m4_start_entry(struct EnemyActor*q,uint16_t n){if(n==65535)return;animation_begin(&q->actor.animation,n);q->actor.movement_flags|=32;}
void m4_start(struct EnemyActor*q,enum M4Animation n){m4_start_entry(q,m4_entries[n]);}
void m4_defense(struct EnemyActor*q,unsigned n){q->actor.defense=n==65535?0:m4_bindings[n].defense;q->actor.defense_count=n==65535?0:m4_bindings[n].count;}
void m4_attack(struct EnemyActor*q,unsigned n){if(n==65535)q->actor.attack=0;else{q->attack_set=(struct HitAttackSet){m4_bindings[n].attack,m4_bindings[n].count};q->actor.attack=&q->attack_set;}}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;int free_actor;};
static uint32_t read(void*ctx,unsigned f,unsigned width){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;if(c->free_actor){if(f==M4F_3B&&width==1)return q->actor.animation.pose;if(f==M4F_5C&&width==4)return q->free104.delays;if(f==M4F_78&&width==2)return (uint16_t)q->free104.phase;if(f==M4F_3A&&width==1)return q->actor.transform.facing;}enemy_fault(c->w);return 0;}
static int write(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;if(!c->free_actor)return enemy_fault(c->w);if(op==AN_FIELD_XOR)v^=read(ctx,f,width);else if(op!=AN_FIELD_SET)return enemy_fault(c->w);if(f==M4F_3B&&width==1)q->actor.animation.pose=(uint8_t)v;else if(f==M4F_3A&&width==1)q->actor.transform.facing=(uint8_t)v;else if(f==M4F_5C&&width==4&&v<=65535)q->free104.delays=(uint16_t)v;else return enemy_fault(c->w);return 1;}
static int delay(void*ctx,unsigned field,unsigned pose,uint8_t*value){struct Context*c=ctx;unsigned row=c->e->payload.enemy.free104.delays;if(!c->free_actor||field!=M4F_5C||row>=4||pose>=12)return enemy_fault(c->w);*value=m4_free_delays[row][pose];return 1;}
static int bind(void*ctx,unsigned kind,uint32_t v){struct Context*c=ctx;unsigned n=v==0xffffffffu?65535:v;if(n!=65535&&n>=sizeof(m4_bindings)/sizeof(*m4_bindings))return enemy_fault(c->w);struct EnemyActor*q=&c->e->payload.enemy;if(kind==AN_DEFENSE)m4_defense(q,n);else if(kind==AN_ATTACK){m4_attack(q,n);hit_begin_attack(&q->actor.hit);}else return enemy_fault(c->w);return 1;}
static int sound(void*ctx,uint16_t s,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,s,stop)?1:enemy_fault(c->w);}
static int call(void*ctx,unsigned n,struct Animation*a){(void)a;struct Context*c=ctx;if(c->free_actor)return free104_callback(c->w,c->e,n);if(n==M4C_85BD8||n==M4C_85B7E)return m4_palette(c->w,n==M4C_85BD8);return enemy_fault(c->w);}
int m4_animate(struct WeaponWorld*w,struct NativeEntity*e,int free_actor){struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;struct Context c={w,e,free_actor};struct AnimationOwner o={.context=&c,.read=read,.write=write,.bind=bind,.sound=sound,.call=call,.delay=delay};int done=animation_step(&a->animation,&m4_program,&o);if(a->animation.fault)enemy_fault(w);return done;}
int m4_patch(struct WeaponWorld*w,unsigned n){return n<22&&scene_video_patch(w->video,&m4_patches[n])?1:enemy_fault(w);}
int m4_palette(struct WeaponWorld*w,int alternate){if(!w->video||!w->video->palette_cycle)return enemy_fault(w);for(unsigned i=0;i<4;i++)if(!w->video->palette_cycle(w->video->palette_context,0x4e + i,scene_palette_high(alternate?(i?0x1fb+i:0x1f7):0x1f8+i),65535,1))return enemy_fault(w);return 1;}
int m4_hurt(struct WeaponWorld*w,struct WeaponActor*a){return !(a->hit.flags&1)&&a->hit.last_kind==1?interaction_sound(w,0x108d):1;}
int m4_has_children(struct WeaponWorld*w,const struct NativeEntity*e){return e->next!=ENTITY_NONE&&w->entities->nodes[e->next].depth>e->depth;}
int m4_in_box(struct WeaponWorld*w,const struct WeaponActor*a,const struct PlayerBodyState*p,const int16_t r[4]){(void)w;if(!p)return 0;int16_t dx=m4_word((p->actor.transform.x>>8)-(a->transform.x>>8)),dy=m4_word(transform_y_up(&p->actor.transform)-transform_y_up(&a->transform));if(a->transform.facing&1)dx=m4_word(-dx);if(a->transform.facing&2)dy=m4_word(-dy);return dx>=r[0]&&dx<=r[1]&&dy>=r[2]&&dy<=r[3];}
int m4_near_box(struct WeaponWorld*w,struct WeaponActor*a,unsigned n){if(n>=3)return enemy_fault(w);for(unsigned i=0;i<2;i++)if(enemy_player_live(w->players[i])&&m4_in_box(w,a,w->players[i],m4_boxes[n]))return 1;return 0;}
struct EnemyActor*m4_parent(struct WeaponWorld*w,struct NativeEntity*e){struct NativeEntity*p=entity_get(w->entities,e->parent);if(!p||p->kind!=ENTITY_ENEMY){enemy_fault(w);return 0;}return &p->payload.enemy;}
static void init(struct NativeEntity*e,unsigned s){uint8_t*p=(uint8_t*)&e->payload.enemy.m4;for(unsigned i=0;i<sizeof(e->payload.enemy.m4);i++)p[i]=255;enemy_schedule(&e->payload.enemy,s);}
struct NativeEntity*m4_child(struct WeaponWorld*w,struct NativeEntity*e,unsigned s,enum EntityInsert order){struct NativeEntity*n=enemy_allocate(w,entity_ref(e),order,&e->payload.enemy.actor.transform,tick);if(n)init(n,s);return n;}
struct NativeEntity*m4_pattern(struct WeaponWorld*w,struct NativeEntity*e,unsigned n){if(n>=7){enemy_fault(w);return 0;}struct NativeEntity*v=m4_child(w,e,M4_PATTERN,ENTITY_LAST_CHILD);if(v)v->payload.enemy.m4.pattern=(uint8_t)n;return v;}
static int watch_stop(const struct NativeEntity*e){return e->kind==ENTITY_ENEMY?e->payload.enemy.m4.watch:-1;}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*n;struct EnemyActor*p;
 switch(a->state){
 case M4_BODY:a->palette=0x1c5;a->hit.hp=m4_rank_value(w,q,M4R_HP);if(!m4_child(w,e,M4_TURRET,ENTITY_FIRST_CHILD))return 0;a->phase=0;q->dead=0;q->m4.hurt=0;break;
 case M4_APPROACH:n=m4_child(w,e,M4_DROPPER,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.m4.variant=1;m4_defense(q,M4B_BODY);m4_start(q,M4A_BODY);break;
 case M4_FIGHT:if(!interaction_sound(w,0x102d)||!m4_patch(w,0))return 0;break;
 case M4_WRECK:m4_attack(q,65535);hit_begin_attack(&a->hit);if(!interaction_sound(w,0x1028)||!m4_palette(w,0))return 0;q->m4.watch=0;if(!mission_event_spawn(w,entity_ref(e),0xec6c8,watch_stop)||!m4_patch(w,1))return 0;break;
 case M4_TURRET:{
  n=m4_child(w,e,M4_WHEEL,ENTITY_FIRST_CHILD);p=m4_parent(w,e);if(!n||!p)return 0;transform_integer_copy(&n->payload.enemy.actor.transform,&p->actor.transform);
  /* The wheel only receives XY; the allocator's other defaults survive. */
  n->payload.enemy.actor.transform.priority=0;n->payload.enemy.actor.transform.facing=0;n->payload.enemy.actor.transform.display_group=0;n->payload.enemy.source_flags=0;
  if(!m4_project(w,a,256,758))return 0;a->palette=0x1c8;m4_start(q,M4A_TURRET);a->transform.priority=256;break;
 }
 case M4_ACTIVE:m4_reset(a);a->display_flags&=253;m4_attack(q,65535);hit_begin_attack(&a->hit);if(!m4_child(w,e,M4_MOUNT,ENTITY_FIRST_CHILD))return 0;break;
 case M4_DEAD:m4_defense(q,65535);if(!interaction_sound(w,0x1033)||!m4_pattern(w,e,4))return 0;m4_reset(a);a->ay=-32;m4_start(q,M4A_TURRET_DEAD);break;
 case M4_MOUNT:a->transform.priority=0;a->transform.facing=0;a->transform.display_group=0;q->source_flags=0;m4_start(q,M4A_MOUNT);break;
 case M4_WHEEL:a->palette=0x1d0;transform_integer_offset(&a->transform,48,97);q->m4.pose=0;m4_start_entry(q,m4_wheel[0]);a->transform.priority=0;break;
 case M4_DROPPER:a->transform.priority=0;a->transform.facing=0;a->transform.display_group=0;q->source_flags=0;if(!m4_project(w,a,384,q->m4.variant?767:600))return 0;q->m4.timer=0;break;
 case M4_PUFF:q->m4.timer=2;break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static int turret(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;int16_t y=0;
 interaction_body_contact(w,a,&m4_contact);if(w->fault||interaction_move(w,a,WMOVE_POINT)<0||!m4_secondary_y(w,a,&y))return 0;
 if(a->vy<0&&y>=784){struct NativeEntity*n=m4_pattern(w,e,5);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,0,-8);n=m4_pattern(w,e,6);if(!n)return 0;n->payload.enemy.actor.transform.priority-=8;q->m4.shake=10;if(!m4_child(w,e,M4_PUFF,ENTITY_FIRST_CHILD))return 0;}
 if((int8_t)q->m4.shake>0){q->m4.shake--;transform_integer_offset(&a->transform,0,q->m4.shake&1?-1:1);}m4_animate(w,e,0);
 m4_defense(q,M4B_TURRET);a->hit.hp=32767;
 if(!m4_secondary_y(w,a,&y))return 0;
 if(a->hit.flags&8){a->vy=m4_rank_value(w,q,M4R_RECOIL);m4_start(q,M4A_RECOIL);a->hit.flags&=247;if(!interaction_sound(w,0x108d))return 0;if(y>720){q->m4.direction<<=1;return 1;}}
 else if(y>=784){q->m4.direction=(q->m4.direction<<1)|1;a->vy=0;return 1;}
 a->vy=m4_word(-m4_rank_value(w,q,M4R_RISE));q->m4.direction<<=1;return !w->fault;
}
static int frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;struct NativeEntity*n;
 if(s==M4_BODY){if((int16_t)w->mission_progress>496){enemy_schedule(q,M4_APPROACH);return 2;}w->enemies->intro_gate=0;return interaction_move(w,a,WMOVE_SYNC)>=0;}
 if(s==M4_APPROACH||s==M4_FIGHT){
  w->enemies->intro_gate=0;if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;q->m4.hurt=0;
  if(a->hit.flags&8){q->m4.hurt=255;m4_start(q,s==M4_APPROACH?M4A_BODY_FIRST_HIT:M4A_BODY_HIT);if(!m4_hurt(w,a))return 0;a->hit.flags&=247;if(s==M4_APPROACH){if(!m4_pattern(w,e,0))return 0;enemy_schedule(q,M4_FIGHT);}}
  if(s==M4_FIGHT&&(a->hit.flags&1)){m4_defense(q,65535);a->phase=255;if(!terrain_stamp(w->interactions->terrain,&a->transform,m4_stamp[0],m4_stamp[1],m4_stamp[2],m4_stamp[3],m4_stamp_cells))return enemy_fault(w);if(!m4_pattern(w,e,1)||!m4_pattern(w,e,2))return 0;q->m4.watch=255;m4_attack(q,M4B_BLAST);hit_begin_attack(&a->hit);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;enemy_schedule(q,M4_WRECK);}
  m4_animate(w,e,0);return !w->fault;
 }
 if(s==M4_WRECK){w->enemies->intro_gate=0;if(q->dead==255)entity_delete(w->entities,e);return 1;}
 if(s==M4_TURRET){if((int16_t)w->mission_progress>496){enemy_schedule(q,M4_ACTIVE);return 2;}interaction_body_contact(w,a,&m4_contact);if(w->fault||interaction_move(w,a,WMOVE_SYNC)<0)return 0;/* $08540A draws even while the lift approaches. */m4_animate(w,e,0);return !w->fault;}
 if(s==M4_ACTIVE){if(!turret(w,e))return 0;p=m4_parent(w,e);if(!p)return 0;if(p->actor.phase==255){m4_defense(q,65535);if(!m4_pattern(w,e,3))return 0;enemy_schedule(q,M4_DEAD);}return !w->fault;}
 if(s==M4_DEAD){if(interaction_move(w,a,WMOVE_POINT)<0)return 0;m4_animate(w,e,0);if(m4_sy(w,a)<=32){p=m4_parent(w,e);if(!p)return 0;p->dead=255;entity_delete(w->entities,e);}return !w->fault;}
 if(s==M4_MOUNT){p=m4_parent(w,e);if(!p)return 0;transform_integer_copy(&a->transform,&p->actor.transform);m4_animate(w,e,0);if(a->hit.flags&8){if(!m4_hurt(w,a))return 0;a->hit.flags&=247;}n=entity_get(w->entities,e->parent);p=m4_parent(w,n);if(!p)return 0;if(p->dead==255)entity_delete(w->entities,e);return !w->fault;}
 if(s==M4_WHEEL){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;n=entity_get(w->entities,e->parent);if(!n)return enemy_fault(w);struct EnemyActor*tur=&n->payload.enemy;p=m4_parent(w,n);if(!p)return 0;a->palette=p->m4.hurt==255?0x1d1:0x1d0;
  if(tur->actor.vy<0){if(m4_animate(w,e,0)){q->m4.pose=(q->m4.pose+1)&3;m4_start_entry(q,m4_wheel[q->m4.pose]);}}
  else{if(tur->actor.vy>0){q->m4.pose=(q->m4.pose-1)&3;m4_start_entry(q,m4_wheel_back[q->m4.pose]);}else m4_start_entry(q,m4_wheel[q->m4.pose]);m4_animate(w,e,0);}if(p->actor.phase==255)entity_delete(w->entities,e);return !w->fault;
 }
 if(s==M4_DROPPER){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;p=m4_parent(w,e);if(!p)return 0;if(p->actor.phase==255){entity_delete(w->entities,e);return 1;}if(m4_has_children(w,e))return 1;if(q->m4.timer>0){q->m4.timer--;return 1;}
  uint8_t params[8]={15,0,0,1,30,255,255,255};struct PlayerTransform t=a->transform;t.display_group=0;n=free104_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,104,&t,q->m4.variant?params:0);if(!n)return 0;q->m4.timer=m4_rank_value(w,q,M4R_DROP);return !w->fault;
 }
 if(s==M4_PUFF){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;m4_attack(q,M4B_PUFF);hit_begin_attack(&a->hit);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;if(--q->m4.timer<0)entity_delete(w->entities,e);return 1;}
 return enemy_fault(w);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;
 for(unsigned budget=0;budget<8;budget++){unsigned s=q->actor.state;if(s==M4_DELETE){entity_delete(p,e);return;}if(q->entry){q->entry=0;int r=s<M4_RAIL_ROW?enter(w,e):m4_scene_enter(w,e);if(!r||w->fault)return;if(r==2)continue;}if(!e->active||e->deleting)return;s=q->actor.state;int r=s<M4_RAIL_ROW?frame(w,e,s):m4_scene_frame(w,e,s);if(r!=2)return;}enemy_fault(w);
}
int m4_spawn_template(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){
 unsigned s;if(id==263)s=M4_FLOOR;else if(id==264)s=M4_WATER_PAIR;else if(id==265)s=M4_DEBRIS_NEW;else if(id==266)s=M4_RAIL_ROW;else if(id==268)s=M4_BODY;else return enemy_fault(w);struct NativeEntity*n=enemy_allocate(w,parent,order,t,tick);if(!n)return 0;init(n,s);if(params)for(unsigned i=0;i<8;i++)n->payload.enemy.params[i]=params[i];return 1;
}
