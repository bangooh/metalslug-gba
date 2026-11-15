#include "fixed_object.h"
#include "../generated/tile-video-data.h"
#include "../generated/fixed-compact-palette-lut.h"
#ifdef __arm__
#define FIX_ROM __attribute__((target("thumb"),optimize("O3"),noinline))
#define FIX_SUFFIX __attribute__((target("thumb"),optimize("O2"),noinline))
#else
#define FIX_ROM __attribute__((noinline))
#define FIX_SUFFIX FIX_ROM
#endif
/* Cache metadata shares the unused tail of FIX staging. Large presentation
 * screens may still use all32KiB: those frames simply remain uncached. */
struct FixedCache {
 uint16_t source[16][16],palettes[16][16],used[16];
 uint16_t compact_colors[16][16];
 uint32_t compact_dirty[4];
 uint16_t text[40][28];
 struct {uint16_t attr0,attr1,tile,count,used;} entries[128];
 uint16_t banks,tiles,count,palette_count,packed_dirty;
 uint8_t packed_map[128][16],packed_bank[128];
};
_Static_assert(sizeof(struct FixedCache)<=8192,"FIX metadata must not overlap the BG loan");
static struct FixedCache*metadata(struct NativeFixedObjects*f){return (struct FixedCache*)((uint8_t*)f->pixels+sizeof(f->pixels)-sizeof(struct FixedCache));}
static uint16_t pattern(const struct NativeTileVideo*v,unsigned x,unsigned y,unsigned*bank){
 /* Fragment coordinates are bounded by x<120,y<88. This exact quotient
  * avoids ARM7 software division in every2x2 HUD fragment. */
 unsigned gx=(x*171u)>>9,gy=(y*171u)>>9;
 uint16_t word=v->fixed[gx][gy];*bank=word>>12;
 return native_fix_fragments[word&4095][(y-gy*3)*3+x-gx*3];
}
static int fail(struct NativeFixedObjects*f){f->fault=1;return 0;}
static unsigned pixel_mask(const uint8_t*data,unsigned bytes){unsigned mask=0;for(unsigned j=0;j<bytes;j++){unsigned byte=data[j];mask|=1u<<(byte&15);mask|=1u<<(byte>>4);}return mask&65534;}
FIX_ROM static int build_fixed(struct NativeFixedObjects*f,struct NativeObjectFrame*o,const struct NativeTileVideo*v,const struct NativePaletteWorld*p,int stable,unsigned offset){
 if(!f)return 0;f->storage_generation++;f->bytes=offset;f->fault=0;
 if(!o||o->fault||o->ready||!v||v->fault||!p||p->fault||p->world!=v->world)return fail(f);
 /* Build occupancy once in the form consumed by rectangle selection. */
 uint32_t occupied[16][20]={{0}};
 unsigned banks=0;
 /* Visit source glyphs first.300 is the verified transparent source glyph,
  * and every2x2 fragment is aligned so it belongs to exactly one8x8 tile. */
 for(unsigned x=0;x<40;x++)for(unsigned y=2;y<30;y++){
  unsigned word=v->fixed[x][y],glyph=word&4095;if(glyph==0x300)continue;
  unsigned mask=1u<<(word>>12);const uint16_t*fragments=native_fix_fragments[glyph];
  unsigned ink=0;for(unsigned j=0;j<9;j++)ink|=fragments[j];if(!ink)continue;
  for(unsigned by=0;by<3;by++){int py=(int)y*6-16+(int)by*2;if(py<0||py>=160)continue;
   /* A cached glyph reserves its whole6x6 cell. A changing digit may light
    * an edge fragment that was blank in the preceding digit. */
   for(unsigned bx=0;bx<3;bx++)if(stable||fragments[by*3+bx]){occupied[word>>12][py/8]|=1u<<((x*6+bx*2)/8);banks|=mask;}
  }
 }
 static const uint8_t shapes[12][2]={{1,1},{2,1},{1,2},{2,2},{4,1},{1,4},{4,2},{2,4},{4,4},{8,4},{4,8},{8,8}};
 /* Up to eight occupied cells fit in one byte. Count rectangle rows
  * without revisiting every cell for all twelve candidate OBJ shapes. */
 static const uint8_t bits[16]={0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4};
 f->banks=banks;unsigned fixed_bank=0;
 for(unsigned bank=0;bank<16;bank++){
 if(!(banks&(1u<<bank)))continue;
 uint32_t*rows=occupied[bank];
 for(unsigned y=0;y<20;y++){
 if(!rows[y])continue;
 for(unsigned x=0;x<30;x++){
  if(!(rows[y]&(1u<<x)))continue;
  unsigned best=0;int best_score=0;
  for(unsigned n=0;n<12;n++){
   unsigned w=shapes[n][0],h=shapes[n][1],count=0;if(x+w>30||y+h>20)continue;
   unsigned columns=(1u<<w)-1;
   for(unsigned yy=0;yy<h;yy++){unsigned occupied=(rows[y+yy]>>x)&columns;count+=bits[occupied&15]+bits[occupied>>4];}
   int score=(int)(count*3-w*h);if(score>best_score){best=n;best_score=score;}
  }
  unsigned w=shapes[best][0],h=shapes[best][1],bytes=w*h*32;
  if(f->bytes+bytes>sizeof(f->pixels)-(offset?sizeof(struct FixedCache):0))return fail(f);
  uint8_t*start=(uint8_t*)f->pixels+f->bytes;
  for(unsigned i=0;i<bytes;i++)start[i]=0;
  for(unsigned yy=0;yy<h;yy++)for(unsigned xx=0;xx<w;xx++){
   unsigned bit=1u<<(x+xx);if(!(rows[y+yy]&bit))continue;rows[y+yy]&=~bit;
   uint8_t*out=start+(yy*w+xx)*32;
   for(unsigned by=0;by<4;by++)for(unsigned bx=0;bx<4;bx++){
    unsigned color;uint16_t value=pattern(v,(x+xx)*4+bx,(y+yy)*4+by+8,&color);
    if(color==bank){out[by*8+bx]=value;out[by*8+bx+4]=value>>8;}
   }
  }
  const uint16_t*colors=native_palette_slot(p,bank);if(!colors)return fail(f);
  if(o->count<128)f->entry_banks[o->count]=bank;
  if(!(stable?native_object_raw_fixed(o,start,x*8,y*8,w*8,h*8,colors,fixed_bank):native_object_raw(o,start,x*8,y*8,w*8,h*8,colors,0,0)))return fail(f);
  f->bytes+=bytes;
 }
 }
 fixed_bank++;
 }
 return 1;
}
int native_fixed_objects(struct NativeFixedObjects*f,struct NativeObjectFrame*o,const struct NativeTileVideo*v,const struct NativePaletteWorld*p){return build_fixed(f,o,v,p,0,0);}
/* A pressure fallback may need more flexible packed rectangles for one
 * frame. Draw them after the cached source pixels and retain the geometry
 * for the next frame instead of rebuilding the whole HUD again. */
