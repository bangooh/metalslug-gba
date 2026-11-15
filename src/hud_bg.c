#include "hud_bg.h"
#include "video_palette.h"
#include "dirty_span.h"
#include "play_world.h"
#include "scene_runtime.h"
#include "../generated/tile-video-data.h"
#ifdef NATIVE_PROFILE
volatile unsigned native_hud_statistics[5];
#define HUDSTAT(i) native_hud_statistics[i]++
#else
#define HUDSTAT(i) ((void)0)
#endif
volatile unsigned native_hud_failure[3];
static void reset_tiles(struct NativeHudFrame*h){
 HUDSTAT(2);
 for(unsigned i=0;i<NATIVE_HUD_TILES;i++){h->occupied[i]=0;h->references[i]=0;}h->occupied[0]=1;h->victim=1;for(unsigned i=0;i<512;i++)h->hash[i]=0;
 for(unsigned i=0;i<4;i++){h->counts[i]=0;for(unsigned j=0;j<16;j++)h->palettes[i][j]=0;}
 for(unsigned i=0;i<1024;i++)h->map[i]=0;
 for(unsigned i=0;i<8;i++)h->pixels[0][i]=0;for(unsigned i=0;i<NATIVE_HUD_DIRTY_WORDS;i++)h->dirty[i]=0;h->dirty[0]=1;
 for(unsigned y=0;y<20;y++)h->rows[y]=0x3fffffffu;h->map_dirty=1;h->source_mask=0;for(unsigned i=0;i<16;i++)h->source_used[i]=0;for(unsigned i=0;i<16;i++)for(unsigned j=0;j<16;j++)h->source_targets[i][j][0]=h->source_targets[i][j][1]=0;
}
static void changed(struct NativeHudFrame*h,unsigned x,unsigned y,int tutorial){
 unsigned left=x*6,right=left+5;int top=(int)y*6-16,bottom=top+5;
 if(bottom<0||top>=160)return;if(top<0)top=0;if(bottom>=160)bottom=159;
 unsigned bits=((1u<<(right/8-left/8+1))-1)<<(left/8);
 for(unsigned row=(unsigned)top/8;row<=(unsigned)bottom/8;row++){
  unsigned target=tutorial&&row>=2&&row<=7?(row==2?7:row-1):row;
  h->rows[target]|=bits;
 }
}
static unsigned glyph_hash(const uint16_t keys[4],unsigned phase){
 uint32_t h=phase;for(unsigned i=0;i<4;i++)h=(h^keys[i])*16777619u;return (h^(h>>16))&511;
}
static void erase(struct NativeHudFrame*h,unsigned hole){
 unsigned at=(hole+1)&511;
 while(h->hash[at]){unsigned id=h->hash[at],home=glyph_hash(h->cache_keys[id],h->cache_phase[id]);if(((at-home)&511)>=((at-hole)&511)){h->hash[hole]=id;h->hash_slot[id]=hole;hole=at;}at=(at+1)&511;}h->hash[hole]=0;
}
/* Offline opacity of approved FIX glyphs; transparent cells need no
 * palette search, fragment assembly or cache lookup. */
