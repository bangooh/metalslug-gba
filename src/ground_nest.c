#include "tank_internal.h"
#include "m4_internal.h"
#include "slug_surface.h"
#include "items.h"
#include "../generated/ground-nest-programs.h"
/* $0619CA..$062922: templates 50..52 share the body controller. All
 * attached crew, missiles, shells, debris, stamps and ordered callbacks
 * use the same native entity tree as the original source. */
enum State {NEW,DRIVE,AVOID,IDLE,TURN,BRAKE,RESUME,AIM,FIRE_FIRST,FIRE,HIT,HIT_BIG,DEAD,WRECK,DEAD_BIG,REMOVE,
 CREW,CREW_A,CREW_NEXT,CREW_HELMET,CREW_OUT,CREW_EXIT,CREW_HIT,ROCKET,PIECE,HELMET,MUZZLE,FLAME,FLASH,SMOKE,SHOT,BLAST,DELETE,DELETE_TREE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static void schedule(struct NativeEntity*e,unsigned s){e->tick=tick;enemy_schedule(&e->payload.enemy,s);}
static void init(struct NativeEntity*e,unsigned s){uint8_t*p=(uint8_t*)&e->payload.enemy.ground_nest;for(unsigned i=0;i<sizeof(e->payload.enemy.ground_nest);i++)p[i]=255;schedule(e,s);}
static struct NativeEntity*child(struct WeaponWorld*w,struct NativeEntity*e,unsigned s,enum EntityInsert o){struct NativeEntity*n=enemy_allocate(w,entity_ref(e),o,&e->payload.enemy.actor.transform,tick);if(n)init(n,s);return n;}
static void start(struct EnemyActor*q,unsigned n){animation_begin(&q->actor.animation,ground_nest_entries[n]);q->actor.movement_flags|=32;}
static void defense(struct EnemyActor*q,unsigned n){q->actor.defense=n==65535?0:ground_nest_bindings[n].defense;q->actor.defense_count=n==65535?0:ground_nest_bindings[n].count;}
static void attack(struct EnemyActor*q,unsigned n){if(n==65535)q->actor.attack=0;else{q->attack_set=(struct HitAttackSet){ground_nest_bindings[n].attack,ground_nest_bindings[n].count};q->actor.attack=&q->attack_set;}}
static int16_t rank(struct WeaponWorld*w,struct EnemyActor*q,unsigned n){struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid||n>=GNR_COUNT){enemy_fault(w);return 0;}return (int16_t)ground_nest_rank[n][(interaction_rank(w,&q->actor,runtime_owner_ground_nest[n]))*8+(q->source_flags&3)*2+((r->player_count_flag&2)>>1)];}
static unsigned random_delay(struct WeaponWorld*w,struct EnemyActor*q){struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid||!w->rng)return enemy_fault(w);unsigned row=ground_nest_rank_choices[(interaction_rank(w,&q->actor,runtime_owner_ground_nest[6]))*8+(q->source_flags&3)*2+((uint8_t)(r->player_count_flag-1)&1)];if(row>=8)return enemy_fault(w);return ground_nest_rank_values[row][(source_random_word(w->rng)&14)>>1]&3;}
static int stamp(struct WeaponWorld*w,struct EnemyActor*q){if(q->ground_nest.stamp>=2)return enemy_fault(w);const struct GroundNestStamp*s=&ground_nest_stamps[q->ground_nest.stamp];return terrain_stamp(w->interactions->terrain,&q->actor.transform,s->dx,s->dy,s->width,s->height,s->cells)?1:enemy_fault(w);}
static int outside(struct WeaponWorld*w,struct WeaponActor*a,int local){return interaction_outside(w,a,local?boss_fx_bounds[8]:ground_nest_bounds,local);}
static int stop_sound(struct WeaponWorld*w){return w->sound&&w->sound(w->scene,0x109d,1)?1:enemy_fault(w);}
static int special(struct WeaponActor*a){unsigned n=a->hit.last_kind;return n==2||n==3||n==21||n==22||n==23||n==24||n==30;}
static int score(struct WeaponWorld*w,struct WeaponActor*a){if((a->display_flags&2)||(int8_t)a->score_owner<0)return 1;return w->items&&item_score_add(w->items,a->score_owner,enemy_hit_scores[a->hit.last_kind&31])?1:enemy_fault(w);}
static int remove_fx(struct WeaponWorld*w,struct NativeEntity*e){if(!interaction_sound(w,0x1033)||!boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,BFX_SMOKE))return 0;for(unsigned i=0;i<17;i++){struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,BFX_METAL_HEAVY);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,0,24);}return 1;}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read(void*ctx,unsigned f,unsigned width){struct Context*c=ctx;struct WeaponActor*a=&c->e->payload.enemy.actor;if(width==1){if(f==GNF_3B)return a->animation.pose;if(f==GNF_3A)return a->transform.facing;}if(width==2){if(f==GNF_2E)return (uint16_t)a->ay;if(f==GNF_2A)return (uint16_t)a->vy;}enemy_fault(c->w);return 0;}
static int write(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){struct Context*c=ctx;struct WeaponActor*a=&c->e->payload.enemy.actor;
 if(f==GNF_3A&&width==1&&op==AN_FIELD_XOR){a->transform.facing^=(uint8_t)v;return 1;}if(op!=AN_FIELD_SET)return enemy_fault(c->w);
 if(f==GNF_3B&&width==1)a->animation.pose=(uint8_t)v;else if(f==GNF_2E&&width==2)a->ay=(int16_t)v;else if(f==GNF_2A&&width==2)a->vy=(int16_t)v;else return enemy_fault(c->w);return 1;}
