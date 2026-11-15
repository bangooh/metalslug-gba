#include "runtime.h"
#include "../src/hud_bg.h"
#include "tile_stream.h"
#include "map_view.h"
#include "../src/native_video.h"
#include "../src/play_world.h"
#include "../src/scene_runtime.h"
#include "../generated/m1-assets.h"
#include "../generated/m2-assets.h"
#include "../generated/background-animation158.h"
#include "../generated/background-animation-row-index.h"
#include "../generated/opening-programs.h"
#include "../generated/script151-programs.h"
#include "../generated/village-programs.h"
#include "../generated/boss-programs.h"
/* One renderer, stage-local ROM catalogs. No source-cell composition or
 * palette search is used for either mission at runtime. */
struct StagePatchBinding {const struct SceneTilePatch*source;uint16_t image,patch;};
#include "campaign_bindings.inc"
struct Cartridge {
 const struct M1Image*images;const struct M1Patch*patches;const struct M1Op*ops;
 const uint16_t*maps,*patchmaps,(*transitions)[2],*ids4,*attrs4,*palette4;
 const uint32_t*tiles4;const uint8_t*opacity;
 unsigned first_image,image_count,tile_count;
 const struct StagePatchBinding*bindings;unsigned binding_count;
 const uint32_t*map_rows;const uint16_t*opacity_ids;const uint16_t*palette_bindings;unsigned repeat_x_mask;
};
static const struct Cartridge cartridges[]={
 {m1_images,m1_patches,m1_ops,m1_maps,m1_patchmaps,m1_transitions,m1_ids4,m1_attrs4,m1_palette4,m1_tiles4,m1_opacity,0,15,M1_TILE_COUNT,0,0,0,0,m1_semantic4,0},
 {m2_images,m2_patches,m2_ops,m2_maps,m2_patchmaps,m2_transitions,m2_ids4,m2_attrs4,m2_palette4,m2_tiles4,m2_opacity,15,7,M2_TILE_COUNT,0,0,0,0,0,0},
 {m3_images,m3_patches,m3_ops,m3_maps,m3_patchmaps,m3_transitions,m3_ids4,m3_attrs4,m3_palette4,m3_tiles4,m3_opacity,26,8,M3_TILE_COUNT,campaign3_bindings,sizeof(campaign3_bindings)/sizeof(*campaign3_bindings),m3_map_rows,m3_opacity_ids,m3_semantic4,M3_REPEAT_X_MASK},
 {m4_images,cart_m4_patches,m4_ops,m4_maps,m4_patchmaps,m4_transitions,m4_ids4,m4_attrs4,cart_m4_palette4,m4_tiles4,m4_opacity,21,5,M4_TILE_COUNT,campaign4_bindings,sizeof(campaign4_bindings)/sizeof(*campaign4_bindings),m4_map_rows,m4_opacity_ids,m4_semantic4,0},
 {m5_images,m5_patches,m5_ops,m5_maps,m5_patchmaps,m5_transitions,m5_ids4,m5_attrs4,m5_palette4,m5_tiles4,m5_opacity,34,1,M5_TILE_COUNT,campaign5_bindings,sizeof(campaign5_bindings)/sizeof(*campaign5_bindings),m5_map_rows,m5_opacity_ids,m5_semantic4,0},
 {m6_images,m6_patches,m6_ops,m6_maps,m6_patchmaps,m6_transitions,m6_ids4,m6_attrs4,m6_palette4,m6_tiles4,m6_opacity,35,8,M6_TILE_COUNT,campaign6_bindings,sizeof(campaign6_bindings)/sizeof(*campaign6_bindings),m6_map_rows,m6_opacity_ids,m6_semantic4,M6_REPEAT_X_MASK}
};
static const struct Cartridge*assets;
const uint16_t*m1_runtime_palette(void){return assets->palette4;}
const uint16_t*m1_runtime_palette_bindings(void){return assets->palette_bindings;}
#ifdef __arm__
#define M1HOT __attribute__((section(".m1hot"),noinline,long_call))
#define M1DMA M1HOT __attribute__((target("arm")))
extern unsigned char __bg_overlay_start[],__bg_legacy_load[],__bg_m1_load[],__bg_legacy_size[],__bg_m1_size[],__opening_load[],__opening_size[];
volatile unsigned char m1_overlay_kind=255;
void m1_runtime_load(unsigned native){
 if(m1_overlay_kind==native)return;
 uint32_t*dst=(uint32_t*)__bg_overlay_start;const uint32_t*src=(const uint32_t*)(native==2?__opening_load:native?__bg_m1_load:__bg_legacy_load);
 unsigned bytes=(uintptr_t)(native==2?__opening_size:native?__bg_m1_size:__bg_legacy_size);
 for(unsigned i=0;i<(bytes+3)/4;i++)dst[i]=src[i];
 __asm__ volatile("":::"memory");m1_overlay_kind=(unsigned char)native;
}
#else
#define M1HOT
#define M1DMA
volatile unsigned char m1_overlay_kind=255;
void m1_runtime_load(unsigned native){(void)native;}
#endif
struct History {uint16_t image,count,patches[192],revision;};
struct State {int16_t stab_x[4],stab_y[4],true_x[4],true_y[4];int8_t dir_x[4],dir_y[4];uint8_t stab_plane[4],stab_valid;struct M1TileStream stream[2];uint8_t groups;uint16_t cleared_image[4];struct M1MapFrame frame;struct History history[4];uint8_t priority[4];int16_t cache_x[4],cache_y[4];uint16_t cache_revision[4],cache_image[4];uint8_t cache_plane[4],cache_valid[4],planned,culling,patched;uint16_t previous_sx[4],previous_sy[4];uint8_t dirty_start[4][32],dirty_end[4][32];uint8_t pool1_scratch,committed_groups,committed,inherited_display;uint8_t animation_phase,animation_layers,animation_last;uint32_t animation_clock;uint16_t staged_frames;uint32_t staged_bytes;};
_Static_assert(sizeof(struct State)<=sizeof(((struct NativeBackgroundFrame*)0)->tiles),"M1 replaces legacy pixel staging");
static struct State*state(struct NativeVideo*v){return (struct State*)v->backgrounds.tiles;}
#include "../src/native_snapshot.h"
unsigned m1_runtime_snapshot(struct NativeVideo*v,struct NativeSnapshotRange*r){
 struct State*s=state(v);r[0]=(struct NativeSnapshotRange){s->history,sizeof(s->history)};
 r[1]=(struct NativeSnapshotRange){s->cleared_image,sizeof(s->cleared_image)};return 2;
}
void m1_runtime_rebuild(struct NativeVideo*v){
 struct State*s=state(v);struct History history[4];uint16_t cleared[4];
 for(unsigned i=0;i<4;i++){history[i]=s->history[i];cleared[i]=s->cleared_image[i];}
 v->backgrounds=(struct NativeBackgroundFrame){0};v->backgrounds.depth4=1;v->bg4_replanned=1;
 m1_runtime_restore(v);m1_stream_init(&s->stream[0],1024);m1_stream_init(&s->stream[1],768);
 for(unsigned i=0;i<4;i++){s->history[i]=history[i];s->cleared_image[i]=cleared[i];
  for(unsigned y=0;y<32;y++)s->dirty_end[i][y]=32;s->frame.dirty_rows[i]=0xffffffffu;}
}
static int floor8(int n){return n>=0?n/8:-((-n+7)/8);}
static __attribute__((unused)) int scale(int n){return n>=0?n*3/4:-((-n*3+3)/4);}
/* Preserve 16.16 parallax fractions until the final 75% pixel conversion.
 * Matches the old ceil(3*x/4) at every integral source coordinate.
 * Split integer/fraction terms avoid overflow across signed16 map wraps. */
