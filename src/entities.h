#ifndef NATIVE_ENTITIES_H
#define NATIVE_ENTITIES_H
#include "player_actions.h"
#include "weapon_animation.h"
#include "slug_aux_actor.h"
#include "item_actor.h"
#include "interaction_actor.h"
#include "enemy_actor.h"
#include "mission_actor.h"
#include "terrain_contact_actor.h"
#define ENTITY_CAPACITY 256
#define ENTITY_NONE 65535u
struct EntityRef {uint16_t index,generation;};
enum EntityKind {ENTITY_GROUP,ENTITY_PLAYER,ENTITY_PLAYER_ARM,ENTITY_PROJECTILE,ENTITY_EFFECT,ENTITY_SLUG,ENTITY_SLUG_AUX,ENTITY_ITEM,ENTITY_INTERACTION,ENTITY_ENEMY,ENTITY_MISSION,ENTITY_TERRAIN_CONTACT};
enum EntityInsert {ENTITY_FIRST_CHILD,ENTITY_LAST_CHILD};
enum EntityProcessing {ENTITY_PROCESS_FX=1};
enum EntityCosmetic {ENTITY_COSMETIC_SPARSE=1,ENTITY_COSMETIC_PRESSURE=2};
struct EntityPool;
struct NativeEntity;
struct SlugSharedState;
typedef void (*EntityTick)(struct EntityPool*,struct NativeEntity*,void*world);
/* Native ARM function/state, never an original PC or CPU register image. */
union EntityPayload {
 /* A fixed source group can itself execute the main mission stream. Keep
  * its allocator/signalling bytes separate from that controller state. */
 struct {uint8_t phase,command,captor_done;struct MissionEventActor mission;} group;
 struct PlayerBodyState player;
 struct PlayerActionLayer arm;
 struct WeaponActor weapon;
 struct SlugAuxActor slug_aux;
 struct ItemActor item;
 struct InteractionActor interaction;
 struct EnemyActor enemy;
 struct MissionEventActor mission;
 struct SlugContact contact;
 struct SlugSharedState *slug; /* Fixed world slot; never copied into each pool node. */
};
struct NativeEntity {
 union EntityPayload payload;
 EntityTick tick;
 struct EntityRef parent;
 uint16_t previous,next,generation,index;
 uint8_t depth,kind,active,deleting,reserved,cosmetic,processing,pad;
};
struct EntityPool {
 struct NativeEntity nodes[ENTITY_CAPACITY];
 uint16_t free_head,current,active_count,peak_count;
 uint16_t displayed_count,steps;
 uint8_t fault,ticking,pad[2];
};
void entities_init(struct EntityPool*);
struct EntityRef entity_ref(const struct NativeEntity*);
struct NativeEntity* entity_get(struct EntityPool*,struct EntityRef);
struct WeaponActor* entity_weapon_actor(struct NativeEntity*);
struct NativeEntity* entity_spawn(struct EntityPool*,struct EntityRef parent,
 enum EntityKind,enum EntityInsert,EntityTick);
/* A permanent system group can be reserved after allocation. It is never
 * recycled into an unrelated projectile or enemy on release. */
void entity_reserve(struct NativeEntity*);
void entity_delete(struct EntityPool*,struct NativeEntity*);
void entity_delete_children(struct EntityPool*,struct NativeEntity*);
int entity_parent_deleting(struct EntityPool*,const struct NativeEntity*);
void entities_tick(struct EntityPool*,void*world);
#endif