static int delay(void*ctx,unsigned f,unsigned pose,uint8_t*out){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;unsigned mode=q->ground_nest.delay_mode,row=q->ground_nest.delay_row;if(f!=GNF_5C||row>=4||mode>2||pose>=(mode==2?13:6))return enemy_fault(c->w);*out=mode==2?ground_nest_avoid_delays[row][pose]:ground_nest_delays[mode][row][pose];return 1;}
static int bind(void*ctx,unsigned kind,uint32_t v){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;unsigned n=v==0xffffffffu?65535:v;if(n!=65535&&n>=sizeof(ground_nest_bindings)/sizeof(*ground_nest_bindings))return enemy_fault(c->w);if(kind==AN_DEFENSE)defense(q,n);else if(kind==AN_ATTACK){attack(q,n);hit_begin_attack(&q->actor.hit);}else return enemy_fault(c->w);return 1;}
static int sound(void*ctx,uint16_t s,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,s,stop)?1:enemy_fault(c->w);}
static int call(void*ctx,unsigned cb,struct Animation*an){(void)an;struct Context*c=ctx;struct WeaponWorld*w=c->w;struct NativeEntity*e=c->e,*n;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 switch(cb){
 case GNC_283CA:hit_begin_attack(&a->hit);return 1;
 case GNC_62774:interaction_priority(w,a,0x4000,20);if(!q->ground_nest.temporary){attack(q,65535);if(!stamp(w,q))return 0;attack(q,GNB_WRECK);hit_begin_attack(&a->hit);return enemy_attack(w,a);}return 1;
 case GNC_627CA:
  if(!interaction_sound(w,0x1067))return 0;n=child(w,e,SHOT,ENTITY_LAST_CHILD);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,a->transform.facing&1?30:-30,44);
  n=child(w,e,SMOKE,ENTITY_LAST_CHILD);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,0,10);return 1;
 case GNC_62816:if(!interaction_sound(w,0x1033))return 0;return boss_fx_pattern(w,entity_ref(e),&a->transform,BP_GROUND_NEST_DEATH)!=0;
 case GNC_62872:n=child(w,e,CREW,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.params[2]=q->params[2];return 1;
 case GNC_6288A:return child(w,e,FLAME,ENTITY_FIRST_CHILD)!=0;
 case GNC_6289C:return item_spawn_drop(w,&a->transform,q->params[3],q->params[4],q->params[5],1,0);
 case GNC_628AC:
  if(!interaction_sound(w,0x10aa)||!child(w,e,MUZZLE,ENTITY_FIRST_CHILD))return 0;n=child(w,e,ROCKET,ENTITY_FIRST_CHILD);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,a->transform.facing&1?16:-16,16);return 1;
 case GNC_62910:return child(w,e,FLASH,ENTITY_FIRST_CHILD)!=0;
 default:return enemy_fault(w);
 }}
