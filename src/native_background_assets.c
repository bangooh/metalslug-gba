#include "native_background_assets.h"
#include "../generated/native-bg-assets.h"
#include "../generated/tile-video-data.h"
extern const uint16_t native_bg_base_opacity[];
volatile uint32_t native_bg_boundary_failure[4];
unsigned native_bg_profile(const struct NativePaletteWorld*p){
 unsigned best=0,best_score=0;
 for(unsigned i=0;i<NATIVE_BG_PROFILE_COUNT;i++){
  unsigned score=0;
  for(unsigned slot=16;slot<118;slot++){
   const struct NativePaletteState*s=&p->slots[slot];
   if(s->valid&&!(s->flags&(64|128))&&native_bg_profiles[i][slot]==s->source)score+=slot<20?16:1;
  }
  if(score>best_score){best=i;best_score=score;}
 }
 return best;
}
__attribute__((section(".iwram.native_assets"),noinline,long_call)) void native_bg_load(unsigned id,uint32_t out[9]){
 const uint8_t*base=native_bg_tile_pages+native_bg_tile_offsets[id>>15];
 const uint16_t*r=native_bg_tile_references[id];
 const uint16_t*a=(const uint16_t*)(base+r[0]*8),*b=(const uint16_t*)(base+r[1]*8);
 const uint16_t*c=(const uint16_t*)(base+r[2]*8),*d=(const uint16_t*)(base+r[3]*8);
 for(unsigned y=0;y<4;y++){out[y]=a[y]|((uint32_t)b[y]<<16);out[y+4]=c[y]|((uint32_t)d[y]<<16);}
 out[8]=native_bg_native_banks[id];
}
__attribute__((section(".iwram.native_assets"),noinline,long_call)) unsigned native_bg_region_map(unsigned region,unsigned profile,unsigned x,unsigned y){
 if(region>=NATIVE_BG_REGION_COUNT)return 0xffffffffu;
 const struct NativeBgRegion*r=&native_bg_regions[region];unsigned width=r->width*3-1,height=r->height*3-1;
 if(x>=width||y>=height)return 0xffffffffu;
 for(unsigned i=0;i<r->variants;i++){
  const struct NativeBgVariant*v=&native_bg_variants[r->map_first+i];if(v->profile!=65535&&v->profile!=profile)continue;
  unsigned at=v->offset+y*width+x,page=native_bg_map_pages[at>>3];
  const uint8_t*p=native_bg_map_data+page*24+(at&7)*3;return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16;
 }
 native_bg_boundary_failure[0]=3;native_bg_boundary_failure[1]=region;native_bg_boundary_failure[2]=profile;native_bg_boundary_failure[3]=x|(y<<16);return 0xffffffffu;
}
unsigned native_bg_opacity_mask(unsigned block,unsigned flip){
 unsigned m=native_bg_base_opacity[block];
 if(flip&1){m=((m&0x5555)<<1)|((m>>1)&0x5555);m=((m&0x3333)<<2)|((m>>2)&0x3333);}
 if(flip&2)m=((m&15)<<12)|((m&240)<<4)|((m&3840)>>4)|((m&61440)>>12);
 return m;
}
static __attribute__((section(".iwram.native_assets"),noipa,long_call)) int boundary_common(const uint32_t keys[4],const struct NativePaletteWorld*p,unsigned profile,uint32_t out[9]){
 (void)p;(void)profile;unsigned sources[4],scores[12]={0};
 for(unsigned j=0;j<4;j++){
  if(keys[j]==NATIVE_BG_EMPTY_KEY){sources[j]=65535;continue;}
  unsigned slot=(keys[j]>>2)&255;sources[j]=slot;
  for(unsigned bank=0;bank<12;bank++)scores[bank]+=native_bg_native_cost[slot][bank];
 }
 unsigned bank=0;for(unsigned i=1;i<12;i++)if(scores[i]<scores[bank])bank=i;
 for(unsigned y=0;y<8;y++)out[y]=0;out[8]=bank;
 for(unsigned j=0;j<4;j++)if(sources[j]!=65535){
  unsigned key=keys[j],flip=key&3;const uint8_t*data=native_bg_blocks[key>>10];
  const uint8_t*map=native_bg_native_remap[sources[j]][bank];
  for(unsigned y=0;y<4;y++){
   unsigned sy=flip&2?3-y:y,word=data[sy*2]|(unsigned)data[sy*2+1]<<8,value=0;
   for(unsigned x=0;x<4;x++){unsigned sx=flip&1?3-x:x;unsigned index=(word>>(sx*4))&15;value|=(unsigned)((map[index>>1]>>((index&1)*4))&15)<<(x*4);}
   out[(j>>1)*4+y]|=value<<((j&1)*16);
  }
 }
 return 1;
}
/* The ending starfield uses only source slot22, so its fifteen original
 * colours fit one exact4bpp bank. Keep this cold specialization in ROM;
 * the shared boundary compositor retains its existing IWRAM allocation. */