static int scroll_x_fixed(uint32_t x){
 int whole=(int16_t)(x>>16);
 unsigned fraction=x&65535;
 return (whole*3+(int)((fraction*3+65535)>>16)+3)>>2;
}

/* v189: layer scroll in 75% pixels (the source plane position, its image
 * origin; Y keeps its fraction like X). */
static void layer_true(struct NativeVideo*v,struct State*s,struct SceneRuntime*r,unsigned p,int*ox,int*oy){
 int sx=scroll_x_fixed(r->maps.planes[p].x),sy=scroll_x_fixed(r->maps.planes[p].y)+16;
 sx+=assets->images[s->history[p].image].origin_x;sy+=assets->images[s->history[p].image].origin_y;
 if(assets->first_image==15&&s->history[p].image==6){
  unsigned generation=v->world->enemies.m5_scene.relink_generation;
  sx+=generation>2?480:generation==2?240:0;
 }
 *ox=sx;*oy=sy;
}
/* v189: parallax step stabilisation (layers that move differently from
 * plane 0, the ground the sprites stand on). Per axis:
 *  - steps follow the ground: while the ground stands still a parallax layer
 *    holds its pixel (corrected only once it is 2 px behind its own value);
 *  - hysteresis: it never steps against its last direction by a single
 *    pixel; a reversal waits until the true value is 2 px away.
 * The shown value stays within 1 px of the true one (2 px at most for one
 * tick), and tiles are planned for the value actually shown. ROM. */
static int sgn(int n){return (n>0)-(n<0);}
static int stabilise(int truth,int prev_truth,int shown,int8_t*dir,int ground_step){
 int err=truth-shown,step=truth-prev_truth;
 if(err==0)return shown;
 if(err<=-3||err>=3){*dir=(int8_t)sgn(err);return truth;}
 if(step==ground_step)goto follow;             /* moving with the ground: no smoothing needed */
 if(ground_step==0){if(err>=2||err<=-2){*dir=(int8_t)sgn(err);return shown+sgn(err);}return shown;}
 if(sgn(err)!=sgn(ground_step)){if(err>=2||err<=-2){*dir=(int8_t)sgn(err);return truth;}return shown;}
 if(*dir&&sgn(err)!=*dir&&err>-2&&err<2)return shown; /* single-pixel reversal: hold */
follow:
 *dir=(int8_t)sgn(err);return truth;
}
static __attribute__((noinline)) void stable_scrolls(struct NativeVideo*v,struct State*s,struct SceneRuntime*r,const unsigned*order,unsigned count,int*out_x,int*out_y){
 int tx[4],ty[4];unsigned ground=4;
 for(unsigned l=0;l<count;l++){layer_true(v,s,r,order[l],&tx[l],&ty[l]);if(order[l]==0)ground=l;}
 int gdx=0,gdy=0,gvalid=ground<4&&(s->stab_valid&(1u<<ground))&&s->stab_plane[ground]==0;
 if(gvalid){gdx=tx[ground]-s->true_x[ground];gdy=ty[ground]-s->true_y[ground];}
 for(unsigned l=0;l<count;l++){
  int valid=(s->stab_valid&(1u<<l))&&s->stab_plane[l]==order[l];
  if(l==ground||!gvalid||!valid||gdx<-8||gdx>8||gdy<-8||gdy>8){s->stab_x[l]=(int16_t)tx[l];s->stab_y[l]=(int16_t)ty[l];s->dir_x[l]=s->dir_y[l]=0;}
  else{
   s->stab_x[l]=(int16_t)stabilise(tx[l],s->true_x[l],s->stab_x[l],&s->dir_x[l],gdx);
   s->stab_y[l]=(int16_t)stabilise(ty[l],s->true_y[l],s->stab_y[l],&s->dir_y[l],gdy);
  }
  s->true_x[l]=(int16_t)tx[l];s->true_y[l]=(int16_t)ty[l];s->stab_plane[l]=(uint8_t)order[l];
  out_x[l]=s->stab_x[l];out_y[l]=s->stab_y[l];
 }
 s->stab_valid=(uint8_t)((1u<<count)-1);
}
/* Preserve the actually displayed tile slots across native mission catalogs.
 * IDs are stage-local, but VRAM addresses survive the catalog reset. */
