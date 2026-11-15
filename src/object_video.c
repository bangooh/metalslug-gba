#include "object_video.h"
#include "../generated/object-masks.h"
#include "../generated/approved-player-data.h"
_Static_assert(sizeof(void*)!=4||sizeof(struct NativeObjectFrame)<=4352,"OBJ frame budget");
static uint8_t *bridge_scratch;static unsigned bridge_capacity,bridge_used;
void native_object_bridge_scratch(void*p,unsigned bytes){bridge_scratch=p;bridge_capacity=bytes;bridge_used=0;}
static int fail(struct NativeObjectFrame*f,unsigned reason){if(f&&!f->fault)f->fault=reason;return 0;}
/* v171 palette memo. Under an unchanged bank state, palette() is a pure
 * function of (colors on mask, mask): its first-fit scan returns the same
 * bank again. Every writer of palettes[]/palette_used[]/palette_count
 * empties the memo. The key is the mask plus the colors themselves,
 * verified inside the remembered bank (captured palettes are distinct
 * pointers even when their colors are equal). */
#define PALETTE_MEMO 8
static uint16_t memo_mask[PALETTE_MEMO];static uint8_t memo_slot[PALETTE_MEMO],memo_count,memo_next;
void native_object_palette_touch(void){memo_count=0;}
void native_object_begin(struct NativeObjectFrame*f,unsigned budget){
 if(!f)return;native_object_bridge_scratch(0,0);f->mutable_generation=0;f->count=f->upload_count=f->tile_count=f->palette_count=f->affine_count=f->ready=f->fault=0;
 f->tile_budget=budget<=NATIVE_OBJ_TILE_LIMIT?budget:0;f->dense_layout=0;
 f->palette_protected=f->palette_spills=f->tile_compactions=0;f->palette_merges=f->palette_exact=f->palette_merge_error=0;f->palette_tolerance=5;
 for(unsigned i=0;i<256;i++)f->scratch.uploads[i]=0;
 for(unsigned i=0;i<16;i++){f->scratch.palettes[i].valid=0;f->palette_used[i]=0;}
 memo_count=0;
 if(!f->tile_budget)f->fault=NOBJ_TILES;
}
static unsigned used(const uint8_t*p,unsigned bytes){unsigned mask=0;for(unsigned i=0;i<bytes;i++)mask|=(1u<<(p[i]&15))|(1u<<(p[i]>>4));return mask&65534;}
/* On overflow only, merge two unprotected actor shade palettes. Every
 * occupied RGB channel changes by at most palette_tolerance/31; lock the bank against
 * a second approximation. HUD/player/projectile palettes stay exact. */
static __attribute__((noinline)) int merge_shades(struct NativeObjectFrame*f){
 memo_count=0;
 unsigned best=f->palette_tolerance+1,from=16,to=16;
 for(unsigned b=1;b<16;b++){
  if(f->palette_protected&(1u<<b))continue;
  for(unsigned a=0;a<b;a++){
   if(f->palette_protected&(1u<<a))continue;
   unsigned common=f->palette_used[a]&f->palette_used[b],needed=1;
   for(unsigned j=1;j<16&&needed<best;j++)if(common&(1u<<j)){
    unsigned ca=f->palettes[a][j],cb=f->palettes[b][j];
    for(unsigned shift=0;shift<15;shift+=5){int d=(int)((ca>>shift)&31)-(int)((cb>>shift)&31);unsigned error=d<0?-d:d;if(error>needed)needed=error;}
   }
   if(needed<best){best=needed;from=b;to=a;if(best==1)goto selected;}
  }
 }
selected:
 if(from<16){
  for(unsigned j=1;j<16;j++)if((f->palette_used[from]&~f->palette_used[to])&(1u<<j))f->palettes[to][j]=f->palettes[from][j];
  f->palette_used[to]|=f->palette_used[from];f->palette_protected|=1u<<to;
  for(unsigned i=0;i<f->count;i++)if((f->objects[i].attr2>>12)==from)f->objects[i].attr2=(f->objects[i].attr2&4095)|(to<<12);
  f->palette_merges++;if(best>f->palette_merge_error)f->palette_merge_error=best;return (int)from;
 }
 return -1;
}
/* Exhaustion is not necessarily a shortage of distinct displayable colors:
 * merge_shades requires TWO unlocked banks. Village debris can leave just
 * one. Fold that bank INTO a locked destination without changing any of its
 * occupied RGB entries. Only the unlocked source receives an approximation;
 * previously merged actors and exact HUD/player colors cannot drift again.
 * If every bank is locked, a new non-exact palette may share a stable bank
 * under the same tolerance. Exact requests must still allocate exact colors. */
