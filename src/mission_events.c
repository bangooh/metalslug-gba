#include "mission_events.h"
#include "enemies.h"
#include "slug_support.h"
#include "scene_runtime.h"
struct EventContext {struct WeaponWorld*w;struct NativeEntity*e;};
static void event_tick(struct EntityPool*,struct NativeEntity*,void*);
static int spawn(void*context,const struct MissionSpawn*s){
 struct EventContext*c=context;
 return c->w->mission_spawn&&c->w->mission_spawn(c->w->scene,s);
}
static int periodic(void*context,const struct MissionTask*t){
 struct EventContext*c=context;struct NativeEntity*n=entity_spawn(c->w->entities,entity_ref(c->e),
  ENTITY_MISSION,ENTITY_FIRST_CHILD,event_tick);
 if(!n)return 0;n->payload.mission.task=*t;return 1;
}
static void event_tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;struct MissionEventWorld*m=w->events;
 if(w->fault)return;
 if(!m||!w->mission_valid||!w->secondary_view_valid||!w->enemies){enemy_fault(w);return;}
 struct MissionEventActor*q=e->kind==ENTITY_GROUP?&e->payload.group.mission:&e->payload.mission;
 if(q->watch){
  struct NativeEntity*parent=entity_get(p,e->parent);
  int stop=parent?q->watch(parent):-1;
  if(stop<0){enemy_fault(w);return;}
  if(stop){entity_delete_children(p,e);entity_delete(p,e);return;}
 }
 struct MissionInput in={.camera_x=(int16_t)w->camera_x,.camera_y=(int16_t)w->camera_y,
  .secondary_x=w->secondary_x_offset,.secondary_y=w->secondary_y_offset,.secondary_valid=1,
  .progress=w->mission_progress,.enemies=w->mission_enemies,.active=w->enemies->damage_enabled,
  .player_count=m->player_count,.slug_present=w->enemies->slug&&w->enemies->slug->present};
 for(unsigned i=0;i<2;i++){
  struct PlayerBodyState*player=w->players[i];if(!enemy_player_live(player))continue;
  in.players[i]=(struct MissionPlayer){.x=(int16_t)((player->actor.transform.x>>8)-w->camera_x),
   .y=(int16_t)((player->actor.transform.y>>8)-w->camera_y),.active=1};
 }
 struct EventContext context={w,e};
 struct MissionExecutor x={.context=&context,.rng=w->rng,.spawn=spawn,.periodic=periodic,
  .executed=m->executed,.spawned=m->spawned,.edge_latch=m->edge_latch};
 mission_task_frame(&x,&q->task,&in);
 m->executed=x.executed;m->spawned=x.spawned;m->edge_latch=x.edge_latch;
 /* $044426/$04445A write the same $108179 byte tested by the camera
  * at $0436DE/$043CE2. Preserve the event-wave lock across both consumers. */
 if(w->scene_runtime)w->scene_runtime->camera.edge_off=(uint8_t)x.edge_latch;
 if(x.fault){m->fault=x.fault;m->fault_source=x.fault_source;enemy_fault(w);return;}
 if(!q->task.active){
  if(q->task.periodic)entity_delete_children(p,e);
  if(e->kind==ENTITY_GROUP){e->tick=0;m->enabled=0;}
  else entity_delete(p,e);
 }
}
int mission_event_begin(struct WeaponWorld*w,struct EntityRef group,uint32_t source){
 uint16_t at=mission_offset(source);struct NativeEntity*e=entity_get(w->entities,group);
 if(!w->events||!e||e->kind!=ENTITY_GROUP||(e->tick&&e->tick!=event_tick)||at==65535)return enemy_fault(w);
 e->payload.group.mission=(struct MissionEventActor){.task={.offset=at,.active=1}};
 e->tick=event_tick;w->events->enabled=1;return 1;
}
int mission_event_stop(struct WeaponWorld*w,struct EntityRef group){
 struct NativeEntity*e=entity_get(w->entities,group);
 if(!w->events||!e||e->kind!=ENTITY_GROUP||(e->tick&&e->tick!=event_tick))return enemy_fault(w);
 e->tick=0;e->payload.group.mission=(struct MissionEventActor){0};w->events->enabled=0;return 1;
}
struct NativeEntity*mission_event_spawn(struct WeaponWorld*w,struct EntityRef parent,uint32_t source,MissionWatchFn watch){
 uint16_t at=mission_offset(source);
 if(!w->events||!w->entities||at==65535){
  if(w->events){w->events->fault=1;w->events->fault_source=source;}enemy_fault(w);return 0;
 }
 struct NativeEntity*e=entity_spawn(w->entities,parent,ENTITY_MISSION,ENTITY_FIRST_CHILD,event_tick);
 if(!e){enemy_fault(w);return 0;}
 e->payload.mission=(struct MissionEventActor){.task={.offset=at,.active=1},.watch=watch};
 w->events->enabled=1;return e;
}
