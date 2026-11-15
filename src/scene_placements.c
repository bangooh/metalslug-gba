#include "scene_placements.h"
#include "../generated/scene-placements.h"
#include "../generated/village-programs.h"
static int16_t word(int x){return (int16_t)(uint16_t)x;}
int scene_placement_spawn(struct WeaponWorld*w,unsigned list,unsigned index){
 if(list>=8||index>=scene_placement_lists[list].count||!w->secondary_view_valid)return enemy_fault(w);
 const struct ScenePlacement*r=&scene_placements[scene_placement_lists[list].first+index];uint16_t x=r->x;if(w->scene_number==11||w->scene_number==12)x&=4095;
 struct PlayerTransform t={.x=(int32_t)word(x-w->secondary_x_offset+w->camera_x)*256,.y=(int32_t)word(r->y-w->secondary_y_offset+w->camera_y)*256,.priority=r->priority,.display_group=255};
 /* $05DCCE: kind 2 calls $04498E, allocating under $100800
  * (enemy group). $100120 is the separate projectile scheduler. */
 struct EntityRef parent=r->kind==2?w->enemy_group:w->prop_group;uint8_t flags=r->flags|(r->kind==2?2:0);
 switch(r->family){
 case PLACEMENT_URBAN:return urban_spawn_scene(w,parent,ENTITY_FIRST_CHILD,&t,(enum UrbanEntry)r->entry,r->config,flags);
 case PLACEMENT_OPENING:return opening_spawn_scene(w,parent,ENTITY_FIRST_CHILD,&t,(enum OpeningEntry)r->entry,r->config,flags);
 case PLACEMENT_VILLAGE:
  /*774EC..7756A never call2942A: their coordinates come from map patches,
   * and other record metadata must not overwrite allocator defaults. */
  if(r->entry>=VILLAGE_MARKER0&&r->entry<=VILLAGE_MARKER6){t=(struct PlayerTransform){.x=-256,.y=513*256,.priority=65535,.facing=0,.display_group=255};flags=r->kind==2?2:0;}
  return village_spawn_scene(w,parent,ENTITY_FIRST_CHILD,&t,(enum VillageEntry)r->entry,r->config,flags,r->palette,r->graph==65535?65535:village_static[r->graph]);
 case PLACEMENT_SIEGE:return siege_spawn_scene(w,parent,ENTITY_FIRST_CHILD,&t,(enum SiegeEntry)r->entry,r->config,flags);
 case PLACEMENT_FORT:return fort_spawn_scene(w,parent,ENTITY_FIRST_CHILD,&t,(enum FortEntry)r->entry,r->config,flags);
 case PLACEMENT_AIRSHIP:return airship_spawn_scene(w,parent,ENTITY_FIRST_CHILD,&t,(enum AirshipEntry)r->entry,flags);
 default:return enemy_fault(w);
 }
}
static int step(struct WeaponWorld*w,unsigned list,uint16_t*cursor,uint16_t x){
 if(!cursor)return enemy_fault(w);if(list>7)list=7;const struct ScenePlacementList*t=&scene_placement_lists[list];if(*cursor>t->count)return enemy_fault(w);
 while(*cursor<t->count){const struct ScenePlacement*r=&scene_placements[t->first+*cursor];if(word(r->x-(uint16_t)(x+320))>=0)return 1;if(!scene_placement_spawn(w,list,*cursor))return 0;(*cursor)++;}return !w->fault;
}
int scene_placements_step(struct WeaponWorld*w,unsigned list,uint16_t*cursor){return w->mission_valid?step(w,list,cursor,w->mission_progress):enemy_fault(w);}
int scene_placements_camera_step(struct WeaponWorld*w,unsigned list,uint16_t*cursor){return w->secondary_view_valid?step(w,list,cursor,(uint16_t)w->secondary_x_offset):enemy_fault(w);}
