#include "map_video_gba.h"
#define R16(a) (*(volatile uint16_t*)(uintptr_t)(a))
static void copy32(const void*src,uintptr_t dst,unsigned words){
 volatile uint32_t*d=(volatile uint32_t*)0x040000d4;
 d[2]=0;d[0]=(uintptr_t)src;d[1]=dst;
 __asm__ volatile("":::"memory");d[2]=0x84000000u|words;
 while(d[2]&0x80000000u){}
 __asm__ volatile("":::"memory");
}
int m1_map_commit_gba(struct M1MapFrame*f,struct M1TileStream*s,const uint32_t*tiles,unsigned tile_count,const uint16_t*palette,const uint8_t priorities[3]){
 if(!f||!s||!tiles||!palette||!priorities||!f->layers||f->layers>3||s->capacity>928)return 0;
 if((R16(0x04000000)&7)||(!(R16(0x04000000)&128)&&!(R16(0x04000004)&1)))return 0;
 for(unsigned i=0;i<s->upload_count;i++)if(s->uploads[i].rom_tile>=tile_count||s->uploads[i].vram_tile>=s->capacity)return 0;
 /* Caller budgets VBlank work before enqueueing. Forced blank is needed
  * for scene loads; this function does not mask an over-budget frame. */
 static const uint32_t blank[16]={0};
 copy32(blank,0x06000000u,16);
 for(unsigned i=0;i<s->upload_count;){
  unsigned first=i,rom=s->uploads[i].rom_tile,slot=s->uploads[i].vram_tile;i++;
  while(i<s->upload_count&&s->uploads[i].rom_tile==rom+i-first&&s->uploads[i].vram_tile==slot+i-first)i++;
  copy32(tiles+rom*16,0x06000000u+slot*64,(i-first)*16);
 }
 for(unsigned l=0;l<f->layers;l++){
  for(unsigned y=0;y<32;y++)if(f->dirty_rows[l]&(1u<<y))copy32(f->maps[l]+y*32,0x06000000u+(29+l)*2048+y*64,16);
  R16(0x04000008+l*2)=(priorities[l]&3)|0x80|((29+l)<<8);
  R16(0x04000010+l*4)=f->scroll_x[l];R16(0x04000012+l*4)=f->scroll_y[l];
  f->dirty_rows[l]=0;
 }
 for(unsigned i=0;i<256;i++)if(R16(0x05000000u+i*2)!=palette[i])R16(0x05000000u+i*2)=palette[i];
 R16(0x04000000)=(R16(0x04000000)&~0x0f00u)|(((1u<<f->layers)-1)<<8);
 s->upload_count=0;return 1;
}