static __attribute__((noinline)) int share_locked(struct NativeObjectFrame*f,const uint16_t*p,unsigned mask){
 memo_count=0;
 unsigned best=f->palette_tolerance+1,from=17,to=16;
 for(unsigned b=0;b<=16;b++){
  if(b<16){if(f->palette_protected&(1u<<b))continue;}
  else if(f->palette_exact)continue;
  const uint16_t*colors=b<16?f->palettes[b]:p;
  unsigned used_mask=b<16?f->palette_used[b]:mask;
  for(unsigned a=0;a<16;a++){
   if(!(f->palette_protected&(1u<<a)))continue;
   unsigned common=used_mask&f->palette_used[a],needed=0;
   for(unsigned j=1;j<16&&needed<best;j++)if(common&(1u<<j)){
    unsigned ca=f->palettes[a][j],cb=colors[j];
    for(unsigned shift=0;shift<15;shift+=5){int d=(int)((ca>>shift)&31)-(int)((cb>>shift)&31);unsigned error=d<0?-d:d;if(error>needed)needed=error;}
   }
   if(needed<best){best=needed;from=b;to=a;if(!best)goto selected;}
  }
 }
selected:
 if(to==16)return -1;
 const uint16_t*colors=from<16?f->palettes[from]:p;
 unsigned used_mask=from<16?f->palette_used[from]:mask;
 for(unsigned j=1;j<16;j++)if((used_mask&~f->palette_used[to])&(1u<<j))f->palettes[to][j]=colors[j];
 f->palette_used[to]|=used_mask;
 if(from<16){
  for(unsigned i=0;i<f->count;i++)if((f->objects[i].attr2>>12)==from)f->objects[i].attr2=(f->objects[i].attr2&4095)|(to<<12);
  f->palette_used[from]=0;
 }
 f->palette_spills++;if(best>f->palette_merge_error)f->palette_merge_error=best;
 /* 0..15 is a reclaimed bank, 16..31 directly selects a stable palette. */
 return from<16?(int)from:16+(int)to;
}
#define PALETTE_REMEMBER(slot_) do{unsigned k_=memo_count<PALETTE_MEMO?memo_count++:(memo_next++&(PALETTE_MEMO-1));memo_mask[k_]=(uint16_t)mask;memo_slot[k_]=(uint8_t)(slot_);}while(0)
static __attribute__((section(".iwram.obj_index_palette"),optimize("O2"),target("arm"),noinline,long_call)) int palette(struct NativeObjectFrame*f,const uint16_t*p,unsigned mask){
 if(!p){fail(f,NOBJ_METADATA);return -1;}
 /* v170: read the requested ROM colors once, and visit only up to the
  * highest common index. Same comparisons and result as before. */
 uint16_t c[16];for(unsigned j=1,b=mask>>1;b;j++,b>>=1)if(b&1)c[j]=p[j];
 for(unsigned k=0;k<memo_count;k++)if(memo_mask[k]==mask){
  unsigned i=memo_slot[k],b=mask>>1;const uint16_t*bank=f->palettes[i];
  if((f->palette_used[i]&mask)!=mask)continue;
  for(unsigned j=1;b;j++,b>>=1)if((b&1)&&bank[j]!=c[j])break;
  if(b)continue; /* same mask, other colors: try the next entry */
  if(f->palette_exact)f->palette_protected|=1u<<i;return (int)i;
 }
 int best=-1;unsigned score=0;
 for(unsigned i=0;i<f->palette_count;i++){
  if(!f->palette_used[i])continue;
  unsigned common=mask&f->palette_used[i],b=common>>1;const uint16_t*bank=f->palettes[i];
  /* v171: unrelated palettes almost always differ at their first common
   * index; test it before the ordered walk (same accept/reject set). */
  if(b){unsigned j=1,t=b;if(!(t&0xff)){t>>=8;j+=8;}if(!(t&0xf)){t>>=4;j+=4;}if(!(t&3)){t>>=2;j+=2;}if(!(t&1))j++;if(bank[j]!=c[j])continue;}
  for(unsigned j=1;b;j++,b>>=1)if((b&1)&&bank[j]!=c[j])break;
  if(b)continue;
  if(common==mask){if(f->palette_exact)f->palette_protected|=1u<<i;PALETTE_REMEMBER(i);return (int)i;}
  unsigned count=0;for(unsigned bits=common;bits;bits&=bits-1)count++;
  if(best<0||count>score){best=(int)i;score=count;}
 }
 if(best>=0){for(unsigned j=1,b=mask>>1;b;j++,b>>=1)if(b&1)f->palettes[best][j]=c[j];f->palette_used[best]|=mask;if(f->palette_exact)f->palette_protected|=1u<<best;memo_count=0;PALETTE_REMEMBER(best);return best;}
 /* Keep actor colors at their previous hardware indices whenever this
  * needs no extra bank compared with the normal compatible-bank packing. */
 int slot=-1,empty=-1;
 for(unsigned i=0;i<16;i++)if(!f->palette_used[i]){
  if(empty<0)empty=i;unsigned b=mask>>1;const uint16_t*bank=f->palettes[i];
  if(b){unsigned j=1,t=b;if(!(t&0xff)){t>>=8;j+=8;}if(!(t&0xf)){t>>=4;j+=4;}if(!(t&3)){t>>=2;j+=2;}if(!(t&1))j++;if(bank[j]!=c[j])continue;}
  for(unsigned j=1;b;j++,b>>=1)if((b&1)&&bank[j]!=c[j])break;
  if(!b){slot=i;break;}
 }
 memo_count=0; /* every path below changes the bank state */
 if(slot<0)slot=empty<0?merge_shades(f):empty;
 if(slot<0){slot=share_locked(f,p,mask);if(slot>=16)return slot-16;}
 if(slot>=0&&(unsigned)slot>=f->palette_count)f->palette_count=slot+1;
 if(slot<0){fail(f,NOBJ_PALETTES);return -1;}
 f->palette_used[slot]=mask;for(unsigned j=0;j<16;j++)f->palettes[slot][j]=p[j];if(f->palette_exact)f->palette_protected|=1u<<slot;memo_count=0;PALETTE_REMEMBER(slot);return slot;
}
/* Reserve protected actors before approximate scenery can lock every bank.
 * Full source palette reservation is conservative and pressure-only. */
