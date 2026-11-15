#include "background_video.h"
#include "dirty_span.h"
static uint16_t palette_dirty;
static void copy(const void*s,uintptr_t dest,unsigned words){
 if(!words)return;
 if(words<=4){const uint32_t*p=s;volatile uint32_t*q=(volatile uint32_t*)dest;for(unsigned i=0;i<words;i++)q[i]=p[i];return;}
 volatile uint32_t*d=(volatile uint32_t*)0x040000d4;
 d[2]=0;d[0]=(uintptr_t)s;d[1]=dest;__asm__ volatile("":::"memory");d[2]=0x84000000u|words;
 while(d[2]&0x80000000u){}__asm__ volatile("":::"memory");
}
int native_background_commit(struct NativeBackgroundFrame*f,struct NativeVideoPalette*p){
 if(!f||f->fault||!f->ready||!p||p->fault||!p->ready)return 0;
 volatile uint16_t*display=(volatile uint16_t*)0x04000000;
 if((*display&7)||(!(*display&128)&&!(*(volatile uint16_t*)0x04000004&1)))return 0;
 if(f->uploaded>f->count)return 0;
 unsigned at=0,first,count;
 while((count=native_dirty_span(f->dirty,f->count,&at,&first)))copy(native_background_pixels(f,first),0x06000000+first*32,count*8);
 for(unsigned i=0;i<31;i++)f->dirty[i]=0;
 f->uploaded=f->count;
 for(unsigned i=0;i<4;i++){
  if(i<f->map_count&&(f->map_dirty&(1u<<i))){
   const uint16_t*map=native_background_4(f)->maps[i];uintptr_t target=0x06000000+(32-!!f->hud_reserved-f->map_count+i)*2048;
   for(unsigned row=0;row<32;row++)if(f->map_rows[i]&(1u<<row)){unsigned range=f->map_ranges[i][row],first=range&255,end=range>>8;if(end>first)copy(map+row*32+first,target+(row*32+first)*2,(end-first)/2);}
  }
  *(volatile uint16_t*)(0x04000008+i*2)=f->control[i];
  *(volatile uint16_t*)(0x04000010+i*4)=f->scroll_x[i];
  *(volatile uint16_t*)(0x04000012+i*4)=f->scroll_y[i];
 }
 for(unsigned bank=0;bank<(f->hud_reserved?12u:16u);bank++)if(palette_dirty&(1u<<bank))copy((f->depth4?native_background_4(f)->physical_colors:p->colors)+bank*16,0x05000000u+bank*32,8);
 *display=(*display&~0x0f00)|f->enable;
 f->map_dirty=0;for(unsigned i=0;i<4;i++)f->map_rows[i]=0;f->ready=p->ready=0;return 1;
}

void native_background_stage_palette(struct NativeBackgroundFrame*f,struct NativeVideoPalette*p){
 const uint16_t*colors=p->colors;unsigned banks=f->hud_reserved?12:16;
 if(f->depth4){
  struct NativeBackground4*b=native_background_4(f);colors=b->physical_colors;
  /* Physical map expansion belongs only to the4bpp path. Keeping it out
   * of map_set leaves ordinary8bpp composition unchanged. */
  for(unsigned l=0;l<f->map_count;l++)if(f->map_dirty&(1u<<l)){
   const volatile uint16_t*old=(const volatile uint16_t*)(0x06000000+(32-!!f->hud_reserved-f->map_count+l)*2048);
   for(unsigned row=0;row<32;row++)if(f->map_rows[l]&(1u<<row)){
    unsigned first=32,end=0;
    for(unsigned x=0;x<32;x++){unsigned at=row*32+x,id=f->maps[l][at],value=id<NATIVE_BG_TILES?id|((unsigned)b->bank[id]<<12):0;b->maps[l][at]=value;if(old[at]!=value){if(first==32)first=x;end=x+1;}}
    f->map_ranges[l][row]=end?((first&~1u)|(((end+1)&~1u)<<8)):0;
   }
  }
  for(unsigned bank=0;bank<banks;bank++){uint16_t*d=b->physical_colors+bank*16;d[0]=bank?0:p->colors[0];for(unsigned j=1;j<16;j++)d[j]=j<=b->counts[bank]?p->colors[b->indices[bank][j]]:0;}
 }
 palette_dirty=native_palette_dirty(colors,0x05000000,banks);
}
