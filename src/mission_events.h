#ifndef NATIVE_MISSION_EVENTS_H
#define NATIVE_MISSION_EVENTS_H
#include "mission_actor.h"
struct WeaponWorld;
struct EntityRef;
struct MissionEventWorld {
 uint32_t executed,spawned,fault_source;
 uint16_t fault,edge_latch;
 uint8_t player_count,enabled;
};
/* $04429E: a first child runs while the parent's selected byte is zero.
 * A nonzero byte removes its whole subtree. NULL watch starts an ordinary
 * event driver. Authored op 12 waits for descendants; it is not a victory. */
struct NativeEntity*mission_event_spawn(struct WeaponWorld*,struct EntityRef,uint32_t, MissionWatchFn);
/* $001DDE installs $0442E6 on the fixed $100800 group itself. Scene-local
 * $052780/$05DD2A clears its actor children without erasing this stream. */
int mission_event_begin(struct WeaponWorld*,struct EntityRef,uint32_t);
int mission_event_stop(struct WeaponWorld*,struct EntityRef);
#endif
