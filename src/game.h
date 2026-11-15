#ifndef GAME_H
#define GAME_H
#include <stdint.h>
#define KEY_A 1u
#define KEY_B 2u
#define KEY_START 8u
#define KEY_SELECT 4u
#define KEY_RIGHT 16u
#define KEY_LEFT 32u
#define KEY_UP 64u
#define KEY_DOWN 128u
struct Player {
 int32_t x,y; /* source pixels Q8, world Y points down */
 int16_t vx,vy; /* source Q8; velocity Y points UP, like $2A(a6) */
 uint16_t frame,anim,air_time;
 uint8_t grounded,facing,jump_kind,jump_window,jump_held,jump_pending,from_jump,pad;
};
struct Game {
 struct Player player;
 uint32_t ticks,landings,jumps,resets;
 int32_t camera_x,camera_y;
 uint16_t previous_keys,ended,scene,pad;
};
#define SCENE_TITLE 0u
#define SCENE_MOVEMENT 1u
#define MOVEMENT_TEST_END 1920
extern struct Game game;
int16_t source_clamp_velocity(int16_t value,int16_t limit);
int16_t source_velocity_step(int16_t value,int16_t acceleration);
int terrain_shape(int x,int y);
int terrain_solid(int x,int y,int platforms);
int terrain_floor(int x,int lo,int hi);
void game_reset(void);
void game_boot(void);
void game_step(unsigned keys);
#endif
