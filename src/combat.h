#ifndef COMBAT_H
#define COMBAT_H
#include "game.h"
#include "hitbox.h"
#include "mission.h"
#define COMBAT_CRATES 12
#define COMBAT_SHOTS 12
struct CombatAnim {uint8_t sequence,pose,age,done;uint16_t frame,pad;};
struct CombatCrate {
 struct HitActor hit;
 int32_t x,y;
 struct CombatAnim anim;
 uint8_t active,breaking,pending,content,quantity,drop_sent,grounded,lifetime;
};
struct CombatShot {struct HitActor hit;int32_t x,y;int16_t vx,vy;uint8_t active,age;};
struct CombatDrop {uint16_t kind,quantity;int16_t x,y;uint8_t lifetime,pad;};
struct Combat {
 struct HitActor player;
 struct CombatAnim animation;
 struct CombatCrate crates[COMBAT_CRATES];
 struct CombatShot shots[COMBAT_SHOTS];
 struct CombatDrop drops[COMBAT_CRATES];
 uint32_t strikes,hits,kills,shots_fired;
 uint16_t sound[8],next_id;
 uint8_t sounds,drop_count,active,pending,alternate,pistol_phase,fault,pad;
 uint16_t player_dead,player_hits;
 struct CombatAnim aim;uint8_t aiming,pad2[3];
};
extern struct Combat combat;
void combat_reset(void);
void combat_clear_sounds(void);
void combat_sound(uint16_t);
void combat_begin_frame(const struct Player*);
void combat_player_damage(void*,const struct HitDamage*);
void combat_drop(unsigned kind,unsigned quantity,unsigned lifetime,int x,int y);
/* Returns zero for unported template or exhaustion, never drops a spawn. */
int combat_spawn(void *user,const struct MissionSpawn *spawn);
void combat_step(const struct Player *player,unsigned keys,unsigned pressed);
/* Explicit test data; not called by campaign code or represented as M1. */
void combat_fixture_init(void);
#endif
