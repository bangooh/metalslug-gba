#include "background_video.h"
#include "background_tile.h"
#include "native_background_assets.h"
#include "../generated/native-bg-assets.h"
#include "play_world.h"
#include "../generated/tile-video-data.h"
#include "../generated/object-masks.h"
#define BG_FAST __attribute__((section(".bglegacy"),noinline,long_call))
/* Exact front-to-back occlusion plus two-pass allocation. Only tiles that
 * the new display reuses are pinned before any cache slot is overwritten. */
static uint32_t key_cache[64][1][4] __attribute__((section(".iwram.bg_cache")));
static uint32_t (*extra_keys)[2][4];static unsigned extra_generation;
/* Occlusion and optional keys are mutually exclusive users of the8KiB
 * unused FIX staging loan. Free4800 bytes of IWRAM for hot executable code. */
#define coverage ((uint32_t(*)[30])extra_keys)
struct OpacityEntry {uint32_t tag,rows[2];};
#define opacity_cache ((struct OpacityEntry*)((uint8_t*)extra_keys+4800))
static const struct NativeBackgroundFrame*coverage_owner;
static unsigned coverage_valid,opacity_valid;
static void clear_opacity(void){if(extra_keys)for(unsigned i=0;i<256;i++)opacity_cache[i].tag=0;}
void native_background_cache_storage(void*data,unsigned generation){
 if(extra_keys==data&&extra_generation==generation)return;
 extra_keys=data;extra_generation=generation;coverage_valid=opacity_valid=0;
 if(extra_keys)for(unsigned i=0;i<128;i++)for(unsigned j=0;j<2;j++)extra_keys[i][j][3]=0;
}
static void clear_keys(const struct NativeBackgroundFrame*f){
 if(f->culling)clear_opacity();
 for(unsigned i=0;i<64;i++)for(unsigned j=0;j<1;j++)key_cache[i][j][3]=0;
 if(extra_keys&&!f->culling)for(unsigned i=0;i<128;i++)for(unsigned j=0;j<2;j++)extra_keys[i][j][3]=0;
}
static inline void map_set(struct NativeBackgroundFrame*f,unsigned layer,unsigned at,unsigned id){
 if(f->maps[layer][at]!=id){f->maps[layer][at]=id;f->map_dirty|=1u<<layer;f->map_rows[layer]|=1u<<(at>>5);}

}
static int fail(struct NativeBackgroundFrame*f,unsigned code){f->fault=code;f->ready=0;return 0;}
static int floor4(int n){return n>=0?n/4:-((-n+3)/4);}
/* Geometry bounds this difference to[-95,95]; avoid a runtime division. */
static int mod96(int n){return n<0?n+96:n>=96?n-96:n;}
static __attribute__((noinline)) uint32_t dirty_rows(uint32_t changed,int top,unsigned height,unsigned rows){
 uint32_t result=0;
 for(unsigned row=0;row<height;row++)if(changed&(1u<<row))for(unsigned by=0;by<3;by++){
  unsigned y=mod96(top+(int)row*3+by);if(y<rows*2)result|=1u<<(y>>1);
 }
 return result;
}
static inline unsigned cache_home(uint32_t key){return (key&0x80000000u)?(key^(key>>11))&2047:key&2047;}
/* Backward-shift deletion prevents tombstones accumulating on long routes. */
static __attribute__((noinline)) void erase_hash(struct NativeBackgroundFrame*f,unsigned hole){
 unsigned at=(hole+1)&2047;
 while(f->hash[at]){
  unsigned id=f->hash[at]-1,distance=f->hash_slot[id]>>11;
  unsigned home=distance<31?(at-distance)&2047:cache_home(native_background_4(f)->hashes[id]);
  if(((at-home)&2047)>=((at-hole)&2047)){
   unsigned d=(hole-home)&2047;f->hash[hole]=id+1;f->hash_slot[id]=hole|((d<31?d:31)<<11);hole=at;
  }at=(at+1)&2047;
 }f->hash[hole]=0;
}
/* Final GBA4bpp bytes and a preassigned bank are the cache identity. */
static BG_FAST unsigned intern(struct NativeBackgroundFrame*f,uint32_t*t,unsigned native){
 struct NativeBackground4*b=native_background_4(f);uint32_t hash;
 if(native!=0xffffffffu)hash=0x80000000u|native;
 else{hash=2166136261u;for(unsigned j=0;j<9;j++)hash=(hash^t[j])*16777619u;hash^=hash>>16;hash&=0x7fffffffu;}
 unsigned home=cache_home(hash),vacant=home;
 while(f->hash[vacant]){
  unsigned id=f->hash[vacant]-1,j=0;const uint32_t*p=native_background_pixels(f,id);
  if(b->hashes[id]==hash){if(native!=0xffffffffu){f->live[id]|=2;return id;}if(b->bank[id]==t[8]){for(;j<8&&p[j]==t[j];j++){}if(j==8){f->live[id]|=2;return id;}}}
  vacant=(vacant+1)&2047;
 }
 unsigned id,capacity=NATIVE_BG_TILES;
 if(f->count<capacity)id=f->count++;
 else{
  unsigned n;for(n=0;n<capacity;n++){id=f->victim++;if(f->victim==capacity)f->victim=1;if(!(f->live[id]&2))break;}
  if(n==capacity){fail(f,NBG_TILES);return 65535;}
  erase_hash(f,f->hash_slot[id]&2047);f->generation[id]=(f->generation[id]+1)&63;if(!f->generation[id])clear_keys(f);
  vacant=home;while(f->hash[vacant])vacant=(vacant+1)&2047;
 }
 if(native!=0xffffffffu)native_bg_load(native,t);
 uint32_t*dst=(uint32_t*)native_background_pixels(f,id);unsigned zero=0;
 for(unsigned j=0;j<8;j++){uint32_t value=t[j];dst[j]=value;zero|=(value-0x11111111u)&~value&0x88888888u;}
 b->hashes[id]=hash;b->bank[id]=t[8];unsigned distance=(vacant-home)&2047;
 f->hash[vacant]=id+1;f->hash_slot[id]=vacant|((distance<31?distance:31)<<11);
 f->live[id]=2|(!zero?128:0);f->dirty[id>>5]|=1u<<(id&31);return id;
}
int native_background_palette_update(struct NativeBackgroundFrame*f,const struct NativePaletteWorld*p,struct NativeVideoPalette*shared){
 (void)shared;f->palette_revision=p->fixed_revision;return 1;
}
/* Coverage words contain four rows of eight pixels. Shifting whole words
 * updates an unaligned8x8 footprint in at most six IWRAM accesses. Clipped
 * edge pixels are excluded before accessing the coverage array. */
