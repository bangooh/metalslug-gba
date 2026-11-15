#include "tank_internal.h"
#include "m4_internal.h"
#include "items.h"
#include "../generated/mortar-programs.h"
/* $06361E..$064550: moving mortar carrier and stationary cannon families.
 * Crew, contacts and weapon handoffs remain real scheduled child entities. */
enum State {MT_NEW,MT_DRIVE,MT_IDLE,MT_DEAD,MT_EXPLODE,CN_NEW,CN_WAIT,CN_OPEN,CN_IDLE,CN_TURN,CN_FIRST,CN_FIRE,CN_DEAD,
 CREW_NEW,CREW_SELECT,CREW_TURN,CREW_FIRE,CREW_DEAD,CREW_EJECT,CONTACT,SHELL,MUZZLE,BLAST,CRATE_NEW,CRATE_INIT,CRATE_IDLE,CRATE_DEAD,DELETE,DELETE_TREE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static void schedule(struct NativeEntity*e,unsigned s){e->tick=tick;enemy_schedule(&e->payload.enemy,s);}
static void init(struct NativeEntity*e,unsigned s){uint8_t*p=(uint8_t*)&e->payload.enemy.mortar;for(unsigned i=0;i<sizeof(e->payload.enemy.mortar);i++)p[i]=255;schedule(e,s);}
static struct NativeEntity*child(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct NativeEntity*n=enemy_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,tick);if(n)init(n,s);return n;}
static void start_entry(struct EnemyActor*q,uint16_t n){if(n!=65535){animation_begin(&q->actor.animation,n);q->actor.movement_flags|=32;}}
static void start(struct EnemyActor*q,unsigned n){start_entry(q,mortar_entries[n]);}
static void defense(struct EnemyActor*q,unsigned n){q->actor.defense=n==65535?0:mortar_bindings[n].defense;q->actor.defense_count=n==65535?0:mortar_bindings[n].count;}
static void attack(struct EnemyActor*q,unsigned n){if(n==65535)q->actor.attack=0;else{q->attack_set=(struct HitAttackSet){mortar_bindings[n].attack,mortar_bindings[n].count};q->actor.attack=&q->attack_set;}}
static int16_t rank(struct WeaponWorld*w,struct EnemyActor*q,unsigned n){struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid||n>=MTR_COUNT){enemy_fault(w);return 0;}return (int16_t)mortar_rank[n][(interaction_rank(w,&q->actor,runtime_owner_mortar[n]))*8+(q->source_flags&3)*2+((r->player_count_flag&2)>>1)];}
static int score(struct WeaponWorld*w,struct WeaponActor*a){if((a->display_flags&2)||(int8_t)a->score_owner<0)return 1;return w->items&&item_score_add(w->items,a->score_owner,enemy_hit_scores[a->hit.last_kind&31])?1:enemy_fault(w);}
static int stop_sound(struct WeaponWorld*w,unsigned n){return w->sound&&w->sound(w->scene,(uint16_t)n,1)?1:enemy_fault(w);}
static void human_palette(struct WeaponWorld*w,struct WeaponActor*a){a->palette=w->enemies->blood_enabled?14:47;}
static int pattern(struct WeaponWorld*w,struct NativeEntity*e,unsigned n){return boss_fx_pattern(w,entity_ref(e),&e->payload.enemy.actor.transform,(enum BossPattern)n)!=0;}
static int smoke(struct WeaponWorld*w,struct NativeEntity*e,int16_t dx){struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,BFX_SMOKE);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,dx,0);return 1;}
static int stage_effect(struct WeaponWorld*w,struct NativeEntity*e,int hit){struct WeaponActor*a=&e->payload.enemy.actor;uint16_t saved=a->transform.priority;if(!tk_sound(w,hit?0x1023:0x1033))return 0;interaction_priority(w,a,0x4000,8);int r=hit?pattern(w,e,BP_MORTAR_HIT):smoke(w,e,0);a->transform.priority=saved;return r;}
static int cannon_hurt(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;if((q->mortar.damaged&1)||q->actor.hit.hp>q->mortar.threshold)return 0;if(!tk_sound(w,0x1027))return 0;q->mortar.damaged|=1;if(!pattern(w,e,BP_CANNON_HIT_A)||!pattern(w,e,BP_CANNON_HIT_B))return 0;return 1;}
static int cannon_select(struct WeaponWorld*w,struct NativeEntity*e,unsigned base){struct EnemyActor*q=&e->payload.enemy;cannon_hurt(w,e);if(w->fault)return 0;start(q,base+(q->mortar.direction&1)+((q->mortar.damaged&1)?2:0));return 1;}
static int fire(struct WeaponWorld*w,struct NativeEntity*e,int mirror){struct EnemyActor*q=&e->payload.enemy;struct NativeEntity*n;if(!tk_sound(w,0x1066))return 0;n=child(w,e,MUZZLE);if(!n)return 0;n->payload.enemy.actor.transform.facing^=mirror;n=child(w,e,SHELL);if(!n)return 0;n->payload.enemy.actor.transform.facing^=mirror;n->payload.enemy.mortar.serial=q->mortar.serial;transform_integer_offset(&n->payload.enemy.actor.transform,mirror?16:-16,60);return 1;}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read(void*ctx,unsigned f,unsigned width){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;if(width==1){if(f==MTF_3B)return q->actor.animation.pose;if(f==MTF_3A)return q->actor.transform.facing;}if(width==2){if(f==MTF_34)return q->selector;if(f==MTF_74)return (uint16_t)q->mortar.repeats;if(f==MTF_28)return (uint16_t)q->actor.vx;}enemy_fault(c->w);return 0;}
static int write(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;if(f==MTF_3A&&width==1&&op==AN_FIELD_XOR){q->actor.transform.facing^=(uint8_t)v;return 1;}if(op!=AN_FIELD_SET)return enemy_fault(c->w);if(f==MTF_3B&&width==1)q->actor.animation.pose=(uint8_t)v;else if(f==MTF_74&&width==2)q->mortar.repeats=(int16_t)v;else if(f==MTF_28&&width==2)q->actor.vx=(int16_t)v;else return enemy_fault(c->w);return 1;}
static int bind(void*ctx,unsigned kind,uint32_t v){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;unsigned n=v==0xffffffffu?65535:v;if(n!=65535&&n>=sizeof(mortar_bindings)/sizeof(*mortar_bindings))return enemy_fault(c->w);if(kind==AN_DEFENSE)defense(q,n);else if(kind==AN_ATTACK){attack(q,n);hit_begin_attack(&q->actor.hit);}else return enemy_fault(c->w);return 1;}
static int sound(void*ctx,uint16_t s,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,s,stop)?1:enemy_fault(c->w);}
static int callback(void*ctx,unsigned cb,struct Animation*an){(void)an;struct Context*c=ctx;struct WeaponWorld*w=c->w;struct NativeEntity*e=c->e;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 switch(cb){
 case MTC_4A110:a->palette=0x15c;return 1;
 case MTC_4A0D4:human_palette(w,a);return 1;
 case MTC_4A1BA:return tk_sound(w,(uint16_t)q->mortar.repeats);
 case MTC_55BCE:return enemy_spawn_ordnance(w,entity_ref(e),&a->transform,ENEMY_GRENADE,a->variant,q->source_flags);
 case MTC_644E8:return fire(w,e,0);
 case MTC_6449E:return fire(w,e,1);
 case MTC_64526:return pattern(w,e,BP_CANNON_DEATH);
 case MTC_63DF2:return stage_effect(w,e,1);
 case MTC_63DB2:return stage_effect(w,e,0);
 case MTC_63E2C:
  a->phase=255;if(q->params[3])return 1;attack(q,MTB_MT_WRECK);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;attack(q,65535);
  {unsigned i=(q->mortar.stamp?2:0)+(a->transform.facing&1);const struct MortarStamp*s=&mortar_stamps[i];return terrain_stamp(w->interactions->terrain,&a->transform,s->dx,s->dy,s->width,s->height,s->cells)?1:enemy_fault(w);}
 default:return enemy_fault(w);
 }}
