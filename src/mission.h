#ifndef MISSION_H
#define MISSION_H
#include <stdint.h>
#define MISSION_TASKS 48
struct SourceRng {uint16_t words[32],head;};
struct MissionPlayer {int16_t x,y;uint8_t active,pad;}; /* source SCREEN coordinates; Y down */
struct MissionInput {
 struct MissionPlayer players[2];
 int16_t camera_x,camera_y;
 int16_t secondary_x,secondary_y;
 uint16_t progress,enemies;
 uint8_t active,player_count,slug_present,secondary_valid;
};
struct MissionSpawn {
 uint32_t source;
 int16_t x,y; /* source world coordinates, Y down */
 uint16_t template;
 uint8_t counted,subtype,params[8];
};
struct MissionTask {
 uint16_t offset,threshold,delay,period,until;
 uint8_t active,periodic,paused,repeats,enemy_limit,pad;
};
struct Mission {
 struct SourceRng rng;
 struct MissionTask tasks[MISSION_TASKS];
 uint32_t executed,spawned,fault_source;
 uint16_t fault,edge_latch;
};
typedef int (*MissionSpawnFn)(void*,const struct MissionSpawn*);
/* Shared command semantics; periodic allocation belongs to the scheduler.
 * Native scene actors use the entity tree. The released M1 adapter retains
 * its fixed task array until the new game loop is connected. */
struct MissionExecutor {
 struct SourceRng *rng;
 void *context;
 MissionSpawnFn spawn;
 int (*periodic)(void*,const struct MissionTask*);
 uint32_t executed,spawned,fault_source;
 uint16_t fault,edge_latch;
};
uint32_t mission_scene_source(unsigned scene);
uint16_t mission_offset(uint32_t source);
uint32_t mission_source(uint16_t offset);
uint16_t mission_command(struct MissionExecutor*,struct MissionTask*,const struct MissionInput*,uint16_t);
void mission_task_frame(struct MissionExecutor*,struct MissionTask*,const struct MissionInput*);
uint16_t source_random(struct SourceRng*,uint16_t);
uint16_t source_random_word(struct SourceRng*);
void source_random_seed(struct SourceRng*,uint16_t);
uint16_t source_random_mask(struct SourceRng*,uint16_t);
void mission_init(struct Mission*,uint16_t);
int mission_start(struct Mission*,uint32_t);
void mission_tick(struct Mission*,const struct MissionInput*,MissionSpawnFn,void*);
uint16_t mission_skip(uint16_t);
/* Exposed for original-instruction differential tests and independently owned
 * script contexts. All arguments are game state, never CPU registers. */
uint16_t mission_exec(struct Mission*,struct MissionTask*,const struct MissionInput*,
                      uint16_t,MissionSpawnFn,void*);
int mission_decode_spawn(const struct MissionInput*,uint16_t,struct MissionSpawn*);
#endif
