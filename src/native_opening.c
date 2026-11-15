#include "native_opening.h"
#include "../generated/opening-replay-data.h"
#include "native_inflate.h"
#include "../generated/opening-visibility.h"
#include "../generated/opening-background.h"
#include "../m1_native/runtime.h"
#define HOT __attribute__((section(".openinghot")))
#define R16(a) (*(volatile uint16_t*)(a))
#define R32(a) (*(volatile uint32_t*)(a))
static uint8_t *cache,*pixels,*background_pixels;
extern uint32_t __opening_lookup[];
#define expand __opening_lookup
static unsigned shown,sound_next,page,cell_next;
static uint8_t *visibility,*opaque;
_Static_assert(OPENING_VISIBILITY_MAX+(OPENING_TILE_COUNT+7)/8+3<=1032,"Opening scratch overflow");
static const struct OpeningCell *cells;
static const struct OpeningMap *maps;
static const struct OpeningDraw *draws;
static const uint16_t (*palettes)[16];
static const struct OpeningFrame *frames;
static const uint16_t *palrefs;
static HOT void dma(const void *src,uintptr_t dst,unsigned bytes){
 volatile uint32_t *d=(volatile uint32_t*)0x040000d4;
 d[2]=0;d[0]=(uintptr_t)src;d[1]=dst;__asm__ volatile("":::"memory");d[2]=0x84000000u|(bytes/4);while(d[2]&0x80000000u){}
}
int native_opening_begin(void *tiles,size_t tile_bytes,void *framebuffer,size_t pixel_bytes){
 if(tile_bytes<OPENING_TILES_BYTES+OPENING_CELLS_BYTES+OPENING_MAPS_BYTES+OPENING_PALETTES_BYTES+OPENING_FRAMES_BYTES+OPENING_PALREFS_BYTES+1032||pixel_bytes<38400+OPENING_DRAWS_BYTES)return 0;
 R16(0x04000000)=128;m1_runtime_load(2);cache=tiles;pixels=framebuffer;background_pixels=framebuffer;shown=~0u;sound_next=0;page=0;
 cells=(const void*)(cache+OPENING_TILES_BYTES);maps=(const void*)((const uint8_t*)cells+((OPENING_CELLS_BYTES+3)&~3u));
 palettes=(const void*)((const uint8_t*)maps+OPENING_MAPS_BYTES);draws=(const void*)(pixels+38400);
 frames=(const void*)((const uint8_t*)palettes+OPENING_PALETTES_BYTES);palrefs=(const void*)((const uint8_t*)frames+OPENING_FRAMES_BYTES);
 if(!native_inflate(opening_frames_packed,(void*)frames,OPENING_FRAMES_BYTES,0)||
    !native_inflate(opening_palrefs_packed,(void*)palrefs,OPENING_PALREFS_BYTES,0)||
    !native_inflate(opening_tiles_packed,cache,OPENING_TILES_BYTES,0)||
    !native_inflate(opening_cells_packed,(void*)cells,OPENING_CELLS_BYTES,0)||
    !native_inflate(opening_maps_packed,(void*)maps,OPENING_MAPS_BYTES,0)||
    !native_inflate(opening_palettes_packed,(void*)palettes,OPENING_PALETTES_BYTES,0)||
    !native_inflate(opening_draws_packed,(void*)draws,OPENING_DRAWS_BYTES,0)||
    !native_inflate(opening_background_packed,background_pixels,38400,0))return 0;
 visibility=(void*)(((uintptr_t)palrefs+OPENING_PALREFS_BYTES+3)&~3u);
 opaque=visibility+OPENING_VISIBILITY_MAX;
 for(unsigned i=0;i<(OPENING_TILE_COUNT+7)/8;i++)opaque[i]=0;
 for(unsigned i=0;i<OPENING_TILE_COUNT;i++){unsigned solid=1;for(unsigned j=0;j<72;j++){unsigned v=cache[i*72+j];if(!(v&15)||!(v>>4)){solid=0;break;}}if(solid)opaque[i>>3]|=1u<<(i&7);}
 /* Lookup stays with the opening overlay in fast IWRAM. */
 for(unsigned i=0;i<256;i++){unsigned mask=(i&15?255:0)|(i&240?65280:0);expand[i]=(mask<<16)|(i&15)|((i&240)<<4);}
 for(unsigned i=0;i<128;i++)((volatile uint16_t*)0x07000000)[i*4]=512;
 R16(0x04000050)=0;R16(0x04000052)=0;R16(0x04000054)=0;
 R16(0x0400000c)=0;R16(0x04000020)=256;R16(0x04000022)=0;R16(0x04000024)=0;R16(0x04000026)=256;
 R32(0x04000028)=R32(0x0400002c)=0;R16(0x0400004c)=0;
 return 1;
}
static const uint8_t resample[13][12]={{0,0,0,0,0,0,0,0,0,0,0,0},{6,11,11,11,11,11,11,11,11,11,11,11},{3,9,11,11,11,11,11,11,11,11,11,11},{2,6,10,11,11,11,11,11,11,11,11,11},{1,4,7,10,11,11,11,11,11,11,11,11},{1,3,6,8,10,11,11,11,11,11,11,11},{1,3,5,7,9,11,11,11,11,11,11,11},{0,2,4,6,7,9,11,11,11,11,11,11},{0,2,3,5,6,8,9,11,11,11,11,11},{0,2,3,4,6,7,8,10,11,11,11,11},{0,1,3,4,5,6,7,9,10,11,11,11},{0,1,2,3,4,6,7,8,9,10,11,11},{0,1,2,3,4,5,6,7,8,9,10,11}};
static inline void put_pixel(uint8_t *p,unsigned value){
 uintptr_t a=(uintptr_t)p;volatile uint16_t *q=(volatile uint16_t*)(a&~1u);unsigned shift=(a&1)*8;
 *q=(*q&~(255u<<shift))|(value<<shift);
}
static HOT void draw(const struct OpeningDraw *d){
 const struct OpeningMap *m=&maps[d->map];unsigned sx=d->sx+1,sy=d->sy+1,flip=d->flags,bank=d->bank*16;
 for(unsigned i=0;i<m->count;i++){
  unsigned bit=cell_next++;if(!(visibility[bit>>3]&(1u<<(bit&7))))continue;
  const struct OpeningCell *c=&cells[m->first+i];
  int xx=(flip&1)?-c->x-12:c->x,yy=(flip&2)?-c->y-12:c->y;
  int left=d->x+((xx*(int)sx)>>8),top=d->y+((yy*(int)sy)>>8);
  int right=d->x+(((xx+12)*(int)sx)>>8),bottom=d->y+(((yy+12)*(int)sy)>>8);
  int w=right-left,h=bottom-top;if(left>=240||top>=160||right<=0||bottom<=0||w<=0||h<=0)continue;
  const uint8_t *src=cache+c->tile*72u;
  int x0=left<0?-left:0,x1=right>240?240-left:w,y0=top<0?-top:0,y1=bottom>160?160-top:h;
  if(sx==256&&sy==256&&!flip && !((left)&1) && x0==0 && x1==12 && (opaque[c->tile>>3]&(1u<<(c->tile&7)))){
   unsigned banks=bank*257;const uint8_t *row=src+y0*6;uint16_t*out=(uint16_t*)(pixels+(top+y0)*240+left);
   for(int y=y0;y<y1;y++){
    out[0]=expand[row[0]]|banks;out[1]=expand[row[1]]|banks;out[2]=expand[row[2]]|banks;
    out[3]=expand[row[3]]|banks;out[4]=expand[row[4]]|banks;out[5]=expand[row[5]]|banks;
    out+=120;row+=6;
   }
  }else if(sx==256&&sy==256&&!flip && !(left&1) && x0==0 && x1==12){
   unsigned banks=bank*257;const uint8_t *row=src+y0*6;uint16_t*out=(uint16_t*)(pixels+(top+y0)*240+left);
   #define OPEN_PAIR(N) do{unsigned q=row[N];if(q){unsigned v=expand[q],mask=v>>16;if(mask==65535)out[N]=v|banks;else out[N]=(out[N]&~mask)|((v|banks)&mask);}}while(0)
   for(int y=y0;y<y1;y++){OPEN_PAIR(0);OPEN_PAIR(1);OPEN_PAIR(2);OPEN_PAIR(3);OPEN_PAIR(4);OPEN_PAIR(5);out+=120;row+=6;}
   #undef OPEN_PAIR
  }else if(sx==256&&sy==256&&!flip){
   for(int y=y0;y<y1;y++){
    uint8_t *dst=pixels+(top+y)*240+left;const uint8_t *row=src+y*6;
    if(!(left&1)){
     uint16_t *out=(uint16_t*)dst;unsigned banks=bank*257;
     for(int x=x0/2;x<x1/2;x++){unsigned v=expand[row[x]],mask=v>>16;if(mask==65535)out[x]=v|banks;else if(mask)out[x]=(out[x]&~mask)|((v|banks)&mask);}
    }else{
     int first=x0;if(!(first&1)){unsigned v=row[first/2]&15;if(v)put_pixel(dst+first,bank|v);first++;}
     uint16_t *out=(uint16_t*)(dst+first);unsigned banks=bank*257;int x=first;
     for(;x+1<x1;x+=2){unsigned q=(row[x/2]>>4)|((row[x/2+1]&15)<<4),v=expand[q],mask=v>>16;if(mask==65535)*out=v|banks;else if(mask)*out=(*out&~mask)|((v|banks)&mask);out++;}
     if(x<x1){unsigned v=row[x/2]>>4;if(v)put_pixel(dst+x,bank|v);}
    }
   }
  }else{
   unsigned xs[12],ys[12];
   for(int x=x0;x<x1;x++){unsigned q=resample[w][x];if(q>11)q=11;xs[x]=(flip&1)?11-q:q;}
   for(int y=y0;y<y1;y++){unsigned q=resample[h][y];if(q>11)q=11;ys[y]=(flip&2)?11-q:q;}
   for(int y=y0;y<y1;y++){
    uint8_t *dst=pixels+(top+y)*240+left;const uint8_t *row=src+ys[y]*6;
    for(int x=x0;x<x1;x++){unsigned q=xs[x],v=(row[q>>1]>>((q&1)*4))&15;if(v)put_pixel(dst+x,bank|v);}
   }
  }
 }
}
HOT int native_opening_frame(unsigned elapsed,struct SourceAudioQueue *audio){
 while(sound_next<OPENING_SOUND_COUNT&&opening_sounds[sound_next].frame<=elapsed){
  const struct OpeningSound *s=&opening_sounds[sound_next++];
  if(s->stop)source_audio_stop_command(audio,s->command);else source_audio_game_command(audio,s->command);
 }
 unsigned frame=elapsed/2;if(frame>=OPENING_FRAME_COUNT)return 1;if(frame==shown)return 0;
 shown=frame;const struct OpeningFrame *f=&frames[frame];
 if(!native_inflate(opening_visibility_packed+opening_visibility_offsets[frame],visibility,opening_visibility_sizes[frame],0))return 1;
 cell_next=0;
 /* Scene script $0940E4 binds the cockpit at source frame159;
  * $051ED6 hides it at183. Its three banks occupy unused slots13..15. */
 unsigned background=frame>=80&&frame<92;
 page^=1;pixels=(uint8_t*)(0x06000000+page*0xa000);
 if(background){dma(background_pixels,(uintptr_t)pixels,38400);}
 else{uint32_t *clear=(uint32_t*)pixels;for(unsigned i=0;i<38400/4;i++)clear[i]=0;}
 for(unsigned i=0;i<f->count;i++)draw(&draws[f->first+i]);
 /* Publish the hidden page during VBlank, including the current VBlank
  * when enough scanlines remain for palette DMA and audio interrupts. */
 if(R16(0x04000006)>200)while(R16(0x04000006)>=160){}
 while(R16(0x04000006)<160){}
 for(unsigned i=0;i<f->pal_count;i++)dma(palettes[palrefs[f->pal_first+i]],0x05000000+i*32,32);
 if(background)for(unsigned i=0;i<48;i++)((volatile uint16_t*)0x05000000)[208+i]=opening_background_palette[i];
 /* Original backdrop palette FF entry F follows the global flash. */
 R16(0x05000000)=opening_backdrops[frame];R16(0x04000000)=0x404|(page<<4);return 0;
}