FIX_ROM int native_fixed_fallback(struct NativeFixedObjects*f,struct NativeObjectFrame*o,const struct NativeTileVideo*v,const struct NativePaletteWorld*p){
 if(!f->cached||!f->bytes||f->bytes>8192){f->cached=0;return native_fixed_objects(f,o,v,p);}
 unsigned bytes=f->bytes,banks=f->banks;uint8_t entry_banks[128];
 for(unsigned i=0;i<128;i++)entry_banks[i]=f->entry_banks[i];
 int ok=build_fixed(f,o,v,p,0,bytes);
 f->bytes=bytes;f->banks=banks;
 for(unsigned i=0;i<128;i++)f->entry_banks[i]=entry_banks[i];
 metadata(f)->packed_dirty=65535;f->upload_pending=1;f->suffix=2;return ok;
}
static __attribute__((section(".m1hot"),noinline,long_call)) int update_colors(struct NativeFixedObjects*f,const struct NativePaletteWorld*p){
 struct FixedCache*c=metadata(f);unsigned seen=0;
 for(unsigned i=0;i<c->count;i++){
  unsigned bank=f->entry_banks[i],hardware=c->entries[i].tile>>12;if(seen&(1u<<hardware))continue;seen|=1u<<hardware;
  const uint16_t*colors=native_palette_slot(p,bank);if(!colors)return 0;
  for(unsigned j=0;j<16;j++)c->source[bank][j]=c->palettes[hardware][j]=colors[j];
 }
 return 1;
}
static void dimensions(unsigned attr0,unsigned attr1,unsigned*w,unsigned*h){
 static const uint8_t sizes[3][4][2]={{{8,8},{16,16},{32,32},{64,64}},{{16,8},{32,8},{32,16},{64,32}},{{8,16},{8,32},{16,32},{32,64}}};
 unsigned shape=attr0>>14,size=attr1>>14;*w=sizes[shape][size][0];*h=sizes[shape][size][1];
}
/* A flashing text field can switch source palette banks without changing
 * its geometry. Rebind a rectangle only when no current glyph inside it
 * still needs the old bank; other cached rectangles retain their colors. */