int native_object_reserve_exact_palette(struct NativeObjectFrame*f,const uint16_t*p){
 uint8_t exact=f->palette_exact;f->palette_exact=1;
 int slot=palette(f,p,65534);f->palette_exact=exact;return slot>=0;
}
/* Rare capacity recovery: pointer-different chunks can contain identical
 * bytes (notably repeated HUD cells). Compact exact duplicates before
 * declaring physical OBJ VRAM exhausted. Keep all pixels and OAM entries. */
static int equal_pixels(const uint8_t*a,const uint8_t*b,unsigned bytes){
 if(a==b)return 1;
 for(unsigned i=0;i<bytes;i++)if(a[i]!=b[i])return 0;return 1;
}
/* A smaller texture may be an exact, tile-aligned substring of another.
 * Share its existing interval without changing a single source pixel. */
static int contained_tiles(const struct NativeObjectUpload*u,const uint8_t*p,unsigned count){
 if(count>u->count)return -1;
 uintptr_t start=(uintptr_t)u->pixels,at=(uintptr_t)p;
 /* Cropped rows retain a pointer into their original ROM texture. */
 if(at>=start&&at-start<=(u->count-count)*32u&&!((at-start)&31u))return (int)((at-start)/32u);
 /* Keep the original whole-texture byte deduplication. Searching every
  * possible content substring costs seconds on ARM7 in a crowded frame. */
 return count==u->count&&equal_pixels(u->pixels,p,count*32)?0:-1;
}
static __attribute__((noinline)) int compact_tiles(struct NativeObjectFrame*f){
 uint16_t old_tile[128],old_count[128],new_tile[128];
 unsigned before=f->upload_count,count=0,tiles=0,old_total=f->tile_count;
 /* Largest first so smaller exact subtextures can reuse their storage. */
 for(unsigned i=1;i<before;i++){
  struct NativeObjectUpload u=f->uploads[i];unsigned j=i;
  while(j&&f->uploads[j-1].count<u.count){f->uploads[j]=f->uploads[j-1];j--;}
  f->uploads[j]=u;
 }
 for(unsigned i=0;i<before;i++){
  struct NativeObjectUpload value=f->uploads[i];old_tile[i]=value.tile;old_count[i]=value.count;
  unsigned j;int offset=-1;
  for(j=0;j<count;j++){offset=contained_tiles(&f->uploads[j],value.pixels,value.count);if(offset>=0)break;}
  if(j==count){value.tile=tiles;tiles+=value.count;f->uploads[count++]=value;offset=0;}
  new_tile[i]=f->uploads[j].tile+(unsigned)offset;
 }
 for(unsigned i=0;i<f->count;i++){
  unsigned tile=f->objects[i].attr2&1023;
  for(unsigned j=0;j<before;j++)if(tile>=old_tile[j]&&tile<old_tile[j]+old_count[j]){
   f->objects[i].attr2=(f->objects[i].attr2&~1023u)|(new_tile[j]+tile-old_tile[j]);break;
  }
 }
 f->upload_count=count;f->tile_count=tiles;f->tile_compactions++;
 for(unsigned i=0;i<256;i++)f->scratch.uploads[i]=0;
 for(unsigned i=0;i<count;i++){
  const struct NativeObjectUpload*u=&f->uploads[i];uintptr_t key=(uintptr_t)u->pixels;unsigned slot=((key>>5)^(key>>13)^u->count)&255;
  while(f->scratch.uploads[slot])slot=(slot+1)&255;f->scratch.uploads[slot]=i+1;
 }
 return tiles<old_total;
}
static int tile_data(struct NativeObjectFrame*f,const uint8_t*p,unsigned n){
 if(!p||!n){fail(f,NOBJ_METADATA);return -1;}
 uintptr_t key=(uintptr_t)p;unsigned slot=((key>>5)^(key>>13)^n)&255;
 while(f->scratch.uploads[slot]){
  unsigned i=f->scratch.uploads[slot]-1;
  if(f->uploads[i].pixels==p&&f->uploads[i].count==n)return f->uploads[i].tile;
  slot=(slot+1)&255;
 }
 if(f->tile_count>f->tile_budget||n>(unsigned)(f->tile_budget-f->tile_count)||f->upload_count==NATIVE_OBJ_LIMIT){
  for(unsigned i=0;i<f->upload_count;i++){
   int at=contained_tiles(&f->uploads[i],p,n);if(at>=0)return f->uploads[i].tile+(unsigned)at;
  }
  if(compact_tiles(f))return tile_data(f,p,n);
  fail(f,NOBJ_TILES);return -1;
 }
 unsigned tile=f->tile_count;f->tile_count+=n;
 f->scratch.uploads[slot]=f->upload_count+1;
 f->uploads[f->upload_count++]=(struct NativeObjectUpload){p,tile,n};return (int)tile;
}
/* FIX textures are fresh mutable staging. Reindex equal RGB colors into
 * existing actor palette holes without changing any displayed pixel. */
/* v171: ROM. Measured 0.00% in missions (the compact OBJ HUD maps with a
 * fixed bank); its IWRAM bytes now hold the palette memo fast path. */
