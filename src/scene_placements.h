#ifndef NATIVE_SCENE_PLACEMENT_DRIVER_H
#define NATIVE_SCENE_PLACEMENT_DRIVER_H
#include "weapons.h"
/* Source96A0E: strictly signed WORD(x - WORD(progress +320)) <0.
 * cursor counts20-byte records; it advances only after successful creation. */
int scene_placements_step(struct WeaponWorld*,unsigned list,uint16_t*cursor);
/*969C2 instead uses signed WORD secondary-camera X; not high-water progress. */
int scene_placements_camera_step(struct WeaponWorld*,unsigned list,uint16_t*cursor);
int scene_placement_spawn(struct WeaponWorld*,unsigned list,unsigned index);
#endif
