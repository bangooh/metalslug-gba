#include "entities.h"
#include "slug_support.h"
_Static_assert(sizeof(struct EntityPool)<=65536,"native object pool exceeds its EWRAM budget");
static void fault(struct EntityPool*p,unsigned code){if(!p->fault)p->fault=(uint8_t)code;}
struct EntityRef entity_ref(const struct NativeEntity*e){
 return e?(struct EntityRef){e->index,e->generation}:(struct EntityRef){ENTITY_NONE,0};
}
struct NativeEntity*entity_get(struct EntityPool*p,struct EntityRef ref){
 if(ref.index>=ENTITY_CAPACITY)return 0;
 struct NativeEntity*e=&p->nodes[ref.index];
 return e->active&&e->generation==ref.generation?e:0;
}
struct WeaponActor*entity_weapon_actor(struct NativeEntity*e){
 if(!e)return 0;
 if(e->kind==ENTITY_SLUG)return e->payload.slug?&e->payload.slug->actor:0;
 if(e->kind==ENTITY_SLUG_AUX)return &e->payload.slug_aux.actor;
 if(e->kind==ENTITY_INTERACTION)return &e->payload.interaction.actor;
 if(e->kind==ENTITY_ENEMY)return &e->payload.enemy.actor;
 if(e->kind==ENTITY_ITEM)return &e->payload.item.actor;
 if(e->kind==ENTITY_PROJECTILE||e->kind==ENTITY_EFFECT)return &e->payload.weapon;
 return 0;
}
void entities_init(struct EntityPool*p){
 *p=(struct EntityPool){.free_head=1,.current=0};
 /* Index zero is a permanent sentinel, not a game object or source address. */
 p->nodes[0]=(struct NativeEntity){.active=1,.reserved=1,.kind=ENTITY_GROUP,.generation=1,
  .parent={ENTITY_NONE,0},.index=0,.previous=0,.next=0,.payload.group={.phase=255,.command=255,.captor_done=255}};
 for(unsigned i=1;i<ENTITY_CAPACITY;i++){
  p->nodes[i].index=(uint16_t)i;
  p->nodes[i].next=(uint16_t)(i+1<ENTITY_CAPACITY?i+1:ENTITY_NONE);
 }
}
static void link_after(struct EntityPool*p,struct NativeEntity*e,unsigned previous){
 struct NativeEntity*before=&p->nodes[previous];
 e->previous=(uint16_t)previous;e->next=before->next;
 p->nodes[e->next].previous=e->index;before->next=e->index;
}
struct NativeEntity*entity_spawn(struct EntityPool*p,struct EntityRef parent,
 enum EntityKind kind,enum EntityInsert order,EntityTick tick){
 if(p->fault)return 0;
 struct NativeEntity*owner=entity_get(p,parent);
 if(!owner||owner->deleting||owner->depth==255||(unsigned)kind>ENTITY_TERRAIN_CONTACT||(!tick&&kind!=ENTITY_GROUP)){
  fault(p,1);return 0;
 }
 if(order!=ENTITY_FIRST_CHILD&&order!=ENTITY_LAST_CHILD){fault(p,3);return 0;}
 if(p->free_head==ENTITY_NONE){fault(p,2);return 0;}
 unsigned index=p->free_head;struct NativeEntity*e=&p->nodes[index];
 p->free_head=e->next;uint16_t generation=(uint16_t)(e->generation+1);
 *e=(struct NativeEntity){.tick=tick,.parent=parent,.index=(uint16_t)index,
  .generation=generation,.depth=(uint8_t)(owner->depth+1),.kind=(uint8_t)kind,.active=1};
 if(kind==ENTITY_GROUP){e->payload.group.phase=e->payload.group.command=e->payload.group.captor_done=255;}
 if(kind==ENTITY_PLAYER||kind==ENTITY_PLAYER_ARM){
  struct PlayerActionLayer*a=kind==ENTITY_PLAYER?&e->payload.player.actor:&e->payload.arm;
  *a=(struct PlayerActionLayer){.animation={.cursor=65535,.drawing=65535,.last_draw_cursor=65535},
   .hit={.id=(uint16_t)index,.hp=-1,.impact_x=-1,.impact_y=-1,.last_attacker=65535,.last_kind=255,.flags=64},
   .shot_phase=255,.local_weapon=255,.melee_selector=255,.owner=255,.damage_owner=255};
  if(kind==ENTITY_PLAYER){e->payload.player.platform_ids=255;e->payload.player.ground_slope=e->payload.player.ground_tag=-1;}
 }
 /* $0004AE prepends; $0006FE appends after all existing descendants. This
  * distinction controls same-tick execution and projectile/arm ordering. */
 unsigned previous=owner->index;
 if(order==ENTITY_LAST_CHILD){
  unsigned next=p->nodes[previous].next;
  while(next&&p->nodes[next].depth>owner->depth){previous=next;next=p->nodes[next].next;}
 }
 link_after(p,e,previous);p->active_count++;
 if(p->active_count>p->peak_count)p->peak_count=p->active_count;
 return e;
}
void entity_reserve(struct NativeEntity*e){e->reserved=1;}
static void release(struct EntityPool*p,struct NativeEntity*e){
 if(!e->index||!e->active){fault(p,4);return;}
 unsigned previous=e->previous,next=e->next;
 p->nodes[previous].next=(uint16_t)next;p->nodes[next].previous=(uint16_t)previous;
 if(p->current==e->index)p->current=(uint16_t)previous;
 e->active=0;e->tick=0;e->deleting=0;p->active_count--;
 if(!e->reserved){e->next=p->free_head;p->free_head=e->index;}
}
static int has_children(struct EntityPool*p,const struct NativeEntity*e){
 return e->next&&p->nodes[e->next].depth>e->depth;
}
void entity_delete(struct EntityPool*p,struct NativeEntity*e){
 if(!e->index||!e->active){fault(p,4);return;}
 /* $000518/$00052A: publish parent deletion, then wait for children. Do
  * not turn this into unconditional recursive destruction. Child handlers
  * observe deletion through the equivalent of $0007B2. */
 e->deleting=1;
 /* $00051E clears the counted-enemy bit even while children survive. */
 struct WeaponActor*a=entity_weapon_actor(e);if(a)a->display_flags&=(uint8_t)~2;
 if(e->kind==ENTITY_PLAYER)e->payload.player.actor.display_flags&=(uint8_t)~2;
 if(e->kind==ENTITY_PLAYER_ARM)e->payload.arm.display_flags&=(uint8_t)~2;
 if(!has_children(p,e))release(p,e);
}
void entity_delete_children(struct EntityPool*p,struct NativeEntity*e){
 if(!e->active){fault(p,4);return;}
 /* $0005B6: explicitly remove every descendant, retaining the parent. */
 while(has_children(p,e))release(p,&p->nodes[e->next]);
}
int entity_parent_deleting(struct EntityPool*p,const struct NativeEntity*e){
 struct NativeEntity*owner=entity_get(p,e->parent);return !owner||owner->deleting;
}
/* $000662..$0006D6. These are source scheduler rules, not animation frame
 * rules. A child can observe its parent's post-handler flags this tick. */
