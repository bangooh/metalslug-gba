#include "scene_placements.h"
#include "airship_internal.h"
#include "fort_internal.h"
/* $088F74..$08B944: native states; source addresses are import provenance only. */
static void tick(struct EntityPool*,struct NativeEntity*,void*);
void as_schedule(struct NativeEntity*e,unsigned s){e->tick=tick;enemy_schedule(&e->payload.enemy,s);}
void as_reset(struct WeaponActor*a){a->vx=a->vy=a->ax=a->ay=0;transform_integer_copy(&a->transform,&a->transform);}
void as_screen(struct WeaponWorld*w,struct WeaponActor*a,int16_t x,int16_t y){transform_integer_offset(&a->transform,as_word(x-as_sx(w,a)),as_word(y-as_sy(w,a)));}
uint16_t as_random(struct WeaponWorld*w){if(!w->rng){enemy_fault(w);return 0;}return source_random_word(w->rng);}
uint16_t as_bounded(struct WeaponWorld*w,uint16_t n){if(!w->rng){enemy_fault(w);return 0;}return source_random(w->rng,n);}
void as_start_entry(struct EnemyActor*q,uint16_t n){if(n==65535)return;animation_begin(&q->actor.animation,n);q->actor.movement_flags|=32;}
void as_start(struct EnemyActor*q,enum AirshipAnimation n){as_start_entry(q,airship_entries[n]);}
void as_defense(struct EnemyActor*q,unsigned n){q->actor.defense=n==65535?0:airship_bindings[n].defense;q->actor.defense_count=n==65535?0:airship_bindings[n].count;}
void as_attack(struct EnemyActor*q,unsigned n){if(n==65535)q->actor.attack=0;else{q->attack_set=(struct HitAttackSet){airship_bindings[n].attack,airship_bindings[n].count};q->actor.attack=&q->attack_set;}}
int as_patch(struct WeaponWorld*w,unsigned n){return n<sizeof(airship_patches)/sizeof(*airship_patches)&&scene_video_patch(w->video,&airship_patches[n])?1:enemy_fault(w);}
int as_stamp(struct WeaponWorld*w,struct NativeEntity*e,unsigned n){if(n>=ASS_COUNT||!w->interactions)return enemy_fault(w);const struct AirshipStamp*t=&airship_stamps[n];return terrain_stamp(w->interactions->terrain,&e->payload.enemy.actor.transform,t->dx,t->dy,t->width,t->height,t->cells)?1:enemy_fault(w);}
int as_lamp(struct WeaponWorld*w,struct EnemyActor*q,int three,int on){
 if(!w->video||!w->video->palette_cycle)return enemy_fault(w);
 for(unsigned i=three?0:1;i<3;i++)if(!w->video->palette_cycle(w->video->palette_context,(uint16_t)(0x10+i),scene_palette_high((uint16_t)((on?0x1ab:0x80)+i)),65535,1))return enemy_fault(w);
 q->actor.palette=on?q->airship.base_palette:q->airship.alternate_palette;return 1;
}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;};
static int bind(void*ctx,unsigned kind,uint32_t value){struct Context*c=ctx;unsigned n=value==0xffffffffu?65535:value;if(n!=65535&&n>=sizeof(airship_bindings)/sizeof(*airship_bindings))return enemy_fault(c->w);if(kind==AN_DEFENSE)as_defense(&c->e->payload.enemy,n);else if(kind==AN_ATTACK){as_attack(&c->e->payload.enemy,n);hit_begin_attack(&c->e->payload.enemy.actor.hit);}else return enemy_fault(c->w);return 1;}
static int sound(void*ctx,uint16_t s,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,s,stop)?1:enemy_fault(c->w);}
static int call(void*ctx,unsigned n,struct Animation*a){(void)a;struct Context*c=ctx;switch(n){case ASC_8B45C:return as_lamp(c->w,&c->e->payload.enemy,0,1);case ASC_8B490:return as_lamp(c->w,&c->e->payload.enemy,0,0);case ASC_8B4C4:return as_lamp(c->w,&c->e->payload.enemy,1,1);case ASC_8B50E:return as_lamp(c->w,&c->e->payload.enemy,1,0);default:return enemy_fault(c->w);}}
int as_animate(struct WeaponWorld*w,struct NativeEntity*e){struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;struct Context c={w,e};struct AnimationOwner o={.context=&c,.bind=bind,.sound=sound,.call=call};int done=animation_step(&a->animation,&airship_program,&o);if(a->animation.fault)enemy_fault(w);return done;}
struct EnemyActor*as_parent(struct WeaponWorld*w,struct NativeEntity*e){struct NativeEntity*p=entity_get(w->entities,e->parent);if(!p||p->kind!=ENTITY_ENEMY){enemy_fault(w);return 0;}return &p->payload.enemy;}
static void init(struct NativeEntity*e,unsigned s){uint8_t*p=(uint8_t*)&e->payload.enemy.airship;for(unsigned i=0;i<sizeof(e->payload.enemy.airship);i++)p[i]=255;as_schedule(e,s);}
struct NativeEntity*as_child(struct WeaponWorld*w,struct NativeEntity*e,unsigned s,enum EntityInsert order,int dx,int dy){struct NativeEntity*n=enemy_allocate(w,entity_ref(e),order,&e->payload.enemy.actor.transform,tick);if(n){init(n,s);transform_integer_offset(&n->payload.enemy.actor.transform,(int16_t)dx,(int16_t)dy);}return n;}
struct NativeEntity*as_pattern(struct WeaponWorld*w,struct NativeEntity*e,unsigned id){if(id>=ASP_COUNT){enemy_fault(w);return 0;}struct NativeEntity*n=as_child(w,e,AS_PATTERN,ENTITY_LAST_CHILD,0,0);if(n)n->payload.enemy.airship.pattern=(uint16_t)id;return n;}
int as_blast(struct WeaponWorld*w,struct NativeEntity*e,int dx,int dy){struct NativeEntity*n=ft_child(w,e,FT_BLAST,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.fort.variant=1;transform_integer_offset(&n->payload.enemy.actor.transform,(int16_t)dx,(int16_t)dy);return 1;}
int as_hurt(struct WeaponWorld*w,struct WeaponActor*a){if(!(a->hit.flags&1)&&(a->hit.events&1)&&a->hit.last_kind==1)return interaction_sound(w,0x108d);return 1;}
int as_score(struct WeaponWorld*w,struct WeaponActor*a,uint32_t score){return w->items&&item_score_add(w->items,a->score_owner,score)?1:enemy_fault(w);}
int as_cull(struct WeaponWorld*w,struct NativeEntity*e){return as_sx(w,&e->payload.enemy.actor)<as_word(-e->payload.enemy.airship.margin);}
static int watch_stop(const struct NativeEntity*e){return e->kind==ENTITY_ENEMY?e->payload.enemy.airship.watch:-1;}
int as_watch(struct WeaponWorld*w,struct NativeEntity*e,uint32_t source){e->payload.enemy.airship.watch=0;return mission_event_spawn(w,entity_ref(e),source,watch_stop)!=0;}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;
 for(unsigned budget=0;budget<8;budget++){
  unsigned s=q->actor.state;if(s==AS_DELETE||s==AS_DELETE_TREE){if(s==AS_DELETE_TREE)entity_delete_children(p,e);entity_delete(p,e);return;}
  if(q->entry){q->entry=0;int r=s<AS_BODY?as_structures_enter(w,e):s<AS_PATTERN?as_body_enter(w,e):as_effects_enter(w,e);if(!r||w->fault)return;if(r==2)continue;}
  if(!e->active||e->deleting)return;s=q->actor.state;int r=s<AS_BODY?as_structures_frame(w,e,s):s<AS_PATTERN?as_body_frame(w,e,s):as_effects_frame(w,e,s);if(r!=2)return;
 }enemy_fault(w);
}
int airship_spawn_scene(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,const struct PlayerTransform*t,enum AirshipEntry id,uint8_t flags){
 static const uint8_t states[]={AS_PROP,AS_PROP,AS_PROP,AS_CAMP,AS_BARRELS,AS_ROCKRAIN,AS_BODY,AS_TOWER,AS_TOWER,AS_CRATE,AS_CRATE,AS_CRATE,AS_CRATE,AS_CRATE,AS_SOLID,AS_SOLID,AS_GUN};
 if((unsigned)id>=sizeof(states))return enemy_fault(w);struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;init(e,states[id]);struct EnemyActor*q=&e->payload.enemy;q->actor.display_flags|=flags;
 q->airship.variant=id<=AIRSHIP_PROP2?id:id==AIRSHIP_TOWER_B?1:id>=AIRSHIP_CRATE0&&id<=AIRSHIP_CRATE4?id-AIRSHIP_CRATE0:id==AIRSHIP_SOLID1?1:0;return 1;
}
int airship_spawn_scene5_record(struct WeaponWorld*w,struct EntityRef parent,unsigned index){
 /* Source5DCCE always chooses a global group and FIRST child. */
 (void)parent;return scene_placement_spawn(w,5,index);
}
int airship_spawn_template(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){
 if((id<254||id>258)&&id!=270&&id!=323&&id!=324)return enemy_fault(w);struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;init(e,id>=323?AS_CROUCH_PAIR:id==258?AS_BUNKER:id==270?AS_THROWN:AS_DROP);e->payload.enemy.airship.variant=(uint8_t)(id-(id>=323?323:254));if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];return 1;
}