static FIX_ROM int rebind(struct NativeFixedObjects*f,const struct NativeTileVideo*v,unsigned i,unsigned bank){
 struct FixedCache*c=metadata(f);unsigned w,h,old=f->entry_banks[i];
 dimensions(c->entries[i].attr0,c->entries[i].attr1,&w,&h);
 unsigned left=c->entries[i].attr1&511,top=c->entries[i].attr0&255;
 for(unsigned x=left/6;x<40&&x*6<left+w;x++)for(unsigned y=(top+16)/6;y<30&&y*6<top+h+16;y++){
  unsigned word=v->fixed[x][y];if((word&4095)!=0x300&&(word>>12)==old)return 0;
 }
 unsigned hardware=16,shared=0,previous=c->entries[i].tile>>12;
 for(unsigned j=0;j<c->count;j++)if(j!=i){if(f->entry_banks[j]==bank)hardware=c->entries[j].tile>>12;if((c->entries[j].tile>>12)==previous)shared=1;}
 if(hardware==16){if(!shared)hardware=previous;else if(c->palette_count<16)hardware=c->palette_count++;else return 0;}
 f->entry_banks[i]=bank;c->entries[i].tile=(c->entries[i].tile&4095)|(hardware<<12);return 1;
}
static FIX_ROM unsigned extend_bank(struct NativeFixedObjects*f,unsigned source,unsigned bank){
 struct FixedCache*c=metadata(f);unsigned bytes=c->entries[source].count*32u;
 if(c->count>=64||f->bytes+bytes>sizeof(f->pixels)-sizeof(*c))return 128;
 unsigned hardware=16;
 for(unsigned i=0;i<c->count;i++)if(f->entry_banks[i]==bank){hardware=c->entries[i].tile>>12;break;}
 if(hardware==16){if(c->palette_count==16)return 128;hardware=c->palette_count++;}
 unsigned i=c->count++;c->entries[i]=c->entries[source];
 c->entries[i].tile=c->tiles|(hardware<<12);f->entry_banks[i]=bank;
 c->tiles+=bytes/32;f->bytes+=bytes;c->banks|=1u<<bank;
 /* The packed span moved when source geometry grew. Rebuild every copy. */
 for(unsigned j=0;j<4;j++)c->compact_dirty[j]=0xffffffffu;c->packed_dirty=65535;return i;
}
static __attribute__((section(".m1hot"),noinline,long_call)) int patch_text(struct NativeFixedObjects*f,const struct NativeTileVideo*v,int tracked){
 struct FixedCache*c=metadata(f);uint8_t changed[128]={0};
 for(unsigned x=0;x<40;x++){
 unsigned mask=tracked?(v->fixed_dirty[x]>>2)&0x0fffffffu:0x0fffffffu;
 for(unsigned y=0;mask;y++,mask>>=1){
  if(!(mask&1))continue;
  unsigned word=v->fixed[x][y+2];if(word==c->text[x][y])continue;
  int left=x*6,top=(int)(y+2)*6-16;unsigned bank=word>>12,old_bank=c->text[x][y]>>12;
  for(unsigned i=0;i<c->count;i++){
   if(f->entry_banks[i]!=bank&&f->entry_banks[i]!=old_bank)continue;
   unsigned w,h;dimensions(c->entries[i].attr0,c->entries[i].attr1,&w,&h);
   int a=c->entries[i].attr1&511,b=c->entries[i].attr0&255;
   if(left<a+(int)w&&left+6>a&&top<b+(int)h&&top+6>b)changed[i]=1;
  }
  int complete=0,visible_top=top<0?0:top,visible_bottom=top+6>160?160:top+6;
  for(unsigned i=0;i<c->count;i++)if(f->entry_banks[i]==bank){
   unsigned w,h;dimensions(c->entries[i].attr0,c->entries[i].attr1,&w,&h);int a=c->entries[i].attr1&511,b=c->entries[i].attr0&255;
   if(left>=a&&left+6<=a+(int)w&&visible_top>=b&&visible_bottom<=b+(int)h){complete=1;break;}
  }
  const uint16_t*fragments=native_fix_fragments[word&4095];
  if(!complete)for(unsigned by=0;by<3;by++)for(unsigned bx=0;bx<3;bx++){
   int px=left+bx*2,py=top+by*2;if(py<0||py>=160||!fragments[by*3+bx])continue;
   unsigned i;for(i=0;i<c->count;i++)if(f->entry_banks[i]==bank){
    unsigned w,h;dimensions(c->entries[i].attr0,c->entries[i].attr1,&w,&h);unsigned a=c->entries[i].attr1&511,b=c->entries[i].attr0&255;
    if(px>=(int)a&&px<(int)(a+w)&&py>=(int)b&&py<(int)(b+h))break;
   }
   if(i==c->count){
    for(i=0;i<c->count;i++){
     unsigned w,h;dimensions(c->entries[i].attr0,c->entries[i].attr1,&w,&h);unsigned a=c->entries[i].attr1&511,b=c->entries[i].attr0&255;
     if(px>=(int)a&&px<(int)(a+w)&&py>=(int)b&&py<(int)(b+h)&&rebind(f,v,i,bank)){changed[i]=1;break;}
    }
    if(i==c->count){
     /* A shared rectangle can contain a flashing counter beside a steady
      * counter. Retain it for the old bank and add one reusable bank plane. */
     unsigned previous=c->count;
     for(i=0;i<previous;i++){
      unsigned w,h;dimensions(c->entries[i].attr0,c->entries[i].attr1,&w,&h);unsigned a=c->entries[i].attr1&511,b=c->entries[i].attr0&255;
      if(px>=(int)a&&px<(int)(a+w)&&py>=(int)b&&py<(int)(b+h)){unsigned extra=extend_bank(f,i,bank);if(extra<128){changed[extra]=1;break;}}
     }
     if(i==previous)return 0;
    }
   }
  }
  c->text[x][y]=word;
 }
 }
 for(unsigned i=0;i<c->count;i++)if(changed[i]){
  c->compact_dirty[i>>5]|=1u<<(i&31);
  unsigned bank=f->entry_banks[i],w,h;
  dimensions(c->entries[i].attr0,c->entries[i].attr1,&w,&h);
  unsigned left=c->entries[i].attr1&511,top=c->entries[i].attr0&255;
  uint8_t*start=(uint8_t*)f->pixels+(c->entries[i].tile&1023)*32;
  for(unsigned ty=0;ty<h/8;ty++)for(unsigned tx=0;tx<w/8;tx++){
   uint8_t*out=start+(ty*(w/8)+tx)*32;
   for(unsigned by=0;by<4;by++)for(unsigned bx=0;bx<4;bx++){
    unsigned source_bank;uint16_t value=pattern(v,left/2+tx*4+bx,top/2+ty*4+by+8,&source_bank),mapped=0;
    if(source_bank==bank)mapped=value;
    out[by*8+bx]=mapped;out[by*8+bx+4]=mapped>>8;
   }
  }
  c->entries[i].used=pixel_mask(start,w*h/2);
 }
 /* Actors may share genuinely unused FIX indices. Recompute the claim when
  * a glyph changes before any actor palette is allocated this frame. */
 for(unsigned bank=0;bank<16;bank++)c->used[bank]=0;
 for(unsigned i=0;i<c->count;i++){
  c->used[c->entries[i].tile>>12]|=c->entries[i].used;
 }
 c->packed_dirty=65535;f->revision=v->fixed_revision;f->upload_pending=1;return 1;
}
static FIX_ROM int cache_fixed(struct NativeFixedObjects*f,struct NativeObjectFrame*o,const struct NativeTileVideo*v,const struct NativePaletteWorld*p){
 struct FixedCache*c=metadata(f);
 if(f->bytes>sizeof(f->pixels)-sizeof(*c))return 1;
 *c=(struct FixedCache){.tiles=o->tile_count,.count=o->count,.palette_count=o->palette_count,.banks=f->banks,.packed_dirty=65535};
 for(unsigned b=0;b<16;b++)for(unsigned j=0;j<16;j++)c->compact_colors[b][j]=65535;
 for(unsigned i=0;i<4;i++)c->compact_dirty[i]=0xffffffffu;
 for(unsigned x=0;x<40;x++)for(unsigned y=0;y<28;y++)c->text[x][y]=v->fixed[x][y+2];
 for(unsigned b=0;b<16;b++){
  if(c->banks&(1u<<b)){const uint16_t*colors=native_palette_slot(p,b);if(!colors)return fail(f);for(unsigned j=0;j<16;j++)c->source[b][j]=colors[j];}
  c->used[b]=b<o->palette_count?o->palette_used[b]:0;for(unsigned j=0;j<16;j++)c->palettes[b][j]=o->palettes[b][j];
 }
 for(unsigned i=0;i<o->count;i++){c->entries[i].attr0=o->objects[i].attr0;c->entries[i].attr1=o->objects[i].attr1;c->entries[i].tile=o->objects[i].attr2;c->entries[i].count=o->uploads[i].count;c->entries[i].used=pixel_mask(o->uploads[i].pixels,o->uploads[i].count*32u);}
 f->revision=v->fixed_revision;f->cached=1;f->bottom_clean=0;return 1;
}
static __attribute__((section(".m1hot"),noinline,long_call)) int refresh_fixed(struct NativeFixedObjects*f,const struct NativeTileVideo*v,const struct NativePaletteWorld*p){
 struct FixedCache*c=metadata(f);
 if(f->cached&&f->revision!=v->fixed_revision){
  /* Source HUD code clears and rewrites unchanged glyphs in one tick.
   * Compare the finished surface, not its intermediate write revision. */
  unsigned tracked=0;for(unsigned x=0;x<40;x++)tracked|=v->fixed_dirty[x]&0x3ffffffcu;
  unsigned x;for(x=0;x<40;x++){
   unsigned mask=tracked?(v->fixed_dirty[x]>>2)&0x0fffffffu:0x0fffffffu,y;
   for(y=0;mask;y++,mask>>=1)if((mask&1)&&c->text[x][y]!=v->fixed[x][y+2])break;
   if(mask)break;
  }
  if(x==40)f->revision=v->fixed_revision;
  else{f->bottom_clean=0;if(!patch_text(f,v,tracked!=0))f->cached=0;}
 }
 return f->cached&&f->revision==v->fixed_revision&&update_colors(f,p);
}
FIX_ROM int native_fixed_prefix(struct NativeFixedObjects*f,struct NativeObjectFrame*o,const struct NativeTileVideo*v,const struct NativePaletteWorld*p){
 struct FixedCache*c=metadata(f);
 if(f->suffix){f->upload_pending=1;f->suffix=0;}
 if(refresh_fixed(f,v,p)){
native_object_palette_touch();   o->palette_count=c->palette_count;o->tile_count=c->tiles;o->count=0;
   o->palette_protected=(1u<<c->palette_count)-1;
   for(unsigned b=0;b<c->palette_count;b++){o->palette_used[b]=c->used[b];for(unsigned j=0;j<16;j++)o->palettes[b][j]=c->palettes[b][j];}
   for(unsigned i=0;i<c->count;i++){
    if(!c->entries[i].used)continue;
    o->objects[o->count++]=(struct NativeObjectEntry){0,c->entries[i].attr0,c->entries[i].attr1,c->entries[i].tile};
    if(f->upload_pending){unsigned tile=c->entries[i].tile&1023;o->uploads[o->upload_count++]=(struct NativeObjectUpload){(const uint8_t*)f->pixels+tile*32,tile,c->entries[i].count};}
   }
   /* Stable FIX prefix remains in OBJ VRAM. Actors start after c->tiles. */
   return 1;
 }
 f->cached=0;f->upload_pending=1;if(!build_fixed(f,o,v,p,1,0))return 0;
 return cache_fixed(f,o,v,p);
}
/* Bottom text can remain unchanged for an entire scene, so the ordinary
 * dirty-glyph path cannot repair a stale source rectangle. Validate these
 * small persistent cells before compact packing; only mismatches are written.
 * This also repairs cached source data restored from an affected savestate. */