static int animate(struct WeaponWorld*w,struct NativeEntity*e){struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;struct Context c={w,e};struct AnimationOwner o={.context=&c,.read=read,.write=write,.delay=delay,.bind=bind,.sound=sound,.call=call};int done=animation_step(&a->animation,&ground_nest_program,&o);if(a->animation.fault)enemy_fault(w);return done;}
static int gate(struct WeaponWorld*w,struct EnemyActor*q){q->ground_nest.timer=tk_word(q->ground_nest.timer-1);if(q->ground_nest.timer<=0&&tk_sx(w,&q->actor)<=288){q->ground_nest.timer=0;return 1;}return 0;}
static int avoid(struct WeaponWorld*w,struct EnemyActor*q){if(q->ground_nest.avoid>0){q->ground_nest.avoid--;return 0;}/* $05E5A8: C only if no live player is in front. */for(unsigned i=0;i<2;i++)if(enemy_player_live(w->players[i])){int16_t dx=tk_word((q->actor.transform.x>>8)-(w->players[i]->actor.transform.x>>8));if(q->actor.transform.facing&1?dx<=0:dx>=0)return 0;}return 1;}
static int past(struct WeaponWorld*w,struct EnemyActor*q){uint16_t x=(uint16_t)tk_sx(w,&q->actor);return q->ground_nest.direction&1?x>296:x<24;}
static void delay_row(struct EnemyActor*q){unsigned n=q->ground_nest.clock++>>4;q->ground_nest.delay_row=n>3?3:n;}
static int body_enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*n;unsigned s=a->state;int back;
 switch(s){
 case NEW:
  if(!tk_settle(w,a))return 0;m4_reset(a);a->palette=q->params[6]?0x1cc:0xf2;a->hit_palette=0x76;interaction_priority(w,a,0x8000,8);a->hit.hp=rank(w,q,GNR_HP);q->ground_nest.timer=rank(w,q,GNR_WAIT);defense(q,GNB_BODY);q->ground_nest.direction=0;
  n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_DAMAGE_SMOKE_BODY);if(!n)return 0;n->payload.enemy.actor.attached_offset=0;if(!vehicle_spawn_surface(w,entity_ref(e),&a->transform))return 0;a->hit.type_flags|=4;attack(q,GNB_DRIVE);schedule(e,DRIVE);return 2;
 case DRIVE:
  if(q->params[0]){schedule(e,IDLE);return 2;}if(!interaction_sound(w,0x109d))return 0;m4_reset(a);back=a->transform.facing!=q->ground_nest.direction;start(q,back?GNA_REVERSE:GNA_DRIVE);a->vx=tk_faced(a,back?256:-256);q->ground_nest.delay_mode=back;q->ground_nest.clock=0;delay_row(q);break;
 case AVOID:if(!interaction_sound(w,0x109d))return 0;q->ground_nest.avoid=rank(w,q,GNR_AVOID);q->ground_nest.delay_mode=2;q->ground_nest.delay_row=random_delay(w,q);start(q,GNA_AVOID);break;
 case IDLE:start(q,GNA_IDLE);break;
 case TURN:q->ground_nest.direction^=1;/* FALLTHROUGH */
 case BRAKE:back=tk_faced(a,a->vx)>0;start(q,back?GNA_BRAKE_BACK:GNA_BRAKE);q->ground_nest.delay_mode=back;q->ground_nest.clock=0;a->ax=tk_word(-(a->vx>>5));if(!stop_sound(w))return 0;a->state=BRAKE;break;
 case RESUME:if(gate(w,q)){schedule(e,AIM);return 2;}if(q->params[0]){schedule(e,IDLE);return 2;}back=a->transform.facing!=q->ground_nest.direction;start(q,back?GNA_RESUME_BACK:GNA_RESUME);a->ax=tk_faced(a,back?256:-256)>>5;break;
 case AIM:start(q,GNA_AIM);break;
 case FIRE_FIRST:q->ground_nest.repeats=(uint8_t)rank(w,q,GNR_REPEATS);/* FALLTHROUGH */
 case FIRE:q->ground_nest.timer=rank(w,q,GNR_PERIOD);start(q,(int8_t)q->ground_nest.repeats>1&&q->ground_nest.timer<=60?GNA_FIRE_FAST:GNA_FIRE);a->state=FIRE;break;
 case HIT:case HIT_BIG:if(!stop_sound(w))return 0;a->hit.flags&=247;m4_reset(a);start(q,s==HIT?GNA_HIT:GNA_HIT_BIG);break;
 case DEAD:m4_reset(a);n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMOKE);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,a->transform.facing&1?24:-24,0);a->phase=255;start(q,GNA_DEAD);break;
 case WRECK:case DEAD_BIG:start(q,s==WRECK?GNA_WRECK:GNA_DEAD_BIG);a->display_flags&=253;m4_reset(a);a->state=WRECK;break;
 case REMOVE:if(!remove_fx(w,e))return 0;schedule(e,DELETE);return 0;
 default:return enemy_fault(w);
 }return !w->fault;
}
/* mode 0 checks HP only after a fresh hit; modes 1/2 always check HP. */
static void body_tail(struct WeaponWorld*w,struct NativeEntity*e,unsigned mode){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;int hp=mode!=0;if(a->hit.flags&8){if(mode!=1)a->hit.flags&=247;if(!tk_hit_sound(w,a))return;a->hit.flags&=247;hp=1;if(!mode)schedule(e,special(a)?HIT_BIG:HIT);}if(hp&&(a->hit.flags&1))schedule(e,special(a)?DEAD_BIG:DEAD);if(outside(w,a,0))schedule(e,DELETE);}
static void body_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(s==DEAD||s==WRECK){if(interaction_move(w,a,WMOVE_SYNC)<0)return;int done=animate(w,e);if(done){if(s==DEAD&&!a->phase)schedule(e,WRECK);if(s==WRECK&&q->ground_nest.temporary)schedule(e,REMOVE);}if(outside(w,a,0))schedule(e,DELETE);return;}
 interaction_body_contact(w,a,&ground_nest_contact);int r=interaction_move(w,a,s==DRIVE||s==BRAKE||s==RESUME?WMOVE_FLAT:WMOVE_SYNC);if(r<0)return;
 if(s==DRIVE&&r)schedule(e,BRAKE);if(s==BRAKE){if(a->ax&&!a->vx)a->ax=0;delay_row(q);}int done=animate(w,e);if(w->fault)return;
 switch(s){
 case DRIVE:if(past(w,q))schedule(e,TURN);if(gate(w,q))schedule(e,BRAKE);if(avoid(w,q))schedule(e,BRAKE);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return;break;
 case AVOID:if(done)schedule(e,IDLE);a->hit.flags&=247;if(!tk_hit_sound(w,a))return;hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return;break;
 case IDLE:if(done)schedule(e,q->params[0]?IDLE:RESUME);if(gate(w,q))schedule(e,AIM);if(avoid(w,q))schedule(e,AVOID);break;
 case BRAKE:if(done)schedule(e,IDLE);gate(w,q);break;
 case RESUME:if(done)schedule(e,DRIVE);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return;break;
 case AIM:if(done)schedule(e,FIRE_FIRST);break;
 case FIRE:if(done&&(int8_t)q->ground_nest.repeats<=0){schedule(e,IDLE);q->ground_nest.timer=rank(w,q,GNR_WAIT);}if((int8_t)q->ground_nest.repeats>0){q->ground_nest.timer=tk_word(q->ground_nest.timer-1);if(q->ground_nest.timer<=0){q->ground_nest.repeats--;if((int8_t)q->ground_nest.repeats>0)schedule(e,FIRE);}}break;
 case HIT:case HIT_BIG:if(done)schedule(e,RESUME);gate(w,q);break;
 default:enemy_fault(w);return;
 }body_tail(w,e,s==AVOID?2:s==HIT_BIG||s==AIM||s==FIRE?1:0);
}
static int crew_follow(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*p=tk_parent(w,e);if(!p)return 0;struct WeaponActor*a=&e->payload.enemy.actor;transform_integer_offset(&a->transform,tk_word((p->actor.transform.x>>8)+(p->actor.transform.facing&1?4:-4)-(a->transform.x>>8)),tk_word(transform_y_up(&p->actor.transform)+40-transform_y_up(&a->transform)));a->transform.priority=p->actor.transform.priority-1;return 1;}
static int crew_release(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*p=tk_parent(w,e);if(!p)return 0;p->actor.phase=0;return 1;}
static int parts_enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned s=a->state;
 switch(s){
 case CREW:{a->palette=26;int slot=enemy_nearest_x(w,a,0);if(slot>=0){a->transform.facing&=254;if((uint16_t)tk_word((w->players[slot]->actor.transform.x>>8)-w->camera_x)>=(uint16_t)tk_sx(w,a))a->transform.facing|=1;}unsigned v=q->params[2];if(v==2)v=tk_random(w)&1;if(v){q->params[2]=0;q->ground_nest.crew_phase=1;start(q,GNA_CREW_A);a->state=CREW_A;}else{q->params[2]=1;q->ground_nest.crew_phase=0;start(q,GNA_CREW_B);a->state=CREW_OUT;}break;}
 case CREW_NEXT:q->ground_nest.crew_phase=2;start(q,GNA_CREW_NEXT);break;
 case CREW_HELMET:if(!child(w,e,HELMET,ENTITY_FIRST_CHILD))return 0;q->ground_nest.crew_phase=0;start(q,GNA_CREW_HELMET);a->state=CREW_OUT;break;
 case CREW_EXIT:{unsigned burning=q->params[2]!=1;a->transform.facing^=1;a->vx=tk_faced(a,-136);a->vy=1087;a->ay=-145;a->ax=0;interaction_priority(w,a,0x8000,24);if(!crew_release(w,e))return 0;if(burning)infantry_force_escape(e,255);else infantry_free_escape(e,255);e->tick(w->entities,e,w);return 0;}
 case CREW_HIT:a->animation.freeze=0;start(q,a->transform.facing&1?GNA_CREW_DEAD:GNA_CREW_DEAD_BACK);if(q->ground_nest.crew_phase&&!child(w,e,q->ground_nest.crew_phase==2?HELMET:PIECE,ENTITY_FIRST_CHILD))return 0;break;
 case ROCKET:a->hit.type_flags|=16;a->transform.priority=0xd000;a->palette=0x167;a->vx=a->transform.facing&1?512:-512;a->ax=a->vx>>5;a->hit.hp=1;start(q,GNA_ROCKET);break;
 case PIECE:case HELMET:start(q,s==PIECE?GNA_PIECE:GNA_HELMET);a->palette=26;a->vx=tk_faced(a,45);a->vy=2902;a->ay=-129;a->ax=0;interaction_priority(w,a,0x8000,0);break;
 case MUZZLE:interaction_priority(w,a,0x4000,28);a->palette=24;start(q,GNA_MUZZLE);break;
 case FLAME:interaction_priority(w,a,0xd000,20);a->palette=0x148;start(q,GNA_FLAME);break;
 case FLASH:a->palette=4;start(q,GNA_FLASH);break;
 case SMOKE:a->palette=8;a->transform.facing^=1;interaction_priority(w,a,0x4000,28);start(q,GNA_SMOKE);break;
 case SHOT:{int16_t speed=rank(w,q,GNR_SHOT);int full=q->params[1]!=0;gun_platform_projectile(w,e,speed,full);return 0;}
 case BLAST:interaction_priority(w,a,0x4000,0);defense(q,65535);boss_fx_transform(w,e,BFX_FIRE);return 0;
 default:return enemy_fault(w);
 }return !w->fault;
}
static void parts_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;int done,r;
 if(s>=CREW_A&&s<=CREW_HIT){if(!crew_follow(w,e))return;if(s==CREW_HIT)a->transform.priority++;done=animate(w,e);if(done){if(s==CREW_A)schedule(e,CREW_NEXT);else if(s==CREW_NEXT)schedule(e,CREW_HELMET);else if(s==CREW_OUT)schedule(e,CREW_EXIT);else if(s==CREW_HIT){if(!crew_release(w,e))return;schedule(e,DELETE);}}if(s!=CREW_HIT&&(a->hit.flags&8)){if(!tk_hit_sound(w,a)||!score(w,a))return;schedule(e,CREW_HIT);}if(entity_parent_deleting(w->entities,e))schedule(e,DELETE);return;}
 if(s==ROCKET){static const uint16_t palettes[]={0x16,0x1d2,0x167};a->palette=palettes[ground_nest_palettes[w->frame_phase&3]];if(interaction_move(w,a,WMOVE_POINT)<0)return;animate(w,e);if(!enemy_attack(w,a))return;if(a->hit.flags&(2|8))schedule(e,BLAST);}
 else if(s==PIECE||s==HELMET){r=interaction_move(w,a,WMOVE_ITEM);if(r<0)return;if(r)schedule(e,DELETE_TREE);animate(w,e);}
 else if(s==MUZZLE||s==FLAME||s==FLASH||s==SMOKE){if(interaction_move(w,a,WMOVE_SYNC)<0)return;if(animate(w,e))schedule(e,DELETE_TREE);if(s==SMOKE)return;}
 else{enemy_fault(w);return;}if(outside(w,a,1))schedule(e,DELETE_TREE);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;for(unsigned i=0;i<8;i++){
 if(q->actor.state==DELETE||q->actor.state==DELETE_TREE){if(q->actor.state==DELETE_TREE)entity_delete_children(p,e);entity_delete(p,e);return;}
 if(q->entry){q->entry=0;int r=q->actor.state<CREW?body_enter(w,e):parts_enter(w,e);if(!r||w->fault)return;if(r==2)continue;}unsigned s=q->actor.state;if(s<CREW)body_frame(w,e,s);else parts_frame(w,e,s);return;
 }enemy_fault(w);}
int ground_nest_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){if(id<50||id>52)return enemy_fault(w);struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;init(e,NEW);e->payload.enemy.ground_nest.temporary=id==52;e->payload.enemy.ground_nest.stamp=id==51;if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];return 1;}