static void inherit_display(struct State*s,const uint16_t*scroll_x,const uint16_t*scroll_y){
#ifdef __arm__
 const volatile uint16_t*io=(const volatile uint16_t*)0x04000000;
 unsigned control=io[0];if(control&7)return;
 for(unsigned l=0;l<4;l++)if(control&(0x100u<<l)){
  unsigned cfg=io[4+l],block=(cfg>>2)&3;
  if((cfg&0xc080)||((block!=0&&block!=2)&&(l!=3||cfg!=0x1f0c)))return;
 }
 for(unsigned l=0;l<4;l++)if(control&(0x100u<<l)){
  unsigned cfg=io[4+l],block=(cfg>>2)&3,group=block/2;
  const volatile uint16_t*map=(const volatile uint16_t*)(0x06000000+((cfg>>8)&31)*2048);
  /* The previous native scene can display only the unscrolled BG3 HUD.
   * Its character block3 overlaps stream1 at tile512, so retain those
   * physical slots while allowing new world tiles into unused VRAM. */
  unsigned hud=l==3&&cfg==0x1f0c;
  unsigned sx=hud?0:scroll_x[l]>>3,sy=hud?0:scroll_y[l]>>3;
  for(unsigned y=0;y<21;y++)for(unsigned x=0;x<31;x++){
   unsigned slot=(block&1)*512+(map[((sy+y)&31)*32+((sx+x)&31)]&1023);
   if(slot<s->stream[group].capacity)s->stream[group].pinned[slot]=1;
  }
 }
 s->inherited_display=1;
#else
 (void)s;(void)scroll_x;(void)scroll_y;
#endif
}
static void inherit_previous_display(struct State*s,const uint16_t*scroll_x,const uint16_t*scroll_y,unsigned previous_native){
 if(previous_native){inherit_display(s,scroll_x,scroll_y);return;}
#ifdef __arm__
 /* First demo entry starts from the legacy renderer's unscrolled BG3
  * HUD. Its exact layout is shared; other legacy scroll history is not. */
 const volatile uint16_t*io=(const volatile uint16_t*)0x04000000;
 if((io[0]&0x0f07)==0x0800&&io[7]==0x1f0c)inherit_display(s,scroll_x,scroll_y);
#endif
}
static void reset(void*ctx,unsigned program){
 unsigned native=program<=7;
 struct NativeVideo*v=ctx;
 /* v168: simulation may now run while a display list awaits its VBlank
  * transfer. Swapping the cartridge catalog or tile streams under that
  * IRQ reader would corrupt it; scene changes wait for the transfer. */
 while(v->commit_pending&&!v->fault){__asm__ volatile("":::"memory");}
 m1_runtime_load(native);unsigned previous_native=v->bg4_replanned;uint16_t sx[4],sy[4];
 for(unsigned l=0;l<4;l++){sx[l]=state(v)->frame.scroll_x[l];sy[l]=state(v)->frame.scroll_y[l];}
 v->backgrounds=(struct NativeBackgroundFrame){0};v->hud_active=0;v->hud_blocked=0;
 v->bg4_replanned=native;v->world->video.native_maps=native;
 static const uint8_t catalog[8]={0,1,3,2,4,5,1,3};
 assets=&cartridges[native?catalog[program]:0];
 if(v->bg4_replanned){
  struct State*s=state(v);m1_stream_init(&s->stream[0],1024);m1_stream_init(&s->stream[1],768);
  inherit_previous_display(s,sx,sy,previous_native);
  for(unsigned i=0;i<4;i++)s->history[i].image=s->cleared_image[i]=65535;
  for(unsigned i=0;i<4;i++){s->frame.dirty_rows[i]=0xffffffffu;for(unsigned y=0;y<32;y++)s->dirty_end[i][y]=32;}
  v->tiles.source_cells=0;
 }else{v->backgrounds.depth4=1;v->tiles.source_cells=native_background_4(&v->backgrounds)->source_cells;}
}
static struct History*history(struct NativeVideo*v,unsigned plane){
 struct History*h=&state(v)->history[plane];unsigned source=v->world->scene_runtime->maps.planes[plane].image;
 unsigned image=assets->first_image==15?(source==SCENE_M5_RELINK_IMAGE?6:source>=15&&source<=20?source-15:65535):source>=assets->first_image?source-assets->first_image:65535;
 if(source==state(v)->cleared_image[plane])image=65535;
 if(image>=assets->image_count)image=65535;
 if(h->image!=image){if(assets->first_image==15&&image==6)h->count=0;h->image=image;h->revision++;}return h;
}
static int patch_cached(struct NativeVideo*,unsigned,unsigned,unsigned);
static int add_patch(struct NativeVideo*v,unsigned id,unsigned plane){
 struct History*h=history(v,plane);
 if(h->image>=assets->image_count)return 0;
 /* Repeating an id replaces its earlier occurrence, preserving last-write order. */
 unsigned at=0;for(unsigned i=0;i<h->count;i++)if(h->patches[i]!=id)h->patches[at++]=h->patches[i];
 if(at>=192)return 0;h->patches[at++]=id;h->count=at;unsigned previous=h->revision;h->revision++;return patch_cached(v,id,plane,previous);
}
static int patch(void*ctx,const struct SceneTilePatch*p){
 struct NativeVideo*v=ctx;if(!v->bg4_replanned)return 1;
 if(assets->bindings){
  unsigned image=history(v,p->plane)->image;
  for(unsigned i=0;i<assets->binding_count;i++){const struct StagePatchBinding*b=&assets->bindings[i];if(b->image==image&&b->source->cells==p->cells)return add_patch(v,b->patch,p->plane);}
  return 0;
 }
 if(assets->first_image==15){
  for(unsigned i=0;i<32;i++)if(village_patches[i].cells==p->cells)return add_patch(v,i,p->plane);
  for(unsigned i=0;i<8;i++)if(boss2_background[i].tiles==p->cells)return add_patch(v,32+i,p->plane);
  return 0;
 }
 for(unsigned i=0;i<76;i++)if(opening_patches[i].cells==p->cells)return add_patch(v,i,p->plane);
 for(unsigned i=0;i<14;i++)if(script151_patches[i].cells==p->cells)return add_patch(v,76+i,p->plane);
 return 0;
}
static int clear(void*ctx,unsigned plane,int16_t x,int16_t y,uint16_t w,uint16_t h){
 struct NativeVideo*v=ctx;if(!v->bg4_replanned)return 1;
 if(assets->first_image)return 0;
 for(unsigned i=0;i<2;i++){const struct Script151Clear*c=&script151_clear[i];if(c->plane==plane&&c->x==x&&c->y==y&&c->width==w&&c->height==h)return add_patch(v,90+i,plane);}
 return 0;
}
static void clear_plane(void*ctx,unsigned plane){struct NativeVideo*v=ctx;if(v->bg4_replanned&&plane<4){struct State*s=state(v);s->cleared_image[plane]=v->world->scene_runtime->maps.planes[plane].image;s->history[plane].count=0;s->history[plane].revision++;}}
void m1_runtime_attach(struct NativeVideo*v){v->world->video.native_clear_plane=clear_plane;v->world->video.native_context=v;v->world->video.native_reset=reset;v->world->video.native_patch=patch;v->world->video.native_clear=clear;}
void m1_runtime_restore(struct NativeVideo*v){
 static const uint8_t catalog[8]={0,1,3,2,4,5,1,3};
 unsigned program=v->world->scene_runtime->program;
 assets=&cartridges[program<8?catalog[program]:0];m1_runtime_load(v->bg4_replanned);
}
static unsigned transform(unsigned op,unsigned old){
 const struct M1Op*p=&assets->ops[op];if(p->replace!=65535)return p->replace;
 unsigned lo=0,hi=p->count;
 while(lo<hi){unsigned mid=(lo+hi)/2,n=assets->transitions[p->offset+mid][0];if(n<old)lo=mid+1;else hi=mid;}
 if(lo==p->count||assets->transitions[p->offset+lo][0]!=old)return 65535;
 return assets->transitions[p->offset+lo][1];
}
/* A terrain write changes only its own cells. Apply it to cached source
 * maps before scrolling; off-screen cells still use the authored history
 * when they enter. Closed ROM transitions include repeated/overlapping writes. */