static void repair_bottom(struct NativeFixedObjects*f,const struct NativeTileVideo*v){
 struct FixedCache*c=metadata(f);unsigned changed=0;
 if(!f->cached)return;
 for(unsigned i=0;i<c->count;i++){
  unsigned w,h;dimensions(c->entries[i].attr0,c->entries[i].attr1,&w,&h);
  unsigned left=c->entries[i].attr1&511,top=c->entries[i].attr0&255;
  if(top+h<=152)continue;
  unsigned bank=f->entry_banks[i],different=0;
  uint8_t*start=(uint8_t*)f->pixels+(c->entries[i].tile&1023)*32;
  for(unsigned yy=0;yy<h/8;yy++)for(unsigned xx=0;xx<w/8;xx++){
   uint8_t*out=start+(yy*(w/8)+xx)*32;
   for(unsigned by=0;by<4;by++)for(unsigned bx=0;bx<4;bx++){
    unsigned source_bank;uint16_t value=pattern(v,left/2+xx*4+bx,top/2+yy*4+by+8,&source_bank);
    if(source_bank!=bank)value=0;
    if(out[by*8+bx]!=(uint8_t)value||out[by*8+bx+4]!=(uint8_t)(value>>8)){
     out[by*8+bx]=(uint8_t)value;out[by*8+bx+4]=(uint8_t)(value>>8);different=1;
    }
   }
  }
  if(different){c->entries[i].used=pixel_mask(start,w*h/2);c->compact_dirty[i>>5]|=1u<<(i&31);changed=1;}
 }
 if(changed){
  for(unsigned b=0;b<16;b++)c->used[b]=0;
  for(unsigned i=0;i<c->count;i++)c->used[c->entries[i].tile>>12]|=c->entries[i].used;
  c->packed_dirty=65535;f->upload_pending=1;
 }
}

