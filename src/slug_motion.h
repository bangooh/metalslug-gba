#ifndef NATIVE_SLUG_MOTION_H
#define NATIVE_SLUG_MOTION_H
#include "slug_support.h"
/* Semantic policies for all eight $02A752..$02A8BE wrappers. B and C
 * differ only by the original $028992 no-op, retained in provenance. */
enum SlugMotionPolicy {SLMOVE_SYNC,SLMOVE_GROUND_B,SLMOVE_GROUND_C,
 SLMOVE_FALL_STEP,SLMOVE_GROUND_E,SLMOVE_GROUND_F,SLMOVE_GROUND_RAW,SLMOVE_AIR};
/* -1 fault; air returns 1 landed/0 airborne; raw returns the terrain
 * change condition; the remaining ground policies return step mismatch. */
int slug_motion_step(struct SlugSharedState*,struct TerrainWorld*,struct PlayerSourceInput,enum SlugMotionPolicy);
void slug_input_flags(struct SlugSharedState*,struct PlayerSourceInput);
void slug_motion_display(struct SlugSharedState*,struct PlayerSourceInput);
/* $02A39A/$02AAF0/$02AB3C. Jump returns source direction index 1..4. */
unsigned slug_jump_input(struct PlayerSourceInput,unsigned layout,int alternate);
int slug_exit_combo(struct PlayerSourceInput);
#endif