static int patch_cached(struct NativeVideo*v,unsigned id,unsigned plane,unsigned previous){
 struct State*s=state(v);const struct History*h=&s->history[plane];
 const struct M1Image*m=&assets->images[h->image];const struct M1Patch*p=&assets->patches[id];
 for(unsigned l=0;l<4;l++){
  if(!s->cache_valid[l]||s->cache_plane[l]!=plane||s->cache_image[l]!=h->image||s->cache_revision[l]!=previous)continue;
  int ox=s->cache_x[l],oy=s->cache_y[l];
  int left=p->x>ox?p->x:ox,top=p->y>oy?p->y:oy,right=p->x+p->width,bottom=p->y+p->height;
  if(right>ox+31)right=ox+31;if(bottom>oy+21)bottom=oy+21;
  if(left<0)left=0;if(top<0)top=0;if(right>m->width)right=m->width;if(bottom>m->height)bottom=m->height;
  for(int y=top;y<bottom;y++)for(int x=left;x<right;x++){
   unsigned at=(y-oy)*31+x-ox,op=assets->patchmaps[p->offset+(y-p->y)*p->width+x-p->x];
   unsigned old=v->backgrounds.maps[l][at],value=transform(op,old);if(value>=assets->tile_count)return 0;
   if(value!=old){v->backgrounds.maps[l][at]=value;s->patched|=1u<<l;}
  }
  s->cache_revision[l]=h->revision;
 }
 return 1;
}
static __attribute__((noinline)) /* v170: ROM, cold */ int viewport_region(struct NativeVideo*v,unsigned layer,const struct History*h,int ox,int oy,int rx,int ry,int rw,int rh){
 const struct M1Image*m=&assets->images[h->image];uint16_t*out=v->backgrounds.maps[layer];
 for(int y=ry;y<ry+rh;y++)for(int x=rx;x<rx+rw;x++){
  int xx=ox+x,yy=oy+y;
  if(assets->repeat_x_mask&(1u<<h->image)){int start=(m->origin_x+7)/8;int phase=(xx-start)%48;if(phase<0)phase+=48;xx=start+phase;}
  out[y*31+x]=xx>=0&&yy>=0&&xx<m->width&&yy<m->height?assets->maps[(assets->map_rows?assets->map_rows[m->offset+yy]:m->offset+yy*m->width)+xx]:0;
 }
 for(unsigned i=0;i<h->count;i++){
  const struct M1Patch*p=&assets->patches[h->patches[i]];
  int left=p->x>ox+rx?p->x:ox+rx,top=p->y>oy+ry?p->y:oy+ry,right=p->x+p->width,bottom=p->y+p->height;
  if(right>ox+rx+rw)right=ox+rx+rw;if(bottom>oy+ry+rh)bottom=oy+ry+rh;
  if(left<0)left=0;if(top<0)top=0;if(right>m->width)right=m->width;if(bottom>m->height)bottom=m->height;
  for(int y=top;y<bottom;y++)for(int x=left;x<right;x++){
   unsigned at=(y-oy)*31+x-ox,op=assets->patchmaps[p->offset+(y-p->y)*p->width+x-p->x];
   unsigned id=transform(op,out[at]);if(id>=assets->tile_count)return 0;out[at]=id;
  }
 }
 return 1;
}
static int viewport(struct NativeVideo*v,unsigned layer,const struct History*h,int ox,int oy){return viewport_region(v,layer,h,ox,oy,0,0,31,21);}
static __attribute__((noinline)) /* v170: ROM, cold */ int shift_viewport(struct NativeVideo*v,unsigned layer,const struct History*h,int ox,int oy,int dx,int dy){
 if(dx<=-31||dx>=31||dy<=-21||dy>=21)return viewport(v,layer,h,ox,oy);
 uint16_t*map=v->backgrounds.maps[layer];
 int x0=dx<0?-dx:0,x1=dx>0?31-dx:31,y0=dy<0?-dy:0,y1=dy>0?21-dy:21;
 for(int k=y0;k<y1;k++){
  int y=dy<0?y1-1-(k-y0):k;uint16_t*dst=map+y*31;const uint16_t*src=map+(y+dy)*31;
  if(dx<0){for(int x=x1-1;x>=x0;x--)dst[x]=src[x+dx];}
  else{for(int x=x0;x<x1;x++)dst[x]=src[x+dx];}
 }
 int top=dy<0?-dy:0,bottom=dy>0?21-dy:21;
 if(dy<0&&!viewport_region(v,layer,h,ox,oy,0,0,31,-dy))return 0;
 if(dy>0&&!viewport_region(v,layer,h,ox,oy,0,bottom,31,dy))return 0;
 if(dx<0&&!viewport_region(v,layer,h,ox,oy,0,top,-dx,bottom-top))return 0;
 if(dx>0&&!viewport_region(v,layer,h,ox,oy,31-dx,top,dx,bottom-top))return 0;
 return 1;
}
/* Remove only rear tiles whose complete on-screen area is opaque in front. */
static __attribute__((noinline)) /* v170: ROM, cold */ void occlude(struct M1MapFrame*f){
 for(unsigned l=1;l<f->layers;l++)for(unsigned y=0;y<21;y++)for(unsigned x=0;x<31;x++){
  unsigned at=l*651+y*31+x;if(!f->refs[at])continue;
  int left=x*8-(f->scroll_x[l]&7),top=y*8-(f->scroll_y[l]&7),right=left+8,bottom=top+8,opaque=1;
  if(left<0)left=0;if(top<0)top=0;if(right>240)right=240;if(bottom>160)bottom=160;
  if(left>=right||top>=bottom){f->refs[at]=0;continue;}
  unsigned width=right-left,mask=(1u<<width)-1;
  for(int yy=top;yy<bottom;yy++){
   unsigned covered=0;
   for(unsigned front=0;front<l;front++){
    unsigned fx=left+(f->scroll_x[front]&7),col=fx>>3,shift=fx&7;
    unsigned fy=yy+(f->scroll_y[front]&7),base=front*651+(fy>>3)*31+col,row=fy&7;
    unsigned bits=assets->opacity[(assets->opacity_ids?assets->opacity_ids[f->refs[base]]:f->refs[base])*8+row]>>shift;
    if(shift+width>8)bits|=(unsigned)assets->opacity[(assets->opacity_ids?assets->opacity_ids[f->refs[base+1]]:f->refs[base+1])*8+row]<<(8-shift);
    covered|=bits;
   }
   if((covered&mask)!=mask){opaque=0;break;}
  }
  if(opaque)f->refs[at]=0;
 }
}
/* v170: the 2x2304-byte request bitmaps stay zero between calls; only the
 * bits this call set (tile 0 plus its upload list) are cleared again.
 * Repeated refs within a layer row-run skip the ROM id lookup. Same result
 * and upload order as the full-clear version. */
