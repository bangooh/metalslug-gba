#include "video_palette.h"
void native_video_palette_begin(struct NativeVideoPalette*p,uint16_t backdrop){
 if(!p)return;*p=(struct NativeVideoPalette){.count=1,.limit=256,.stable=1};
 if(backdrop&0x8000){p->fault=1;return;}p->colors[0]=backdrop;
}
uint16_t native_video_palette_color(struct NativeVideoPalette*p,uint16_t color){
 if(!p||p->fault)return 65535;
 if(p->ready||(color&0x8000)||!p->count){p->fault=1;return 65535;}
 unsigned h=((unsigned)color*2654435761u)>>23;
 for(unsigned n=0;n<512;n++,h=(h+1)&511){
  unsigned i=p->hash[h];if(i){if(p->colors[i]==color)return i;continue;}
  if(p->count==(p->limit?p->limit:256)){p->fault=1;return 65535;}
  i=p->count++;p->colors[i]=color;p->hash[h]=i;return i;
 }
 p->fault=1;return 65535;
}
/* Keep colours that coincide only during a fade separate. Identity is the
 * logical source slot and colour index; displayed RGB is still exact. */
uint16_t native_video_palette_stable_color(struct NativeVideoPalette*p,uint16_t color,uint16_t key){
 if(!p||!p->stable)return native_video_palette_color(p,color);
 if(p->fault||p->ready||(color&32768)||!p->count){p->fault=1;return 65535;}
 for(unsigned i=1;i<p->count;i++)if(p->keys[i]==key&&p->colors[i]==color)return i;
 if(p->count==(p->limit?p->limit:256)){p->fault=1;return 65535;}
 unsigned i=p->count++;p->colors[i]=color;p->keys[i]=key;
 unsigned h=((unsigned)color*2654435761u)>>23;
 while(p->hash[h]&&p->colors[p->hash[h]]!=color)h=(h+1)&511;
 if(!p->hash[h])p->hash[h]=i;return i;
}
int native_video_palette_finish(struct NativeVideoPalette*p){
 if(!p||p->fault||!p->count||p->ready)return 0;p->ready=1;return 1;
}

/* Compare exact32-byte banks in fast RAM. Source and hardware rows are
 * word aligned; the caller stages this mask before entering VBlank. */
__attribute__((section(".iwram.palette_compare"),noinline,long_call))
unsigned native_palette_dirty(const uint16_t*colors,uintptr_t destination,unsigned banks){
 const uint32_t*s=(const uint32_t*)colors;const volatile uint32_t*d=(const volatile uint32_t*)destination;unsigned result=0;
 for(unsigned bank=0;bank<banks;bank++,s+=8,d+=8){
  uint32_t different=(s[0]^d[0])|(s[1]^d[1])|(s[2]^d[2])|(s[3]^d[3])|(s[4]^d[4])|(s[5]^d[5])|(s[6]^d[6])|(s[7]^d[7]);
  if(different)result|=1u<<bank;
 }return result;
}