static int animate(struct WeaponWorld*w,struct NativeEntity*e){struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;struct Context c={w,e};struct AnimationOwner o={.context=&c,.read=read,.write=write,.bind=bind,.sound=sound,.call=callback};int done=animation_step(&a->animation,&mortar_program,&o);if(a->animation.fault)enemy_fault(w);return done;}
static int crews(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;unsigned row=q->params[2]&3;for(unsigned i=0;i<mortar_crew_counts[row];i++){const struct MortarCrew*c=&mortar_crews[row][i];struct NativeEntity*n;if(c->kind){n=tk_child(w,e,TK_CREW_ZERO,ENTITY_FIRST_CHILD);if(!n)return 0;struct EnemyActor*b=&n->payload.enemy;b->params[0]=1;b->params[1]=32;b->params[2]=(uint8_t)c->x;b->params[3]=(uint8_t)c->y;b->params[4]=255;b->params[5]=q->mortar.direction;b->params[6]=0;}else{n=child(w,e,CREW_NEW);if(!n)return 0;n->payload.enemy.mortar.pose=(uint8_t)i;n->payload.enemy.mortar.x=c->x;n->payload.enemy.mortar.y=c->y;}}return 1;}
static int new_damage_smoke(struct WeaponWorld*w,struct NativeEntity*e){struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,BFX_DAMAGE_SMOKE_BODY);if(!n)return 0;n->payload.enemy.actor.attached_offset=0;return 1;}
static int explosion(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;if(!tk_sound(w,0x102f)||!smoke(w,e,-32)||!smoke(w,e,32))return 0;for(unsigned i=0;i<16;i++){struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&q->actor.transform,BFX_CRASH_FRAGMENT);if(!n)return 0;n->payload.enemy.params[3]=q->params[3];}for(unsigned i=0;i<13;i++){struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&q->actor.transform,BFX_METAL_HEAVY);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,0,24);}entity_delete(w->entities,e);return 1;}
static int crew_follow(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy,*p=tk_parent(w,e);if(!p)return 0;uint8_t facing=q->actor.transform.facing;q->actor.transform=p->actor.transform;q->actor.transform.facing=facing;q->actor.transform.priority=tk_word(q->actor.transform.priority+q->mortar.priority);transform_integer_offset(&q->actor.transform,p->actor.transform.facing==1?q->mortar.x:tk_word(-q->mortar.x),tk_word(q->mortar.y+mortar_crew_offsets[p->actor.animation.pose&3]));return 1;}
static int cannon_turn_needed(struct WeaponWorld*w,struct EnemyActor*q){for(unsigned i=0;i<2;i++)if(enemy_player_live(w->players[i])){int16_t dx=tk_word((q->actor.transform.x>>8)-(w->players[i]->actor.transform.x>>8));if(q->mortar.direction&1?dx<=0:dx>=0)return 0;}return 1;}
static int crate_stamp(struct WeaponWorld*w,struct EnemyActor*q,int dead){if(q->mortar.kind==2)return 1;unsigned i=4+q->mortar.kind+(dead?2:0);if(i>=8)return enemy_fault(w);const struct MortarStamp*s=&mortar_stamps[i];return terrain_stamp(w->interactions->terrain,&q->actor.transform,s->dx,s->dy,s->width,s->height,s->cells)?1:enemy_fault(w);}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned s=a->state;
 switch(s){
 case MT_NEW:
  if(!tk_settle(w,a))return 0;m4_reset(a);if(!child(w,e,CONTACT))return 0;a->palette=44;a->hit_palette=0x7a;a->hit.hp=rank(w,q,MTR_MT_HP);q->mortar.threshold=a->hit.hp>>1;q->mortar.damaged=0;interaction_priority(w,a,0x8000,8);a->transform.facing=q->params[0]&1;q->mortar.direction=(q->params[0]>>1)&1;q->mortar.limit=q->params[1]<<4;if(!crews(w,e))return 0;a->phase=0;defense(q,MTB_MT_BODY);if(!new_damage_smoke(w,e))return 0;q->mortar.grenade_hits=q->mortar.signal=0;if(!tk_sound(w,0x10a5))return 0;schedule(e,MT_DRIVE);return 2;
 case MT_DRIVE:attack(q,MTB_MT_DRIVE);a->vx=a->transform.facing&1?384:-192;q->mortar.hurt_graph=MTA_MT_DRIVE_HURT;start(q,q->mortar.damaged?MTA_MT_DRIVE_HURT:MTA_MT_DRIVE);break;
 case MT_IDLE:m4_reset(a);q->mortar.hurt_graph=MTA_MT_IDLE_HURT;start(q,q->mortar.damaged?MTA_MT_IDLE_HURT:MTA_MT_IDLE);break;
 case MT_DEAD:a->hit.flags|=1;if(!stop_sound(w,0x10a5))return 0;a->display_flags&=253;m4_reset(a);start(q,MTA_MT_DEAD);break;
 case MT_EXPLODE:explosion(w,e);return 0;
 case CN_NEW:a->hit.flags|=8;defense(q,q->mortar.kind==63?MTB_CN_BODY_B:MTB_CN_BODY_A);a->palette=0x83;a->hit_palette=0x8d;q->mortar.timer=rank(w,q,MTR_CN_WAIT);a->hit.hp=rank(w,q,MTR_CN_HP);q->mortar.threshold=a->hit.hp>>1;q->mortar.direction=q->mortar.damaged=0;interaction_priority(w,a,0x100,20);m4_reset(a);if(!new_damage_smoke(w,e))return 0;q->mortar.limit=(q->params[0]&31)<<4;start(q,MTA_CN_WAIT);a->state=CN_WAIT;break;
 case CN_OPEN:a->hit.flags&=247;start(q,MTA_CN_OPEN);break;
 case CN_IDLE:return cannon_select(w,e,MTA_CN_IDLE_A);
 case CN_TURN:if(!cannon_turn_needed(w,q)){schedule(e,CN_FIRST);return 2;}if(!cannon_select(w,e,MTA_CN_TURN_A))return 0;q->mortar.direction^=1;break;
 case CN_FIRST:q->mortar.serial=0;q->mortar.repeats=rank(w,q,MTR_CN_COUNT);/* FALLTHROUGH */
 case CN_FIRE:q->mortar.timer=rank(w,q,MTR_CN_PERIOD);a->state=CN_FIRE;return cannon_select(w,e,MTA_CN_FIRE_A);
 case CN_DEAD:start(q,MTA_CN_DEAD);if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;animate(w,e);schedule(e,DELETE);return 0;
 case CREW_NEW:a->palette=14;q->mortar.priority=-1;q->mortar.timer=tk_random(w)&15;/* FALLTHROUGH */
 case CREW_SELECT:start_entry(q,mortar_mt_crew[q->mortar.pose&3]);a->state=CREW_SELECT;break;
 case CREW_TURN:start(q,MTA_CREW_TURN);break;
 case CREW_FIRE:{int i=enemy_nearest_x(w,a,0);if(i>=0){int16_t x=tk_word((w->players[i]->actor.transform.x>>8)-w->camera_x);int right=x>=tk_sx(w,a);if(right!=!!(a->transform.facing&1)){schedule(e,CREW_TURN);return 2;}}q->mortar.timer=rank(w,q,MTR_CREW_WAIT);start(q,MTA_CREW_FIRE);a->variant=(tk_random(w)&3)+20;break;}
 case CREW_DEAD:human_palette(w,a);q->mortar.timer=0;start(q,MTA_CREW_DEAD);break;
 case CREW_EJECT:human_palette(w,a);soldier_mortar_eject(w,e);return 0;
 case CONTACT:break;
 case SHELL:{const int16_t*v=mortar_shell[(uint16_t)q->mortar.serial&3];a->ay=v[1];a->vy=v[2];a->vx=tk_faced(a,v[0]);a->hit.type_flags|=16;attack(q,MTB_CN_SHELL);a->palette=0x1d2;hit_begin_attack(&a->hit);interaction_priority(w,a,0xd000,20);start(q,MTA_CN_SHELL);break;}
 case MUZZLE:a->palette=0x83;interaction_priority(w,a,0xd000,20);start(q,MTA_CN_MUZZLE);break;
 case CRATE_NEW:a->palette=79;a->phase=q->dead=0;if(!crate_stamp(w,q,0))return 0;a->hit.hp=rank(w,q,MTR_CRATE_HP);a->display_flags&=253;m4_reset(a);schedule(e,CRATE_INIT);return 0;
 case CRATE_INIT:{uint16_t priority=a->transform.priority+1;interaction_priority(w,a,priority,priority&28);start(q,MTA_CRATE_IDLE);a->state=CRATE_IDLE;break;}
 case CRATE_DEAD:a->display_flags&=253;if(!crate_stamp(w,q,1))return 0;start(q,MTA_CRATE_DEAD);break;
 case BLAST:interaction_priority(w,a,0x4000,0);defense(q,65535);boss_fx_transform(w,e,BFX_BLAST);return 0;
 default:return enemy_fault(w);
 }return !w->fault;
}
static void mt_outside(struct WeaponWorld*w,struct NativeEntity*e){struct WeaponActor*a=&e->payload.enemy.actor;if(tk_sx(w,a)<=-128||interaction_outside(w,a,mortar_bounds[0],0))schedule(e,DELETE);}
static void mt_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(s==MT_DEAD){if(interaction_move(w,a,WMOVE_SYNC)<0)return;if(animate(w,e)&&q->params[3])schedule(e,MT_EXPLODE);mt_outside(w,e);return;}
 if(q->mortar.signal)a->hit.events|=1;q->mortar.signal=0;interaction_body_contact(w,a,&mortar_contacts[0]);if(interaction_move(w,a,s==MT_DRIVE?WMOVE_POINT:WMOVE_SYNC)<0)return;animate(w,e);
 if(s==MT_DRIVE){int16_t x=tk_sx(w,a);if(q->mortar.direction?x>=q->mortar.limit:x<=q->mortar.limit)schedule(e,MT_IDLE);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return;}
 if(!tk_hit_sound(w,a))return;if(a->hit.flags&8){a->hit.flags&=247;if(a->hit.last_kind==4)q->mortar.grenade_hits++;if(!q->mortar.damaged&&q->mortar.threshold>=a->hit.hp){q->mortar.damaged=1;start(q,q->mortar.hurt_graph);if(!stage_effect(w,e,1))return;}}
 if((int8_t)q->mortar.grenade_hits>=1||(a->hit.flags&1))schedule(e,MT_DEAD);mt_outside(w,e);
}
static void cn_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(interaction_move(w,a,WMOVE_SYNC)<0)return;int done=animate(w,e);if(w->fault)return;
 switch(s){
 case CN_WAIT:if(tk_sx(w,a)<=q->mortar.limit)schedule(e,CN_OPEN);break;
 case CN_OPEN:if(done)schedule(e,CN_IDLE);break;
 case CN_IDLE:if(cannon_hurt(w,e))schedule(e,CN_IDLE);if(tk_sx(w,a)>=32&&tk_sx(w,a)<=288){q->mortar.timer=tk_word(q->mortar.timer-1);if(q->mortar.timer<=0)schedule(e,CN_TURN);}break;
 case CN_TURN:if(done)schedule(e,CN_FIRST);break;
 case CN_FIRE:if(done){schedule(e,CN_FIRE);q->mortar.serial=tk_word(q->mortar.serial+1);q->mortar.repeats=tk_word(q->mortar.repeats-1);if(q->mortar.repeats<=0){schedule(e,CN_IDLE);q->mortar.timer=rank(w,q,MTR_CN_WAIT);}}break;
 default:enemy_fault(w);return;
 }if(a->hit.flags&8){if(!score(w,a)||!tk_hit_sound(w,a))return;a->hit.flags&=247;}if(a->hit.flags&1)schedule(e,CN_DEAD);if(interaction_outside(w,a,mortar_bounds[1],0))schedule(e,DELETE);
}
static void crew_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(!crew_follow(w,e))return;
 if(s==CREW_DEAD){q->mortar.timer=tk_word(q->mortar.timer+1);if(q->mortar.timer<10||(q->mortar.timer&1)){if(q->mortar.timer>=30)schedule(e,DELETE_TREE);if(!animate(w,e))q->mortar.timer=0;}if(interaction_outside(w,a,boss_fx_bounds[8],0))schedule(e,DELETE_TREE);return;}
 int done=animate(w,e);if(s==CREW_SELECT){q->mortar.timer=tk_word(q->mortar.timer-1);if(q->mortar.timer<=0){q->mortar.timer=0;schedule(e,CREW_FIRE);}}else if(done)schedule(e,CREW_SELECT);
 struct EnemyActor*p=tk_parent(w,e);if(!p)return;if(p->actor.phase==255){const int16_t*v=mortar_crew_launch[q->mortar.pose&3];a->vx=tk_faced(a,v[0]);a->ax=0;a->ay=v[1];a->vy=v[2];schedule(e,CREW_EJECT);}if(a->hit.flags&8){schedule(e,CREW_DEAD);if(!score(w,a))return;}if(interaction_outside(w,a,boss_fx_bounds[8],0))schedule(e,DELETE_TREE);
}
static void contact_frame(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy,*p=tk_parent(w,e);if(!p)return;struct WeaponActor*a=&q->actor;a->transform=p->actor.transform;interaction_body_contact(w,a,&mortar_contacts[1]);struct TerrainMotion m={.platform_ids=a->platform_ids};int16_t x=(int16_t)(a->transform.x>>8),y=transform_y_up(&a->transform);if(!terrain_platform_publish(w->interactions->terrain,&m,tk_word(x+(a->transform.facing&1?0:-28)),tk_word(y+44),28,0)||!terrain_platform_publish(w->interactions->terrain,&m,tk_word(x-48),tk_word(y+28),96,0)){enemy_fault(w);return;}a->platform_ids=m.platform_ids;animate(w,e);if((p->actor.hit.flags&1)||entity_parent_deleting(w->entities,e))schedule(e,DELETE_TREE);}
static void parts_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(s==CONTACT){contact_frame(w,e);return;}if(s>=CREW_SELECT&&s<=CREW_DEAD){crew_frame(w,e,s);return;}
 if(s==SHELL){int r=interaction_move(w,a,WMOVE_POINT);if(r<0)return;if(r)schedule(e,BLAST);int16_t vx=a->vx;if(vx>=0)vx=tk_word(-vx);unsigned angle=enemy_angle(vx>>4,a->vy>>4);if(angle<96)angle=96;if(angle>192)angle=192;q->selector=(angle-96)>>2;a->palette=w->frame_phase&1?0x1d2:0x167;animate(w,e);if(!enemy_attack(w,a))return;if(a->hit.flags&2)schedule(e,BLAST);if(interaction_outside(w,a,mortar_bounds[2],1))schedule(e,DELETE_TREE);return;}
 if(s==CRATE_IDLE||s==CRATE_DEAD){if(interaction_move(w,a,WMOVE_SYNC)<0)return;animate(w,e);if(s==CRATE_IDLE){if(a->hit.flags&8){if(!tk_hit_sound(w,a))return;a->hit.flags&=247;}if(a->hit.flags&1)schedule(e,CRATE_DEAD);}if(interaction_outside(w,a,mortar_bounds[3],0))schedule(e,DELETE);return;}
 if(s==MUZZLE){if(interaction_move(w,a,WMOVE_SYNC)<0)return;if(animate(w,e)||interaction_outside(w,a,boss_fx_bounds[8],1))schedule(e,DELETE_TREE);return;}enemy_fault(w);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;for(unsigned i=0;i<4;i++){
 if(q->actor.state==DELETE||q->actor.state==DELETE_TREE){if(q->actor.state==DELETE_TREE)entity_delete_children(p,e);entity_delete(p,e);return;}
 if(q->entry){q->entry=0;int r=enter(w,e);if(!r||w->fault)return;if(r==2)continue;}unsigned s=q->actor.state;if(s<=MT_DEAD)mt_frame(w,e,s);else if(s>=CN_WAIT&&s<=CN_FIRE)cn_frame(w,e,s);else parts_frame(w,e,s);return;
 }enemy_fault(w);}
int mortar_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){if((id<60||id>63)&&(id<122||id>124))return enemy_fault(w);struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;init(e,id>=122?CRATE_NEW:id<62?MT_NEW:CN_NEW);struct EnemyActor*q=&e->payload.enemy;q->mortar.kind=id>=122?124-id:(uint8_t)id;q->mortar.stamp=id==61;if(params)for(unsigned i=0;i<8;i++)q->params[i]=params[i];return 1;}