static __attribute__((optimize("O2"),noinline)) int raw_palette(struct NativeObjectFrame*f,uint8_t*p,unsigned bytes,const uint16_t*colors){
 
 unsigned mask=used(p,bytes);struct NativeObjectPaletteCache*cache=0;
 if(!colors){fail(f,NOBJ_METADATA);return -1;}
 for(unsigned i=0;i<16;i++){
  struct NativeObjectPaletteCache*c=&f->scratch.palettes[i];
  if(!c->valid){if(!cache)cache=c;continue;}
  if(c->source!=colors)continue;cache=c;
  if((mask&~c->mask)||c->bank>=f->palette_count)break;
  unsigned index;for(index=1;index<16;index++)if((mask&(1u<<index))&&(!c->map[index]||f->palettes[c->bank][c->map[index]]!=colors[index]))break;
  if(index<16)break;
  for(unsigned j=0;j<bytes;j++)p[j]=c->map[p[j]&15]|(c->map[p[j]>>4]<<4);
  f->palette_protected|=1u<<c->bank;return c->bank;
 }
 if(!cache)cache=&f->scratch.palettes[((uintptr_t)colors>>5)&15];
 for(unsigned bank=0;bank<f->palette_count;bank++){
  uint16_t candidate[16];for(unsigned j=0;j<16;j++)candidate[j]=f->palettes[bank][j];
  uint8_t map[16]={0};unsigned claimed=f->palette_used[bank],index;
  for(index=1;index<16;index++)if(mask&(1u<<index)){
   unsigned dest=1;for(;dest<16;dest++)if((claimed&(1u<<dest))&&candidate[dest]==colors[index])break;
   if(dest==16){for(dest=1;dest<16&&(claimed&(1u<<dest));dest++){}if(dest==16)break;candidate[dest]=colors[index];claimed|=1u<<dest;}
   map[index]=dest;
  }
  if(index<16)continue;
  memo_count=0;
  for(unsigned j=1;j<16;j++)f->palettes[bank][j]=candidate[j];f->palette_used[bank]=claimed;
  *cache=(struct NativeObjectPaletteCache){.source=colors,.mask=mask,.bank=bank,.valid=1};for(unsigned j=0;j<16;j++)cache->map[j]=map[j];
  for(unsigned j=0;j<bytes;j++)p[j]=map[p[j]&15]|(map[p[j]>>4]<<4);
  f->palette_protected|=1u<<bank;return bank;
 }
 int bank=palette(f,colors,mask);
 if(bank>=0){f->palette_protected|=1u<<bank;*cache=(struct NativeObjectPaletteCache){.source=colors,.mask=mask,.bank=bank,.valid=1};for(unsigned j=0;j<16;j++)cache->map[j]=j;}
 return bank;
}
int native_object_palette_mapping(struct NativeObjectFrame*f,const uint16_t*colors,unsigned mask,uint8_t map[16]){
 if(!f||f->fault||f->ready)return -1;
 for(unsigned j=0;j<16;j++)map[j]=(mask&(1u<<j))?j:0;
 return raw_palette(f,map,16,colors);
}
static int raw(struct NativeObjectFrame*f,uint8_t*p,int x,int y,unsigned w,unsigned h,const uint16_t*colors,unsigned priority,uint32_t order,int fixed_bank){
 static const uint8_t sizes[3][4][2]={{{8,8},{16,16},{32,32},{64,64}},{{16,8},{32,8},{32,16},{64,32}},{{8,16},{8,32},{16,32},{32,64}}};
 if(!f||f->fault||f->ready||priority>3)return fail(f,NOBJ_STATE);
 unsigned shape,size;for(shape=0;shape<3;shape++){for(size=0;size<4;size++)if(sizes[shape][size][0]==w&&sizes[shape][size][1]==h)break;if(size<4)break;}
 if(shape==3)return fail(f,NOBJ_METADATA);
 if(x>=240||y>=160||x+(int)w<=0||y+(int)h<=0)return 1;
 if(f->count==NATIVE_OBJ_LIMIT)return fail(f,NOBJ_CAPACITY);
 int pal;
 if(fixed_bank<=-2){pal=-fixed_bank-2;if((unsigned)pal>=f->palette_count)return fail(f,NOBJ_PALETTES);f->palette_protected|=1u<<pal;}
 else if(fixed_bank>=0){
  if(fixed_bank>=16||!colors)return fail(f,NOBJ_PALETTES);
  pal=fixed_bank;
  memo_count=0;
  if((unsigned)pal>=f->palette_count){if((unsigned)pal!=f->palette_count)return fail(f,NOBJ_PALETTES);f->palette_count++;f->palette_used[pal]=0;}
  f->palette_used[pal]|=used(p,w*h/2);for(unsigned j=0;j<16;j++)f->palettes[pal][j]=colors[j];
  f->palette_protected|=1u<<pal;
 }else pal=raw_palette(f,p,w*h/2,colors);
 if(pal<0)return 0;int tile=tile_data(f,p,w*h/64);if(tile<0)return 0;
 f->objects[f->count++]=(struct NativeObjectEntry){order,((unsigned)y&255)|(shape<<14),((unsigned)x&511)|(size<<14),(unsigned)tile|(priority<<10)|((unsigned)pal<<12)};
 return 1;
}
int native_object_raw(struct NativeObjectFrame*f,uint8_t*p,int x,int y,unsigned w,unsigned h,const uint16_t*colors,unsigned priority,uint32_t order){return raw(f,p,x,y,w,h,colors,priority,order,-1);}
int native_object_raw_fixed(struct NativeObjectFrame*f,uint8_t*p,int x,int y,unsigned w,unsigned h,const uint16_t*colors,unsigned bank){return raw(f,p,x,y,w,h,colors,0,0,(int)bank);}
int native_object_raw_mapped(struct NativeObjectFrame*f,uint8_t*p,int x,int y,unsigned w,unsigned h,unsigned bank){if(bank>=16)return fail(f,NOBJ_PALETTES);return raw(f,p,x,y,w,h,0,0,0,-2-(int)bank);}
static int floor256(int n){return n>=0?n/256:-((-n+255)/256);}
static int matrix(struct NativeObjectFrame*f,unsigned x,unsigned y,unsigned flip){
 int pa=(int)((65536u+x/2)/x),pd=(int)((65536u+y/2)/y);
 if(flip&1)pa=-pa;if(flip&2)pd=-pd;
 if(pa< -32768||pa>32767||pd< -32768||pd>32767){fail(f,NOBJ_ZOOM);return -1;}
 for(unsigned i=0;i<f->affine_count;i++)if(f->affine[i][0]==pa&&f->affine[i][1]==pd)return (int)i;
 if(f->affine_count==NATIVE_OBJ_AFFINE_LIMIT){fail(f,NOBJ_ZOOM);return -1;}
 unsigned n=f->affine_count++;f->affine[n][0]=pa;f->affine[n][1]=pd;return (int)n;
}
/* Full-zoom pressure retry: discard whole offscreen texture halves.
 * Vertical halves remain contiguous in GBA 1D tile order. No pixel repack,
 * no missing visible pixels, and no extra OAM entry. */
