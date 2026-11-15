#ifndef NATIVE_MISSION_ACTOR_H
#define NATIVE_MISSION_ACTOR_H
#include "mission.h"
struct NativeEntity;
/* Typed access to the authored parent byte; no source memory-offset reads. */
typedef int (*MissionWatchFn)(const struct NativeEntity*);
struct MissionEventActor {struct MissionTask task;MissionWatchFn watch;};
#endif
