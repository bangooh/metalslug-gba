#include "tank_internal.h"
#include "scene_video.h"
#include "../generated/presentation-programs.h"
enum State {PANELS,PROMPT,BLANK,TEXT,LOGO,LOGO_FRAME};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static struct NativeEntity*allocate(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,unsigned s){struct NativeEntity*e=enemy_allocate(w,p,o,t,tick);if(e)enemy_schedule(&e->payload.enemy,s);return e;}
static int rect(struct WeaponWorld*w,int x,int y,uint16_t tile,unsigned width,unsigned height,int atlas){return scene_video_fixed_rect(w->video,x,y,tile,width,height,atlas)?1:enemy_fault(w);}
static void tick(struct EntityPool*pool,struct NativeEntity*e,void*world){(void)pool;struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(q->entry){q->entry=0;switch(a->state){
 case PANELS:if(!rect(w,14,3,0xc8c0,12,4,1)||!rect(w,14,7,0xc9c0,12,4,1))return;a->display_flags&=253;enemy_schedule(q,PROMPT);return;
 case PROMPT:q->scripted_input.timer=1;q->scripted_input.period=16;q->scripted_input.delay=0;a->state=BLANK;break;
 case LOGO:{a->palette=0xdc;interaction_priority(w,a,0xf000,28);a->vx=a->vy=a->ax=a->ay=0;transform_integer_copy(&a->transform,&a->transform);transform_integer_offset(&a->transform,tk_word(160-tk_sx(w,a)),tk_word(432-tk_sy(w,a)));a->display_flags&=253;struct PlayerTransform t={0};if(!allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&t,PROMPT))return;animation_begin(&a->animation,presentation_entries[PRA_LOGO]);a->movement_flags|=32;a->state=LOGO_FRAME;return;}
 default:enemy_fault(w);return;
 }}
 if(a->state==LOGO_FRAME){animation_step(&a->animation,&presentation_program,0);if(a->animation.fault)enemy_fault(w);return;}
 if(a->state==BLANK){if(!rect(w,10,24,0x3480,20,2,0))return;}
 else if(a->state==TEXT){if(!w->video||!w->video->start_prompt_valid||w->video->start_prompt>=5){enemy_fault(w);return;}const uint16_t*t=presentation_prompt[w->video->start_prompt];for(unsigned i=0;i<20;i++)if(!rect(w,10+i,24,t[i],1,2,1))return;}
 else{enemy_fault(w);return;}
 q->scripted_input.timer=tk_word(q->scripted_input.timer-1);if(!q->scripted_input.timer){q->scripted_input.timer=q->scripted_input.period;a->state=a->state==BLANK?TEXT:BLANK;}
}
int presentation_spawn(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,int logo){return allocate(w,p,o,t,logo?LOGO:PANELS)!=0;}
