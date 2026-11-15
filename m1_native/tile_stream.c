#include "tile_stream.h"
#ifdef __arm__
#define M1HASH __attribute__((section(".m1hot"),noinline,long_call))
#else
#define M1HASH
#endif
static unsigned home(unsigned id){return (id*2654435761u)>>21;}
M1HASH unsigned m1_stream_find(const struct M1TileStream*s,unsigned id){
 unsigned at=home(id);while(s->hash[at]){unsigned e=s->hash[at];if((e>>16)==id+1)return e&65535;at=(at+1)&2047;}return 65535;
}
static M1HASH void insert(struct M1TileStream*s,unsigned id,unsigned slot){unsigned at=home(id);while(s->hash[at])at=(at+1)&2047;s->hash[at]=((id+1)<<16)|slot;}
static M1HASH void erase(struct M1TileStream*s,unsigned id){
 unsigned hole=home(id);while((s->hash[hole]>>16)!=id+1)hole=(hole+1)&2047;
 unsigned at=(hole+1)&2047;while(s->hash[at]){unsigned h=home((s->hash[at]>>16)-1);if(((at-h)&2047)>=((at-hole)&2047)){s->hash[hole]=s->hash[at];hole=at;}at=(at+1)&2047;}s->hash[hole]=0;
}
int m1_stream_init(struct M1TileStream*s,unsigned capacity){
 if(!s||capacity<2||capacity>M1_VRAM_TILES)return 0;
 for(unsigned i=0;i<M1_HASH_SIZE;i++)s->hash[i]=0;
 for(unsigned i=0;i<M1_VRAM_TILES;i++){s->owner[i]=65535;s->pinned[i]=0;}
 insert(s,0,0);s->owner[0]=0;s->capacity=capacity;s->upload_count=0;s->requested_dirty=1;return 1;
}
static M1HASH int finish_plan(struct M1TileStream*s,unsigned needed){
 if(!needed){s->upload_count=0;return 1;}
 unsigned free_slots=0;for(unsigned i=1;i<s->capacity;i++)free_slots+=!(s->pinned[i]&1);
 if(needed>free_slots)return 0;
 s->upload_count=needed;unsigned cursor=1,reuse=1;
 for(unsigned i=0;i<needed;i++){
  unsigned id=s->uploads[i].rom_tile;
  /* Prefer slots absent from the displayed frame, allowing safe transfer
   * during visible scanlines. Bit1 retains the previous frame's pin. */
  while(cursor<s->capacity&&s->pinned[cursor])cursor++;
  unsigned slot;
  if(cursor<s->capacity)slot=cursor++;
  else{while(s->pinned[reuse]&1)reuse++;slot=reuse++;}
  unsigned old=s->owner[slot];
  if(old!=65535)erase(s,old);
  s->owner[slot]=id;insert(s,id,slot);s->pinned[slot]|=1;s->uploads[i].vram_tile=slot;
 }
 return 1;
}

int m1_stream_plan(struct M1TileStream*s,const uint16_t*refs,unsigned count){
 if(!s||!refs||count>M1_MAX_REFS||s->capacity<2||s->capacity>M1_VRAM_TILES)return 0;
 for(unsigned i=0;i<count;i++)if(refs[i]>=M1_ROM_TILES)return 0;
 for(unsigned i=0;i<M1_ROM_TILES/32;i++)((uint32_t*)s->requested)[i]=0;
 for(unsigned i=0;i<M1_VRAM_TILES/4;i++)((uint32_t*)s->pinned)[i]=(((uint32_t*)s->pinned)[i]&0x01010101u)<<1;
 s->requested_dirty=1;s->pinned[0]=1;unsigned needed=0;
 for(unsigned i=0;i<count;i++){
  unsigned id=refs[i];if(s->requested[id>>3]&(1u<<(id&7)))continue;
  s->requested[id>>3]|=1u<<(id&7);unsigned slot=m1_stream_find(s,id);
  if(slot!=65535)s->pinned[slot]|=1;
  else {if(needed>=s->capacity)return 0;s->uploads[needed++].rom_tile=id;
  }
 }
 return finish_plan(s,needed);
}

M1HASH int m1_stream_plan_unique(struct M1TileStream*s,unsigned count){
 if(!s||s->capacity<2||s->capacity>M1_VRAM_TILES||count>=s->capacity)return 0;
 for(unsigned i=0;i<M1_VRAM_TILES/4;i++)((uint32_t*)s->pinned)[i]=(((uint32_t*)s->pinned)[i]&0x01010101u)<<1;
 s->pinned[0]=1;unsigned needed=0;
 for(unsigned i=0;i<count;i++){
  unsigned id=s->uploads[i].rom_tile;if(!id||id>=M1_ROM_TILES)return 0;
  unsigned slot=m1_stream_find(s,id);
  if(slot!=65535)s->pinned[slot]|=1;else s->uploads[needed++].rom_tile=id;
 }
 return finish_plan(s,needed);
}