static M1HOT int group_fits(struct State*s,unsigned mask){
 unsigned counts[2]={1,1};
 for(unsigned g=0;g<2;g++){struct M1TileStream*t=&s->stream[g];if(t->requested_dirty){for(unsigned i=0;i<M1_ROM_TILES/32;i++)((uint32_t*)t->requested)[i]=0;t->requested_dirty=0;}t->requested[0]=1;}
 int fits=1;
 for(unsigned l=0;l<s->frame.layers&&fits;l++){
  unsigned g=(mask>>l)&1;struct M1TileStream*t=&s->stream[g];uint8_t*req=t->requested;unsigned cap=t->capacity,n=counts[g];
  const uint16_t*refs=&s->frame.refs[l*651];unsigned prev=65536;
  for(unsigned i=0;i<651;i++){
   unsigned ref=refs[i];if(ref==prev)continue;prev=ref;
   unsigned id=assets->ids4[ref];uint8_t*b=&req[id>>3];unsigned bit=1u<<(id&7);
   if(!(*b&bit)){*b|=bit;if(++n>cap){n--;*b&=~bit;fits=0;break;}t->uploads[n-2].rom_tile=id;}
  }
  counts[g]=n;
 }
 for(unsigned g=0;g<2;g++){struct M1TileStream*t=&s->stream[g];
  for(unsigned i=0;i+1<counts[g];i++){unsigned id=t->uploads[i].rom_tile;t->requested[id>>3]&=~(1u<<(id&7));}
  t->requested[0]&=~1u;
  if(fits)t->upload_count=counts[g]-1;}
 return fits;
}
static int groups(struct State*s){
 if(group_fits(s,s->groups))return 1;
 for(unsigned mask=0;mask<(1u<<s->frame.layers);mask++)if(group_fits(s,mask)){s->groups=mask;return 1;}return 0;
}
/* A scrolling ring map keeps its overlapping cells in their hardware slots.
 * Visit only the exposed rectangles, rather than testing all 651 cells of
 * every layer to discover that almost all of them can be skipped. */
