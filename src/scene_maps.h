#ifndef NATIVE_SCENE_MAPS_H
#define NATIVE_SCENE_MAPS_H
#include "scene_video.h"
#include "../generated/scene-programs.h"
#define SCENE_NO_IMAGE 65535
struct SceneMapPlane {
 uint32_t x,y;
 int16_t dx,dy,rect_x,rect_y,width,height,cached_x,cached_y,scale_x,scale_y;
 uint16_t image,collision,controls,control_index;
 uint8_t flags,dirty,cursor_y,cursor_flags,column_flags[32];
};
/* Only $106F6C has a linked collision image in the authored scripts. */
struct SceneMaps {struct SceneMapPlane planes[4];uint8_t collision[4096],fault,ending;};
int scene_maps_load(struct SceneMaps*,struct SceneVideoWorld*,const struct SceneDescriptor*);
int scene_map_bind(struct SceneMaps*,struct SceneVideoWorld*,unsigned,unsigned image,unsigned collision,uint16_t x,uint16_t y);
void scene_map_cursor(struct SceneMaps*,struct SceneVideoWorld*,unsigned,uint8_t flags,uint8_t y);
int scene_map_probe(struct SceneMaps*,struct SceneVideoWorld*,unsigned,unsigned axis);
int scene_map_clear(struct SceneMaps*,struct SceneVideoWorld*,unsigned);
int scene_map_hide(struct SceneMaps*,struct SceneVideoWorld*,unsigned);
int scene_map_commit(struct SceneMaps*,struct SceneVideoWorld*,unsigned);
int scene_maps_step(struct SceneMaps*,struct SceneVideoWorld*,int16_t dx,int16_t dy);
/* $043F02/$043F5E take source SCREEN X/Y-up. */
uint8_t scene_map_cell(const struct SceneMaps*,const struct SceneVideoWorld*,uint16_t x,uint16_t y_up);
void scene_map_write_cell(struct SceneMaps*,const struct SceneVideoWorld*,uint16_t x,uint16_t y_up,uint8_t value);
#endif