/* M1 needs the cached geometry and colors, not an intermediate OAM frame. */
__attribute__((section(".m1hot"),noinline,long_call)) int native_fixed_refresh(struct NativeFixedObjects*f,struct NativeObjectFrame*o,const struct NativeTileVideo*v,const struct NativePaletteWorld*p){
 if(refresh_fixed(f,v,p)){
  /* v171: bottom entries are a pure function of FIX rows 17..29, which the
   * text snapshot (rows 2..29) mirrors. The source rewrites unchanged
   * glyphs every tick, so write-dirty bits are not a change signal; only a
   * text patch or a new cache makes the repair necessary. */
  if(!f->bottom_clean){repair_bottom(f,v);f->bottom_clean=1;}
  /* Preserve the prefix's palette history: the actor allocator uses colors
   * in unclaimed banks to keep stable bank assignments under pressure. */
  struct FixedCache*c=metadata(f);
  for(unsigned b=0;b<c->palette_count;b++)for(unsigned j=0;j<16;j++)o->palettes[b][j]=c->palettes[b][j];
  native_object_palette_touch();return 1;
 }
 f->cached=0;f->upload_pending=1;
 if(!build_fixed(f,o,v,p,1,0)||!cache_fixed(f,o,v,p))return 0;
 unsigned budget=o->tile_budget;native_object_begin(o,budget);return 1;
}
/* Actor-first palettes can require different FIX indices each frame. Keep
 * the source-indexed geometry in the first span and remap a second span;
 * never destroy the cache merely because the palette allocation differed. */