struct BackgroundPlan {
 int8_t owner[64];uint8_t part[64],height[32];int16_t top[32];
 uint8_t plane,rows,cols,pending;
};
static BG_FAST int coverage_bits(int x,int y,uint32_t top,uint32_t bottom,int write){
 unsigned sx=x&7,sy=(y&3)*8;int col=x>=0?x/8:-((-x+7)/8),row=floor4(y);
 uint32_t right=((1u<<sx)-1)*0x01010101u,left=~right;
 uint32_t a[2]={(top<<sx)&left,(top>>(8-sx))&right};
 uint32_t b[2]={(bottom<<sx)&left,(bottom>>(8-sx))&right};
 unsigned visible=0;
 for(unsigned side=0;side<(sx?2u:1u);side++){
  if((unsigned)(col+(int)side)>=30)continue;
  uint32_t bits[3]={a[side],b[side],0};
  if(sy){bits[2]=bits[1]>>(32-sy);bits[1]=(bits[1]<<sy)|(bits[0]>>(32-sy));bits[0]<<=sy;}
  for(unsigned k=0;k<(sy?3u:2u);k++)if((unsigned)(row+(int)k)<40){
   uint32_t*c=&coverage[row+k][col+side];visible|=bits[k]&~*c;if(write)*c|=bits[k];
  }
 }
 return visible!=0;
}
/* Edge cells may expose previously offscreen pixels during a sub-cell pan.
 * Only prune complete8x8 cells so common scrolling can reuse their masks. */
