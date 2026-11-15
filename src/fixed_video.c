#include "fixed_video.h"
#include "../generated/tile-video-data.h"
static int fail(struct NativeFixedFrame*f,unsigned code){if(f){f->fault=code;f->ready=0;}return 0;}
int native_fixed_layout_valid(const struct NativeFixedLayout*l){
 if(!l||l->char_block>3||l->screen_block>31||!l->tile_budget||l->tile_budget>NATIVE_FIXED_TILE_LIMIT||l->first_tile+l->tile_budget>1024)return 0;
 unsigned first=l->char_block*16384u+l->first_tile*64u,last=first+l->tile_budget*64u;
 unsigned map=l->screen_block*2048u;
 return last<=65536&&(last<=map||first>=map+2048);
}
static uint16_t intern(struct NativeFixedFrame*f,const uint32_t*tile){
 uint32_t hash=2166136261u;
 for(unsigned j=0;j<16;j++)hash=(hash^tile[j])*16777619u;
 unsigned h=hash&1023;
 for(unsigned n=0;n<1024;n++,h=(h+1)&1023){
  unsigned i=f->hash[h];
  if(i){i--;unsigned j;for(j=0;j<16&&f->tiles[i][j]==tile[j];j++){}if(j==16)return i;continue;}
  if(f->tile_count==f->layout.tile_budget){fail(f,NFIX_TILES);return 65535;}
  i=f->tile_count++;for(unsigned j=0;j<16;j++)f->tiles[i][j]=tile[j];f->hash[h]=i+1;return i;
 }
 fail(f,NFIX_TILES);return 65535;
}
struct Colors {uint8_t index[16][16];uint16_t mapped[16];};
static int microtile(uint16_t pattern,unsigned bank,struct Colors*cache,const struct NativePaletteWorld*p,struct NativeVideoPalette*shared,uint16_t*out){
 const uint16_t*colors=0;unsigned indexes[4];
 for(unsigned i=0;i<4;i++){
  unsigned nibble=(pattern>>(4*i))&15;
  if(!nibble){indexes[i]=0;continue;}
  if(!(cache->mapped[bank]&(1u<<nibble))){
   if(!colors)colors=native_palette_slot(p,bank);if(!colors)return 0;
   unsigned index=native_video_palette_color(shared,colors[nibble]);if(index==65535)return 0;
   cache->index[bank][nibble]=index;cache->mapped[bank]|=1u<<nibble;
  }
  indexes[i]=cache->index[bank][nibble];
 }
 out[0]=indexes[0]|(indexes[1]<<8);out[1]=indexes[2]|(indexes[3]<<8);return 1;
}
int native_fixed_prepare(struct NativeFixedFrame*f,const struct NativeTileVideo*v,const struct NativePaletteWorld*p,struct NativeVideoPalette*colors,const struct NativeFixedLayout*l){
 if(!f)return 0;
 if(!v||v->fault||!p||p->fault||p->world!=v->world||!colors||colors->fault||colors->ready||!colors->count)return fail(f,NFIX_STATE);
 if(!native_fixed_layout_valid(l))return fail(f,NFIX_LAYOUT);
 /* Rebuild only a bounded600-tile FIX surface. Deduplication makes blank
  * and repeated glyph fragments consume VRAM once. Pixel packing stays
  * outside VBlank; commit is only DMA and register writes. */
 f->fault=f->ready=0;f->tile_count=0;f->layout=*l;
 for(unsigned i=0;i<1024;i++)f->hash[i]=0;
 uint32_t tile[16]={0};if(intern(f,tile)!=0)return fail(f,NFIX_METADATA);
 for(unsigned i=0;i<1024;i++)f->map[i]=l->first_tile;
 struct Colors cache={0};
 for(unsigned ty=0;ty<20;ty++)for(unsigned tx=0;tx<30;tx++){
  /* Visible source raster starts at Y16.75%=12, then crop4 gives an
   * offset16. A2x2 fragment therefore starts at source micro-row8. */
  uint16_t*half=(uint16_t*)tile;
  for(unsigned by=0;by<4;by++){
   unsigned sy=ty*4+by+8,row=sy/3,ry=sy%3;
   for(unsigned bx=0;bx<4;bx++){
    unsigned sx=tx*4+bx,column=sx/3,rx=sx%3;
    uint16_t word=v->fixed[column][row];
    uint16_t pattern=native_fix_fragments[word&4095][ry*3+rx],pair[2];
    if(!microtile(pattern,word>>12,&cache,p,colors,pair))return fail(f,NFIX_PALETTE);
    half[by*8+bx]=pair[0];half[by*8+bx+4]=pair[1];
   }
  }
  unsigned index=intern(f,tile);if(index==65535)return 0;
  f->map[ty*32+tx]=l->first_tile+index;
 }
 f->ready=1;return 1;
}