static void crop_vertical(struct NativeScaledChunk*c,int*y,unsigned flip){
 static const uint8_t sizes[3][4][2]={{{8,8},{16,16},{32,32},{64,64}},{{16,8},{32,8},{32,16},{64,32}},{{8,16},{8,32},{16,32},{32,64}}};
 while(c->height>=16){
  unsigned half=c->height/2,shape,size;int top=*y+(int)half<=0,bottom=*y+(int)half>=160;
  if(!top&&!bottom)return;
  for(shape=0;shape<3;shape++){for(size=0;size<4;size++)if(sizes[shape][size][0]==c->width&&sizes[shape][size][1]==half)break;if(size<4)break;}
  if(shape==3)return;
  if(top!=!!(flip&2))c->byte_offset+=(unsigned)c->width*half/2;
  if(top)*y+=half;c->height=half;c->shape=shape;c->size=size;
 }
}
/* A clipped texture may intersect the viewport only through transparent
 * padding. Reject that empty intersection on the capacity retry, before
 * spending OBJ VRAM/OAM. Test source palette indices (not RGB), preserve
 * both flips, and leave affine sampling on its existing path. */
static __attribute__((noinline)) int visible_pixels(const struct NativeScaledChunk*c,int x,int y,unsigned flip){
 if(x>=0&&y>=0&&x+(int)c->width<=240&&y+(int)c->height<=160)return 1;
 const uint8_t*p=native_scaled_pixels(c);if(!p)return 1;
 int left=x<0?-x:0,top=y<0?-y:0;
 int right=x+(int)c->width>240?240-x:c->width,bottom=y+(int)c->height>160?160-y:c->height;
 for(int yy=top;yy<bottom;yy++){
  unsigned sy=(flip&2)?c->height-1-yy:yy;
  for(int xx=left;xx<right;xx++){
   unsigned sx=(flip&1)?c->width-1-xx:xx;
   unsigned at=((sy>>3)*(c->width>>3)+(sx>>3))*32+(sy&7)*4+((sx&7)>>1);
   if((p[at]>>((sx&1)*4))&15)return 1;
  }
 }
 return 0;
}
/* Tile-exhaustion-only fallback. Horizontal halves are not contiguous in
 * 1D OBJ memory across tile rows. Keep the original row stride and expose
 * each retained 8px row as a legal OBJ instead of reinterpreting its width.
 * Small savings are not worth increasing OAM pressure. */
static __attribute__((noinline)) unsigned horizontal_span(const struct NativeScaledChunk*c,int x,unsigned*left,unsigned*width){
 unsigned w=c->width,cut=0;
 while(w>=16){unsigned half=w/2;if(x+(int)half<=0){cut+=half;x+=half;w=half;}else if(x+(int)half>=240)w=half;else break;}
 if((c->width-w)*c->height/64<16)return 1;
 *left=cut;*width=w;return c->height/8;
}
/* Pressure-only exact sampling for unit-X bridge textures. Affine canvas
 * bounds can touch the viewport even when every sampled texel is transparent.
 * Such chunks need neither OAM nor a full texture upload. */
static __attribute__((noinline)) int bridge_visible_pixels(const struct NativeScaledChunk*c,int x,int y,unsigned zy){
 const uint8_t*p=native_scaled_pixels(c);unsigned pd=(65536u+zy/2)/zy;
 int left=x<0?-x:0,right=x+c->width>240?240-x:c->width;
 int top=y<0?-y:0,bottom=y+c->height>160?160-y:c->height;
 for(int dy=top;dy<bottom;dy++){
  int sy=floor256(((dy-(int)c->height/2)*(int)pd)+(int)c->height*128);
  if(sy<0||sy>=c->height)continue;
  for(int sx=left;sx<right;sx++){
   unsigned at=((sy>>3)*(c->width>>3)+(sx>>3))*32+(sy&7)*4+((sx&7)>>1);
   if((p[at]>>((sx&1)*4))&15)return 1;
  }
 }return 0;
}
/* Under pressure, rasterize a shrinking unit-X bridge exactly as the GBA
 * affine sampler would. Reuse unused FIX staging, then emit a smaller normal
 * OBJ. No extra permanent pixel buffer and no approximation of source rows. */
