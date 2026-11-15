#include "scene_camera.h"
static int16_t word(int32_t x){return (int16_t)(uint16_t)x;}
static uint32_t fixed(uint16_t x){return (uint32_t)x<<16;}
static uint16_t integer(uint32_t x){return (uint16_t)(x>>16);}
static uint32_t asr(uint32_t x,unsigned n){return (uint32_t)((int32_t)x>>n);}
static void smoothing_reset(struct SceneCamera*c){c->smoothing=c->velocity=0;}
static void anchor(struct SceneCamera*c){c->base=c->progress;c->origin_x=integer(c->x);c->origin_y=integer(c->raw_y);}
void scene_camera_reset(struct SceneCamera*c){c->seed_latch=0;c->x=c->y=c->raw_y=0;c->dx=c->dy=0;c->center_x=160;c->center_y=384;c->min_x=c->min_y=c->slope=0;c->max_x=c->max_y=-1;c->progress=0;c->edge_off=0;anchor(c);smoothing_reset(c);}
void scene_camera_position(struct SceneCamera*c,uint16_t x,uint16_t y,int reset_progress){if(reset_progress){c->axis=1;c->progress=0;}c->x=fixed(x);c->y=c->raw_y=fixed(y);c->dx=c->dy=0;anchor(c);smoothing_reset(c);}
void scene_camera_axis(struct SceneCamera*c,uint8_t axis){c->axis=axis;anchor(c);}
void scene_camera_progress(struct SceneCamera*c,uint16_t progress){c->progress=progress;anchor(c);}
/* $04374C preserves unsigned WORD coordinate preparation and wrapping
 * LONG calculations, then signed comparisons for direction and saturation. */
static uint32_t follow_axis(uint16_t lo,uint16_t hi,uint16_t center,uint16_t near,uint16_t far){
 uint32_t a=fixed(lo),b=fixed(hi),d=asr(a+b,1)-fixed(center)+65536;
 if(d<131072)return 0;d-=65536;a-=fixed(near);b-=fixed(far);d+=asr(d,1);
 if((int32_t)d>=0){if((int32_t)a<0)d=0;else if((int32_t)d>(int32_t)a)d=a;}
 else{if((int32_t)b>0)d=0;else if((int32_t)d<(int32_t)b)d=b;}
 d=asr(d,4);if((int32_t)d>524288)d=524288;if((int32_t)d< -524288)d=(uint32_t)-524288;return d;
}
void scene_camera_follow(const struct SceneCamera*c,uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1,int valid,uint32_t*vx,uint32_t*vy){
 *vx=*vy=0;if(c->edge_off||!valid)return;y0=(uint16_t)(512-y0);y1=(uint16_t)(512-y1);
 if(x1<x0){uint16_t n=x0;x0=x1;x1=n;}if(y1<y0){uint16_t n=y0;y0=y1;y1=n;}
 *vx=follow_axis(x0,x1,(uint16_t)c->center_x,30,279);*vy=follow_axis(y0,y1,(uint16_t)(c->center_y-integer(c->smoothing)),0,208);
}
/* $043E3A: bounds checks are UNSIGNED. FFFFFFFF is the no-crossing-clamp
 * sentinel. A max bound contains a fractionalFFFF, unlike the min bound. */
static int16_t integrate(uint32_t*position,uint32_t velocity,int16_t min,int16_t max){
 uint32_t old=*position,low=fixed((uint16_t)min),high=fixed((uint16_t)max)|65535,boundary;
 if(!velocity)return 0;if((int32_t)velocity<0){if(low>=old)return 0;boundary=low;}else{if(high<old)return 0;boundary=high;}
 uint32_t next=old+velocity;if(boundary!=0xffffffffu&&(((boundary-next)^(boundary-old))&0x80000000u))next=boundary;
 *position=next;return word((int32_t)integer(next)-integer(old));
}
static void smooth(struct SceneCamera*c){
 uint32_t old=c->smoothing,d=c->velocity;
 /* The source divides LARGE velocities by8. The small-velocity branch
  * skips ASR; do not copy the inverted claim in the assembly commentary. */
 if((int32_t)d>=16384||(int32_t)d<=-16384)d=asr(d,3);
 c->velocity-=d+old;uint32_t next=old+c->velocity;
 if((int32_t)next>524288)next=524288;else if((int32_t)next<=-524288)next=(uint32_t)-524288;
 c->smoothing=next;c->smoothing_delta=word((int32_t)integer(next)-integer(old));
}
void scene_camera_integrate(struct SceneCamera*c,uint32_t vx,uint32_t vy){
 c->dx=integrate(&c->x,vx,c->min_x,c->max_x);int16_t low=c->min_y,high=c->max_y;
 if(c->slope){vy=fixed((uint16_t)(c->slope<0?c->dx:word(-c->dx)));int16_t distance=word(c->max_x-integer(c->x));int16_t offset=word(((int32_t)c->slope*distance)>>8);low=word(low+offset);high=word(high+offset);}
 c->dy=integrate(&c->raw_y,vy,low,high);uint16_t progress;
 if(c->axis==3)progress=(uint16_t)(integer(c->raw_y)-c->origin_y);else if(c->axis==2)progress=(uint16_t)(c->origin_y-integer(c->raw_y));else if(!c->axis)progress=(uint16_t)(c->origin_x-integer(c->x));else progress=(uint16_t)(integer(c->x)-c->origin_x);
 progress=(uint16_t)(progress+c->base);if(progress>c->progress)c->progress=progress;
 smooth(c);c->y=c->raw_y+c->smoothing;c->dy=word(c->dy+c->smoothing_delta);
}
int scene_camera_at_edge(const struct SceneCamera*c,uint8_t gate){if(c->edge_off||!gate)return 1;if(c->axis==3)return integer(c->raw_y)==(uint16_t)c->max_y;if(c->axis==2)return integer(c->raw_y)==(uint16_t)c->min_y;if(!c->axis)return integer(c->x)==(uint16_t)c->min_x;return integer(c->x)==(uint16_t)c->max_x;}
