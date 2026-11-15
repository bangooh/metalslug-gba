#ifndef SOURCE_TERRAIN_MOTION_H
#define SOURCE_TERRAIN_MOTION_H
#include "weapons.h"
#include "../generated/terrain-rules.h"
/* World-space adaptation of the original 16-slot moving-platform ring. */
struct SourcePlatform {int16_t x,y_up,width,dx,dy_up;uint8_t id,pad;};
struct SourcePlatforms {
 struct SourcePlatform records[16];
 uint8_t first,end,write,last_id;
};
struct TerrainWorld {
 void *map;
 int (*cell)(void*,int world_x,int world_y,uint8_t*raw);
 int (*write_cell)(void*,int world_x,int world_y,uint8_t raw);
 struct SourcePlatforms platforms;
 int16_t camera_x,camera_y,delta_x,delta_y;
 /* Collision-grid pixels minus native world pixels (map relink/parallax). */
 int16_t cell_offset_x,cell_offset_y;
 int16_t stop_x,stop_y,step_x,step_y;
 uint8_t scene,scene1_horizontal_lock,fault,pad;
};
struct TerrainMotion {
 struct PlayerTransform *transform;
 struct HitActor *hit;
 int16_t vx,vy,ax,ay,slope,ground_tag;
 uint8_t movement_flags,collision_flags,platform_ids,player_boundary;
 int8_t previous_direction;
};
struct TerrainSample {
 const struct SourceTerrainHalf *half;
 uint8_t shape,raw,column,row,platform,half_index,pad[2];
};
void terrain_platform_reset(struct SourcePlatforms*);
void terrain_platform_frame(struct SourcePlatforms*);
int terrain_platform_publish(struct TerrainWorld*,struct TerrainMotion*,int16_t x,int16_t y_up,int16_t width,int force_id14);
int terrain_platform_at(struct TerrainWorld*,struct TerrainMotion*,int16_t x,int16_t y_up);
int terrain_lookup(struct TerrainWorld*,struct TerrainMotion*,int16_t x,int16_t y_up,struct TerrainSample*);
int terrain_move(struct TerrainWorld*,struct TerrainMotion*,enum WeaponMove,struct WeaponContact*);
/* $02783A: camera/platform synchronization and depth, without physics. */
int terrain_synchronize(struct TerrainWorld*,struct TerrainMotion*);
/* $02785C/$02788C/$027A18: Slug uses a separate integer terrain Y,
 * sharing the actor's fractional Y. Drawing depth still uses body Y. */
int terrain_move_ground_origin(struct TerrainWorld*,struct TerrainMotion*,enum WeaponMove,int16_t *ground_y_up);
/* $027EBA: 1 contact (and event bit 6), 0 air, -1 conversion/map fault. */
int terrain_foot_contact(struct TerrainWorld*,struct TerrainMotion*);
/* Same foot probe at an explicit position ($027EC2), and nonempty shape
 * probe including moving platforms ($0280C6). Returns -1 on map faults. */
int terrain_contact_at(struct TerrainWorld*,struct TerrainMotion*,int16_t x,int16_t y_up);
int terrain_shape_at(struct TerrainWorld*,struct TerrainMotion*,int16_t x,int16_t y_up);
/* $0280C6: shape lookup with moving-platform substitution, not foot contact. */
int terrain_weapon_point_sample(void*,const struct WeaponActor*,int16_t,int16_t,struct WeaponContact*);
int terrain_weapon_point_shape(void*,const struct WeaponActor*,int16_t,int16_t,uint8_t*);
/* Typed consumers for WeaponWorld.move/probe when scene points to this world. */
int terrain_weapon_move(void*,struct WeaponActor*,enum WeaponMove,struct WeaponContact*);
int terrain_weapon_probe(void*,const struct WeaponActor*,struct WeaponContact*);
int terrain_weapon_foot_contact(void*,struct WeaponActor*);
/* $027F60: preserve integer position and VX/VY, retain probe side effects. */
int terrain_weapon_ground_test(struct TerrainWorld*,struct WeaponActor*);
/* $027F08/$027FD8 ground-state merge, including platform carry and terrain
 * shape byte comparison against the preceding state byte. */
int terrain_weapon_ground_status(struct TerrainWorld*,struct WeaponActor*,uint8_t*grounded,int point);
/* $043FAC: column-major collision cells; clipped columns do not consume data. */
int terrain_stamp(struct TerrainWorld*,const struct PlayerTransform*,int16_t dx,int16_t dy_up,uint16_t width,uint16_t height,const uint8_t*cells);
#endif
