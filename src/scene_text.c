#include "scene_text.h"
#include "enemies.h"
#include "bosses.h"
#include "scene_runtime.h"
#include "scene_video.h"
#include "../generated/scene-text-data.h"
enum State {WATCH,CONTINUED,GO_START,GO_FRAME};
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
static int cell_rect(struct SceneVideoWorld*v,uint16_t at,uint16_t tile,unsigned width,unsigned height,int atlas){return scene_video_fixed_rect(v,(int)at/32,(int)at%32,tile,width,height,atlas);}
/* GBA adaptation: transparent side columns reveal the full240px viewport. */
int scene_text_clear_border(struct SceneVideoWorld*v,int deep){return scene_video_fixed_rect(v,0,0,32,40,2,0)&&scene_video_fixed_rect(v,0,deep?26:30,32,40,deep?6:2,0)&&scene_video_fixed_rect(v,0,0,0x300,1,32,0)&&scene_video_fixed_rect(v,39,0,0x300,1,32,0);}
int scene_text_hex(struct SceneVideoWorld*v,unsigned col,unsigned row,uint16_t value){for(unsigned i=0;i<4;i++)if(!scene_video_fixed_rect(v,col+i,row,scene_fixed_hex[(value>>(12-4*i))&15],1,1,0))return 0;return 1;}
/* The cursor is a column-major cell index (sourceFIX address minus7000),
 * not a world position.2x2 writes preserve column/row tile order. */
static int glyph(struct WeaponWorld*w,uint16_t cursor,uint16_t tile){return cell_rect(w->video,cursor,tile,2,2,1)?1:enemy_fault(w);}
static const struct FixedTimelineFrame*frame(struct WeaponWorld*w,struct EnemyActor*q){unsigned id=q->scene_text.timeline;if(id>=COUNT(scene_text_timelines)){enemy_fault(w);return 0;}const struct FixedTimeline*t=&scene_text_timelines[id];if(q->scene_text.frame>=t->count){enemy_fault(w);return 0;}return &t->frames[q->scene_text.frame];}
static int fixed_draw(struct WeaponWorld*w,struct EnemyActor*q,const struct FixedTimelineFrame*f,int clear){if(clear&&f->tile==65535)return 1;return cell_rect(w->video,q->scene_text.cursor,clear?255:f->tile,f->width,f->height,!clear)?1:enemy_fault(w);}
static int timeline_begin(struct WeaponWorld*w,struct EnemyActor*q,unsigned id){q->scene_text.timeline=id;q->scene_text.frame=0;const struct FixedTimelineFrame*f=frame(w,q);if(!f)return 0;q->scene_text.clock=f->ticks;return fixed_draw(w,q,f,0);}
/*5DB6A: a zero record jumps and draws immediately, but does not decrement
 * its new duration again. Return1 only atFFFF, -1 for a service fault. */
static int timeline_step(struct WeaponWorld*w,struct EnemyActor*q){const struct FixedTimelineFrame*f=frame(w,q);if(!f)return -1;if(f->tile==65535)return 1;q->scene_text.clock--;if((int16_t)q->scene_text.clock>0)return 0;if(!fixed_draw(w,q,f,1))return -1;q->scene_text.frame++;f=frame(w,q);if(!f)return -1;if(!f->tile)return timeline_begin(w,q,f->jump)?0:-1;if(f->tile==65535)return 1;q->scene_text.clock=f->ticks;return fixed_draw(w,q,f,0)?0:-1;}
static int label(struct WeaponWorld*w,unsigned id){uint16_t cursor=0xae;for(unsigned i=0;i<16;i++){uint8_t c=scene_continue_labels[id][i];if(c==255)return 1;uint16_t tile=(uint16_t)(0xb00+((c&8)<<5)+((c&0x77)<<1))|0x4000;if(!glyph(w,cursor,tile))return 0;cursor+=64;}return enemy_fault(w);}
static void tick(struct EntityPool*pool,struct NativeEntity*e,void*world){struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(q->entry){q->entry=0;switch(a->state){
 case WATCH:q->scene_text.cursor=0x408;q->scene_text.progress=65535;q->scene_text.clock=0;w->enemies->cutscene.playback_mode=0;break;
 case CONTINUED:q->scene_text.clock=120;if(!label(w,0))return;break;
 default:enemy_fault(w);return;
 }}
 if(a->state==CONTINUED){q->scene_text.clock--;if((int16_t)q->scene_text.clock<0){w->enemies->scripted_player_lock=0;if(label(w,1))entity_delete(pool,e);}return;}
 if(a->state==WATCH){if(!w->scene_runtime||!w->mission_valid){enemy_fault(w);return;}if(q->scene_text.progress!=w->mission_progress){q->scene_text.progress=w->mission_progress;q->scene_text.clock=120;}
  else if(q->scene_text.clock)q->scene_text.clock--;
  else if(!(w->mission_enemies|pool->displayed_count)&&(enemy_player_live(w->players[0])||enemy_player_live(w->players[1]))&&!scene_camera_at_edge(&w->scene_runtime->camera,w->enemies->intro_gate)&&w->enemies->damage_enabled){q->scene_text.clock=120;w->enemies->cutscene.playback_mode=2;}
  if(!w->enemies->cutscene.playback_mode)return;a->state=GO_START;
 }
 if(a->state==GO_START){unsigned variant=w->enemies->cutscene.variant;if(variant>2)variant=0;if(!timeline_begin(w,q,scene_go_timelines[variant])||!interaction_sound(w,0x10d5))return;a->state=GO_FRAME;}
 if(a->state==GO_FRAME){int done=timeline_step(w,q);if(done<0)return;if(done){w->enemies->cutscene.playback_mode--;if(!w->enemies->cutscene.playback_mode){const struct FixedTimelineFrame*f=frame(w,q);if(!f||!fixed_draw(w,q,f,1))return;a->state=WATCH;}else a->state=GO_START;}return;}
 enemy_fault(w);
}
int scene_text_spawn(struct WeaponWorld*w,struct EntityRef p,enum SceneTextEntry id){if((unsigned)id>SCENE_TEXT_ENTRY_CONTINUED)return enemy_fault(w);struct PlayerTransform t={.x=-256,.y=513*256,.priority=65535,.display_group=255};struct NativeEntity*e=enemy_allocate(w,p,ENTITY_FIRST_CHILD,&t,tick);if(!e)return 0;enemy_schedule(&e->payload.enemy,id);return 1;}