static __attribute__((noinline,long_call)) void pack_pixels(uint32_t*out,const uint32_t*in,unsigned bytes,const uint8_t lookup[256]){
 /* Staging is word aligned. Four byte transfers on the16-bit EWRAM bus
  * cost more than one word transfer; share this lookup across a whole bank. */
 for(unsigned i=0;i<bytes/4;i++){uint32_t v=in[i];out[i]=lookup[v&255]|((uint32_t)lookup[(v>>8)&255]<<8)|((uint32_t)lookup[(v>>16)&255]<<16)|((uint32_t)lookup[v>>24]<<24);}
}
FIX_SUFFIX __attribute__((long_call)) int native_fixed_suffix(struct NativeFixedObjects*f,struct NativeObjectFrame*o,const struct NativeTileVideo*v,const struct NativePaletteWorld*p){
 struct FixedCache*c=metadata(f);f->suffix=1;
 if(c->packed_dirty==32768){c->packed_dirty=65535;for(unsigned i=0;i<4;i++)c->compact_dirty[i]=0xffffffffu;}
 if(!refresh_fixed(f,v,p)||2u*f->bytes>sizeof(f->pixels)-sizeof(*c)){
  f->cached=0;return native_fixed_objects(f,o,v,p);
 }
 /* Map each rectangle's used colors independently. Requiring a whole
  * source bank to fit one actor bank needlessly failed under pressure,
  * discarding this cache even though the exact packed HUD still fit. */
 uint8_t lookup[256],last_map[16];int lookup_valid=0;
 unsigned memo_source=16,memo_mask=0,memo_bank=0;const uint8_t*memo_map=0;
 for(unsigned i=0;i<c->count;i++){
  unsigned mask=c->entries[i].used;if(!mask)continue;
  unsigned source=c->entries[i].tile>>12,hardware=c->packed_bank[i],j;
  const uint16_t*colors=c->palettes[source];uint8_t*map=c->packed_map[i];
  int valid=0,changed=c->packed_dirty||f->upload_pending;
  /* Adjacent rectangles from one source bank often use a subset of
   * colors already reserved by the preceding rectangle this frame. */
  if(source==memo_source&&!(mask&~memo_mask)){
   hardware=memo_bank;valid=2;
   for(j=0;j<16;j++){if(map[j]!=memo_map[j])changed=1;map[j]=memo_map[j];}
  }
  if(!valid)for(unsigned attempt=0;attempt<=o->palette_count;attempt++){
   unsigned target=attempt?attempt-1:hardware;if(target>=o->palette_count)continue;
   uint16_t candidate[16];unsigned claimed=o->palette_used[target];
   for(j=0;j<16;j++)candidate[j]=o->palettes[target][j];
   for(j=1;j<16;j++)if(mask&(1u<<j)){
    unsigned dest=map[j];
    if(!dest||((claimed&(1u<<dest))&&candidate[dest]!=colors[j]))break;
    candidate[dest]=colors[j];claimed|=1u<<dest;
   }
   if(j==16){hardware=target;valid=1;break;}
  }
  if(!valid){
   uint8_t fresh[16];int target=native_object_palette_mapping(o,colors,mask,fresh);if(target<0)return fail(f);
   for(j=0;j<16;j++){if(map[j]!=fresh[j])changed=1;map[j]=fresh[j];}hardware=target;
  }else if(valid==1)for(j=1;j<16;j++)if(mask&(1u<<j)){
   unsigned dest=map[j];o->palettes[hardware][dest]=colors[j];o->palette_used[hardware]|=1u<<dest;native_object_palette_touch();
  }
  memo_source=source;memo_mask=mask;memo_bank=hardware;memo_map=map;
  c->packed_bank[i]=hardware;o->palette_protected|=1u<<hardware;
  unsigned w,h,tile=c->entries[i].tile&1023,bytes=c->entries[i].count*32u;
  uint32_t*copy=(uint32_t*)((uint8_t*)f->pixels+f->bytes)+tile*8;
  if(changed){
   for(j=0;j<16&&lookup_valid&&last_map[j]==map[j];j++){}
   if(j<16){
    for(unsigned hi=0;hi<16;hi++)for(unsigned lo=0;lo<16;lo++)lookup[hi*16+lo]=(map[hi]<<4)|map[lo];
    for(j=0;j<16;j++)last_map[j]=map[j];lookup_valid=1;
   }
   pack_pixels(copy,f->pixels+tile*8,bytes,lookup);
  }
  dimensions(c->entries[i].attr0,c->entries[i].attr1,&w,&h);
  if(!native_object_raw_mapped(o,(uint8_t*)copy,c->entries[i].attr1&511,c->entries[i].attr0&255,w,h,hardware))return fail(f);
 }
 c->packed_dirty=0;return 1;
}