static BG_FAST int covered(int x,int y){
 if(x<0||y<0||x+8>240||y+8>160)return 0;
 unsigned col=(unsigned)x>>3,row=(unsigned)y>>2,sx=x&7,sy=(y&3)*8;
 uint32_t left=((255u<<sx)&255u)*0x01010101u;
 const uint32_t*p=&coverage[row][col];
 if((p[0]&(left<<sy))!=(left<<sy)||(p[30]&left)!=left)return 0;
 if(sy){uint32_t tail=left>>(32-sy);if((p[60]&tail)!=tail)return 0;}
 if(sx){
  uint32_t right=((1u<<sx)-1)*0x01010101u;p++;
  if((p[0]&(right<<sy))!=(right<<sy)||(p[30]&right)!=right)return 0;
  if(sy){uint32_t tail=right>>(32-sy);if((p[60]&tail)!=tail)return 0;}
 }
 return 1;
}
/* Coalesce opaque tiles: one rectangle replaces up to30 per-tile calls. */
static BG_FAST void cover_run(int x,int y,unsigned width){
 int right=x+(int)width,bottom=y+8;if(x<0)x=0;if(y<0)y=0;if(right>240)right=240;if(bottom>160)bottom=160;
 if(x>=right||y>=bottom)return;
 unsigned first=(unsigned)x>>3,last=(unsigned)(right-1)>>3,top=(unsigned)y>>2,end=(unsigned)(bottom-1)>>2;
 for(unsigned row=top;row<=end;row++){
  uint32_t vertical=0xffffffffu;if(row==top)vertical<<=(y&3)*8;if(row==end&&(bottom&3))vertical&=(1u<<((bottom&3)*8))-1;
  for(unsigned col=first;col<=last;col++){
   unsigned mask=255;if(col==first)mask&=255u<<(x&7);if(col==last&&(right&7))mask&=(1u<<(right&7))-1;
   coverage[row][col]|=vertical&(mask*0x01010101u);
  }
 }
}
static int full_coverage(void){
 for(unsigned y=0;y<40;y++)for(unsigned x=0;x<30;x++)if(coverage[y][x]!=0xffffffffu)return 0;
 return 1;
}
static BG_FAST int keys_for(const struct BackgroundPlan*q,const struct NativeTileVideo*v,unsigned tx,unsigned ty,uint8_t frame,int enabled,uint32_t keys[4],struct NativeBackgroundFrame*f){
 for(unsigned j=0;j<4;j++)keys[j]=0x0fffffffu;
 if(v->source_cells&&!(f->native_profile&128)){
  int first_col=q->owner[tx*2];unsigned first_dy=first_col<0?96:mod96((int)(ty*2)-q->top[first_col]);
  unsigned cell=first_col<0||first_dy>=q->height[first_col]*3u?0:v->source_cells[q->plane][first_col][first_dy/3];
  if(cell&&cell<NATIVE_BG_SOURCE_CELLS){
   unsigned region=native_bg_cell_regions[cell];const struct NativeBgRegion*r=&native_bg_regions[region];
   unsigned part_x=q->part[tx*2],part_y=first_dy%3,matched=1;
   for(unsigned j=1;j<4&&matched;j++){
    unsigned by=j>>1,bx=j&1;int col=q->owner[tx*2+bx];if(col<0){matched=0;break;}
    unsigned dy=mod96((int)(ty*2+by)-q->top[col]);
    if(dy>=q->height[col]*3u||q->part[tx*2+bx]!=(part_x+bx)%3||dy%3!=(part_y+by)%3||v->source_cells[q->plane][col][dy/3]!=cell+((part_x+bx)/3)*r->height+(part_y+by)/3){matched=0;break;}
   }
   if(matched){unsigned origin=native_bg_cell_origins[cell],x=(origin&65535)+part_x,y=(origin>>16)+part_y;if(x<r->width*3u-1&&y<r->height*3u-1){unsigned id=native_bg_region_map(region,f->native_profile,x,y);if(id==0xffffffffu)return 0;if(id)keys[0]=NATIVE_BG_ASSET_MARK|id;return 1;}}
  }
 }
 for(unsigned bx=0;bx<2;bx++){
  unsigned mx=tx*2+bx;int col=q->owner[mx];if(col<0)continue;
  unsigned dy=mod96((int)(ty*2)-q->top[col]),last_row=99,flip=0,prefix=0,x=0;const uint32_t*fragments=0;
  for(unsigned by=0;by<2;by++,dy=dy==95?0:dy+1){
   if(dy>=q->height[col]*3u)continue;
   unsigned row=dy/3;
   if(row!=last_row){
    const struct NativeTileCell*c=&v->planes[q->plane].cells[col][row];unsigned a=c->attributes,bank=(a>>4)&15,tile=c->tile;
    if(bank>1)return 0;
    f->auto_mask|=(a&8)?7:(a&4)?3:0;if(a&12)f->auto_planes|=1u<<q->plane;
    if(enabled){if(a&8)tile=(tile&~7u)|(frame&7);else if(a&4)tile=(tile&~3u)|(frame&3);}
    unsigned index=native_bg_lookup[(bank<<16)|tile];if(index>=NATIVE_BG_TILE_COUNT)return 0;
    fragments=native_bg_fragments[index];flip=a&3;prefix=(a>>8)<<2|flip;
    x=q->part[mx];if(flip&1)x=2-x;last_row=row;
   }
   unsigned y=dy-row*3;if(flip&2)y=2-y;
   unsigned block=fragments[y*3+x];if(block>=NATIVE_BG_BLOCK_COUNT)return 0;
   if(native_bg_opacity_mask(block,0))keys[by*2+bx]=block<<10|prefix;
  }
 }
 return 1;
}
static BG_FAST unsigned native_resident(const struct NativeBackgroundFrame*f,unsigned asset){
 uint32_t identity=0x80000000u|asset;unsigned slot=cache_home(identity);
 const struct NativeBackground4*b=native_background_4(f);
 while(f->hash[slot]){unsigned id=f->hash[slot]-1;if(b->hashes[id]==identity)return id;slot=(slot+1)&2047;}
 return 65535;
}
static BG_FAST uint32_t*key_lookup(struct NativeBackgroundFrame*f,const uint32_t keys[4],uint32_t packed[4],unsigned*hit){
 /* Prebuilt asset IDs are exact identities. Pin existing VRAM entries
  * directly during the gather pass, before any replacement can evict them.
  * They need neither fragment-key packing nor a duplicate key cache. */
 if((keys[0]&0x0f000000u)==NATIVE_BG_ASSET_MARK){
  *hit=native_resident(f,keys[0]&0xffffffu);
  return 0;
 }
 packed[0]=keys[0]|keys[1]<<28;packed[1]=keys[1]>>4|keys[2]<<24;packed[2]=keys[2]>>8|keys[3]<<20;packed[3]=keys[3]>>12;
 uint32_t hash=2166136261u;for(unsigned j=0;j<4;j++)hash=(hash^packed[j])*16777619u;
 unsigned slot=(hash^(hash>>16))&127,internal=1,ways=internal+(extra_keys&&!f->culling?2:0);
 uint32_t(*set)[4]=key_cache[slot&63],*entry=0;*hit=65535;
 for(unsigned way=0;way<ways;way++){
  uint32_t*e=way>=internal?extra_keys[slot][way-internal]:set[way];unsigned stamp=e[3]>>16,id=stamp&1023;
  if(!id||id>NATIVE_BG_TILES||f->generation[id-1]!=(stamp>>10)){if(!entry)entry=e;continue;}
  if(e[0]==packed[0]&&e[1]==packed[1]&&e[2]==packed[2]&&(e[3]&65535)==packed[3]){*hit=id-1;return e;}
 }
 if(entry)return entry;if(f->cache_evict>=ways)f->cache_evict=0;
 unsigned way=f->cache_evict++;
 return way>=internal?extra_keys[slot][way-internal]:set[way];
}
static inline uint32_t spread_rows(unsigned mask){return (mask&15)|((mask&240)<<4)|((mask&3840)<<8)|((mask&61440)<<12);}
static void opacity_keys(const uint32_t keys[4],uint32_t rows[2]){
 rows[0]=rows[1]=0;
 if((keys[0]&0x0f000000u)==NATIVE_BG_ASSET_MARK){uint32_t t[9];native_bg_load(keys[0]&0xffffff,t);for(unsigned i=0;i<64;i++)if((t[i/8]>>((i&7)*4))&15)rows[i>>5]|=1u<<(i&31);return;}
 for(unsigned j=0;j<4;j++)if(keys[j]!=0x0fffffffu)rows[j>>1]|=spread_rows(native_bg_opacity_mask(keys[j]>>10,keys[j]&3))<<((j&1)*4);
}
static inline unsigned nonzero4(uint32_t v){
 uint32_t bits=(((v&0x7f7f7f7fu)+0x7f7f7f7fu)|v)&0x80808080u;
 return ((bits>>7)*0x01020408u)>>24;
}
static __attribute__((noinline)) void opacity_tile(const uint32_t tile[16],uint32_t rows[2]){
 for(unsigned part=0;part<2;part++){
  uint32_t mask=0;
  for(unsigned row=0;row<4;row++){unsigned at=part*8+row*2;mask|=(nonzero4(tile[at])|(nonzero4(tile[at+1])<<4))<<(row*8);}
  rows[part]=mask;
 }
}
/* The unused tail of the FIX loan holds exact masks for composed8x8
 * tiles. Generation tags prevent reusing a recycled tile's coverage. */
