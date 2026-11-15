#include "native_video.h"
#include "play_world.h"
#include "scene_runtime.h"
#include "session.h"
#include "result_background.h"
#include "../generated/result-affine.inc"
#define R16(a) (*(volatile uint16_t*)(a))
#define R32(a) (*(volatile uint32_t*)(a))
int native_result_background_prepare(struct NativeVideo*v){
 if(!v->world->session||v->world->session->state!=SESSION_FINAL){
  v->result_mode=v->result_loaded=0;return 0;
 }
 v->result_mode=1;v->result_visible=0;v->hud_active=0;
 v->backgrounds.fade_control=v->backgrounds.fade_level=0;
 v->backgrounds.ready=v->colors.ready=1;v->colors.count=16;
 for(unsigned i=0;i<16;i++)v->colors.colors[i]=0;
 for(unsigned i=0;i<v->objects.count;i++){
  const struct NativeObjectDraw*d=&v->objects.draws[i];
  if(d->part_first==255)continue;
  for(unsigned j=0;j<d->part_count;j++){
   const struct NativeCapturedPart*p=&v->objects.parts[d->part_first+j];
   if(p->map!=5026||d->pose.zoom_x<2||d->pose.zoom_y<2)continue;
   if(v->result_visible){v->fault=1;return 1;}
   unsigned zx=d->pose.zoom_x+1,zy=d->pose.zoom_y+1;
   int pa=(65536+zx/2)/zx,pd=(65536+zy/2)/zy;
   if(p->flip&1)pa=-pa;if(p->flip&2)pd=-pd;
   v->result_pa=pa;v->result_pd=pd;
   v->result_x=(RESULT_BG_X<<8)-native_screen_x(p->x)*pa-((p->flip&1)?256:0);
   v->result_y=(RESULT_BG_Y<<8)-native_screen_y(p->y)*pd-((p->flip&2)?256:0);
   /* Captured OBJ colours already contain the source fade. Applying the
    * hardware fade as well would shade this layer twice. */
   for(unsigned k=1;k<16;k++)v->colors.colors[k]=d->palette.state.colors[k];
   v->result_visible=1;
  }
 }
 return 1;
}
static void copy(const void*src,unsigned dst,unsigned bytes){
 R32(0x040000dc)=0;R32(0x040000d4)=(uintptr_t)src;R32(0x040000d8)=dst;
 __asm__ volatile("":::"memory");R32(0x040000dc)=0x84000000u|bytes/4;
 while(R32(0x040000dc)&0x80000000u){}__asm__ volatile("":::"memory");
}
int native_result_background_commit(struct NativeVideo*v){
 if(!v->result_loaded){
  copy(result_bg_tiles0,0x06000000,sizeof(result_bg_tiles0));
  copy(result_bg_tiles1,0x06004000,sizeof(result_bg_tiles1));
  copy(result_bg_map0,0x0600e000,1024);copy(result_bg_map1,0x0600e800,1024);
  v->result_loaded=1;
 }
 copy(v->colors.colors,0x05000000,32);
 for(unsigned i=0;i<2;i++){
  R16(0x0400000c+i*2)=3|(i<<2)|((28+i)<<8)|(1<<14);
  unsigned a=0x04000020+i*16;
  R16(a)=v->result_pa;R16(a+2)=0;R16(a+4)=0;R16(a+6)=v->result_pd;
  R32(a+8)=v->result_x;R32(a+12)=v->result_y;
 }
 R16(0x04000000)=(R16(0x04000000)&~0xf07)|2|(v->result_visible?0xc00:0);
 v->backgrounds.ready=v->colors.ready=0;return 1;
}