static M1HOT void map_region(struct State*s,unsigned l,unsigned x0,unsigned y0,unsigned x1,unsigned y1){
 struct M1MapFrame*f=&s->frame;struct M1TileStream*t=&s->stream[(s->groups>>l)&1];
 for(unsigned y=y0;y<y1;y++)for(unsigned x=x0;x<x1;x++){
  unsigned n=l*651+y*31+x,row=((f->scroll_y[l]>>3)+y)&31,at=row*32+(((f->scroll_x[l]>>3)+x)&31);
  unsigned id=m1_stream_find(t,assets->ids4[f->refs[n]])|assets->attrs4[f->refs[n]];
  if(f->maps[l][at]!=id){
   f->maps[l][at]=id;unsigned col=at&31;
   if(!(f->dirty_rows[l]&(1u<<row))){s->dirty_start[l][row]=col;s->dirty_end[l][row]=col+1;}
   else{if(col<s->dirty_start[l][row])s->dirty_start[l][row]=col;if(col>=s->dirty_end[l][row])s->dirty_end[l][row]=col+1;}
   f->dirty_rows[l]|=1u<<row;
  }
 }
}
static __attribute__((noinline)) void resize_pool1(struct State*s,unsigned capacity){
 m1_stream_init(&s->stream[1],capacity);
 for(unsigned i=0;i<capacity;i++)s->stream[1].pinned[i]=1;
}
static __attribute__((noinline)) int hud_budget(struct NativeVideo*v,struct State*s,unsigned count){
 /* The fourth map disables BG HUD, but need not evict the world tile
  * cache. Keep the reserved HUD arena until tile pressure requires it. */
 /* Experimental always-OBJ HUD: release its BG tile arena. */
 (void)count;
 unsigned capacity=768;
 v->hud_active=0;
 if(s->stream[1].capacity==capacity)return 0;
 resize_pool1(s,capacity);s->planned=0;return 1;
}
/* Sparse source animation follows destruction; cached maps remain unchanged. */
static __attribute__((noinline)) unsigned animate_refs155(struct State*s,unsigned layer,unsigned phase){
 unsigned image=s->history[s->cache_plane[layer]].image;
 const struct M1Image*m=&assets->images[image];
 const struct AutoSet158*set=&auto_sets158[assets-cartridges];const struct AutoRowSet*rows=&auto_row_sets[assets-cartridges];unsigned active=0;
 for(int y=0;y<21;y++){
  int yy=s->cache_y[layer]+y;if(yy<0||yy>=m->height)continue;
  for(int x=0;x<31;){
   int xx=s->cache_x[layer]+x,end=31;
   if(assets->repeat_x_mask&(1u<<image)){int start=(m->origin_x+7)/8;int wrapped=(xx-start)%48;if(wrapped<0)wrapped+=48;xx=start+wrapped;if(end>x+48-wrapped)end=x+48-wrapped;}
   if(xx<0){int skip=-xx;if(skip>end-x)skip=end-x;x+=skip;continue;}
   if(xx>=m->width){x=end;continue;}
   if(end>x+(int)m->width-xx)end=x+m->width-xx;
   unsigned first=(image<<24)+yy*m->width+xx,last=first+end-x,lo=rows->rows[rows->images[image]+yy],limit=rows->rows[rows->images[image]+yy+1],hi=limit;
   while(lo<hi){unsigned mid=(lo+hi)/2;if(set->data[mid].key<first)lo=mid+1;else hi=mid;}
   for(;lo<limit&&set->data[lo].key<last;lo++){
    const struct AutoTile158*a=&set->data[lo];unsigned at=layer*651+y*31+x+a->key-first;
    if(s->frame.refs[at]==a->base){s->frame.refs[at]=a->phase[phase];active=1;}
   }
   x=end;
  }
 }
 return active;
}
static __attribute__((noinline)) unsigned animation_tick155(struct NativeVideo*v,struct State*s){
 s->animation_clock+=(uint8_t)(v->auto_frame-s->animation_last);s->animation_last=v->auto_frame;unsigned phase=(s->animation_clock>>(assets==&cartridges[0]?4:3))&1;
 unsigned dirty=s->animation_phase!=phase?s->animation_layers:0;
 s->animation_phase=phase;return dirty;
}
static __attribute__((noinline)) void animation_refs155(struct NativeVideo*v,struct State*s,unsigned count){
 struct M1MapFrame*f=&s->frame;unsigned n=0;
 for(unsigned l=0;l<count;l++)for(unsigned i=0;i<651;i++)f->refs[n++]=v->backgrounds.maps[l][i];
 s->animation_layers=0;
 for(unsigned l=0;l<count;l++)if(animate_refs155(s,l,s->animation_phase))s->animation_layers|=1u<<l;
}
static __attribute__((noinline)) void map_changed_animation(struct State*s,struct NativeVideo*v,unsigned l){
 unsigned image=s->history[s->cache_plane[l]].image;const struct M1Image*m=&assets->images[image];
 const struct AutoSet158*set=&auto_sets158[assets-cartridges];const struct AutoRowSet*rows=&auto_row_sets[assets-cartridges];
 for(unsigned y=0;y<21;y++){
  int yy=s->cache_y[l]+y;if(yy<0||yy>=m->height)continue;
  for(unsigned x=0;x<31;){int xx=s->cache_x[l]+x;unsigned end=31;
   if(assets->repeat_x_mask&(1u<<image)){int start=(m->origin_x+7)/8,wrapped=(xx-start)%48;if(wrapped<0)wrapped+=48;xx=start+wrapped;if(end>x+48-wrapped)end=x+48-wrapped;}
   if(xx<0){unsigned n=-xx;if(n>end-x)n=end-x;x+=n;continue;}if(xx>=m->width){x=end;continue;}
   if(end>x+m->width-xx)end=x+m->width-xx;
   unsigned first=(image<<24)+yy*m->width+xx,last=first+end-x,lo=rows->rows[rows->images[image]+yy],limit=rows->rows[rows->images[image]+yy+1],hi=limit;
   while(lo<hi){unsigned mid=(lo+hi)/2;if(set->data[mid].key<first)lo=mid+1;else hi=mid;}
   for(;lo<limit&&set->data[lo].key<last;lo++){const struct AutoTile158*a=&set->data[lo];unsigned col=x+a->key-first;
    if(a->phase[0]!=a->phase[1]&&v->backgrounds.maps[l][y*31+col]==a->base)map_region(s,l,col,y,col+1,y+1);
   }x=end;
  }
 }
}
static __attribute__((noinline)) void map_dirty_regions(struct State*s,struct NativeVideo*v,unsigned full,unsigned animation_dirty,const int*dxs,const int*dys){
 for(unsigned l=0;l<s->frame.layers;l++){
  int dx=dxs[l],dy=dys[l];
  if((full&(1u<<l))||dx<=-31||dx>=31||dy<=-21||dy>=21){map_region(s,l,0,0,31,21);continue;}
  unsigned top=dy<0?-dy:0,bottom=dy>0?21-dy:21;
  if(top)map_region(s,l,0,0,31,top);if(bottom<21)map_region(s,l,0,bottom,31,21);
  if(dx<0)map_region(s,l,0,top,-dx,bottom);if(dx>0)map_region(s,l,31-dx,top,31,bottom);
  if(animation_dirty&(1u<<l))map_changed_animation(s,v,l);
 }
}