static const uint32_t fix_visible[128]={
#include "../generated/native-fix-visible.inc"
};
static int cell_empty(const struct NativeTileVideo*v,unsigned tx,unsigned ty){
 unsigned x0=tx*8/6,x1=(tx*8+7)/6,y0=(ty*8+16)/6,y1=(ty*8+23)/6;
 for(unsigned x=x0;x<=x1;x++)for(unsigned y=y0;y<=y1;y++){
  unsigned id=v->fixed[x][y]&4095;
  if(fix_visible[id>>5]&(1u<<(id&31)))return 0;
 }
 return 1;
}
static __attribute__((noinline)) int render_cell(struct NativeHudFrame*h,const struct NativeTileVideo*v,const struct NativePaletteWorld*p,unsigned tx,unsigned ty,unsigned source_y){
 unsigned cell=ty*32+tx,old=h->map[cell],old_id=old&1023;
 if(old_id)h->references[old_id]--;
 if(cell_empty(v,tx,source_y)){if(old){h->map[cell]=0;h->map_dirty=1;}return 1;}
 unsigned gx0=tx*8/6,gy0=(source_y*8+16)/6,phase=(tx%3)+3*(source_y%3);
 uint16_t keys[4]={v->fixed[gx0][gy0],v->fixed[gx0+1][gy0],v->fixed[gx0][gy0+1],v->fixed[gx0+1][gy0+1]};
 unsigned home=glyph_hash(keys,phase),slot=home;
 while(h->hash[slot]){
  unsigned id=h->hash[slot],i=0;
  if(h->cache_phase[id]==phase){for(;i<4&&h->cache_keys[id][i]==keys[i];i++){}if(i==4){HUDSTAT(3);h->references[id]++;unsigned entry=id|((12+h->cache_bank[id])<<12);if(old!=entry){h->map[cell]=entry;h->map_dirty=1;}return 1;}}
  slot=(slot+1)&511;
 }
 HUDSTAT(4);
 uint16_t colors[15];uint8_t indices[64]={0};uint16_t source_keys[64];unsigned count=0;
 for(unsigned by=0;by<4;by++)for(unsigned bx=0;bx<4;bx++){
  unsigned mx=tx*4+bx,my=source_y*4+by+8,gx=(mx*171)>>9,gy=(my*171)>>9;
  unsigned word=v->fixed[gx][gy],bank=word>>12;
  uint16_t pattern=native_fix_fragments[word&4095][(my-gy*3)*3+mx-gx*3];if(!pattern)continue;
  const uint16_t*palette=native_palette_slot(p,bank);if(!palette)return 0;
  if(!(h->source_mask&(1u<<bank))){for(unsigned j=0;j<16;j++)h->source_colors[bank][j]=palette[j];h->source_mask|=1u<<bank;}
  for(unsigned j=0;j<4;j++){
   unsigned value=(pattern>>(j*4))&15;if(!value)continue;h->source_used[bank]|=1u<<value;unsigned c=palette[value],at;
   for(at=0;at<count&&colors[at]!=c;at++){}
   if(at==count){if(count==15){native_hud_failure[0]=1;native_hud_failure[1]=tx;native_hud_failure[2]=ty;return 0;}colors[count++]=c;}
   unsigned pixel=(by*2+(j>>1))*8+bx*2+(j&1);indices[pixel]=at+1;source_keys[pixel]=bank*16+value;
  }
 }
 unsigned id=0;
 if(!count){if(old){h->map[cell]=0;h->map_dirty=1;}return 1;}
 unsigned best=4,score=16;
 for(unsigned bank=0;bank<4;bank++){
  unsigned missing=0;for(unsigned i=0;i<count;i++){unsigned j;for(j=1;j<=h->counts[bank]&&h->palettes[bank][j]!=colors[i];j++){}missing+=j>h->counts[bank];}
  if(h->counts[bank]+missing<=15&&missing<score){best=bank;score=missing;if(!missing)break;}
 }
 if(best==4){native_hud_failure[0]=2;native_hud_failure[1]=tx;native_hud_failure[2]=ty;return 0;}uint8_t remap[16]={0};
 for(unsigned i=0;i<count;i++){
  unsigned j;for(j=1;j<=h->counts[best]&&h->palettes[best][j]!=colors[i];j++){}
  if(j>h->counts[best]){h->counts[best]++;h->palettes[best][j]=colors[i];}remap[i+1]=j;
 }
 for(unsigned pixel=0;pixel<64;pixel++)if(indices[pixel]){unsigned key=source_keys[pixel],target=best*16+remap[indices[pixel]];h->source_targets[key>>4][key&15][target>>5]|=1u<<(target&31);}
 unsigned tries;
 for(tries=0;tries<h->tile_limit-1u;tries++){id=h->victim++;if(h->victim==h->tile_limit)h->victim=1;if(!h->references[id])break;}
 if(tries==h->tile_limit-1u){native_hud_failure[0]=3;native_hud_failure[1]=tx;native_hud_failure[2]=ty;return 0;}
 if(h->occupied[id])erase(h,h->hash_slot[id]);
 slot=home;while(h->hash[slot])slot=(slot+1)&511;h->hash[slot]=id;h->hash_slot[id]=slot;
 for(unsigned i=0;i<4;i++)h->cache_keys[id][i]=keys[i];h->cache_phase[id]=phase;h->cache_bank[id]=best;h->references[id]=1;h->occupied[id]=1;
 uint8_t*dst=(uint8_t*)h->pixels[id];
 for(unsigned i=0;i<32;i++)dst[i]=remap[indices[i*2]]|(remap[indices[i*2+1]]<<4);
 /* A new cache allocation must reach VRAM even when its bytes match old
  * scratch contents. Unoccupied slots are not uploaded during a reset. */
 h->dirty[id>>5]|=1u<<(id&31);
 unsigned entry=id|((12+best)<<12);if(h->map[cell]!=entry){h->map[cell]=entry;h->map_dirty=1;}
 return 1;
}
/* Retain tile indices through palette fades when every shared RGB still
 * agrees. Conflicting aliases trigger the exact full rebuild below. */
