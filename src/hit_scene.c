#include "hit_scene.h"
static int fail(struct HitScene*s,unsigned reason){if(!s->fault)s->fault=(uint8_t)reason;return 0;}
static int resolve(struct HitScene*s,struct NativeEntity*e,struct HitTarget*t){
 *t=(struct HitTarget){0};
 if(e->kind==ENTITY_PLAYER||e->kind==ENTITY_PLAYER_ARM){
  struct PlayerActionLayer*a=e->kind==ENTITY_PLAYER?&e->payload.player.actor:&e->payload.arm;
  a->hit.x=(int16_t)(a->transform.x>>8);a->hit.y=transform_y_up(&a->transform);a->hit.flip=a->transform.facing;
  *t=(struct HitTarget){.actor=&a->hit,.defenses=a->defense,.count=a->defense?1:0};return 1;
 }
 if(e->kind==ENTITY_PROJECTILE||e->kind==ENTITY_EFFECT||e->kind==ENTITY_SLUG||e->kind==ENTITY_SLUG_AUX||e->kind==ENTITY_ITEM||e->kind==ENTITY_INTERACTION||e->kind==ENTITY_ENEMY){
  struct WeaponActor*a=entity_weapon_actor(e);if(!a)return fail(s,6);
  a->hit.x=(int16_t)(a->transform.x>>8);a->hit.y=transform_y_up(&a->transform);a->hit.flip=a->transform.facing;
  *t=(struct HitTarget){.actor=&a->hit,.defenses=a->defense,.count=a->defense_count};return 1;
 }
 /* Terrain followers and mission-event controllers carry scheduler state,
  * never an actor defense record. They can be descendants of enemy groups. */
 if(e->kind==ENTITY_GROUP||e->kind==ENTITY_TERRAIN_CONTACT||e->kind==ENTITY_MISSION)return 1;
 return s->resolve&&s->resolve(s->world,e,t)?1:fail(s,1);
}
static int target(struct HitScene*s,struct NativeEntity*e,struct HitActor*a,const struct HitAttack*attack,struct HitActor**selected){
 if(!e||(a->flags&2))return 1;
 /* Pure FX have no defense. Avoid actor resolution/coordinate writes for
  * every bullet, while allowing an explicitly rebound defense normally. */
 if((e->processing&ENTITY_PROCESS_FX)&&e->kind==ENTITY_ENEMY&&
    (!e->payload.enemy.actor.defense||!e->payload.enemy.actor.defense_count))return 1;
 struct HitTarget t;if(!resolve(s,e,&t))return 0;
 if(!t.actor||!t.defenses||!t.count)return 1;
 enum HitResult result=hit_pair(a,attack,t.actor,t.defenses,t.count,s->damage,s->world);
 if(result!=HIT_MISS&&selected)*selected=t.actor;
 return 1;
}
static int fixed(struct HitScene*s,struct EntityRef ref,struct HitActor*a,const struct HitAttack*attack,struct HitActor**selected){
 return target(s,entity_get(s->entities,ref),a,attack,selected);
}
static int descendants(struct HitScene*s,struct EntityRef ref,struct HitActor*a,const struct HitAttack*attack,struct HitActor**selected){
 /* $028930 gates only groups 3/4. Attack header byte 6 bypasses the gate. */
 if(!attack->offscreen){
  int16_t x=(int16_t)(a->x-s->camera_x),y=(int16_t)(a->y+s->camera_y);
  if(x<0||x>321||y<255||y>496)return 1;
 }
 if(a->flags&2)return 1;
 struct NativeEntity*root=entity_get(s->entities,ref);if(!root)return fail(s,2);
 unsigned index=root->next;
 for(unsigned budget=0;index&&s->entities->nodes[index].depth>root->depth;budget++){
  if(budget>=ENTITY_CAPACITY)return fail(s,3);
  struct NativeEntity*e=&s->entities->nodes[index];uint16_t generation=e->generation;
  if(!target(s,e,a,attack,selected))return 0;
  if(a->flags&2)return 1;
  /* Source damage handlers mark targets for their own next tick. Recycling
   * a target synchronously here would invalidate the original traversal. */
  if(!e->active||e->generation!=generation)return fail(s,4);
  index=e->next;
 }
 return 1;
}
int hit_scene_attack(struct HitScene*s,struct HitActor*a,const struct HitAttack*attack,struct HitActor**selected){
 if(s->fault||!s->entities)return 0;
 if(attack->kind>34||(attack->shape!=0&&attack->shape!=4))return fail(s,5);
 s->last_player=255;if(selected)*selected=0;
 switch(attack->group){
  case 0:
   if(!fixed(s,s->players[0],a,attack,selected)||!fixed(s,s->players[1],a,attack,selected))return 0;
   return fixed(s,s->slug,a,attack,selected);
  case 1:case 2:{
   unsigned player=attack->group-1;
   if(!fixed(s,s->players[player],a,attack,selected))return 0;
   unsigned contact=a->events&2;
   if(!fixed(s,s->slug,a,attack,selected))return 0;
   s->last_player=contact?(uint8_t)player:255;return 1;
  }
  case 3:return descendants(s,s->enemy_group,a,attack,selected);
  case 4:return descendants(s,s->projectile_group,a,attack,selected);
  default:return fail(s,5);
 }
}