M1HOT int m1_runtime_prepare(struct NativeVideo*v){
 struct State*s=state(v);struct M1MapFrame*f=&s->frame;struct SceneRuntime*r=v->world->scene_runtime;
 unsigned order[4],count=0,changed=0,scroll_changed=0,full=0,old_groups=s->groups,old_culling=s->culling;int delta_x[4]={0},delta_y[4]={0};
 for(unsigned p=0;p<4;p++){
  history(v,p);if(s->history[p].image>=assets->image_count||!v->world->video.views[p].valid)continue;
  /* Source columns are one-based; NativeTileVideo stores column-1.
   * The view's first_column is the preceding source column, hence the
   * first stored entry is [first], including the one-column edge case. */
  int visible=(v->world->video.native_visible>>p)&1;
  if(visible)order[count++]=p;
 }
 for(unsigned i=1;i<count;i++){unsigned p=order[i],j=i;while(j&&v->world->video.views[order[j-1]].first_column<v->world->video.views[p].first_column){order[j]=order[j-1];j--;}order[j]=p;}
 /* Reserve BG3 and its tile arena only when all world planes fit. */
 if(hud_budget(v,s,count))full=15;
 /* All active source planes are retained. */
 if(f->layers!=count){changed=1;full=15;}f->layers=count;f->ref_count=count*651;
 int stable_x[4],stable_y[4];stable_scrolls(v,s,r,order,count,stable_x,stable_y);
 for(unsigned l=0;l<count;l++){
  unsigned p=order[l];int sx=stable_x[l],sy=stable_y[l];
  int ox=floor8(sx),oy=floor8(sy);f->scroll_x[l]=sx&255;f->scroll_y[l]=sy&255;
  if(s->previous_sx[l]!=f->scroll_x[l]||s->previous_sy[l]!=f->scroll_y[l])scroll_changed=1;
  s->previous_sx[l]=f->scroll_x[l];s->previous_sy[l]=f->scroll_y[l];
  s->priority[l]=v->world->video.views[p].first_column>=v->world->video.pools.upper_begin?1:3;
  struct History*h=&s->history[p];
  delta_x[l]=ox-s->cache_x[l];delta_y[l]=oy-s->cache_y[l];
  if(!s->planned||!s->cache_valid[l]||s->cache_plane[l]!=p||s->cache_image[l]!=h->image||s->cache_revision[l]!=h->revision)full|=1u<<l;
  if(!s->cache_valid[l]||s->cache_plane[l]!=p||s->cache_image[l]!=h->image||s->cache_revision[l]!=h->revision||s->cache_x[l]!=ox||s->cache_y[l]!=oy){
   changed=1;if(full&(1u<<l)){if(!viewport(v,l,h,ox,oy))return 0;}
   else if(!shift_viewport(v,l,h,ox,oy,delta_x[l],delta_y[l]))return 0;
   s->cache_valid[l]=1;s->cache_plane[l]=p;s->cache_image[l]=h->image;s->cache_revision[l]=h->revision;s->cache_x[l]=ox;s->cache_y[l]=oy;
  }

 }
 unsigned animation_dirty=animation_tick155(v,s);
 if(animation_dirty)changed=1;
 if(s->patched){changed=1;full|=s->patched;s->patched=0;}
 if(s->planned&&!changed&&(!s->culling||!scroll_changed)){v->backgrounds.depth4=1;v->backgrounds.ready=1;return 1;}
 animation_refs155(v,s,count);
 s->culling=0;
 if(!groups(s)){
  if(s->stream[1].capacity==512){v->hud_active=0;v->hud_blocked=1;resize_pool1(s,768);full=15;}
  if(!groups(s)){s->culling=1;occlude(f);if(!groups(s))return 0;}
 }
 if(v->hud_active)v->hud_blocked=0;
 /* Main may have used an inactive character block as transfer scratch.
  * Its old residency records must be invalidated before that block returns. */
 if(s->pool1_scratch&&(s->groups&((1u<<f->layers)-1))){
  m1_stream_init(&s->stream[1],s->stream[1].capacity);s->pool1_scratch=0;
  if(!group_fits(s,s->groups))return 0;
 }
 for(unsigned g=0;g<2;g++)if(!m1_stream_plan_unique(&s->stream[g],s->stream[g].upload_count))return 0;
 if(old_groups!=s->groups||old_culling||s->culling)full=15;
 s->planned=1;
 map_dirty_regions(s,v,full,animation_dirty,delta_x,delta_y);
 v->backgrounds.ready=1;v->backgrounds.map_count=count;v->backgrounds.depth4=1;v->backgrounds.native_tiles+=count*651;v->backgrounds.count=1792;
 return 1;
}
static void copy(const void*src,unsigned dst,unsigned words){volatile uint32_t*d=(volatile uint32_t*)0x040000d4;d[2]=0;d[0]=(uintptr_t)src;d[1]=dst;__asm__ volatile("":::"memory");d[2]=0x84000000|words;while(d[2]&0x80000000){} }
static int display_hidden(void){
 const volatile uint16_t*io=(const volatile uint16_t*)0x04000000;
 unsigned control=io[0];if(control&128)return 1;
 if(control&7)return 0;
 const volatile uint16_t*pal=(const volatile uint16_t*)0x05000000;
 if(pal[0]&32767)return 0;
 /* BLDY is write-only: its readback is open bus, not the displayed fade.
  * Only actual black palette data proves this unblanked display hidden. */
 for(unsigned i=1;i<256;i++)if(pal[i]&32767)return 0;
 if(control&0x1000){
  const volatile uint16_t*oam=(const volatile uint16_t*)0x07000000;unsigned banks=0;
  for(unsigned i=0;i<128;i++){
   unsigned a=oam[i*4];if(!(a&0x100)&&(a&0x200))continue;
   if(a&0x2000){for(unsigned j=1;j<256;j++)if(pal[256+j]&32767)return 0;return 1;}
   banks|=1u<<(oam[i*4+2]>>12);
  }
  for(unsigned bank=0;bank<16;bank++)if(banks&(1u<<bank))for(unsigned i=1;i<16;i++)if(pal[256+bank*16+i]&32767)return 0;
 }
 return 1;
}
void m1_runtime_prestage_background(struct NativeVideo*v){
 struct State*s=state(v);
 if(s->stream[0].upload_count+s->stream[1].upload_count<96)return;
 int black=0;
 if(!s->committed){
  /* Forced blank or actual black BG/OBJ palette data proves the previous
   * display hidden. Include unused colors in active OBJ banks; a hardware
   * fade alone is insufficient because BLDY cannot be read back. */
  black=display_hidden();
  if(!black&&!s->inherited_display)return;
 }
 for(unsigned g=0;g<2;g++){
  struct M1TileStream*t=&s->stream[g];unsigned kept=0;
  for(unsigned i=0;i<t->upload_count;){
   struct M1TileUpload u=t->uploads[i];
   if(!black&&(t->pinned[u.vram_tile]&2)){t->uploads[kept++]=u;i++;continue;}
   unsigned first=i++;
   while(i<t->upload_count&&(black||!(t->pinned[t->uploads[i].vram_tile]&2))&&t->uploads[i].rom_tile==u.rom_tile+i-first&&t->uploads[i].vram_tile==u.vram_tile+i-first)i++;
   copy(assets->tiles4+u.rom_tile*8,0x06000000+g*0x8000+u.vram_tile*32,(i-first)*8);
  }
  t->upload_count=kept;
 }
}
unsigned m1_runtime_heavy_transfer_bytes(struct NativeVideo*v){
 struct State*s=state(v);unsigned tiles=s->stream[0].upload_count+s->stream[1].upload_count;
 if(tiles<96)return 0;
 unsigned bytes=tiles*32+448;
 for(unsigned l=0;l<s->frame.layers;l++)for(unsigned y=0;y<32;y++)if(s->frame.dirty_rows[l]&(1u<<y))
  bytes+=2*(((s->dirty_end[l][y]+1u)&~1u)-(s->dirty_start[l][y]&~1u));
 return bytes;
}
void m1_runtime_prestage_objects(struct NativeVideo*v,unsigned background_bytes){
 struct State*s=state(v);
 /* Both the displayed and next frame must leave pool1 unused. Tile0 is
  * refreshed by BG commit, so scratch starts at tile1, below screen maps. */
 if(v->hud_active)return;
 if(!s->committed||s->committed_groups||(s->groups&((1u<<s->frame.layers)-1)))return;
 unsigned bytes=native_object_prestage(&v->objects.frame,0x06008020,0x5fe0,background_bytes);
 if(bytes){s->pool1_scratch=1;s->staged_frames++;s->staged_bytes+=bytes;}
}
M1DMA int m1_runtime_commit(struct NativeVideo*v){
 struct State*s=state(v);struct M1MapFrame*f=&s->frame;
 for(unsigned g=0;g<2;g++){
  struct M1TileStream*t=&s->stream[g];unsigned base=0x06000000+g*0x8000;copy(assets->tiles4,base,8);
  for(unsigned i=0;i<t->upload_count;){struct M1TileUpload*u=&t->uploads[i];unsigned first=i++;while(i<t->upload_count&&t->uploads[i].rom_tile==u->rom_tile+i-first&&t->uploads[i].vram_tile==u->vram_tile+i-first)i++;copy(assets->tiles4+u->rom_tile*8,base+u->vram_tile*32,(i-first)*8);}
  t->upload_count=0;
 }
 for(unsigned l=0;l<f->layers;l++){
  /* Transfer only changed columns; round outward for word-aligned DMA.
   * Adjacent whole rows can still share one DMA command. */
  for(unsigned y=0;y<32;){
   if(!(f->dirty_rows[l]&(1u<<y))){y++;continue;}
   unsigned first=y++,start=s->dirty_start[l][first]&~1u,end=(s->dirty_end[l][first]+1u)&~1u;
   if(!start&&end==32)while(y<32&&(f->dirty_rows[l]&(1u<<y))&&s->dirty_start[l][y]==0&&s->dirty_end[l][y]==32)y++;
   unsigned at=first*32+start,words=((y-first-1)*32+end-start)/2;
   copy(f->maps[l]+at,0x0600e000+l*2048+at*2,words);
  }
  *(volatile uint16_t*)(0x04000008+l*2)=(((s->groups>>l)&1)<<3)|((28+l)<<8)|s->priority[l];
  *(volatile uint16_t*)(0x04000010+l*4)=f->scroll_x[l];*(volatile uint16_t*)(0x04000012+l*4)=f->scroll_y[l];f->dirty_rows[l]=0;
 }
 copy(v->colors.colors,0x05000000,96);
 s->committed_groups=s->groups&((1u<<f->layers)-1);s->committed=1;
 *(volatile uint16_t*)0x04000000=(*(volatile uint16_t*)0x04000000&~0xf00)|(((1u<<f->layers)-1)<<8);
 v->backgrounds.ready=v->colors.ready=0;return 1;
}