static __attribute__((noinline)) int update_palette(struct NativeHudFrame*h,const struct NativePaletteWorld*p){
 unsigned changed=0;
 for(unsigned slot=0;slot<16;slot++)if(h->source_mask&(1u<<slot)){const uint16_t*colors=native_palette_slot(p,slot);if(!colors)return 0;for(unsigned i=1;i<16;i++)if((h->source_used[slot]&(1u<<i))&&colors[i]!=h->source_colors[slot][i]){changed=1;break;}}
 if(!changed)return 1;
 uint16_t next[4][16],seen[4]={0};
 for(unsigned b=0;b<4;b++)for(unsigned i=0;i<16;i++)next[b][i]=h->palettes[b][i];
 for(unsigned slot=0;slot<16;slot++)if(h->source_mask&(1u<<slot)){
  const uint16_t*colors=native_palette_slot(p,slot);if(!colors)return 0;
  for(unsigned c=1;c<16;c++)if(h->source_used[slot]&(1u<<c)){
   unsigned color=colors[c];
   for(unsigned half=0;half<2;half++){uint32_t bits=h->source_targets[slot][c][half];while(bits){unsigned bit=__builtin_ctz(bits),target=half*32+bit,b=target>>4,i=target&15;bits&=bits-1;if((seen[b]&(1u<<i))&&next[b][i]!=color)return 0;seen[b]|=1u<<i;next[b][i]=color;}}
  }
 }
 for(unsigned b=0;b<4;b++)for(unsigned i=0;i<16;i++)h->palettes[b][i]=next[b][i];
 for(unsigned slot=0;slot<16;slot++)if(h->source_mask&(1u<<slot)){const uint16_t*colors=native_palette_slot(p,slot);for(unsigned c=0;c<16;c++)h->source_colors[slot][c]=colors[c];}
 return 1;
}
/* Reset/allocation bookkeeping is cold; retain the existing IWRAM budget
 * for the preparation loop and the measured main/IRQ stack reservations. */
static __attribute__((noinline)) int finish_prepare(struct NativeHudFrame*h,int full){
 if(full||h->tile_span!=h->tile_limit){
  h->tile_span=h->tile_limit;h->map_dirty=1;
  /* Only allocated tiles can be named by this map. The first ending
   * frame formerly copied all320slots, including unused scratch bytes,
   * and overran VBlank when combined with the new background. */
  for(unsigned i=0;i<NATIVE_HUD_TILES;i++)if(h->occupied[i])h->dirty[i>>5]|=1u<<(i&31);
 }
 h->valid=1;return 1;
}
/* v170: ROM. BG HUD is unused while the always-OBJ HUD is active in
 * missions; IWRAM is needed for the hot object palette path. */
