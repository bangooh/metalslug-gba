#ifndef ACTORS_H
#define ACTORS_H
#include "combat.h"
#define ACTOR_CAPACITY 32
enum ActorKind {ACTOR_NONE,ACTOR_SOLDIER,ACTOR_CRITTER,ACTOR_SIGN,ACTOR_CHARGER,ACTOR_PROJECTILE,ACTOR_FOOD,ACTOR_POW};
enum SoldierState {SOLDIER_WALK,SOLDIER_IDLE,SOLDIER_MELEE,SOLDIER_DEAD,SOLDIER_TURN,SOLDIER_THROW,SOLDIER_LEAVE,SOLDIER_WINDUP};
struct ActorAnim {uint16_t frame;uint8_t sequence,pose,age,done,defense,pad;};
struct Actor {
 struct HitActor hit;
 int32_t x,y;
 struct ActorAnim animation;
 uint32_t source;
 int16_t vx,vy;
 int16_t home_x,spawn_y,gravity;
 uint16_t timer,template;
 uint8_t active,kind,state,counted,grounded,subtype,params[8];
};
struct Actors {struct Actor slots[ACTOR_CAPACITY];uint32_t spawned,killed,despawned,grenades,score,pickups,rescued;uint16_t fault,peak;};
extern struct Actors actors;
void actors_reset(void);
int actors_spawn(const struct MissionSpawn*,struct SourceRng*,int camera_x);
void actors_step(const struct Player*,struct SourceRng*,int camera_x,int camera_y);
unsigned actors_enemies(void);
/* Appends live source-ordered target views, without allocating or copying art. */
unsigned actors_targets(struct HitActor **,const struct HitDefense **,unsigned);
void actors_damage(void*,const struct HitDamage*);
int actor_pickup_overlap(int16_t dx,int16_t dy,uint8_t player_flags,unsigned x_offset,unsigned x_span,unsigned y_offset,unsigned y_span);
#endif