static void prepare(struct EntityPool*p,struct NativeEntity*e){
 struct Animation*animation;struct HitActor*hit;uint8_t*timer,*motion,*display;
 if(e->kind==ENTITY_GROUP||e->kind==ENTITY_MISSION||e->kind==ENTITY_TERRAIN_CONTACT)return;
 if(e->kind==ENTITY_PLAYER||e->kind==ENTITY_PLAYER_ARM){
  struct PlayerActionLayer*a=e->kind==ENTITY_PLAYER?&e->payload.player.actor:&e->payload.arm;
  animation=&a->animation;hit=&a->hit;timer=&a->damage_timer;
  motion=&a->movement_flags;display=&a->display_flags;
  if(e->kind==ENTITY_PLAYER)e->payload.player.animation_changed=0;
 }else{
  struct WeaponActor*a=entity_weapon_actor(e);if(!a){fault(p,7);return;}
  animation=&a->animation;hit=&a->hit;timer=&a->damage_timer;
  motion=&a->movement_flags;display=&a->display_flags;
 }
 hit->events&=1;*motion&=0x17;
 if(animation->freeze)animation->freeze--;
 if(*display&2)p->displayed_count++;
 if(*timer)(*timer)--;
 if(hit->invulnerable)hit->invulnerable--;
}
static void finish(struct NativeEntity*e){
 if(e->kind==ENTITY_PLAYER||e->kind==ENTITY_PLAYER_ARM){
  struct PlayerActionLayer*a=e->kind==ENTITY_PLAYER?&e->payload.player.actor:&e->payload.arm;
  a->hit.flags&=(uint8_t)~64;a->hit.events&=0xfa;a->movement_flags&=0x68;
 }else if(e->kind==ENTITY_PROJECTILE||e->kind==ENTITY_EFFECT||e->kind==ENTITY_SLUG||e->kind==ENTITY_SLUG_AUX||e->kind==ENTITY_ITEM||e->kind==ENTITY_INTERACTION||e->kind==ENTITY_ENEMY){
  struct WeaponActor*a=entity_weapon_actor(e);if(!a)return;
  a->hit.flags&=(uint8_t)~64;a->hit.events&=0xfa;a->movement_flags&=0x68;
 }
}
void entities_tick(struct EntityPool*p,void*world){
 if(p->fault)return;
 if(p->ticking){fault(p,5);return;}
 p->ticking=1;p->current=0;p->displayed_count=0;p->steps=0;unsigned budget=4096;
 /* Read next AFTER each callback. New children must run in source insertion
  * order in this tick; freed current nodes return the iterator to previous. */
 while(p->nodes[p->current].next){
  if(!budget--){fault(p,6);break;}
  p->current=p->nodes[p->current].next;struct NativeEntity*e=&p->nodes[p->current];
  uint16_t generation=e->generation;prepare(p,e);
  if(p->fault)break;
  if(e->deleting){if(!has_children(p,e))release(p,e);}
  else if(e->tick)e->tick(p,e,world);
  /* $000546 jumps past the post-handler block when it releases current.
   * Do not modify a recycled slot that a callback has already reused. */
  if(e->active&&e->generation==generation){finish(e);p->steps++;}
  if(p->fault)break;
 }
 p->current=0;p->ticking=0;
}
