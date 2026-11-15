#ifndef CAMPAIGN_H
#define CAMPAIGN_H
#include "actors.h"
struct Campaign {
 struct Mission mission;
 uint32_t ticks,failed_source;
 uint16_t failed_template,unsupported,respawns;
 uint8_t combo_timer,combo_count,combo_active,continue_count;
};
extern struct Campaign campaign;
void campaign_reset(void);
void campaign_step(struct Player*,unsigned keys,unsigned pressed);
int campaign_spawn(void*,const struct MissionSpawn*);
#endif