static __attribute__((noinline)) int starfield_boundary(const uint32_t keys[4],uint32_t out[9]){
 unsigned present=0;
 for(unsigned j=0;j<4;j++)if(keys[j]!=NATIVE_BG_EMPTY_KEY){
  if(((keys[j]>>2)&255)!=22)return 0;present=1;
 }
 if(!present)return 0;
 for(unsigned y=0;y<8;y++)out[y]=0;out[8]=11;
 for(unsigned j=0;j<4;j++)if(keys[j]!=NATIVE_BG_EMPTY_KEY){
  unsigned key=keys[j],flip=key&3;const uint8_t*data=native_bg_blocks[key>>10];
  for(unsigned y=0;y<4;y++){
   unsigned sy=flip&2?3-y:y,word=data[sy*2]|(unsigned)data[sy*2+1]<<8;
   if(flip&1)word=((word&15)<<12)|((word&240)<<4)|((word&3840)>>4)|((word>>12)&15);
   out[(j>>1)*4+y]|=word<<((j&1)*16);
  }
 }
 return 1;
}
/* Ending town bypasses the lossy prebuilt indices, using original fragments.
 * Tables are ROM-only; bit7 belongs only to the ending renderer profile. */
#include "../generated/ending-runtime-remap.inc"
static int ending_original_boundary(const uint32_t keys[4],unsigned profile,uint32_t out[9]){
 unsigned group=profile==0?0:profile==1?1:profile==4?2:profile==5?3:4;
 unsigned scores[12]={0},records[4]={0},used[4]={0};
 for(unsigned j=0;j<4;j++)if(keys[j]!=NATIVE_BG_EMPTY_KEY){
  unsigned slot=(keys[j]>>2)&255;if(slot>=118)return 0;
  unsigned id=ending_remap_ids[group][slot];records[j]=id;
  for(unsigned b=0;b<12;b++)scores[b]+=ending_costs[id][b];
  if(profile==12){const uint8_t*data=native_bg_blocks[keys[j]>>10];
   for(unsigned k=0;k<8;k++){unsigned a=data[k]&15,b=data[k]>>4;if(a)used[j]|=1u<<a;if(b)used[j]|=1u<<b;}
  }
 }
 unsigned bank=0;for(unsigned b=1;b<12;b++)if(scores[b]<scores[bank])bank=b;
 if(profile==12)for(unsigned b=0;b<12;b++){
  unsigned fits=1;for(unsigned j=0;j<4;j++)if(keys[j]!=NATIVE_BG_EMPTY_KEY){unsigned slot=(keys[j]>>2)&255;if((ending_exact_masks[slot][b]&used[j])!=used[j])fits=0;}
  if(fits){bank=b;break;}
 }
 for(unsigned y=0;y<8;y++)out[y]=0;out[8]=bank;
 for(unsigned j=0;j<4;j++)if(keys[j]!=NATIVE_BG_EMPTY_KEY){
  unsigned key=keys[j],flip=key&3;
  const uint8_t*data=native_bg_blocks[key>>10];const uint8_t*map=ending_remaps[records[j]][bank];
  for(unsigned y=0;y<4;y++){
   unsigned sy=flip&2?3-y:y,word=data[sy*2]|(unsigned)data[sy*2+1]<<8,value=0;
   for(unsigned x=0;x<4;x++){unsigned sx=flip&1?3-x:x,index=(word>>(sx*4))&15;value|=(unsigned)((map[index>>1]>>((index&1)*4))&15)<<(x*4);}
   out[(j>>1)*4+y]|=value<<((j&1)*16);
  }
 }
 return 1;
}
int native_bg_boundary(const uint32_t keys[4],const struct NativePaletteWorld*p,unsigned profile,uint32_t out[9]){
 if(profile&128)return ending_original_boundary(keys,profile&127,out);
 (void)p;
 if(profile==12&&starfield_boundary(keys,out))return 1;
 int result=boundary_common(keys,p,profile,out);
 /* Before reserving bank11, both banks0/11 were entirely RGB0 in profile12.
  * Retain the old opaque-black RGB and alpha for companion-plane edges. */
 if(profile==12&&out[8]==11)out[8]=0;
 return result;
}
