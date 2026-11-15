#ifndef NATIVE_SCENE_DRIVER_H
#define NATIVE_SCENE_DRIVER_H
#include "scene_runtime.h"
/* Seven967FE..9698A handlers and the three8CE64/8D0A8/8D0FA endings.
 * Ending names refer to source entry points, not player-count guesses. */
enum SceneDriverEntry {SCENE_DRIVER_0,SCENE_DRIVER_1,SCENE_DRIVER_2,SCENE_DRIVER_3,SCENE_DRIVER_4,SCENE_DRIVER_5,SCENE_DRIVER_7,SCENE_DRIVER_ENDING_B,SCENE_DRIVER_ENDING_D};
/* Schedules entry on the next play_world_tick. The outer boot owns removal
 * of old actors, player lives, event startup, and ending branch selection.
 * scene is the original106ECE; timing_variant is106EAE. */
int scene_driver_begin(struct SceneRuntime*,struct NativePlayWorld*,enum SceneDriverEntry,unsigned scene,uint8_t timing_variant);
int scene_driver_frame(struct SceneRuntime*,struct NativePlayWorld*);
#endif
