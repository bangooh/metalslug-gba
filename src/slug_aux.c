#include "slug_aux.h"
#include "../generated/slug-aux-programs.h"
static int fail(struct WeaponWorld*w,unsigned code){if(!w->fault)w->fault=(uint8_t)code;return 0;}
static void retire(struct EntityPool*p,struct NativeEntity*e){e->payload.slug_aux.actor.active=0;entity_delete(p,e);}
static struct SlugSharedState*parent(struct EntityPool*p,struct NativeEntity*e){
 struct NativeEntity*owner=entity_get(p,e->parent);return owner&&owner->kind==ENTITY_SLUG?owner->payload.slug:0;
}
static void overlay_tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;struct SlugAuxActor*q=&e->payload.slug_aux;struct WeaponActor*a=&q->actor;
 if(w->fault)return;if(a->state==2){retire(p,e);return;}
 struct SlugSharedState*s=parent(p,e);if(!s){fail(w,23);return;}
 if(!a->state){
  unsigned player=slug_ridden_by(s,0)?0:slug_ridden_by(s,1)?1:2;
  if(player==2){retire(p,e);return;}
  struct PlayerBodyState*rider=w->players[player];if(!rider||rider->actor.local_weapon>=5){fail(w,23);return;}
  unsigned visual=rider->character;
  a->owner=rider->actor.owner;a->palette=(uint16_t)(visual?0x177:0x176);
  q->palette_alternate=0x190;q->palette_impact=(uint16_t)(visual?0x192:0x191);q->sprite_template=a->hit_palette=(uint16_t)(visual?0x90:0x8f);
  a->display_flags=(uint8_t)((a->display_flags&~16u)|(visual?16:0));
  animation_begin(&a->animation,slug_overlay_entries[visual][rider->actor.local_weapon]);a->movement_flags|=32;
  uint16_t angle=(uint16_t)(32-(s->angle>>1));if(angle>64)angle=64;
  q->offset_x=slug_overlay_offsets[angle][0];q->offset_y_up=slug_overlay_offsets[angle][1];a->state=1;
 }
 if(!w->move){fail(w,23);return;}struct WeaponContact hit;
 if(w->move(w->scene,a,WMOVE_SYNC,&hit)<0){fail(w,23);return;}
 /* Integer-only parent follow retains the child's fractional position. */
 transform_integer_offset(&a->transform,(int16_t)((s->actor.transform.x>>8)+q->offset_x-(a->transform.x>>8)),
  (int16_t)(transform_y_up(&s->actor.transform)+q->offset_y_up-transform_y_up(&a->transform)));
 a->transform.priority=(uint16_t)((s->actor.transform.priority&0xffe3)|4);
 const struct AnimationOwner owner={0};
 int complete=animation_step(&a->animation,&slug_overlay_program,&owner);
 if(a->animation.fault){fail(w,23);return;}
 if(complete){a->state=2;if(!w->sound||!w->sound(w->scene,0x108b,0))fail(w,23);}
}
int slug_spawn_mount_overlay(struct SlugAnimationContext*c){
 struct WeaponWorld*w=c->weapons;
 if(!w||!w->entities||w->fault){if(!c->slug->fault)c->slug->fault=6;return 0;}
 struct NativeEntity*e=entity_spawn(w->entities,c->entity,ENTITY_SLUG_AUX,ENTITY_FIRST_CHILD,overlay_tick);
 if(!e){if(!c->slug->fault)c->slug->fault=6;return 0;}
 struct WeaponActor*a=&e->payload.slug_aux.actor;
 *a=(struct WeaponActor){.active=1,.owner=255,.damage_owner=255,.score_owner=255,.platform_ids=255,.ground_slope=-1,.ground_tag=-1,
  .phase=255,.ttl=255,.raw_surface=255,.origin_y_up=-1,.trail_drawing=65535,
  .hit={.id=e->index,.hp=-1,.impact_x=-1,.impact_y=-1,.last_attacker=65535,.last_kind=255,.flags=64},
  .animation={.drawing=65535,.cursor=65535,.last_draw_cursor=65535}};
 a->transform=c->slug->actor.transform;a->transform.display_group=0;
 transform_integer_copy(&a->transform,&c->slug->actor.transform);return 1;
}
