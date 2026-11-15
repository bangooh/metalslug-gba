#include "object_video.h"
#include "video_palette.h"
#include <string.h>
/* DMA3 is synchronous. The hardware main must serialize all DMA users and
 * budget aggregate BG/FIX/OBJ transfers, not just the OBJ staging tile cap. */
/* Persistent physical VRAM records survive frames in which a drawing is absent. */
static struct NativeObjectUpload previous[NATIVE_OBJ_LIMIT];
static uint32_t previous_generation[NATIVE_OBJ_LIMIT];
static unsigned previous_victim;static uint16_t palette_dirty;
static uint32_t upload_dirty[4];
static uint8_t tile_owner[1024];
static uint16_t physical_tile[1024];
static const struct NativeObjectFrame*previous_owner;
static int immutable(const void*p){uintptr_t a=(uintptr_t)p;return a>=0x08000000u&&a<0x0a000000u;}
static unsigned home(const struct NativeObjectUpload*u){unsigned key=(uintptr_t)u->pixels;return ((key>>5)^(key>>13)^u->count)&31;}
static int find(const struct NativeObjectUpload*u,unsigned generation){
 int rom=immutable(u->pixels);if(!rom&&!generation)return -1;
 unsigned base=home(u)*4;
 for(unsigned j=base;j<base+4;j++)if(previous[j].pixels==u->pixels&&previous[j].count==u->count&&(rom||previous_generation[j]==generation))return (int)j;
 return -1;
}
void native_object_resident_reset(void){
 for(unsigned i=0;i<128;i++)previous[i].pixels=0;
 for(unsigned i=0;i<1024;i++)tile_owner[i]=0;
 previous_victim=0;previous_owner=0;
}
static int available(const uint32_t used[32],unsigned start,unsigned count){
 while(count){unsigned at=start&31,n=32-at;if(n>count)n=count;uint32_t mask=n==32?0xffffffffu:((1u<<n)-1)<<at;if(used[start>>5]&mask)return 0;start+=n;count-=n;}return 1;
}
/* Lowest aligned free extent, searched a word at a time. */
static unsigned first_free(const uint32_t used[32],unsigned count,unsigned budget){
 if(count&&!(count&(count-1))&&count<=32){
  static const uint32_t masks[]={0xffffffffu,0x55555555u,0x11111111u,0x01010101u,0x00010001u,1};
  unsigned log=0;while((1u<<log)<count)log++;
  for(unsigned word=0;word*32+count<=budget;word++){
   uint32_t occupied=used[word];for(unsigned shift=1;shift<count;shift*=2)occupied|=occupied>>shift;
   uint32_t candidates=~occupied&masks[log];
   if(candidates){unsigned at=word*32+__builtin_ctz(candidates);return at+count<=budget?at:budget;}
  }
  return budget;
 }
 unsigned stride=(count&&!(count&(count-1)))?count:1;
 for(unsigned at=0;at+count<=budget;at+=stride)if(available(used,at,count))return at;
 return budget;
}
static void reserve(uint32_t used[32],unsigned start,unsigned count){
 while(count){unsigned at=start&31,n=32-at;if(n>count)n=count;used[start>>5]|=n==32?0xffffffffu:((1u<<n)-1)<<at;start+=n;count-=n;}
}
static __attribute__((section(".iwram.object_residency"),noinline,long_call)) void remove_entry(unsigned id){
 struct NativeObjectUpload*u=&previous[id];
 /* Records own disjoint whole extents: record() invalidates every
  * overlapping record before publishing a new extent. */
 memset(tile_owner+u->tile,0,u->count);
 u->pixels=0;
}
static void record(const struct NativeObjectUpload*u,unsigned generation){
 for(unsigned tile=u->tile;tile<u->tile+u->count;tile++)if(tile_owner[tile])remove_entry(tile_owner[tile]-1);
 if(!immutable(u->pixels)&&!generation)return;
 unsigned base=home(u)*4,id;
 for(id=base;id<base+4;id++)if(!previous[id].pixels)break;
 if(id==base+4){id=base+(previous_victim++&3);remove_entry(id);}
 previous[id]=*u;previous_generation[id]=generation;memset(tile_owner+u->tile,id+1,u->count);
}
static __attribute__((noinline)) int pin_mutable(const struct NativeObjectFrame*f,uint32_t used[32],uint16_t positions[128]){for(unsigned i=0;i<f->upload_count;i++){const struct NativeObjectUpload*u=&f->uploads[i];positions[i]=65535;if(!immutable(u->pixels)){if(u->tile+u->count>f->tile_budget||!available(used,u->tile,u->count))return 0;int found=find(u,f->mutable_generation);positions[i]=u->tile|((found>=0&&previous[found].tile==u->tile)?0x8000:0);reserve(used,u->tile,u->count);}}return 1;}
__attribute__((section(".iwram.object_residency"),noinline,long_call)) int native_object_resident_stage(struct NativeObjectFrame*f){
 if(!f||f->fault||!f->ready)return 0;
 if(previous_owner!=f){native_object_resident_reset();previous_owner=f;}
 uint32_t used[32]={0};uint16_t positions[128];uint8_t pending[128];
 /* Pin mutable staging (the HUD prefix) before retaining actor tiles. */
 if(!pin_mutable(f,used,positions))return 0;
 for(unsigned i=0;i<f->upload_count;i++){
  const struct NativeObjectUpload*u=&f->uploads[i];if(positions[i]!=65535)continue;int found=find(u,f->mutable_generation);
  if(found>=0){const struct NativeObjectUpload*p=&previous[found];if(p->tile+u->count<=f->tile_budget&&available(used,p->tile,u->count)){positions[i]=p->tile|0x8000;reserve(used,p->tile,u->count);}}
 }
 int compact=0;
 for(unsigned i=0;i<f->upload_count;i++)if(positions[i]==65535){
  unsigned n=f->uploads[i].count,at=f->uploads[i].tile;
  if(at+n<=f->tile_budget&&available(used,at,n)){positions[i]=at;reserve(used,at,n);continue;}
  at=0;
  /* GBA OBJ chunks have power-of-two tile extents. Aligned allocation
   * bounds the search; an atomic compact layout handles fragmentation. */
  at=first_free(used,n,f->tile_budget);
  if(at+n>f->tile_budget){compact=1;break;}
  positions[i]=at;reserve(used,at,n);
 }
 if(compact)for(unsigned i=0;i<f->upload_count;i++){unsigned tile=f->uploads[i].tile;positions[i]=(positions[i]==(tile|0x8000))?tile|0x8000:tile;}
 unsigned moved=0;
 for(unsigned i=0;i<f->upload_count;i++){pending[i]=!(positions[i]&0x8000);positions[i]&=1023;moved|=positions[i]!=f->uploads[i].tile;}
 if(moved){
  for(unsigned i=0;i<f->upload_count;i++){const struct NativeObjectUpload*u=&f->uploads[i];for(unsigned j=0;j<u->count;j++)physical_tile[u->tile+j]=positions[i]+j;}
  for(unsigned o=0;o<f->count;o++){unsigned tile=f->oam[o][2]&1023;if(tile>=f->tile_count)return 0;f->oam[o][2]=(f->oam[o][2]&0xfc00)|physical_tile[tile];}
  for(unsigned i=0;i<f->upload_count;i++)f->uploads[i].tile=positions[i];
  /* Transfers carry explicit VRAM destinations. Keep upload order rather
   * than insertion-sorting the entire resident list after every relocation.
   * Adjacent source/destination runs are still coalesced during commit. */
 }
 for(unsigned i=0;i<4;i++)upload_dirty[i]=0;
 for(unsigned i=0;i<f->upload_count;i++)if(pending[i]){upload_dirty[i>>5]|=1u<<(i&31);record(&f->uploads[i],f->mutable_generation);}
 return 1;
}
static void copy16(const void*source,uintptr_t destination,unsigned count){
 /* DMA count0 means65536, not an empty transfer (blank drawings are valid). */
 if(!count)return;
 if(count<=16){const uint16_t*s=source;volatile uint16_t*d=(volatile uint16_t*)destination;for(unsigned i=0;i<count;i++)d[i]=s[i];return;}
 volatile uint32_t*dma=(volatile uint32_t*)0x040000D4;
 dma[2]=0;dma[0]=(uintptr_t)source;dma[1]=destination;
 __asm__ volatile("":::"memory");dma[2]=0x80000000u|count;
 while(dma[2]&0x80000000u){}__asm__ volatile("":::"memory");
}
unsigned native_object_prestage(struct NativeObjectFrame*f,uintptr_t scratch,unsigned capacity,unsigned background_bytes){
 if(!f||!f->ready||f->fault||previous_owner!=f)return 0;
 unsigned total=background_bytes+1536,used=0;
 for(unsigned i=0;i<f->upload_count;i++)if(upload_dirty[i>>5]&(1u<<(i&31)))total+=f->uploads[i].count*32;
 /* The village destruction frame also pays for many disjoint BG/OBJ DMAs.
  * Measured 28 KiB aggregate transfers can overrun VBlank despite fitting
  * the old 32 KiB byte-only estimate. Reserve setup/IRQ time as well. */
 if(total<=24576)return 0;
 for(unsigned i=0;i<f->upload_count;i++){
  struct NativeObjectUpload*u=&f->uploads[i];unsigned bytes=u->count*32;
  if(!(upload_dirty[i>>5]&(1u<<(i&31)))||!immutable(u->pixels)||bytes>capacity-used)continue;
  copy16(u->pixels,scratch+used,bytes/2);u->pixels=(const uint8_t*)(scratch+used);used+=bytes;
 }
 return used;
}
int native_object_commit(struct NativeObjectFrame*f){
 if(!f||f->fault||!f->ready)return 0;
 volatile uint16_t*display=(volatile uint16_t*)0x04000000;
 volatile uint16_t*status=(volatile uint16_t*)0x04000004;
 uint16_t control=*display;
 if((control&7)>2||(!(control&0x80)&&!(*status&1)))return 0;
 if(previous_owner!=f)return 0;
 for(unsigned i=0;i<f->upload_count;){
  const struct NativeObjectUpload*u=&f->uploads[i];
  if(!(upload_dirty[i>>5]&(1u<<(i&31)))){i++;continue;}
  unsigned end=i+1,count=u->count;
  while(end<f->upload_count){const struct NativeObjectUpload*n=&f->uploads[end];if(!(upload_dirty[end>>5]&(1u<<(end&31)))||n->tile!=u->tile+count||n->pixels!=u->pixels+count*32)break;count+=n->count;end++;}
  copy16(u->pixels,0x06010000u+(unsigned)u->tile*32,count*16);
  i=end;
 }
 for(unsigned bank=0;bank<f->palette_count;bank++)if(palette_dirty&(1u<<bank))copy16(f->palettes[bank],0x05000200u+bank*32u,16);
 copy16(f->oam,0x07000000,NATIVE_OBJ_LIMIT*4);
 *display=control|0x1040;f->ready=0;return 1;
}

void native_object_stage_palette(struct NativeObjectFrame*f){palette_dirty=native_palette_dirty(&f->palettes[0][0],0x05000200,f->palette_count);}
