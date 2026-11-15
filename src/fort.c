#include "scene_placements.h"
#include "fort_internal.h"
static void tick(struct EntityPool*,struct NativeEntity*,void*);
void ft_schedule(struct NativeEntity*e,unsigned s){e->tick=tick;enemy_schedule(&e->payload.enemy,s);}
void ft_reset(struct WeaponActor*a){a->vx=a->vy=a->ax=a->ay=0;transform_integer_copy(&a->transform,&a->transform);}
uint16_t ft_random(struct WeaponWorld*w){if(!w->rng){enemy_fault(w);return 0;}return source_random_word(w->rng);}
uint16_t ft_bounded(struct WeaponWorld*w,uint16_t n){if(!w->rng){enemy_fault(w);return 0;}return source_random(w->rng,n);}
uint16_t ft_masked(struct WeaponWorld*w,uint16_t n){if(!w->rng){enemy_fault(w);return 0;}return source_random_mask(w->rng,n);}
void ft_start_entry(struct EnemyActor*q,uint16_t n){if(n==65535)return;animation_begin(&q->actor.animation,n);q->actor.movement_flags|=32;}
void ft_start(struct EnemyActor*q,enum FortAnimation n){ft_start_entry(q,fort_entries[n]);}
void ft_defense(struct EnemyActor*q,unsigned n){q->actor.defense=n==65535?0:fort_bindings[n].defense;q->actor.defense_count=n==65535?0:fort_bindings[n].count;}
void ft_attack(struct EnemyActor*q,unsigned n){if(n==65535)q->actor.attack=0;else{q->attack_set=(struct HitAttackSet){fort_bindings[n].attack,fort_bindings[n].count};q->actor.attack=&q->attack_set;}}
int ft_patch(struct WeaponWorld*w,unsigned n){return n<sizeof(fort_patches)/sizeof(*fort_patches)&&scene_video_patch(w->video,&fort_patches[n])?1:enemy_fault(w);}
int ft_stamp(struct WeaponWorld*w,struct NativeEntity*e,unsigned n){if(n>=FTS_COUNT||!w->interactions)return enemy_fault(w);const struct FortStamp*t=&fort_stamps[n];return terrain_stamp(w->interactions->terrain,&e->payload.enemy.actor.transform,t->dx,t->dy,t->width,t->height,t->cells)?1:enemy_fault(w);}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read(void*ctx,unsigned f,unsigned width){struct Context*c=ctx;if(width==1&&f==FTF_20_1)return c->e->payload.enemy.actor.phase;enemy_fault(c->w);return 0;}
static int write(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){struct Context*c=ctx;if(f!=FTF_20_1||width!=1)return enemy_fault(c->w);uint8_t*p=&c->e->payload.enemy.actor.phase;if(op==AN_FIELD_SET)*p=(uint8_t)v;else if(op==AN_FIELD_AND)*p&=(uint8_t)v;else if(op==AN_FIELD_OR)*p|=(uint8_t)v;else if(op==AN_FIELD_XOR)*p^=(uint8_t)v;else return enemy_fault(c->w);return 1;}
static int bind(void*ctx,unsigned kind,uint32_t value){struct Context*c=ctx;unsigned n=value==0xffffffffu?65535:value;if(n!=65535&&n>=sizeof(fort_bindings)/sizeof(*fort_bindings))return enemy_fault(c->w);if(kind==AN_DEFENSE)ft_defense(&c->e->payload.enemy,n);else if(kind==AN_ATTACK){ft_attack(&c->e->payload.enemy,n);hit_begin_attack(&c->e->payload.enemy.actor.hit);}else return enemy_fault(c->w);return 1;}
static int sound(void*ctx,uint16_t s,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,s,stop)?1:enemy_fault(c->w);}
static int call(void*ctx,unsigned n,struct Animation*a){(void)a;struct Context*c=ctx;if(n>=sizeof(fort_callback_patches)/sizeof(*fort_callback_patches))return enemy_fault(c->w);return ft_patch(c->w,fort_callback_patches[n]);}
int ft_animate(struct WeaponWorld*w,struct NativeEntity*e){struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;struct Context c={w,e};struct AnimationOwner o={.context=&c,.read=read,.write=write,.bind=bind,.sound=sound,.call=call};int done=animation_step(&a->animation,&fort_program,&o);if(a->animation.fault)enemy_fault(w);return done;}
struct EnemyActor*ft_parent(struct WeaponWorld*w,struct NativeEntity*e){struct NativeEntity*p=entity_get(w->entities,e->parent);if(!p||p->kind!=ENTITY_ENEMY){enemy_fault(w);return 0;}return &p->payload.enemy;}
static void init(struct NativeEntity*e,unsigned s){uint8_t*p=(uint8_t*)&e->payload.enemy.fort;for(unsigned i=0;i<sizeof(e->payload.enemy.fort);i++)p[i]=255;ft_schedule(e,s);}
struct NativeEntity*ft_child(struct WeaponWorld*w,struct NativeEntity*e,unsigned s,enum EntityInsert order){struct NativeEntity*n=enemy_allocate(w,entity_ref(e),order,&e->payload.enemy.actor.transform,tick);if(n)init(n,s);return n;}
struct NativeEntity*ft_pattern(struct WeaponWorld*w,struct NativeEntity*e,unsigned id){if(id>=FTP_COUNT){enemy_fault(w);return 0;}struct NativeEntity*n=ft_child(w,e,FT_PATTERN,ENTITY_LAST_CHILD);if(n)n->payload.enemy.fort.pattern=(uint16_t)id;return n;}
int ft_hurt(struct WeaponWorld*w,struct WeaponActor*a,int unconditional){if(unconditional||(!(a->hit.flags&1)&&(a->hit.events&1)&&a->hit.last_kind==1))return interaction_sound(w,0x108d);return 1;}
int ft_score(struct WeaponWorld*w,struct WeaponActor*a,uint32_t score){return w->items&&item_score_add(w->items,a->score_owner,score)?1:enemy_fault(w);}
int ft_cull(struct WeaponWorld*w,struct NativeEntity*e){return ft_sx(w,&e->payload.enemy.actor)<ft_word(-e->payload.enemy.fort.margin);}
int ft_children(struct WeaponWorld*w,struct NativeEntity*e){return e->next&&w->entities->nodes[e->next].depth>e->depth;}
static int watch_stop(const struct NativeEntity*e){return e->kind==ENTITY_ENEMY?e->payload.enemy.fort.watch:-1;}
int ft_watch(struct WeaponWorld*w,struct NativeEntity*e,uint32_t source){e->payload.enemy.fort.watch=0;return mission_event_spawn(w,entity_ref(e),source,watch_stop)!=0;}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;
 for(unsigned budget=0;budget<8;budget++){
  unsigned s=q->actor.state;if(s==FT_DELETE){entity_delete(p,e);return;}
  if(q->entry){q->entry=0;int r=s<FT_CAR?ft_structures_enter(w,e):s<FT_PATTERN?ft_vehicles_enter(w,e):ft_effects_enter(w,e);if(!r||w->fault)return;if(r==2)continue;}
  if(!e->active||e->deleting)return;s=q->actor.state;int r=s<FT_CAR?ft_structures_frame(w,e,s):s<FT_PATTERN?ft_vehicles_frame(w,e,s):ft_effects_frame(w,e,s);if(r!=2)return;
 }enemy_fault(w);
}
int fort_spawn_scene(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,const struct PlayerTransform*t,enum FortEntry id,uint16_t config,uint8_t flags){
 static const uint8_t states[]={FT_ROOT,FT_ROOT,FT_ROOT,FT_ROOT,FT_ROOT,FT_PROP,FT_PROP,FT_PROP,FT_SIGN,FT_WALL,FT_HATCH,FT_HATCH,FT_QUAD,FT_HELI,FT_BARRIER,FT_CAR};
 (void)config;if((unsigned)id>=sizeof(states))return enemy_fault(w);struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;init(e,states[id]);struct EnemyActor*q=&e->payload.enemy;q->actor.display_flags|=flags;
 q->fort.variant=id<=FORT_ROOT4?id:id>=FORT_PROP0&&id<=FORT_PROP2?id-FORT_PROP0:id==FORT_HATCH5?5:id==FORT_HATCH6?6:0;return 1;
}
int fort_spawn_scene4_record(struct WeaponWorld*w,struct EntityRef parent,unsigned index){
 /* Source5DCCE always chooses a global group and FIRST child. */
 (void)parent;return scene_placement_spawn(w,4,index);
}
int fort_spawn_template(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){
 if(id!=269)return enemy_fault(w);struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;init(e,FT_HELI);if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];return 1;
}
