#ifndef NATIVE_SCENE_RUNTIME_H
#define NATIVE_SCENE_RUNTIME_H
#include "scene_camera.h"
#include "scene_maps.h"
#include "source_audio.h"
#include "entities.h"
struct NativePlayWorld;
struct WeaponWorld;
/* Caller-owned scene storage, separate from the bounded simulation arena.
 * Supply the two original SCREEN points selected by the scene controller.
 * Selection of living players/Slug/replay is a separate original policy. */
struct SceneFollow {uint16_t x[2],y_up[2];uint8_t valid;};
struct SceneRuntime {
 struct SceneMaps maps;
 struct SceneCamera camera;
 struct SourceAudioQueue audio;
 struct SceneFollow follow;
 struct EntityRef group;
 /* Continuous native world origin follows background DELTAS. Rebinding
  * a map must not teleport existing SCREEN actors in the world adaptation. */
 int16_t native_camera_x,native_camera_y;
 uint16_t command,spawn_hint;
 uint8_t scene,program,mode,signal,active,fault;
 struct {uint16_t cursor;uint8_t kind,entry,phase,timing_variant;} driver;
 /* SourceDIP100001 and raw input10E208, only required by debug paths. */
 uint8_t debug_dip,debug_input,debug_input_valid;
};
int scene_runtime_attach(struct SceneRuntime*,struct NativePlayWorld*);
/* Program index and $106ECE scene number are distinct (ending programs
 *14/15 run under scene numbers11/12). Loader itself does not choose them. */
int scene_runtime_load(struct SceneRuntime*,struct NativePlayWorld*,unsigned program,unsigned scene);
int scene_runtime_frame(struct SceneRuntime*,struct NativePlayWorld*);
/* Publish world offsets and mutable map services to the native actor group. */
void scene_runtime_publish(struct SceneRuntime*,struct NativePlayWorld*);
struct SceneSpawnHint {int16_t x,y_up;uint16_t mode,spacing;uint8_t item,quantity,lifetime;};
int scene_runtime_spawn_hint(struct SceneRuntime*,struct SceneSpawnHint*);
void scene_runtime_follow_players(struct SceneRuntime*,const struct NativePlayWorld*);
int scene_runtime_m5_relink(struct SceneRuntime*,struct WeaponWorld*,int16_t target,int wrap);
#endif
