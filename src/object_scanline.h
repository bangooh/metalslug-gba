#ifndef NATIVE_OBJECT_SCANLINE_H
#define NATIVE_OBJECT_SCANLINE_H
#include "object_video.h"
/* Conservative OAM scanline cost, including affine transparent bounds and
 * wrapped Y. Used only to request a cosmetic/dense retry, never to discard
 * structural objects or turn an otherwise drawable frame into a fault.
 * Scratch is on the stack; persistent GBA RAM layout is unchanged. */
static inline int native_object_scanline_overflow(const struct NativeObjectFrame*f,unsigned limit){
 static const uint8_t sizes[3][4][2]={{{8,8},{16,16},{32,32},{64,64}},{{16,8},{32,8},{32,16},{64,32}},{{8,16},{8,32},{16,32},{32,64}}};
 int32_t delta[161]={0};
 for(unsigned i=0;i<f->count;i++){
  unsigned a=f->objects[i].attr0,b=f->objects[i].attr1,shape=a>>14;
  if(shape>=3||(!(a&256)&&(a&512)))continue;
  unsigned w=sizes[shape][b>>14][0],h=sizes[shape][b>>14][1],affine=!!(a&256);
  if(affine&&(a&512)){w*=2;h*=2;}
  unsigned cost=affine?10+w*2:w,y=a&255,end=y+h;
  if(y<160){unsigned stop=end<160?end:160;delta[y]+=cost;delta[stop]-=cost;}
  if(end>256){unsigned stop=end-256;if(stop>160)stop=160;delta[0]+=cost;delta[stop]-=cost;}
 }
 int32_t used=0;for(unsigned y=0;y<160;y++){used+=delta[y];if((unsigned)used>limit)return 1;}
 return 0;
}
#endif
