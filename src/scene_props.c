#include "tank_internal.h"
#include "m4_internal.h"
#include "../generated/scene-prop-programs.h"
enum State {STATIC317,ROOF,ROOF_DIE,DRAIN,SIGN_A,SIGN_B,SIGN,BOAT,BOAT_BLAST,BOAT_SPECIAL,HEADING,PLATFORM,RECORD,FROG,FROG_JUMP,FROG_RUN};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static void schedule(struct NativeEntity*e,unsigned s){enemy_schedule(&e->payload.enemy,s);}
static struct NativeEntity*allocate(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,unsigned s){struct NativeEntity*e=enemy_allocate(w,p,o,t,tick);if(e){e->payload.enemy.scene_prop.timer=-1;schedule(e,s);}return e;}
static struct NativeEntity*child(struct WeaponWorld*w,struct NativeEntity*p,unsigned s){struct PlayerTransform t=p->payload.enemy.actor.transform;t.display_group=p->payload.enemy.source_flags;return allocate(w,entity_ref(p),ENTITY_FIRST_CHILD,&t,s);}
static void reset(struct WeaponActor*a){a->vx=a->vy=a->ax=a->ay=0;transform_integer_copy(&a->transform,&a->transform);}
static void start_entry(struct EnemyActor*q,unsigned n){animation_begin(&q->actor.animation,n);q->actor.movement_flags|=32;}
static void start(struct EnemyActor*q,unsigned n){start_entry(q,scene_prop_entries[n]);}
static void defense(struct EnemyActor*q,unsigned n){q->actor.defense=n==65535?0:scene_prop_bindings[n].defense;q->actor.defense_count=n==65535?0:scene_prop_bindings[n].count;}
static void attack_binding(struct EnemyActor*q,unsigned n){if(n==65535)q->actor.attack=0;else{q->attack_set=(struct HitAttackSet){scene_prop_bindings[n].attack,scene_prop_bindings[n].count};q->actor.attack=&q->attack_set;}}
struct Context{struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read(void*ctx,unsigned f,unsigned width){struct Context*c=ctx;if(f!=SPF_FACING||width!=1){enemy_fault(c->w);return 0;}return c->e->payload.enemy.actor.transform.facing;}
static int field(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){struct Context*c=ctx;uint32_t old=read(ctx,f,width);if(op==AN_FIELD_AND)v&=old;else if(op==AN_FIELD_OR)v|=old;else if(op==AN_FIELD_XOR)v^=old;else if(op!=AN_FIELD_SET)return enemy_fault(c->w);c->e->payload.enemy.actor.transform.facing=v;return !c->w->fault;}
static int bind(void*ctx,unsigned kind,uint32_t n){struct Context*c=ctx;if(kind!=AN_DEFENSE)return enemy_fault(c->w);if(n==0xffffffffu)n=65535;else if(n>=sizeof(scene_prop_bindings)/sizeof(*scene_prop_bindings))return enemy_fault(c->w);defense(&c->e->payload.enemy,n);return 1;}
static int sound(void*ctx,uint16_t n,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,n,stop)?1:enemy_fault(c->w);}
static int animate(struct WeaponWorld*w,struct NativeEntity*e){struct Context c={w,e};struct AnimationOwner o={.context=&c,.read=read,.write=field,.bind=bind,.sound=sound};int r=animation_step(&e->payload.enemy.actor.animation,&scene_prop_program,&o);if(e->payload.enemy.actor.animation.fault)enemy_fault(w);return r;}
static int sync(struct WeaponWorld*w,struct WeaponActor*a){return interaction_move(w,a,WMOVE_SYNC)>=0;}
static int bounds(struct WeaponWorld*w,struct NativeEntity*e,unsigned n){if(interaction_outside(w,&e->payload.enemy.actor,scene_prop_bounds[n],0)){entity_delete(w->entities,e);return 0;}return !w->fault;}
static int stamp(struct WeaponWorld*w,struct WeaponActor*a,unsigned n){const struct ScenePropStamp*t=&scene_prop_stamps[n];return terrain_stamp(w->interactions->terrain,&a->transform,t->dx,t->dy,t->width,t->height,t->cells)?1:enemy_fault(w);}
static int platform(struct WeaponWorld*w,struct WeaponActor*a,int dx,int dy,int width){struct TerrainMotion m={.platform_ids=a->platform_ids};if(!terrain_platform_publish(w->interactions->terrain,&m,tk_word((a->transform.x>>8)+dx),tk_word(transform_y_up(&a->transform)+dy),(int16_t)width,0))return enemy_fault(w);a->platform_ids=m.platform_ids;return 1;}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 switch(a->state){
 case STATIC317:if(!tk_settle(w,a)||!stamp(w,a,2))return 0;a->palette=0x1c6;start(q,SPA_STATIC317);break;
 case ROOF:a->palette=0x3d;q->scale_x=q->scale_y=255;q->scene_prop.timer=40;a->transform.facing=0;a->hit.hp=20;start(q,SPA_ROOF);break;
 case ROOF_DIE:attack_binding(q,SPB_ROOF_BLAST);hit_begin_attack(&a->hit);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;start(q,SPA_ROOF_DIE);break;
 case SIGN_A:case SIGN_B:{unsigned n=a->state==SIGN_B;a->palette=n?0x1b6:0x186;start(q,n?SPA_SIGN_B:SPA_SIGN_A);a->transform.priority=0;if(!sync(w,a)||!stamp(w,a,n))return 0;a->state=SIGN;break;}
 case BOAT:{struct InteractionWorld*r=w->interactions;if(!r->rank_valid)return enemy_fault(w);a->hit.hp=scene_prop_boat_rank[(interaction_rank(w,&q->actor,runtime_owner_scene_prop[0]))*8+(q->source_flags&3)*2+((r->player_count_flag&2)>>1)];start(q,SPA_BOAT);defense(q,q->params[0]==1?SPB_BOAT_B:SPB_BOAT_A);reset(a);if(!tk_settle(w,a))return 0;a->transform.priority=0x2000;a->palette=0x1ca;break;}
 case BOAT_BLAST:q->scene_prop.timer=8;break;
 case BOAT_SPECIAL:{a->palette=0x1c6;start(q,SPA_BOAT_SPECIAL);a->transform.facing=0;reset(a);uint16_t r=tk_random(w);/* Source4F9C4 overwrites the earlier +1000 result: no initial vertical launch. */a->vx=tk_word(r&255);if(!(r&1))a->vx=tk_word(-a->vx);a->ay=(r&127)+127;a->transform.priority-=16;break;}
 case HEADING:if(q->params[0])a->transform.facing|=1;interaction_priority(w,a,0x8000,24);a->palette=14;start(q,SPA_HEADING);break;
 case PLATFORM:a->palette=14;animation_drawing(&a->animation,&scene_prop_program,SCENE_PROP_PLATFORM_DRAWING);break;
 case RECORD:{int16_t n=q->params[0]>99?99:q->params[0];if(n>w->enemies->mission_time)w->enemies->mission_time=n;entity_delete(w->entities,e);return 0;}
 case FROG:if(!tk_settle(w,a))return 0;a->palette=0x126;q->scale_x=q->scale_y=255;a->transform.facing=0;reset(a);a->transform.priority=0;start(q,SPA_FROG);break;
 case FROG_JUMP:a->vy=1024;start(q,SPA_FROG_JUMP);break;
 case FROG_RUN:reset(a);a->vx=128;start(q,a->transform.facing&1?SPA_FROG:SPA_FROG_B);break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static int frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(s==STATIC317){if(!sync(w,a))return 0;animate(w,e);if(interaction_outside(w,a,scene_prop_bounds[3],q->params[7]!=0))entity_delete(w->entities,e);return !w->fault;}
 if(s==ROOF){if(!sync(w,a))return 0;animate(w,e);if(a->hit.flags&8){if(!tk_hit_sound(w,a))return 0;a->hit.flags&=247;}if(a->hit.flags&1)schedule(e,ROOF_DIE);if(tk_sx(w,a)<tk_word(-q->scene_prop.timer))entity_delete(w->entities,e);return !w->fault;}
 if(s==ROOF_DIE){if(!sync(w,a))return 0;if(animate(w,e)){attack_binding(q,65535);hit_begin_attack(&a->hit);if(!tk_sound(w,0x1028)||!boss_fx_pattern(w,entity_ref(e),&a->transform,BP_ROOF_DEATH))return 0;a->state=DRAIN;}return !w->fault;}
 if(s==DRAIN){if(!sync(w,a))return 0;if(!m4_has_children(w,e))entity_delete(w->entities,e);return !w->fault;}
 if(s==SIGN){if(!sync(w,a))return 0;animate(w,e);return bounds(w,e,1);}
 if(s==BOAT){if(!sync(w,a))return 0;if(tk_sx(w,a)>=328)a->hit.invulnerable=2;if(q->params[0]!=1){interaction_body_contact(w,a,&scene_prop_boat_contact);if(!platform(w,a,-18,36,36))return 0;}animate(w,e);a->palette=w->frame_phase&8?0x1d8:0x1ca;
  if(a->hit.flags&8){a->palette=0x1cb;a->hit.flags&=247;if(!tk_sound(w,0x108d))return 0;}
  if(a->hit.flags&1){if(!tk_sound(w,0x1030)||!child(w,e,BOAT_BLAST))return 0;for(unsigned i=0;i<3;i++)if(!boss_fx_pattern(w,entity_ref(e),&a->transform,(enum BossPattern)(BP_BOAT_SMOKE+i)))return 0;if(!(tk_random(w)&63)&&!child(w,e,BOAT_SPECIAL))return 0;entity_delete(w->entities,e);return !w->fault;}return bounds(w,e,2);
 }
 if(s==BOAT_BLAST){if(!sync(w,a))return 0;attack_binding(q,SPB_BOAT_BLAST);hit_begin_attack(&a->hit);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;q->scene_prop.timer=tk_word(q->scene_prop.timer-1);if(q->scene_prop.timer<0)entity_delete(w->entities,e);return !w->fault;}
 if(s==BOAT_SPECIAL){if(interaction_move(w,a,WMOVE_POINT)<0)return 0;animate(w,e);if(!(w->frame_phase&7)){if(!boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_BLAST))return 0;a->transform.priority--;}return bounds(w,e,2);}
 if(s==HEADING){if(!w->sound_reply_valid)return enemy_fault(w);if(!w->sound_reply){a->transform.facing^=1;start(q,SPA_HEADING);}else if(w->sound_reply>0)start_entry(q,scene_prop_heading[w->frame_phase&15]);if(!sync(w,a))return 0;animate(w,e);if((uint16_t)tk_word(tk_sx(w,a)+48)>=416)entity_delete(w->entities,e);return !w->fault;}
 if(s==PLATFORM){const int16_t*v=scene_prop_velocity[w->inputs[0].held&15];int16_t*speed[]={&a->vx,&a->vy};for(unsigned i=0;i<2;i++){int16_t d=tk_word(v[i]-*speed[i]);if(d){d>>=4;if(!d)d=1;*speed[i]=tk_word(*speed[i]+d);}}if(!sync(w,a))return 0;weapon_integrate_free(a);if(!platform(w,a,-16,4,32))return 0;return animation_output_direct(&a->animation,AN_OUTPUT_DIRECT)||enemy_fault(w);}
 if(s==FROG){if(!sync(w,a))return 0;animate(w,e);int r=terrain_weapon_foot_contact(w->interactions->terrain,a);if(r<0)return enemy_fault(w);if(!r)schedule(e,FROG_JUMP);if(!(tk_random(w)&7))start(q,SPA_FROG_B);return bounds(w,e,0);}
 if(s==FROG_JUMP){int r=interaction_move(w,a,WMOVE_ITEM);if(r<0)return 0;if(r)schedule(e,FROG_RUN);animate(w,e);return bounds(w,e,0);}
 if(s==FROG_RUN){if(interaction_move(w,a,WMOVE_GROUND)<0)return 0;animate(w,e);int r=terrain_weapon_foot_contact(w->interactions->terrain,a);if(r<0)return enemy_fault(w);if(!r)schedule(e,FROG_JUMP);if(!(tk_random(w)&15))schedule(e,FROG_JUMP);return bounds(w,e,0);}
 return enemy_fault(w);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){(void)p;struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;if(q->entry){q->entry=0;if(!enter(w,e)||w->fault)return;}if(e->active&&!e->deleting)frame(w,e,q->actor.state);}
int scene_prop_spawn_template(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){unsigned s=0;struct NativeEntity*e=0;
 switch(id){case 317:s=STATIC317;break;case 250:s=RECORD;break;case 252:s=HEADING;break;case 259:s=ROOF;break;case 260:s=BOAT;break;case 261:s=SIGN_A;break;case 262:s=SIGN_B;break;case 272:e=obstacle_a_variant2(w,p,o,t);break;case 273:e=obstacle_flame(w,p,o,t);break;case 274:e=enemy_allocate(w,p,o,t,tick);if(e&&!human_launch_direct(w,e))return 0;break;case 319:s=PLATFORM;break;default:return enemy_fault(w);}
 if(id<272||id>274)e=allocate(w,p,o,t,s);if(!e)return 0;if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];return 1;
}
struct NativeEntity*scene_prop_frog(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t){return allocate(w,p,o,t,FROG);}
