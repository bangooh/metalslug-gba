#include "slug_surface.h"
#include "../generated/slug-surface-programs.h"
enum State {NEW,TRACK,WAIT,FLASH,FLASH_FRAME,ATTACH,ATTACH_FRAME,DETACH,DETACH_FRAME,DELETE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static int fail(struct WeaponWorld*w){if(!w->fault)w->fault=27;return 0;}
static int16_t word(int v){return (int16_t)(uint16_t)v;}
static struct NativeEntity*spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,const struct PlayerTransform*t){
 struct NativeEntity*e=entity_spawn(w->entities,parent,ENTITY_SLUG_AUX,order,tick);if(!e){fail(w);return 0;}
 e->payload.slug_aux.actor=(struct WeaponActor){.active=1,.owner=255,.damage_owner=255,.score_owner=255,.platform_ids=255,.ground_slope=-1,.ground_tag=-1,
  .phase=255,.ttl=255,.raw_surface=255,.origin_y_up=-1,.trail_drawing=65535,
  .hit={.id=e->index,.hp=-1,.impact_x=-1,.impact_y=-1,.last_attacker=65535,.last_kind=255,.flags=64},
  .animation={.drawing=65535,.cursor=65535,.last_draw_cursor=65535}};
 e->payload.slug_aux.actor.transform=*t;transform_integer_copy(&e->payload.slug_aux.actor.transform,t);
 e->payload.slug_aux.actor.animation.pose=0;e->payload.slug_aux.surface.raw=255;return e;
}
static void retire(struct WeaponWorld*w,struct NativeEntity*e){e->payload.slug_aux.actor.active=0;entity_delete(w->entities,e);}
static uint32_t read_field(void*user,unsigned field,unsigned width){
 struct SlugAuxActor*q=user;if(field==0&&width==1)return q->actor.animation.pose;
 if(field==1&&width==4)return q->surface.delay_table;q->actor.animation.fault=3;return 0;
}
static int write_field(void*user,unsigned field,unsigned width,enum AnimationFieldOperation op,uint32_t value){
 struct SlugAuxActor*q=user;if(field||width!=1||op!=AN_FIELD_SET){q->actor.animation.fault=3;return 0;}
 q->actor.animation.pose=(uint8_t)value;return 1;
}
static int delay(void*user,unsigned table,unsigned pose,uint8_t*value){
 struct SlugAuxActor*q=user;if(table!=1||q->surface.delay_table>=12||pose>=10)return 0;
 *value=(q->kind?slug_surface_pow_delays:slug_surface_delays)[q->surface.delay_table][pose];return 1;
}
static int animate(struct WeaponWorld*w,struct SlugAuxActor*q){
 struct AnimationOwner owner={.context=q,.read=read_field,.write=write_field,.delay=delay};
 int done=animation_step(&q->actor.animation,&slug_surface_program,&owner);if(q->actor.animation.fault)fail(w);return done;
}
static void start(struct SlugAuxActor*q,uint16_t entry){animation_begin(&q->actor.animation,entry);q->actor.movement_flags|=32;}
static int probe(struct WeaponWorld*w,struct SlugAuxActor*q,const struct WeaponActor*parent){
 struct WeaponContact hit;if(!w->probe||!w->probe(w->scene,parent,&hit))return fail(w);
 q->surface.material=hit.raw&0xc0;q->surface.air=(uint8_t)!hit.solid;
 if(hit.solid)q->surface.raw=hit.raw;return 1;
}
static void follow(struct SlugAuxActor*q,const struct WeaponActor*p){
 q->surface.old_x=word(q->actor.transform.x>>8);q->surface.old_y=transform_y_up(&q->actor.transform);
 transform_integer_offset(&q->actor.transform,word((p->transform.x>>8)-(q->actor.transform.x>>8)),
  word(transform_y_up(&p->transform)-transform_y_up(&q->actor.transform)));
}
static void flags(struct SlugAuxActor*q){
 q->actor.display_flags|=64;if(q->surface.material==0x80)q->actor.display_flags&=(uint8_t)~64u;
}
static void surface_y(struct WeaponWorld*w,struct SlugAuxActor*q){
 if(q->surface.material==0x40)q->actor.transform.y=(w->scene_number==6||w->scene_number==9?0x1ef:0x167)*256;
}
static int detached(struct WeaponWorld*w,struct NativeEntity*e,int copy_angle,const struct WeaponActor*parent){
 struct SlugAuxActor*q=&e->payload.slug_aux;
 struct NativeEntity*n=spawn(w,entity_ref(e),ENTITY_LAST_CHILD,&q->actor.transform);if(!n)return 0;
 struct SlugAuxActor*r=&n->payload.slug_aux;r->kind=q->kind;r->surface.raw=q->surface.raw;r->actor.state=DETACH;
 r->actor.display_flags|=q->actor.display_flags&64;
 if(copy_angle)r->actor.trail_angle=parent->trail_angle;return 1;
}
static int outside(struct WeaponWorld*w,struct WeaponActor*a){
 int16_t x=word((a->transform.x>>8)-w->camera_x),y=word(transform_y_up(&a->transform)+w->camera_y);
 a->hit.flags|=128;return x<0||x>321||y<255||y>496;
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;struct SlugAuxActor*q=&e->payload.slug_aux;struct WeaponActor*a=&q->actor;
 if(w->fault)return;if(a->state==DELETE){retire(w,e);return;}
 if(a->state==DETACH){
  /* $077A16: $82 branches past ADDQ; only $8C falls through it. */
  start(q,(q->kind?slug_surface_pow_detach:slug_surface_detach)[q->surface.raw==0x8c]);a->palette=w->scene_number==5?0x138:0x10;a->transform.priority++;a->state=DETACH_FRAME;
 }
 if(a->state==DETACH_FRAME){
  struct WeaponContact hit;if(!w->move||w->move(w->scene,a,WMOVE_SYNC,&hit)<0){fail(w);return;}
  if(animate(w,q))a->state=DELETE;if(outside(w,a))a->state=DELETE;return;
 }
 struct WeaponActor*parent=entity_weapon_actor(entity_get(p,e->parent));if(!parent){fail(w);return;}
 if(a->state==NEW){
  if(w->scene_number!=0&&w->scene_number!=9){retire(w,e);return;}a->palette=0x10;a->transform.priority=0xe000;a->state=TRACK;
 }
 enum State state=(enum State)a->state;
 if(state==TRACK){
  follow(q,parent);if(!probe(w,q,parent))return;
  if(q->surface.air){if(q->surface.material!=0x40&&q->surface.material!=0x80)a->state=WAIT;}
  else {flags(q);a->state=FLASH;}
 }else if(state==WAIT){if(!probe(w,q,parent))return;if(!q->surface.air){flags(q);a->state=FLASH;}}
 else {
  if(state==FLASH){
   if(!probe(w,q,parent))return;unsigned variant=q->surface.material==0x80&&(q->surface.raw==0x82||q->surface.raw==0x8c);
   start(q,(q->kind?slug_surface_pow_flash:slug_surface_flash)[variant]);a->state=state=FLASH_FRAME;
  }
  if(state==FLASH_FRAME){
   follow(q,parent);if(!probe(w,q,parent))return;
   if(q->surface.air){surface_y(w,q);a->state=TRACK;if(!probe(w,q,parent))return;
    if(q->surface.material&&q->surface.material!=0xc0&&!detached(w,e,1,parent))return;
   }else if(q->surface.material&&q->surface.material!=0xc0){surface_y(w,q);flags(q);if(animate(w,q))a->state=ATTACH;}
  }else {
   if(state==ATTACH){q->surface.selector=65535;a->state=state=ATTACH_FRAME;}
   if(state!=ATTACH_FRAME){fail(w);return;}
   q->surface.old_x=word(a->transform.x>>8);q->surface.old_y=transform_y_up(&a->transform);
   if(!probe(w,q,parent))return;
   transform_integer_offset(&a->transform,word((parent->transform.x>>8)-(a->transform.x>>8)),word(transform_y_up(&parent->transform)-transform_y_up(&a->transform)));
   surface_y(w,q);unsigned which=parent->vx?(parent->vx<0?8:4):0;
   if(q->surface.material==0x80){
    if(q->surface.raw==0x82||q->surface.raw==0x8c)which+=2;
    if(!w->secondary_view_valid){fail(w);return;}
    uint16_t secondary=(uint16_t)word(512-transform_y_up(&a->transform)-w->camera_y+w->secondary_y_offset);
    if(secondary>=0xd20&&secondary<0xe58)which++;
   }
   if(q->surface.selector!=which){q->surface.selector=(uint16_t)which;start(q,(q->kind?slug_surface_pow_attach:slug_surface_attach)[which]);}
   if(q->surface.material&&q->surface.material!=0xc0){
    if(q->surface.air){a->state=TRACK;if(!detached(w,e,0,parent))return;}
    else {flags(q);q->surface.delay_table=(uint8_t)q->surface.selector;(void)animate(w,q);}
   }
  }
 }
 if(entity_parent_deleting(p,e))a->state=DELETE;
}
int slug_spawn_surface(struct SlugAnimationContext*c){return spawn(c->weapons,c->entity,ENTITY_FIRST_CHILD,&c->slug->actor.transform)!=0;}

int pow_spawn_surface(struct WeaponWorld*w,struct EntityRef parent,const struct PlayerTransform*t){
 (void)t;struct PlayerTransform empty={0};
 struct NativeEntity*e=spawn(w,parent,ENTITY_FIRST_CHILD,&empty);if(!e)return 0;e->payload.slug_aux.kind=1;return 1;
}

/* Direct source $07773E entry for non-Slug vehicle parents. */
int vehicle_spawn_surface(struct WeaponWorld*w,struct EntityRef parent,const struct PlayerTransform*t){return spawn(w,parent,ENTITY_FIRST_CHILD,t)!=0;}
