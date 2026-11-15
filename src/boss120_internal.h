#ifndef NATIVE_BOSS120_INTERNAL_H
#define NATIVE_BOSS120_INTERNAL_H
#include "tank_internal.h"
#include "m4_internal.h"
#include "../generated/boss120-programs.h"
enum Boss120State {B120_ROOT,B120_DELAY,B120_ENTER,B120_DECEL,B120_OPEN,B120_CRUISE,B120_DASH_APPROACH,B120_DASH_BRAKE,B120_DASH_WAIT,B120_DASH_REPEAT,B120_DASH_RETURN,B120_BOMB_OPEN,B120_BOMB_EXIT,B120_BOMB_RUN,B120_REENTER_WAIT,B120_REENTER,B120_ALIGN,B120_SHOT,B120_SHOT_RESET,B120_MISSILE_OPEN,B120_MISSILE_PREP,B120_MISSILE_NEXT,B120_MISSILE_WAIT,B120_MISSILE_CLOSE,B120_BURST_OPEN,B120_BURST_WAIT,B120_DIE,B120_FALL,B120_EXPLODE,B120_SCRIPT,B120_SCRIPT_PATH,B120_SCRIPT_OPEN,B120_SCRIPT_SHOT,B120_SCRIPT_EXIT,B120_RETIRE,
 B120_PART_NEW,B120_PART_RESTART,B120_PART_IDLE,B120_PART_ANIM,B120_PART_BURST,B120_PART_AIM_INIT,B120_PART_HOLD,B120_PART_AIM,B120_PART_CENTER,B120_PART_FIRE,B120_PART_FIRE_END,B120_PART_HURT,B120_PART_SCRIPT_AIM_INIT,B120_PART_SCRIPT_AIM,B120_PART_SCRIPT_FIRE,
 B120_WRECK,B120_WRECK_BOUNCE,B120_WRECK_GROUND,B120_WRECK_WAIT,B120_WRECK_REMOVE,B120_MUZZLE,B120_MISSILE_MUZZLE,B120_SHELL,B120_SHELL_HIT,B120_SHELL_SMOKE,B120_GROUND,B120_GROUND_ACTIVE,B120_TRAIL,B120_BOMB,B120_BOMB_HIT,B120_FIRE,B120_MISSILE,B120_MISSILE_FLY,B120_MISSILE_HIT,B120_FRAGMENT,B120_FLASH,B120_BLOOD,B120_RADIAL,B120_RADIAL_ACTIVE,B120_DELETE,B120_DELETE_TREE};
void b120_schedule(struct NativeEntity*,unsigned);
struct NativeEntity*b120_child(struct WeaponWorld*,struct NativeEntity*,unsigned);
void b120_start(struct EnemyActor*,unsigned);
void b120_start_entry(struct EnemyActor*,uint16_t);
void b120_defense(struct EnemyActor*,unsigned);
void b120_attack(struct EnemyActor*,unsigned);
int b120_animate(struct WeaponWorld*,struct NativeEntity*);
int16_t b120_rank(struct WeaponWorld*,struct EnemyActor*,unsigned);
int b120_path(struct WeaponWorld*,struct NativeEntity*);
void b120_velocity(struct EnemyActor*,uint16_t);
int b120_turn(struct EnemyActor*,int);
int b120_follow(struct WeaponWorld*,struct NativeEntity*);
int b120_command(struct WeaponWorld*,struct NativeEntity*);
int b120_writeback(struct WeaponWorld*,struct NativeEntity*);
int b120_ack(struct WeaponWorld*,struct NativeEntity*);
int b120_part_enter(struct WeaponWorld*,struct NativeEntity*);
int b120_part_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int b120_effect_enter(struct WeaponWorld*,struct NativeEntity*);
int b120_effect_frame(struct WeaponWorld*,struct NativeEntity*,unsigned);
int b120_callback(struct WeaponWorld*,struct NativeEntity*,unsigned);
#endif