static void cached_opacity(const struct NativeBackgroundFrame*f,unsigned id,uint32_t rows[2]){
 struct OpacityEntry*c=&opacity_cache[id&255];unsigned tag=(id+1)|((unsigned)f->generation[id]<<10);
 if(c->tag!=tag){if(f->depth4){const uint8_t*p=(const uint8_t*)native_background_pixels(f,id);c->rows[0]=c->rows[1]=0;for(unsigned i=0;i<64;i++)if((p[i>>1]>>((i&1)*4))&15)c->rows[i>>5]|=1u<<(i&31);}else opacity_tile(native_background_pixels(f,id),c->rows);c->tag=tag;}
 rows[0]=c->rows[0];rows[1]=c->rows[1];
}
/* Translate exact foreground coverage. Newly exposed edge pixels are zero
 * until the corresponding visible tiles add their full opacity below. */
static void shift_coverage(int dx,int dy){
 uint8_t row[30];int begin=dy>0?159:0,end=dy>0?-1:160,step=dy>0?-1:1;
 for(int y=begin;y!=end;y+=step){
  int sy=y-dy;
  for(unsigned x=0;x<30;x++)row[x]=(unsigned)sy<160?(uint8_t)(coverage[sy>>2][x]>>((sy&3)*8)):0;
  for(unsigned x=0;x<30;x++){
   unsigned bits;
   if(dx>=0)bits=(row[x]<<dx)|(x?row[x-1]>>(8-dx):0);
   else bits=(row[x]>>-dx)|(x<29?row[x+1]<<(8+dx):0);
   uint32_t*word=&coverage[y>>2][x];unsigned shift=(y&3)*8;
   *word=(*word&~(255u<<shift))|((bits&255u)<<shift);
  }
 }
}
static __attribute__((noinline)) unsigned active_views(const struct NativeTileVideo*v,unsigned order[4]){
 const struct SceneVideoWorld*w=&v->world->video;unsigned count=0;
 for(unsigned plane=0;plane<4;plane++){
  const struct SceneTileView*view=&w->views[plane];if(!view->valid)continue;
  if(!w->pools.valid||view->first_column+32>380)return 5;
  unsigned col;
  for(col=0;col<32;col++){
   const struct NativeTileColumn*c=&v->columns[view->first_column+col];unsigned x=(c->x>>7)&511;
   if(c->hidden||(x>=320&&x<496))continue;
   unsigned height=c->y&63;if(!height||height>32)height=32;
   unsigned row;
   for(row=0;row<height;row++){
    const struct NativeTileCell*cell=&v->planes[plane].cells[col][row];
    /* Clearing a source plane leaves its column positions alive. Tile0
     * is transparent in both source banks, irrespective of palette/flip.
     * Do not spend a hardware BG slot on that cleared plane: it could
     * displace a visible rear plane (notably the ending starfield).
     * Animation and invalid banks remain eligible for normal validation. */
    if(cell->tile||(cell->attributes&0xec))break;
   }
   if(row<height)break;
  }
  if(col<32)order[count++]=plane;
 }
 for(unsigned i=1;i<count;i++){unsigned n=order[i],j=i;while(j&&w->views[order[j-1]].first_column<w->views[n].first_column){order[j]=order[j-1];j--;}order[j]=n;}
 return count>NATIVE_BG_VISIBLE_LIMIT?NATIVE_BG_VISIBLE_LIMIT:count;
}
/* Preserve the horizontal BG origin across a change of visible anchor.
 * Reusing a retained source strip avoids a one-pixel phase jump when the
 * first visible strip changes.  Only integer arithmetic; no extra tiles. */
static int stable_scroll_x(const struct NativeBackgroundFrame*f,const struct NativeTileVideo*v,
                           unsigned first,unsigned layer,const uint32_t*changed,unsigned old_sx){
 for(unsigned col=0;col<32;col++){
  const struct NativeTileColumn*c=&v->columns[first+col];
  unsigned ox=f->previous_columns[layer][col][0];
  if(c->hidden||ox==65535||(changed&&changed[col]))continue;
  unsigned nx=(c->x>>7)&511;
  if((ox>=320&&ox<496)||(nx>=320&&nx<496))continue;
  int before=ox>=496?(int)ox-512:(int)ox;
  int after=nx>=496?(int)nx-512:(int)nx;
  if(after-before < -16||after-before >16)continue;
  return (old_sx+(unsigned)(floor4(before*3)-floor4(after*3)))&255;
 }
 return -1;
}
/* No tile enters or leaves the prepared window when scrolling within its
 * existing8px cells. Validate every source column before changing anything;
 * then update hardware scroll alone, without touching cache pins or maps. */