static __attribute__((noinline)) const uint8_t*bridge_rasterize(struct NativeObjectFrame*f,struct NativeScaledChunk*c,const uint8_t*src,int*y,unsigned zy){
 if(!bridge_scratch)return 0;
 int pd=(int)((65536u+zy/2)/zy),first=-1,last=-1;
 for(int dy=0;dy<c->height;dy++){
  if(*y+dy<0||*y+dy>=160)continue;
  int sy=floor256((dy-(int)c->height/2)*pd+(int)c->height*128);
  if(sy>=0&&sy<c->height){if(first<0)first=dy;last=dy;}
 }
 if(first<0)return 0;
 unsigned h=8;while(h<(unsigned)(last-first+1))h*=2;
 if(h>=c->height)return 0;
 static const uint8_t sizes[3][4][2]={{{8,8},{16,16},{32,32},{64,64}},{{16,8},{32,8},{32,16},{64,32}},{{8,16},{8,32},{16,32},{32,64}}};
 unsigned shape,size;for(shape=0;shape<3;shape++){for(size=0;size<4;size++)if(sizes[shape][size][0]==c->width&&sizes[shape][size][1]==h)break;if(size<4)break;}
 if(shape==3)return 0;
 unsigned bytes=c->width*h/2;if(bytes>bridge_capacity-bridge_used)return 0;
 uint8_t*dst=bridge_scratch+bridge_used;bridge_used+=bytes;
 for(unsigned i=0;i<bytes/4;i++)((uint32_t*)dst)[i]=0;
 for(int dy=first;dy<=last;dy++){
  unsigned sy=(unsigned)floor256((dy-(int)c->height/2)*pd+(int)c->height*128),ty=(unsigned)(dy-first);
  for(unsigned bx=0;bx<c->width/8;bx++){
   unsigned from=((sy/8)*(c->width/8)+bx)*32+(sy&7)*4;
   unsigned to=((ty/8)*(c->width/8)+bx)*32+(ty&7)*4;
   *(uint32_t*)(dst+to)=*(const uint32_t*)(src+from);
  }
 }
 *y+=first;c->height=h;c->shape=shape;c->size=size;f->mutable_generation=0;
 return dst;
}
int native_object_map(struct NativeObjectFrame*f,unsigned id,const struct NativeObjectPlacement*p){
 if(!f||!p)return 0;if(f->fault||f->ready)return fail(f,NOBJ_STATE);
 const struct NativeScaledMap*m=f->dense_layout&&p->zoom_x==255&&p->zoom_y==255?
  native_scaled_dense_map(id):native_scaled_zoom_map(id,p->zoom_x,p->zoom_y);if(f->dense_layout&&p->zoom_x==255&&p->zoom_y>=2&&p->zoom_y<255){
  const struct NativeScaledMap*bridge=native_scaled_bridge_map(id);if(bridge)m=bridge;
 }if(!m)return fail(f,NOBJ_METADATA);
 if(p->flip>3||p->priority>3||p->x<-32768||p->x>32767||p->y<-32768||p->y>32767)return fail(f,NOBJ_METADATA);
 /* The two smallest ratios are already baked along the selected axes. */
 unsigned zx=p->zoom_x<2?256:(unsigned)p->zoom_x+1,zy=p->zoom_y<2?256:(unsigned)p->zoom_y+1;
 int affine=zx!=256||zy!=256;
 /* All source zooms shrink. A double-size affine canvas wastes scanline
  * cycles on transparent margins. Retain it only for the -1:1 axis whose
  * source pixel zero lies one pixel beyond the normal affine rectangle. */
 int doubled=affine&&(((p->flip&1)&&zx==256)||((p->flip&2)&&zy==256));
 int sx=native_screen_x(p->x),sy=native_screen_y(p->y),pal=-1,mat=-1;unsigned palette_mask=0;
 /* Ordinary pre-scaled ROM chunks need neither affine slicing nor the
  * capacity-recovery crop path. Avoid copying chunk metadata twice and
  * entering the nested slice/row loops for every visible object. */
 if(!affine&&!f->dense_layout){
  for(unsigned i=0;i<m->count;i++){
   const struct NativeScaledChunk*c=native_scaled_chunk(m->first_chunk+i);
   if(!c)return fail(f,NOBJ_METADATA);
   int x=sx+((p->flip&1)?-c->x-c->width:c->x);
   int y=sy+((p->flip&2)?-c->y-c->height:c->y);
   if(x>=240||y>=160||x+(int)c->width<=0||y+(int)c->height<=0)continue;
   if(f->count==NATIVE_OBJ_LIMIT)return fail(f,NOBJ_CAPACITY);
   const uint16_t*mask=native_chunk_masks[m->first_chunk+i];unsigned needed=mask[0]|mask[1];
   if(pal<0||(needed&~palette_mask)){int previous=pal;pal=palette(f,p->palette,needed);if(pal<0)return 0;palette_mask=previous==pal?palette_mask|needed:needed;}
   int tile=tile_data(f,native_scaled_pixels(c),(unsigned)c->width*c->height/64);if(tile<0)return 0;
   struct NativeObjectEntry*o=&f->objects[f->count++];o->order=p->order;
   o->attr0=((unsigned)y&255)|((unsigned)c->shape<<14);
   o->attr1=((unsigned)x&511)|((unsigned)p->flip<<12)|((unsigned)c->size<<14);
   o->attr2=(unsigned)tile|((unsigned)p->priority<<10)|((unsigned)pal<<12);
  }
  return 1;
 }
 for(unsigned i=0;i<m->count;i++){
  const struct NativeScaledChunk*base=native_scaled_chunk(m->first_chunk+i);if(!base)return fail(f,NOBJ_METADATA);
  /* Only a double-size 64px texture has the 128px Y-wrap problem.
   * Normal affine bounds keep one texture and one sampling origin. */
  unsigned slices=doubled&&base->height==64?2:1;
  for(unsigned slice=0;slice<slices;slice++){
  struct NativeScaledChunk chunk=*base;struct NativeScaledChunk*c=&chunk;
  if(slices==2){c->height=32;c->y+=slice*32;c->byte_offset+=slice*(unsigned)c->width*16;
   c->shape=c->width==64?1:0;c->size=c->width==64?3:2;}
  int x,y,bw=c->width,bh=c->height;
  if(affine){
   int cx=c->width/2,cy=c->height/2;
   int px=(p->flip&1)?-c->x-cx-1:c->x+cx,py=(p->flip&2)?-c->y-cy-1:c->y+cy;
   if(doubled){bw*=2;bh*=2;}
   x=sx+floor256(px*(int)zx)-bw/2;y=sy+floor256(py*(int)zy)-bh/2;
  }else{x=sx+((p->flip&1)?-c->x-c->width:c->x);y=sy+((p->flip&2)?-c->y-c->height:c->y);}
  if(x>=NATIVE_DISPLAY_WIDTH||y>=NATIVE_DISPLAY_HEIGHT||x+bw<=0||y+bh<=0)continue;
  if(f->dense_layout&&affine&&!p->flip&&zx==256&&(id==9153||id==9154)&&!bridge_visible_pixels(c,x,y,zy))continue;
  if(f->dense_layout&&!affine)crop_vertical(c,&y,p->flip);
  if(f->dense_layout&&!affine&&!visible_pixels(c,x,y,p->flip))continue;
  unsigned left=0,width=c->width,rows=1;
  if((f->dense_layout&2)&&!affine)rows=horizontal_span(c,x,&left,&width);
  struct NativeScaledChunk whole=*c;int first_x=x,first_y=y;
  for(unsigned row=0;row<rows;row++){
  *c=whole;x=first_x;y=first_y;
  if(width!=whole.width){
   unsigned source_x=(p->flip&1)?whole.width-left-width:left;
   unsigned source_y=(p->flip&2)?whole.height-8-row*8:row*8;
   c->byte_offset+=source_y*whole.width/2+source_x*4;
   c->width=width;c->height=8;c->shape=width==8?0:1;c->size=width==32?1:0;
   x+=left;y+=row*8;
   if(!visible_pixels(c,x,y,p->flip))continue;
  }
  if(f->count==NATIVE_OBJ_LIMIT)return fail(f,NOBJ_CAPACITY);
  const uint16_t*mask=native_chunk_masks[m->first_chunk+i];
  unsigned needed=slices==2?mask[slice]:mask[0]|mask[1];
  if(pal<0||(needed&~palette_mask)){int previous=pal;pal=palette(f,p->palette,needed);if(pal<0)return 0;palette_mask=previous==pal?palette_mask|needed:needed;}
  const uint8_t*texture=native_scaled_pixels(c);int draw_affine=affine;
  if(affine&&!p->flip&&zx==256&&(id==9153||id==9154)){
   const uint8_t*baked=bridge_rasterize(f,c,texture,&y,zy);
   if(baked){texture=baked;draw_affine=0;}
  }
  unsigned texture_tiles=(unsigned)c->width*c->height/64;
  /* Bottom clipping: hardware never samples the invisible trailing tile
   * rows. Keep the original OBJ shape, matrix and texture origin. Only its
   * upload/allocation prefix shrinks; no visible texel changes. */
  if(f->dense_layout&&!p->flip&&!doubled&&y+c->height>160){
   int last=159-y;
   if(draw_affine)last=floor256((last-(int)c->height/2)*(int)((65536u+zy/2)/zy)+(int)c->height*128);
   if(last>=0&&last<c->height){unsigned prefix=((unsigned)last/8+1)*(c->width/8);if(prefix<texture_tiles)texture_tiles=prefix;}
  }
  int tile=tile_data(f,texture,texture_tiles);if(tile<0)return 0;
  if(draw_affine&&mat<0){mat=matrix(f,zx,zy,p->flip);if(mat<0)return 0;}
  struct NativeObjectEntry*o=&f->objects[f->count++];o->order=p->order;
  o->attr0=((unsigned)y&255)|((unsigned)c->shape<<14)|(draw_affine?(doubled?0x0300:0x0100):0);
  o->attr1=((unsigned)x&511)|(draw_affine?((unsigned)mat<<9):((unsigned)p->flip<<12))|((unsigned)c->size<<14);
  o->attr2=(unsigned)tile|((unsigned)p->priority<<10)|((unsigned)pal<<12);
  }
  }
 }
 return 1;
}
int native_object_approved(struct NativeObjectFrame*f,const uint8_t*pixels,const struct NativeObjectPlacement*p){
 if(!f||!p||!pixels||f->fault||f->ready||p->zoom_x<2||p->zoom_y<2)return fail(f,NOBJ_STATE);
 unsigned zx=(unsigned)p->zoom_x+1,zy=(unsigned)p->zoom_y+1;
 int affine=zx!=256||zy!=256,sx=native_screen_x(p->x),sy=native_screen_y(p->y),pal=-1,mat=-1;
 int doubled=affine&&(((p->flip&1)&&zx==256)||((p->flip&2)&&zy==256));
 unsigned slices=doubled?2:1;
 for(unsigned slice=0;slice<slices;slice++){
  int cx=-32,cy=-56+(int)slice*32,w=64,h=doubled?32:64,x,y,bw=w,bh=h;
  if(affine){int mx=(p->flip&1)?-cx-w/2-1:cx+w/2,my=(p->flip&2)?-cy-h/2-1:cy+h/2;if(doubled){bw*=2;bh*=2;}x=sx+floor256(mx*(int)zx)-bw/2;y=sy+floor256(my*(int)zy)-bh/2;}
  else{x=sx+((p->flip&1)?-cx-w:cx);y=sy+((p->flip&2)?-cy-h:cy);}
  if(x>=240||y>=160||x+bw<=0||y+bh<=0)continue;
  if(f->count==NATIVE_OBJ_LIMIT)return fail(f,NOBJ_CAPACITY);
  if(pal<0){
   uintptr_t offset=(uintptr_t)pixels-(uintptr_t)native_approved_player_pixels;
   if(offset>=35u*2048||(offset&2047))return fail(f,NOBJ_METADATA);
   const uint16_t*m=native_player_masks[offset/2048];
   pal=palette(f,p->palette,m[0]|m[1]);if(pal<0)return 0;
  }
  int tile=tile_data(f,pixels+slice*1024,w*h/64);if(tile<0)return 0;
  if(affine&&mat<0){mat=matrix(f,zx,zy,p->flip);if(mat<0)return 0;}
  f->objects[f->count++]=(struct NativeObjectEntry){p->order,((unsigned)y&255)|(affine?(doubled?0x4300:0x0100):0),((unsigned)x&511)|(affine?((unsigned)mat<<9):((unsigned)p->flip<<12))|0xc000,(unsigned)tile|((unsigned)p->priority<<10)|((unsigned)pal<<12)};
 }
 return 1;
}
struct DrawContext {struct NativeObjectFrame*frame;NativeObjectPartResolver resolve;void*context;};
static int part(void*ctx,const struct NativeDrawPart*p,const struct NativeScaledMap*m){
 (void)m;struct DrawContext*c=ctx;struct NativeObjectPlacement out={0};
 if(!c->resolve(c->context,p,&out))return fail(c->frame,NOBJ_METADATA);
 return native_object_map(c->frame,p->map,&out);
}
int native_object_drawing(struct NativeObjectFrame*f,const struct Animation*a,NativeObjectPartResolver resolve,void*ctx){
 if(!f||!resolve)return fail(f,NOBJ_METADATA);struct DrawContext c={f,resolve,ctx};
 if(!native_scaled_drawing_visit(a,part,&c))return fail(f,NOBJ_METADATA);return 1;
}
__attribute__((section(".iwram.render_hot"),noinline,long_call)) int native_object_finish(struct NativeObjectFrame*f){
 if(!f||f->fault||f->ready)return fail(f,NOBJ_STATE);
 /* Stable merge sort of byte indices, followed by in-place permutation.
  * OAM scratch is no longer needed for uploads/palettes. This bounds busy
  * scene sorting without allocating another object array or changing ties. */
 uint8_t*a=(uint8_t*)f->oam,*b=a+128;
 unsigned prefix=0,suffix=f->count;
 while(prefix<f->count&&!f->objects[prefix].order)prefix++;
 while(suffix>prefix&&!f->objects[suffix-1].order)suffix--;
 unsigned hud=prefix+f->count-suffix,actors=suffix-prefix;
 for(unsigned i=0;i<actors;i++)a[hud+i]=prefix+i;
 if(actors<=8){
  for(unsigned i=hud+1;i<f->count;i++){
   unsigned value=a[i],j=i;while(j>hud&&f->objects[a[j-1]].order>f->objects[value].order){a[j]=a[j-1];j--;}a[j]=value;
  }
 }else for(unsigned width=1;width<actors;width*=2){
  for(unsigned start=hud;start<f->count;start+=width*2){
   unsigned middle=start+width,end=middle+width;
   if(middle>f->count)middle=f->count;if(end>f->count)end=f->count;
   unsigned left=start,right=middle;
   for(unsigned at=start;at<end;at++)b[at]=right==end||(left<middle&&f->objects[a[left]].order<=f->objects[a[right]].order)?a[left++]:a[right++];
  }
  uint8_t*swap=a;a=b;b=swap;
 }
 /* If a generic caller supplied order-zero entries inside the actor run,
  * keep their original tie order relative to suffix HUD too. */
 unsigned zeros=0;while(zeros<actors&&!f->objects[a[hud+zeros]].order)zeros++;
 if(zeros)for(unsigned i=0;i<zeros;i++)a[prefix+i]=a[hud+i];
 for(unsigned i=0;i<prefix;i++)a[i]=i;
 for(unsigned i=suffix;i<f->count;i++)a[prefix+zeros+i-suffix]=i;
 for(unsigned i=0;i<f->count;i++)if(a[i]!=i){
  struct NativeObjectEntry value=f->objects[i];unsigned j=i;
  while(a[j]!=i){unsigned next=a[j];f->objects[j]=f->objects[next];a[j]=j;j=next;}
  f->objects[j]=value;a[j]=j;
 }
 for(unsigned i=0;i<NATIVE_OBJ_LIMIT;i++){
  f->oam[i][0]=i<f->count?f->objects[i].attr0:0x0200;
  f->oam[i][1]=i<f->count?f->objects[i].attr1:0;
  f->oam[i][2]=i<f->count?f->objects[i].attr2:0;f->oam[i][3]=0;
 }
 for(unsigned i=0;i<f->affine_count;i++){
  f->oam[i*4][3]=(uint16_t)f->affine[i][0];f->oam[i*4+1][3]=0;
  f->oam[i*4+2][3]=0;f->oam[i*4+3][3]=(uint16_t)f->affine[i][1];
 }
 f->ready=1;return 1;
}