/* One OBJ palette for the M1 HUD leaves fifteen banks for gameplay.
 * Geometry stays cached; only changed glyphs or colour mappings are packed. */
__attribute__((section(".m1hot"),noinline,long_call)) int native_fixed_compact(struct NativeFixedObjects*f,struct NativeObjectFrame*o){
 /* Keep the HUD pale yellow exact in the compact path; the nearby gray
  * remains represented by RGB555(16,16,16). */
 static const uint16_t colors[16]={0,0,32767,1023,31,16,992,31744,32736,25368,12684,528,16912,31775,511,16383};
 struct FixedCache*c=metadata(f);if(!f->cached)return fail(f);
 /* Suffix palettes and compact palettes use different map formats. */
 if(!c->packed_dirty)for(unsigned i=0;i<4;i++)c->compact_dirty[i]=0xffffffffu;
 /* A long POW roster can exceed the two-span cache budget. Its source
  * still fits: pack in place for this frame and rebuild on the next one.
  * Never overwrite the metadata tail or retain remapped source indices. */
 unsigned inplace=2u*f->bytes>sizeof(f->pixels)-sizeof(*c);
native_object_palette_touch(); o->palette_count=1;o->palette_used[0]=65534;o->palette_protected=1;
 for(unsigned j=0;j<16;j++)o->palettes[0][j]=colors[j];
 static uint32_t generation;unsigned content_changed=0;
 unsigned changed_banks=0;
 for(unsigned bank=0;bank<c->palette_count;bank++){
  unsigned changed=0;
  for(unsigned j=1;j<16;j++)if(c->compact_colors[bank][j]!=c->palettes[bank][j])changed=1;
  if(changed)changed_banks|=1u<<bank;else if(c->packed_dirty==32768)continue;c->packed_map[bank][0]=0;
  for(unsigned j=1;j<16;j++){
   unsigned color=c->palettes[bank][j];c->compact_colors[bank][j]=color;
   /* RGB555 distance and first-index ties were compiled from colors[].
    * Bit15 was ignored by the original three-channel distance loop. */
   c->packed_map[bank][j]=native_fixed_compact_nearest[color&32767];
  }
 }
 for(unsigned i=0;i<c->count;i++){
  if(!c->entries[i].used)continue;
  unsigned bank=c->entries[i].tile>>12,changed=inplace||(c->compact_dirty[i>>5]&(1u<<(i&31)))||(changed_banks&(1u<<bank));
  const uint8_t*map=c->packed_map[bank];
  unsigned tile=c->entries[i].tile&1023,bytes=c->entries[i].count*32u,w,h;
  uint8_t*dst=(uint8_t*)f->pixels+(inplace?0:f->bytes)+tile*32;const uint8_t*src=(const uint8_t*)f->pixels+tile*32;
  if(changed){content_changed=1;for(unsigned j=0;j<bytes;j++)dst[j]=map[src[j]&15]|(map[src[j]>>4]<<4);}
  dimensions(c->entries[i].attr0,c->entries[i].attr1,&w,&h);
  if(!native_object_raw_mapped(o,dst,c->entries[i].attr1&511,c->entries[i].attr0&255,w,h,0))return fail(f);
 }
 if(inplace)f->cached=0;
 if(content_changed||!generation){generation++;if(!generation)generation++;}
 o->mutable_generation=generation;
 for(unsigned i=0;i<4;i++)c->compact_dirty[i]=0;
 c->packed_dirty=32768;return 1;
}