__attribute__((noinline)) int native_hud_prepare_limit(struct NativeHudFrame*h,const struct NativeTileVideo*v,const struct NativePaletteWorld*p,int retained,unsigned limit){
 HUDSTAT(0);
 if(!h||!v||!p||v->fault||p->fault)return 0;
 if(!limit||limit>NATIVE_HUD_TILES)return 0;
 /* The tutorial's FIX explanation occupies y=24..63; y=16..23 is blank.
  * Rotate this six-row band up one hardware tile (8 GBA pixels). Keep
  * source cells, panel OBJ, title and lower button graphics unchanged.
  * Both sampling and dirty targets use this mapping, so page clears work. */
 int tutorial=v->world&&v->world->scene_runtime&&v->world->scene_runtime->program==9;
 if(h->tile_limit!=limit)retained=0;h->tile_limit=limit;
 if(!retained){h->valid=0;h->source_mask=0;}
 if(h->valid&&h->revision==v->fixed_revision&&h->palette_revision==p->fixed_revision){HUDSTAT(1);return 1;}
 int full=!h->valid;
 if(!full&&h->palette_revision!=p->fixed_revision)full=!update_palette(h,p);
 h->palette_revision=p->fixed_revision;
 if(full)reset_tiles(h);else for(unsigned y=0;y<20;y++)h->rows[y]=0;
 if(full||h->revision!=v->fixed_revision){
  if(!full)for(unsigned x=0;x<40;x++){uint32_t bits=v->fixed_dirty[x]&0x3ffffffcu;while(bits){unsigned y=__builtin_ctz(bits);bits&=bits-1;changed(h,x,y,tutorial);}}
  h->revision=v->fixed_revision;
 }
 for(unsigned attempt=0;attempt<2;attempt++){
  int ok=1;
  for(unsigned y=0;y<20&&ok;y++){unsigned source_y=tutorial&&y>=2&&y<=7?(y==7?2:y+1):y;uint32_t bits=h->rows[y];while(bits){unsigned x=__builtin_ctz(bits);bits&=bits-1;if(!render_cell(h,v,p,x,y,source_y)){ok=0;break;}}}
  if(ok)return finish_prepare(h,full);
  if(attempt==0)reset_tiles(h);
 }
 h->valid=0;return 0;
}
/* Permanent320-tile HUD uses0xc000..0xe7ff. Two BG maps use
 * screenblocks29..30; BG3 keeps its map in screenblock31. */
void native_hud_layout(struct NativeHudFrame*h,unsigned background_maps){(void)background_maps;h->tile_base=0;}
static void transfer(const void*source,uintptr_t dest,unsigned words){
 if(words<=4){const uint32_t*s=source;volatile uint32_t*d=(volatile uint32_t*)dest;for(unsigned i=0;i<words;i++)d[i]=s[i];return;}
 volatile uint32_t*d=(volatile uint32_t*)0x040000d4;
 d[2]=0;d[0]=(uintptr_t)source;d[1]=dest;__asm__ volatile("":::"memory");d[2]=0x84000000u|words;while(d[2]&0x80000000u){}__asm__ volatile("":::"memory");
}
void native_hud_commit(struct NativeHudFrame*h){
 unsigned base=0x0600c000u+h->tile_base*32;
 unsigned at=0,first,count;
 while((count=native_dirty_span(h->dirty,h->tile_span,&at,&first)))transfer(h->pixels[first],base+first*32,count*8);
 for(unsigned word=0;word<NATIVE_HUD_DIRTY_WORDS;word++)h->dirty[word]=0;
 if(h->map_dirty){
  for(unsigned row=0;row<20;row++)if(h->rows[row]){unsigned first=__builtin_ctz(h->rows[row])&~1u,end=(32-__builtin_clz(h->rows[row])+1)&~1u;transfer(h->map+row*32+first,0x0600f800+(row*32+first)*2,(end-first)/2);}
  h->map_dirty=0;
 }
 for(unsigned bank=0;bank<4;bank++)if(h->palette_dirty&(1u<<bank))transfer(h->palettes[bank],0x05000180u+bank*32,8);
 *(volatile uint16_t*)0x0400000e=0x1f0c;*(volatile uint16_t*)0x0400001c=0;*(volatile uint16_t*)0x0400001e=0;
 *(volatile uint16_t*)0x04000000|=0x0800;
}

static void stage_map(struct NativeHudFrame*h,const volatile uint16_t*displayed){
 if(!h->map_dirty)return;
 /* Compare actual VRAM, not an assumed previous scene. The final-results
  * affine screen can reuse BG memory before this HUD becomes active again.
  * Retain only changed cells in the existing row masks, outside VBlank. */
 for(unsigned row=0;row<20;row++)if(h->rows[row]){
  unsigned changed=0;
  for(unsigned x=0;x<30;x++){unsigned at=row*32+x;if(h->map[at]!=displayed[at])changed|=1u<<x;}
  h->rows[row]=changed;
 }
}
void native_hud_stage_palette(struct NativeHudFrame*h){
 h->palette_dirty=native_palette_dirty(&h->palettes[0][0],0x05000180,4);
 stage_map(h,(const volatile uint16_t*)0x0600f800);
}

int native_hud_prepare(struct NativeHudFrame*h,const struct NativeTileVideo*v,const struct NativePaletteWorld*p,int retained){return native_hud_prepare_limit(h,v,p,retained,NATIVE_HUD_TILES);}