static __attribute__((noinline)) int scroll_only(struct NativeBackgroundFrame*f,const struct NativeTileVideo*v,const unsigned order[4],unsigned count){
 uint16_t scroll[4][2];
 unsigned common_shift[2]={0};
 for(unsigned layer=0;layer<count;layer++){
  unsigned plane=order[layer],first=v->world->video.views[plane].first_column;
  if(!(f->enable&(256u<<layer)))return 0;
  unsigned dx=0,dy=0,found=0,anchor=0;
  int stable_x=-1;
  for(unsigned col=0;col<32;col++){
   if(v->planes[plane].dirty[col])return 0;
   const struct NativeTileColumn*c=&v->columns[first+col];
   unsigned ox=f->previous_columns[layer][col][0],oy=f->previous_columns[layer][col][1];
   if((ox==65535)!=!!c->hidden)return 0;if(c->hidden)continue;
   unsigned x=(c->x>>7)&511,y=(c->y>>7)&511;
   if((c->y&127)!=(oy&127))return 0;
   if(stable_x<0&&!(ox>=320&&ox<496)&&!(x>=320&&x<496)){
    int before=ox>=496?(int)ox-512:(int)ox;
    int after=x>=496?(int)x-512:(int)x;
    if(after-before>=-16&&after-before<=16)
     stable_x=(f->scroll_x[layer]+(unsigned)(floor4(before*3)-floor4(after*3)))&255;
   }
   unsigned cx=(x-ox)&511,cy=(y-(oy>>7))&511;
   if(found&&(cx!=dx||cy!=dy))return 0;dx=cx;dy=cy;found=1;
   if((x<320||x>=496)!=(ox<320||ox>=496))return 0;
   if(!anchor&&(x<320||x>=496)){
    int sx=x>=496?(int)x-512:(int)x;
    scroll[layer][0]=stable_x>=0?(unsigned)stable_x:(col*12u-(unsigned)floor4(sx*3))&255;
    int movement=(int)y-(int)(oy>>7);
    if(movement< -16||movement>16)continue;
    scroll[layer][1]=(f->scroll_y[layer]+(unsigned)(floor4((496-(int)(oy>>7))*3)-floor4((496-(int)y)*3)))&255;anchor=1;
   }
  }
  if(!found||!anchor)return 0;
  for(unsigned axis=0;axis<2;axis++){
   unsigned old=axis?f->scroll_y[layer]:f->scroll_x[layer],now=scroll[layer][axis];
   /* Occlusion remains valid only when every layer moves together. */
   unsigned shift=(now-old)&255;
   if(f->culling&&layer&&shift!=common_shift[axis])return 0;
   if(!layer)common_shift[axis]=shift;
   if((old>>3)!=(now>>3))return 0;
   unsigned extent=axis?160:240;
   if((extent+(now&7)+7)/8>(extent+(old&7)+7)/8)return 0;
  }
 }
 for(unsigned layer=0;layer<count;layer++){
  unsigned first=v->world->video.views[order[layer]].first_column;
  f->scroll_x[layer]=scroll[layer][0];f->scroll_y[layer]=scroll[layer][1];
  for(unsigned col=0;col<32;col++){
   const struct NativeTileColumn*c=&v->columns[first+col];f->previous_columns[layer][col][0]=c->hidden?65535:(c->x>>7)&511;f->previous_columns[layer][col][1]=c->y;
  }
 }
 return 1;
}
#ifdef __arm__
#define BG_ROM __attribute__((target("thumb"),optimize("O3"),noinline))
#else
#define BG_ROM __attribute__((noinline))
#endif
/* Recycled strips may change their Y origin while the rest of a plane
 * moves normally. Select an unchanged strip for the hardware map origin. */
