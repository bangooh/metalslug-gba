#include "bosses.h"
#include "scene_video.h"
/*52580/525A4: independent signed8.8 RGB interpolation task; does not setCF.
 * Source526AA clears its update flag, but2CB6..2CE0 retains palette RAM
 * when the previous flag was80. Our renderer derives colours from fade,
 * so retain the final transform until another fade or reset replaces it.
 * Clearing mode here would restore full colour between red and black. */
static int16_t word(int n){return (int16_t)(uint16_t)n;}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){struct WeaponWorld*w=world;if(w->fault)return;if(!w->video){enemy_fault(w);return;}struct EnemyActor*q=&e->payload.enemy;uint8_t*rgb[]={&w->video->fade.red,&w->video->fade.green,&w->video->fade.blue};if(q->entry){q->entry=0;if(!q->rgb_fade.remaining){enemy_fault(w);return;}w->video->fade.mode=128;for(unsigned i=0;i<3;i++){q->rgb_fade.initial[i]=*rgb[i];q->rgb_fade.accumulator[i]=0;int16_t delta=word((*rgb[i]<<8)-((uint16_t)q->rgb_fade.target[i]<<8));q->rgb_fade.step[i]=word(delta/q->rgb_fade.remaining);}}
 if(q->rgb_fade.done){entity_delete(p,e);return;}q->rgb_fade.remaining=word(q->rgb_fade.remaining-1);if(!q->rgb_fade.remaining){for(unsigned i=0;i<3;i++)*rgb[i]=(uint8_t)q->rgb_fade.target[i];q->rgb_fade.done=1;return;}for(unsigned i=0;i<3;i++){q->rgb_fade.accumulator[i]=word(q->rgb_fade.accumulator[i]+q->rgb_fade.step[i]);*rgb[i]=(uint8_t)(q->rgb_fade.initial[i]-(q->rgb_fade.accumulator[i]>>8));}}
int scene_rgb_fade_spawn(struct WeaponWorld*w,struct EntityRef parent,int16_t red,int16_t green,int16_t blue,int16_t ticks){if(!ticks)return enemy_fault(w);struct PlayerTransform t={.x=-256,.y=513*256,.priority=65535,.display_group=255};struct NativeEntity*e=enemy_allocate(w,parent,ENTITY_FIRST_CHILD,&t,tick);if(!e)return 0;e->payload.enemy.rgb_fade=(__typeof__(e->payload.enemy.rgb_fade)){.remaining=ticks,.target={red,green,blue}};return 1;}
