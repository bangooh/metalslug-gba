#include "slug_death_fx.h"
#include "../generated/slug-death-programs.h"
enum Kind {FX_EXPLOSION,FX_SMOKE,FX_SPAWNER,FX_DEBRIS,FX_BLAST};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static int fail(struct WeaponWorld*w){if(!w->fault)w->fault=25;return 0;}
static int16_t word(int v){return (int16_t)(uint16_t)v;}
static struct NativeEntity*spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,unsigned kind,const struct PlayerTransform*t){
 if(w->fault||!w->entities){fail(w);return 0;}
 struct NativeEntity*e=entity_spawn(w->entities,parent,ENTITY_SLUG_AUX,order,tick);if(!e){fail(w);return 0;}
 struct SlugAuxActor*q=&e->payload.slug_aux;
 q->actor=(struct WeaponActor){.active=1,.owner=255,.damage_owner=255,.score_owner=255,.platform_ids=255,.ground_slope=-1,.ground_tag=-1,
  .phase=255,.ttl=255,.raw_surface=255,.origin_y_up=-1,.trail_drawing=65535,
  .hit={.id=e->index,.hp=-1,.impact_x=-1,.impact_y=-1,.last_attacker=65535,.last_kind=255,.flags=64},
  .animation={.drawing=65535,.cursor=65535,.last_draw_cursor=65535}};
 q->kind=(uint8_t)kind;q->actor.transform=*t;q->actor.transform.display_group=0;
 transform_integer_copy(&q->actor.transform,t);return e;
}
static void retire(struct WeaponWorld*w,struct NativeEntity*e){e->payload.slug_aux.actor.active=0;entity_delete(w->entities,e);}
static int outside(struct WeaponWorld*w,struct WeaponActor*a,const int16_t*b){
 int16_t x=word((a->transform.x>>8)-w->camera_x),y=word(transform_y_up(&a->transform)+w->camera_y);
 a->hit.flags|=128;return x<b[0]||x>word(320+b[1])||y<word(256-b[3])||y>word(496-b[2]);
}
static int animate(struct WeaponWorld*w,struct WeaponActor*a){
 const struct AnimationOwner owner={0};int complete=animation_step(&a->animation,&slug_death_program,&owner);
 if(a->animation.fault)fail(w);return complete;
}
static int move(struct WeaponWorld*w,struct WeaponActor*a,enum WeaponMove policy){
 struct WeaponContact hit;if(!w->move)return fail(w);
 return w->move(w->scene,a,policy,&hit)>=0?1:fail(w);
}
static int attack(struct WeaponWorld*w,struct WeaponActor*a){
 if(!w->attack)return fail(w);a->hit.x=word(a->transform.x>>8);a->hit.y=transform_y_up(&a->transform);a->hit.flip=a->transform.facing;
 for(unsigned i=0;i<a->attack->count;i++)if(!w->attack(w->scene,&a->hit,&a->attack->records[i],0))return fail(w);return 1;
}
static void spawn_frame(struct WeaponWorld*w,struct NativeEntity*e){
 struct SlugAuxActor*q=&e->payload.slug_aux;struct WeaponActor*a=&q->actor;
 const struct SlugDebrisSpawner*d=&slug_debris_spawner;
 struct WeaponActor*parent=entity_weapon_actor(entity_get(w->entities,e->parent));if(!parent||!w->rng){fail(w);return;}
 /* $044048 only adjusts camera words. Native positions are already world
  * positions; do not substitute platform synchronization for this call. */
 if(--q->frame){if(!--q->distance)retire(w,e);return;}q->frame=d->interval;
 int dx=(parent->transform.facing&1)?-d->dx:d->dx;
 int dy=(parent->transform.facing&1)?-d->dy:d->dy; /* source tests bit 0 for BOTH base offsets */
 transform_integer_offset(&a->transform,word((parent->transform.x>>8)+dx-(a->transform.x>>8)),
  word(transform_y_up(&parent->transform)+dy-transform_y_up(&a->transform)));
 unsigned choice=source_random(w->rng,16);
 struct NativeEntity*child=spawn(w,entity_ref(e),ENTITY_LAST_CHILD,FX_DEBRIS,&a->transform);if(!child)return;
 struct SlugAuxActor*r=&child->payload.slug_aux;r->frame=slug_debris_choices[choice];
 dx=source_random(w->rng,d->rx);if(a->transform.facing&1)dx=-dx;
 dy=source_random(w->rng,d->ry);if(a->transform.facing&2)dy=-dy;
 transform_integer_offset(&r->actor.transform,dx,dy);r->actor.display_flags|=a->display_flags&64;
 if(!--q->distance)retire(w,e);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 (void)p;struct WeaponWorld*w=world;struct SlugAuxActor*q=&e->payload.slug_aux;struct WeaponActor*a=&q->actor;
 static const int16_t bounds[4]={0,1,0,1};
 if(w->fault)return;
 if(!a->state){
  a->state=1;
  if(q->kind==FX_SPAWNER){q->frame=slug_debris_spawner.interval;q->distance=slug_debris_spawner.count;}
  else if(q->kind==FX_DEBRIS){
   if(q->frame>=5||!w->rng){fail(w);return;}const struct SlugDebrisSpec*d=&slug_debris_specs[q->frame];
   animation_begin(&a->animation,d->entry);a->movement_flags|=32;a->palette=d->palette;
   a->transform.facing|=source_random_word(w->rng)&1;a->vy=0x7f8;a->ay=-68;a->ax=0;
   int16_t vx=(int16_t)source_random(w->rng,0x3ff);a->vx=(vx&1)?word(-vx):vx;
   uint16_t random=source_random(w->rng,0x7ff);a->vy=word(a->vy-random);a->transform.facing|=random&1;
   if(outside(w,a,d->bounds)){retire(w,e);return;}a->defense=0;a->defense_count=0;
  }else if(q->kind==FX_BLAST){
   /* Source child has no copy-transform call: its facing stays zero. */
   a->transform.x=word(w->camera_x+160)*256;a->transform.y=(512-(int32_t)word(384-w->camera_y))*256;
   a->attack=&slug_attack_sets[SATT_DESTROYED_C];hit_begin_attack(&a->hit);hit_begin_attack(&a->hit);
   if(!attack(w,a))return;a->attack=0;hit_begin_attack(&a->hit);a->state=2;return;
  }else{
   animation_begin(&a->animation,slug_death_entries[q->kind==FX_SMOKE?SDA_SMOKE:SDA_EXPLOSION]);a->movement_flags|=32;
   a->palette=q->kind==FX_SMOKE?4:0x163;a->transform.priority=q->kind==FX_SMOKE?0x2000:0xd000;
  }
 }
 if(q->kind==FX_BLAST){retire(w,e);return;}
 if(q->kind==FX_SPAWNER){spawn_frame(w,e);return;}
 if(q->kind==FX_DEBRIS){
  if(a->vy<=-0x500)a->vy=-0x4ff;if(!move(w,a,WMOVE_POINT))return;
  if(!w->sprite_budget.valid){fail(w);return;}
  uint16_t quarter=(uint16_t)(w->sprite_budget.last-w->sprite_budget.first)>>2;
  int complete=0;if(w->sprite_budget.counter>=quarter)complete=animate(w,a);
  if(w->fault)return;if(complete||outside(w,a,slug_debris_specs[q->frame].bounds))retire(w,e);return;
 }
 if(!move(w,a,WMOVE_SYNC))return;int complete=animate(w,a);if(w->fault)return;
 if(complete||outside(w,a,bounds))retire(w,e);
}
int slug_spawn_death_fx(struct SlugAnimationContext*c){
 struct WeaponWorld*w=c->weapons;if(!w||!w->sound||!w->sound(w->scene,0x1034,0))return w?fail(w):0;
 if(!spawn(w,c->entity,ENTITY_FIRST_CHILD,FX_EXPLOSION,&c->slug->actor.transform))return 0;
 return spawn(w,c->entity,ENTITY_LAST_CHILD,FX_SPAWNER,&c->slug->actor.transform)!=0;
}
int slug_spawn_death_smoke(struct SlugAnimationContext*c){return spawn(c->weapons,c->entity,ENTITY_FIRST_CHILD,FX_SMOKE,&c->slug->actor.transform)!=0;}
int slug_spawn_self_destruct_blast(struct SlugAnimationContext*c){
 const struct PlayerTransform empty={0};struct NativeEntity*e=spawn(c->weapons,c->entity,ENTITY_FIRST_CHILD,FX_BLAST,&empty);
 if(!e)return 0;
 /* $0517FE copies $68, then $05CCC8 copies the damage attribution $6D. */
 e->payload.slug_aux.actor.owner=c->slug->actor.owner;
 e->payload.slug_aux.actor.damage_owner=c->slug->actor.damage_owner;return 1;
}