static BG_ROM int stable_scroll_y(const struct NativeBackgroundFrame*f,const struct NativeTileVideo*v,unsigned first,unsigned layer,const uint32_t changed[32],unsigned old_sy){
 for(unsigned col=0;col<32;col++){
   const struct NativeTileColumn*c=&v->columns[first+col];
   unsigned old=f->previous_columns[layer][col][0],oy=f->previous_columns[layer][col][1];
   if(c->hidden||old==65535||changed[col]||(oy&127)!=(c->y&127))continue;
   int movement=(int)(c->y>>7)-(int)(oy>>7);
   if(movement< -16||movement>16)continue;
   int before=floor4((496-(int)(oy>>7))*3)-4;
   int after=floor4((496-(int)(c->y>>7))*3)-4;
   return (old_sy+(unsigned)(before-after))&255;
 }
 return -1;
}
BG_FAST int native_background_prepare(struct NativeBackgroundFrame*f,const struct NativeTileVideo*v,const struct NativePaletteWorld*p,struct NativeVideoPalette*shared,uint8_t frame,int enabled){
 if(!f)return 0;f->ready=f->fault=0;
 if(!v||v->fault||!v->world||!p||p->fault||p->world!=v->world||!shared||shared->fault||shared->ready)return fail(f,NBG_STATE);
 /* Pending retirement of occlusion keeps every cached pixel/palette.
  * Revisit hidden map cells once, bypassing only whole-frame fast exits. */
 int unprune=f->culling==3;
 if(unprune){f->culling=0;clear_keys(f);coverage_valid=opacity_valid=0;}
 const struct SceneVideoWorld*w=&v->world->video;
 unsigned order[4],map_count=active_views(v,order);if(map_count>4)return fail(f,NBG_COLUMNS);
 if(f->layer_limit&&map_count>f->layer_limit)map_count=f->layer_limit;
 unsigned reserved=map_count?map_count:1;
 if(f->map_count!=reserved){f->count=0;f->map_count=reserved;}
 uint32_t signature=w->pools.upper_begin;
 for(unsigned i=0;i<4;i++){signature=signature*16777619u^w->views[i].first_column;signature=signature*16777619u^w->views[i].valid;}
 for(unsigned i=0;i<map_count;i++)signature=signature*16777619u^(order[i]+1);
 if(!unprune&&f->count&&f->palette_revision==p->fixed_revision&&f->tile_revision==v->revision&&f->view_signature==signature&&f->auto_enabled==!!enabled&&(!enabled||!((f->auto_frame^frame)&f->auto_mask))){f->ready=1;return 1;}
 unsigned animation=enabled?(f->auto_frame^frame)&f->auto_mask:0;
 int retained=f->count&&f->palette_revision==p->fixed_revision&&f->view_signature==signature&&f->auto_enabled==!!enabled;
 if(!unprune&&retained&&!animation&&scroll_only(f,v,order,map_count)){coverage_valid=0;f->tile_revision=v->revision;f->auto_frame=frame;f->ready=1;return 1;}
 unsigned previous_enable=f->enable;f->enable=0;if(!retained)f->auto_mask=f->auto_planes=0;
 uint32_t tile[16]={0};
 if(!f->count||f->palette_revision!=p->fixed_revision){
  if(f->depth4){struct NativeBackground4*b=native_background_4(f);uint32_t*words=(uint32_t*)b;for(unsigned i=0;i<__builtin_offsetof(struct NativeBackground4,source_cells)/4;i++)words[i]=0;for(unsigned bank=0;bank<12;bank++){b->counts[bank]=15;for(unsigned j=1;j<16;j++)b->indices[bank][j]=bank*16+j;}}
  f->count=f->uploaded=f->cache_evict=0;f->victim=1;f->map_dirty=15;for(unsigned i=0;i<4;i++)f->map_rows[i]=0xffffffffu;
  for(unsigned i=0;i<31;i++)f->dirty[i]=0;
  for(unsigned i=0;i<NATIVE_BG_TILES;i++){f->live[i]=0;f->generation[i]=0;}
  for(unsigned i=0;i<2048;i++)f->hash[i]=0;clear_keys(f);
  if(intern(f,tile,0xffffffffu)!=0)return fail(f,NBG_METADATA);f->palette_revision=p->fixed_revision;
 }
 for(unsigned i=0;i<f->count;i++)f->live[i]&=128;
 int prune=f->culling==1;f->live[0]|=2;
 if(f->culling&&!extra_keys)return fail(f,NBG_TILES);
 int reuse_coverage=f->culling&&map_count==2&&retained&&!animation&&coverage_valid&&coverage_owner==f;
 coverage_valid=0;coverage_owner=f;
 if(f->culling){if(!opacity_valid)clear_opacity();opacity_valid=1;}else opacity_valid=0;
 struct BackgroundPlan plans[4]={0};
 /* Gather every reused tile first. IRQ still displays the preceding VRAM;
  * EWRAM pixels can be replaced once this pass has pinned the new display. */
 int occluded=0;unsigned pending_count=0;
 for(unsigned layer=0;layer<map_count;layer++){
  unsigned plane=order[layer];const struct SceneTileView*view=&w->views[plane];struct BackgroundPlan*q=&plans[layer];q->plane=plane;
  unsigned old_sx=f->scroll_x[layer],old_sy=f->scroll_y[layer],old_x=old_sx>>3,old_y=old_sy>>3;
  f->control[layer]=(32-!!f->hud_reserved-f->map_count+layer)*256+(f->depth4?0:128)+3;f->scroll_x[layer]=f->scroll_y[layer]=0;
  if(!view->valid)continue;
  if(!w->pools.valid||view->first_column+32>380)return fail(f,NBG_COLUMNS);
  unsigned priority=view->first_column>=w->pools.upper_begin?1:3;
  f->control[layer]=(32-!!f->hud_reserved-f->map_count+layer)*256+(f->depth4?0:128)+priority;
  int8_t*owner=q->owner,old_owner[64];uint8_t old_part[64],*part=q->part,*height=q->height;int16_t*top=q->top;uint32_t changed[32];
  for(unsigned col=0;col<32;col++){changed[col]=v->planes[plane].dirty[col];if(animation&&(f->auto_planes&(1u<<plane)))for(unsigned row=0;row<32;row++){unsigned attr=v->planes[plane].cells[col][row].attributes;if(animation&((attr&8)?7:(attr&4)?3:0))changed[col]|=1u<<row;}}
  int uniform=retained&&!!(previous_enable&(1u<<(8+layer))),translation=0,same_columns=1;unsigned delta_x=0,delta_y=0,any_dirty=0;
  for(unsigned col=0;col<32;col++){
   const struct NativeTileColumn*c=&v->columns[view->first_column+col];
   unsigned old=f->previous_columns[layer][col][0],oy=f->previous_columns[layer][col][1];
   any_dirty|=changed[col];if(!!c->hidden!=(old==65535))same_columns=0;
   if(c->hidden||old==65535)continue;
   unsigned x=(c->x>>7)&511,y=(c->y>>7)&511;
   if((oy&127)!=(c->y&127)){uniform=0;continue;}
   unsigned dx=(x-old)&511,dy=(y-(oy>>7))&511;
   if(!translation){delta_x=dx;delta_y=dy;translation=1;}else if(dx!=delta_x||dy!=delta_y)uniform=0;
  }
  if(!translation)uniform=0;
  for(unsigned x=0;x<64;x++)owner[x]=old_owner[x]=-1;
  for(unsigned col=0;col<32;col++){
   int x=f->previous_columns[layer][col][0];if(x==65535)continue;
   if(x>=496)x-=512;else if(x>=320)continue;
   int left=(floor4(x*3)+(int)old_sx)/4;
   for(unsigned bx=0;bx<3;bx++){old_owner[(left+bx)&63]=col;old_part[(left+bx)&63]=bx;}
  }
  int anchor_y=retained&&(previous_enable&(256u<<layer))?
   stable_scroll_y(f,v,view->first_column,layer,changed,old_sy):-1;
  int anchor_x=retained&&(previous_enable&(256u<<layer))?
   stable_scroll_x(f,v,view->first_column,layer,changed,old_sx):-1;
  int phase=0;
  for(unsigned col=0;col<32;col++){
   const struct NativeTileColumn*c=&v->columns[view->first_column+col];
   height[col]=0;if(c->hidden)continue;
   int x=(c->x>>7)&511;if(x>=496)x-=512;else if(x>=320)continue;
   int y=496-((c->y>>7)&511),sx=floor4(x*3),sy=floor4(y*3)-4;
   if(!phase){f->scroll_x[layer]=anchor_x>=0?(unsigned)anchor_x:(col*12u-(unsigned)sx)&255;f->scroll_y[layer]=anchor_y>=0?(unsigned)anchor_y:(unsigned)-sy&255;phase=1;}
   unsigned hx=f->scroll_x[layer]&7,vy=f->scroll_y[layer]&7;
   if(((sx+(int)hx)&3)||((sy+(int)vy)&3))return fail(f,NBG_COLUMNS);
   /* Chained columns need a different coordinate producer. SceneMaps emits
    * independently positioned columns; reject an unexpected chain bit. */
   if(c->y&64)return fail(f,NBG_COLUMNS);
   top[col]=(sy+(int)vy)/4;height[col]=c->y&63;
   if(!height[col]||height[col]>32)height[col]=32;
   int left=(sx+(int)hx)/4;
   for(int bx=0;bx<3;bx++)if(left+bx>=0&&left+bx<64){owner[left+bx]=col;part[left+bx]=bx;}
  }
  if(!phase){if(layer==0){reuse_coverage=0;if(f->culling)for(unsigned y=0;y<40;y++)for(unsigned x=0;x<30;x++)coverage[y][x]=0;}continue;}f->enable|=1u<<(8+layer);
  /* A32-column source ring spans384 scaled pixels, whereas a GBA map
   * wraps at256. Rebuild on an origin discontinuity, never reuse the
   * coincidentally overlapping map cells from another source-ring lap. */
  int move_x=(int)((delta_x+256)&511)-256,move_y=(int)((delta_y+256)&511)-256;
  int scroll_x=(int)((f->scroll_x[layer]-old_sx+128)&255)-128,scroll_y=(int)((f->scroll_y[layer]-old_sy+128)&255)-128;
  int error_x=scroll_x+move_x*3/4,error_y=scroll_y-move_y*3/4;
  if(error_x< -1||error_x>1||error_y< -1||error_y>1)uniform=0;
  /* A source column can cross the320px clip boundary without changing
   * its hidden flag or the8px map origin. Such edge fragments must be
   * composed now; otherwise an empty column scrolls across the screen. */
  if(same_columns)for(unsigned x=0;x<62;x++){unsigned old=(((f->scroll_x[layer]>>3)*2)+x)&63;if(old_owner[old]!=owner[x]||(owner[x]>=0&&old_part[old]!=part[x])){same_columns=0;break;}}

  q->cols=(240+(f->scroll_x[layer]&7)+7)/8;q->rows=(160+(f->scroll_y[layer]&7)+7)/8;
  if(occluded){f->enable&=~(1u<<(8+layer));q->rows=0;goto gathered;}
  /* A recycled or independently moving strip must not invalidate every
   * other strip in its plane. Compare the exact source fragment position
   * of each column, including the256px map wrap versus384px source wrap. */
  uint32_t changed_rows[32]={0},rebuild[31],outside=0,any_rows=0;
  unsigned delta=((f->scroll_y[layer]>>3)-old_y)&31;
  uint32_t before_wrap=delta?(1u<<(32-delta))-1:0xffffffffu,visible_rows=(1u<<q->rows)-1;
  for(unsigned col=0;col<32;col++)if(height[col]){
   unsigned previous_y=f->previous_columns[layer][col][1],previous_height=previous_y&63;if(!previous_height)previous_height=32;
   if(!retained||f->previous_columns[layer][col][0]==65535||previous_height!=height[col])changed_rows[col]=visible_rows;
   else{
    int old_top=(floor4((496-(int)(previous_y>>7))*3)-4+(int)(old_sy&7))/4;
    int difference=(int)(delta*2)+top[col]-old_top;
    uint32_t same=(difference==0||difference==96||difference==-96)?before_wrap:0;
    if(difference==64||difference==-32)same|=~before_wrap;
    changed_rows[col]=visible_rows&~same;
   }
   if(changed[col])changed_rows[col]|=dirty_rows(changed[col],top[col],height[col],q->rows);
   any_rows|=changed_rows[col];
  }
  int unchanged=uniform&&same_columns&&!any_dirty&&!any_rows&&old_x==(f->scroll_x[layer]>>3)&&old_y==(f->scroll_y[layer]>>3);
  int cover_needed=f->culling&&layer+1<map_count;
  int reuse_front=layer==0&&reuse_coverage&&unchanged;
  int shift_x=(int)old_sx-(int)f->scroll_x[layer],shift_y=(int)old_sy-(int)f->scroll_y[layer];
  if(f->culling&&layer==0){
   if(reuse_front){if(shift_x||shift_y)shift_coverage(shift_x,shift_y);}
   else for(unsigned cy=0;cy<40;cy++)for(unsigned cx=0;cx<30;cx++)coverage[cy][cx]=0;
  }
  if(!unprune&&unchanged&&(!f->culling||(reuse_front&&!shift_x&&!shift_y))&&(240+(old_sx&7)+7)/8>=q->cols&&(160+(old_sy&7)+7)/8>=q->rows){
   for(unsigned y=0;y<q->rows;y++)for(unsigned x=0;x<q->cols;x++)f->live[f->maps[layer][((old_y+y)&31)*32+((old_x+x)&31)]]|=2;
   q->rows=0;goto gathered;
  }
  /* Collapse source dirty rows and ownership into one bit per output cell.
   * The hot tile loop then needs no modulo/division or column traversal. */
  for(unsigned ty=0;ty<q->rows;ty++)if((((f->scroll_y[layer]>>3)+ty-old_y)&31)>=(160+(old_sy&7)+7)/8)outside|=1u<<ty;
  for(unsigned tx=0;tx<q->cols;tx++){
   unsigned mx=((f->scroll_x[layer]>>3)+tx)&31;int a=owner[tx*2],b=owner[tx*2+1];
   rebuild[tx]=!retained||!(previous_enable&(256u<<layer))||((mx-old_x)&31)>=(240+(old_sx&7)+7)/8||old_owner[mx*2]!=a||old_owner[mx*2+1]!=b||(a>=0&&old_part[mx*2]!=part[tx*2])||(b>=0&&old_part[mx*2+1]!=part[tx*2+1])?0xffffffffu:outside|(a<0?0:changed_rows[a])|(b<0?0:changed_rows[b]);
  }
  for(unsigned ty=0;ty<q->rows;ty++){
   uint32_t opacity[31][2];int y=(int)(ty*8)-(int)(f->scroll_y[layer]&7);
   for(unsigned tx=0;tx<q->cols;tx++){
    opacity[tx][0]=opacity[tx][1]=0;
    unsigned map_x=((f->scroll_x[layer]>>3)+tx)&31,map_y=((f->scroll_y[layer]>>3)+ty)&31,at=map_y*32+map_x;
    int x=(int)(tx*8)-(int)(f->scroll_x[layer]&7);
    if(prune&&layer&&covered(x,y)){map_set(f,layer,at,NATIVE_BG_HIDDEN);continue;}
    unsigned id=65535;
    if(!(rebuild[tx]&(1u<<ty))&&f->maps[layer][at]<NATIVE_BG_TILES){id=f->maps[layer][at];if(!f->culling){f->live[id]|=2;continue;}}
    uint32_t keys[4],packed[4];
    if(id==65535){
     if(!keys_for(q,v,tx,ty,frame,enabled,keys,f))return fail(f,NBG_METADATA);
     if(keys[0]==0x0fffffffu&&keys[1]==0x0fffffffu&&keys[2]==0x0fffffffu&&keys[3]==0x0fffffffu){map_set(f,layer,at,0);continue;}
     key_lookup(f,keys,packed,&id);
    }
    int need_mask=cover_needed&&(!reuse_front||(shift_x>0&&x<shift_x)||(shift_x<0&&x+8>240+shift_x)||(shift_y>0&&y<shift_y)||(shift_y<0&&y+8>160+shift_y));
    if(id!=65535){
     f->live[id]|=2;map_set(f,layer,at,id);
     if(need_mask&&id){if(f->live[id]&128)opacity[tx][0]=opacity[tx][1]=0xffffffffu;else cached_opacity(f,id,opacity[tx]);}
    }else{
     unsigned marker=65535;
     /* The second half of the loan remembers unresolved source keys. It
      * survives cache eviction and saves a second ROM lookup/composition
      * plan for up to256 cells. Larger rebuilds use the ordinary path. */
     if(extra_keys&&!f->culling&&pending_count<256){uint32_t*saved=(uint32_t*)(extra_keys+128)+pending_count*4;for(unsigned j=0;j<4;j++)saved[j]=keys[j];marker=0x8000u|pending_count++;}
     map_set(f,layer,at,marker);q->pending=1;if(need_mask)opacity_keys(keys,opacity[tx]);
    }
   }
   if(cover_needed)for(unsigned tx=0;tx<q->cols;){
    int x=(int)(tx*8)-(int)(f->scroll_x[layer]&7);
    if(opacity[tx][0]==0xffffffffu&&opacity[tx][1]==0xffffffffu){
     unsigned start=tx;do{tx++;}while(tx<q->cols&&opacity[tx][0]==0xffffffffu&&opacity[tx][1]==0xffffffffu);
     cover_run(x,y,(tx-start)*8);
    }else{if(opacity[tx][0]||opacity[tx][1])coverage_bits(x,y,opacity[tx][0],opacity[tx][1],1);tx++;}
   }
  }
  if(cover_needed)occluded=full_coverage();
 gathered:
  for(unsigned col=0;col<32;col++){const struct NativeTileColumn*c=&v->columns[view->first_column+col];f->previous_columns[layer][col][0]=c->hidden?65535:(c->x>>7)&511;f->previous_columns[layer][col][1]=c->y;}
 }
 /* Only unresolved, visible cells need pixel composition or tile allocation. */
 for(unsigned layer=0;layer<4;layer++){
  const struct BackgroundPlan*q=&plans[layer];
  if(!q->pending)continue;
  for(unsigned ty=0;ty<q->rows;ty++)for(unsigned tx=0;tx<q->cols;tx++){
   unsigned at=(((f->scroll_y[layer]>>3)+ty)&31)*32+(((f->scroll_x[layer]>>3)+tx)&31);
   unsigned marker=f->maps[layer][at];if(!(marker&0x8000))continue;
   uint32_t keys[4],packed[4];unsigned id;
   if(marker!=65535){const uint32_t*saved=(uint32_t*)(extra_keys+128)+(marker&255)*4;for(unsigned j=0;j<4;j++)keys[j]=saved[j];}
   else if(!keys_for(q,v,tx,ty,frame,enabled,keys,f))return fail(f,NBG_METADATA);
   uint32_t*entry=key_lookup(f,keys,packed,&id);
   if(id==65535){
    unsigned native=0xffffffffu;
    if((keys[0]&0x0f000000u)==NATIVE_BG_ASSET_MARK){native=keys[0]&0xffffff;f->native_tiles++;}
    else{if(!native_bg_boundary(keys,p,f->native_profile,tile))return fail(f,NBG_PALETTE);f->boundary_tiles++;}
    id=intern(f,tile,native);if(id==65535)return 0;
    if(entry){for(unsigned j=0;j<3;j++)entry[j]=packed[j];entry[3]=packed[3]|(((id+1)|((unsigned)f->generation[id]<<10))<<16);}
   }
   f->live[id]|=2;map_set(f,layer,at,id);
  }
 }
 coverage_valid=f->culling&&map_count==2&&(f->enable&256);
 f->tile_revision=v->revision;f->view_signature=signature;f->auto_frame=frame;f->auto_enabled=!!enabled;f->ready=1;return 1;
}
int native_background_repack4(struct NativeBackgroundFrame*f,void*scratch,unsigned hud){(void)f;(void)scratch;(void)hud;return 0;}
