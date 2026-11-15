#ifndef HITBOX_H
#define HITBOX_H
#include <stdint.h>

/* Native game data, not the original CPU's actor memory layout.
 * Coordinates use the source's upward Y axis and signed 16-bit pixels. */
struct HitRect { int16_t left,right,bottom,top; };
struct HitAttack {
 struct HitRect rect;
 int16_t damage;
 uint8_t group,kind,strength,shape,offscreen;
};
struct HitAttackSet {const struct HitAttack *records;uint16_t count;};
struct HitDefense {
 struct HitRect rect;
 const uint8_t *filter; /* 36 entries, 1=ignore; NULL selects source $27928C. */
 uint8_t resistance;
};
struct HitActor {
 int16_t x,y,hp,impact_x,impact_y;
 uint16_t id,last_attacker;
 uint8_t flip,invulnerable,flags,events,last_kind,type_flags;
 /* Typed $0283D8 result: struck defense resistance, or 255 for a block.
  * This is distinct from the target's last received attack kind. */
 uint8_t contact_resistance;
};
enum HitResult { HIT_MISS, HIT_BLOCKED, HIT_CONTACT, HIT_DAMAGE };
struct HitDamage {
 uint16_t attacker,target;
 int16_t before,after;
 uint8_t kind,killed;
};
typedef void (*HitDamageSink)(void *user,const struct HitDamage *event);

void hit_begin_attack(struct HitActor *actor);
enum HitResult hit_pair(struct HitActor *attacker,const struct HitAttack *attack,
 struct HitActor *target,const struct HitDefense *defenses,unsigned count,
 HitDamageSink sink,void *user);
#endif
