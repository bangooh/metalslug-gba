#ifndef NATIVE_SCENE_CAMERA_H
#define NATIVE_SCENE_CAMERA_H
#include <stdint.h>
/* Original scroll values are wrapping16.16 quantities. unsigned storage
 * preserves LONG arithmetic; signed interpretation is explicit at branches. */
struct SceneCamera {
 uint32_t x,y,raw_y,smoothing,velocity,seed_x,seed_y;
 uint16_t progress,base,origin_x,origin_y,seed_latch;
 int16_t dx,dy,smoothing_delta,center_x,center_y,min_x,min_y,max_x,max_y,slope;
 uint8_t axis,edge_off;
};
/* $043568 reset retains the seed velocities and axis until script writes. */
void scene_camera_reset(struct SceneCamera*);
void scene_camera_position(struct SceneCamera*,uint16_t x,uint16_t y,int reset_progress);
void scene_camera_axis(struct SceneCamera*,uint8_t);
void scene_camera_progress(struct SceneCamera*,uint16_t);
/* $0436DE projects two SCREEN origins to caller scroll velocities. `valid`
 * represents the caller's nonnegative coordinate pair, not player liveness. */
void scene_camera_follow(const struct SceneCamera*,uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1,int valid,uint32_t*vx,uint32_t*vy);
/* $043BC6 step, after the script updates bounds/position/axis. The caller
 * selects seed velocities BEFORE executing that script ($0437DA). */
void scene_camera_integrate(struct SceneCamera*,uint32_t vx,uint32_t vy);
/* $043CE2, with original $10E39A scene gate. */
int scene_camera_at_edge(const struct SceneCamera*,uint8_t scene_gate);
#endif
