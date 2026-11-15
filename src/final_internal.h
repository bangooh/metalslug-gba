#ifndef NATIVE_FINAL_INTERNAL_H
#define NATIVE_FINAL_INTERNAL_H
#include "bosses.h"
#include "../generated/final-programs.h"
enum FinalState {FB_ROOT,FB_WAIT_CHILDREN,FB_WAIT_END,FB_END,FB_LEFT,FB_RIGHT,FB_LEFT_ALONE,FB_RIGHT_ALONE,
 FB_ENTRANCE,FB_SNAP,FB_IDLE,FB_TURN,FB_TURN_SHORT,FB_PAUSE,FB_PAUSE_RESET,FB_RESUME,FB_STAND,FB_STAND_B,
 FB_DIE,FB_EXPLODE,FB_RESET,FB_LIMB_NEW,FB_LIMB,FB_LIMB_RESET,FB_LIMB_A,FB_LIMB_B,FB_LIMB_C,FB_LIMB_D,
 FB_HEAD,FB_HEAD_IDLE,FB_HEAD_A,FB_HEAD_B,FB_HEAD_C,FB_HEAD_HIT,FB_HEAD_DIE,FB_HEAD_FLASH,FB_HEAD_RESPAWN,FB_HEAD_DEATH,
 FB_CANNON,FB_CANNON_B,FB_SHELL,FB_SHELL_PRIORITY,FB_MUZZLE,FB_CASING,FB_FLAME,
 FB_BEAM,FB_BEAM_FLIGHT,FB_BEAM_B,FB_BEAM_B_FLIGHT,FB_FLAME_FOLLOW,FB_FLAME_FLIGHT,
 FB_HEAD_SHARD,FB_HEAD_SHARD_SMALL,FB_PUFF,FB_FLASH,FB_FIRE,FB_FIRE_LEFT40,FB_FIRE_LEFT48,FB_SMOKE,FB_SHELL_HIT,FB_DELETE,FB_DELETE_TREE};
static inline int16_t fb_word(int x){return (int16_t)(uint16_t)x;}
static inline int16_t fb_sx(struct WeaponWorld*w,const struct WeaponActor*a){return fb_word((a->transform.x>>8)-w->camera_x);}
static inline int16_t fb_sy(struct WeaponWorld*w,const struct WeaponActor*a){return fb_word(transform_y_up(&a->transform)+w->camera_y);}
static inline int16_t fb_faced(const struct WeaponActor*a,int16_t x){return a->transform.facing&1?fb_word(-x):x;}
void fb_screen(struct WeaponWorld*,struct WeaponActor*,int16_t,int16_t);
uint16_t fb_random(struct WeaponWorld*);
int16_t fb_rank(struct WeaponWorld*,struct EnemyActor*,enum FinalRank);
int fb_sound(struct WeaponWorld*,unsigned,int stop);
int fb_hit_sound(struct WeaponWorld*,struct WeaponActor*);
void fb_start(struct EnemyActor*,enum FinalAnimation);
void fb_start_entry(struct EnemyActor*,uint16_t);
void fb_defense(struct EnemyActor*,unsigned);
void fb_attack(struct EnemyActor*,unsigned);
int fb_animate(struct WeaponWorld*,struct NativeEntity*);
struct EnemyActor*fb_parent(struct WeaponWorld*,struct NativeEntity*);
struct NativeEntity*fb_child(struct WeaponWorld*,struct NativeEntity*,enum FinalState);
int fb_follow(struct WeaponWorld*,struct NativeEntity*,int pose_offset);
int fb_parts_enter(struct WeaponWorld*,struct NativeEntity*);
int fb_parts_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int fb_emit(struct WeaponWorld*,struct NativeEntity*,unsigned);
int fb_crash_fragments(struct WeaponWorld*,struct NativeEntity*);
#endif
