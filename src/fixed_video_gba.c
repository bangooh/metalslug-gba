#include "fixed_video.h"
static void copy16(const void*src,uintptr_t dst,unsigned count){
 if(!count)return;
 volatile uint32_t*dma=(volatile uint32_t*)0x040000d4;
 dma[2]=0;dma[0]=(uintptr_t)src;dma[1]=dst;
 __asm__ volatile("":::"memory");dma[2]=0x80000000u|count;
 while(dma[2]&0x80000000u){}__asm__ volatile("":::"memory");
}
int native_fixed_commit(struct NativeFixedFrame*f,const struct NativeVideoPalette*p){
 if(!f||f->fault||!f->ready||!p||p->fault||!p->ready||!native_fixed_layout_valid(&f->layout))return 0;
 volatile uint16_t*display=(volatile uint16_t*)0x04000000;
 volatile uint16_t*status=(volatile uint16_t*)0x04000004;
 uint16_t control=*display;
 /* TextBG0 also exists in modes1/2, but the joint four-plane plan uses0.
  * Do not silently change an active display mode or overlap another owner. */
 if((control&7)||(!(control&128)&&!(*status&1)))return 0;
 const struct NativeFixedLayout*l=&f->layout;
 copy16(f->tiles,0x06000000u+l->char_block*16384u+l->first_tile*64u,f->tile_count*32u);
 copy16(f->map,0x06000000u+l->screen_block*2048u,1024);
 copy16(p->colors,0x05000000,p->count);
 /* BG0 8bpp,32x32,priority0; preserve other BG controls and scroll. */
 *(volatile uint16_t*)0x04000008=0x80u|(l->char_block<<2)|(l->screen_block<<8);
 *(volatile uint16_t*)0x04000010=0;*(volatile uint16_t*)0x04000012=0;
 *display=control|0x100;f->ready=0;return 1;
}
