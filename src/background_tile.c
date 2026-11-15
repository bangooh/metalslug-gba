#include "background_tile.h"
#include "../generated/tile-video-data.h"
__attribute__((section(".iwram.background"),noinline,long_call)) uint32_t native_background_key(const struct NativeTileCell*c,uint8_t frame,int enabled,unsigned x,unsigned y){
 if(!c||x>=3||y>=3)return UINT32_MAX;
 unsigned a=c->attributes,bank=(a>>4)&15,tile=c->tile;
 if(bank>1)return UINT32_MAX;
 if(enabled){if(a&8)tile=(tile&~7u)|(frame&7);else if(a&4)tile=(tile&~3u)|(frame&3);}
 unsigned index=native_bg_lookup[(bank<<16)|tile];if(index>=NATIVE_BG_TILE_COUNT)return UINT32_MAX;
 unsigned flip=a&3;if(flip&1)x=2-x;if(flip&2)y=2-y;
 unsigned block=native_bg_fragments[index][y*3+x];if(block>=NATIVE_BG_BLOCK_COUNT)return UINT32_MAX;
 return block<<10|(a>>8)<<2|flip;
}
int native_background_block(const struct NativeTileCell*c,uint8_t frame,int enabled,unsigned x,unsigned y,uint8_t out[8]){
 if(!out)return 0;uint32_t key=native_background_key(c,frame,enabled,x,y);if(key==UINT32_MAX)return 0;
 unsigned block=key>>10,flip=key&3;
 const uint8_t*p=native_bg_blocks[block];
 for(unsigned y=0;y<4;y++){unsigned sy=flip&2?3-y:y,word=p[sy*2]|(unsigned)p[sy*2+1]<<8;if(flip&1)word=((word&15)<<12)|((word&240)<<4)|((word&3840)>>4)|((word>>12)&15);out[y*2]=word;out[y*2+1]=word>>8;}
 return 1;
}
