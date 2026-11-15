#include "tile_video.h"
#include "play_world.h"
#include "scene_runtime.h"
#include "../generated/native-bg-assets.h"
static const uint16_t*source_begin,*source_end;
static unsigned source_base,source_cache_next;
static struct {const uint16_t*begin,*end;unsigned base;} source_cache[8];
static int fail(struct NativeTileVideo*v){if(v){v->fault=1;if(v->world)v->world->video.fault=1;}return 0;}
static int local_column(const struct NativeTileVideo*v,unsigned plane,unsigned column){
 if(!v||v->fault||!v->world||plane>=SCENE_TILE_PLANE_COUNT||!column||column>380)return -1;
 const struct SceneTileView*t=&v->world->video.views[plane];
 unsigned local=(column-t->first_column-1)&1023;
 return t->valid&&local<32?(int)local:-1;
}
int native_tile_video_column(void*ctx,unsigned plane,uint16_t column,uint16_t x,uint16_t y,int hidden){
 struct NativeTileVideo*v=ctx;if(!v||v->fault||!column||column>380)return fail(v);
 /* Global range clear happens BEFORE new views are installed. It is only a
  * hide operation; accepting arbitrary global writes would bypass ownership. */
 if(plane==SCENE_TILE_PLANE_COUNT){if(!hidden)return fail(v);}
 else if(local_column(v,plane,column)<0)return fail(v);
 struct NativeTileColumn*c=&v->columns[column-1];
 if(hidden){if(!c->hidden){c->hidden=1;c->y=0;v->revision++;}}
 else if(c->hidden||c->x!=x||c->y!=y){c->x=x;c->y=y;c->hidden=0;v->revision++;}
 return 1;
}
/* One validation and one world/view lookup for a map update. Keep every
 * column word and revision increment identical to the individual calls. */
int native_tile_video_columns(void*ctx,unsigned plane,uint16_t ox,uint16_t oy,unsigned left,const uint8_t*flags){
 struct NativeTileVideo*v=ctx;
 if(!v||v->fault||!v->world||plane>=4||left>=32||!flags)return fail(v);
 const struct SceneTileView*t=&v->world->video.views[plane];
 if(!t->valid)return fail(v);
 unsigned first=t->first_column,right=(left+22)&31;
 const struct SceneRuntime*s=v->world->scene_runtime;
 int ending_ground=s&&(s->program==14||s->program==15)&&plane==0;
 for(unsigned j=0;j<23;j++){
  unsigned i=(left+j)&31,column=(first+i+1)&1023;
  if(!column||column>380)return fail(v);
  struct NativeTileColumn*c=&v->columns[column-1];
  if(i==left||i==right){if(!c->hidden){c->hidden=1;c->y=0;v->revision++;}}
  else{
   uint16_t x=(i<<11)+ox,y=((t->column_y[i]<<11)+oy)|flags[i];
   /* Reveal existing ground rows formerly hidden by the bottom FIX mask.
    * Source strips wrap over 512 pixels (384 after scaling). */
   if(ending_ground&&(y&63)&&(y&63)<32){
    int top=(496-(int)((y>>7)&511))*3;
    top=(top>=0?top/4:-((-top+3)/4))-4;
    if(top>=160)top-=384;
    int end=top+(y&63)*12;
    if(end>=140&&end<160){unsigned extra=(160-end+11)/12;if((y&63)+extra<=32)y+=extra;}
   }
   if(c->hidden||c->x!=x||c->y!=y){c->x=x;c->y=y;c->hidden=0;v->revision++;}
  }
 }
 return 1;
}
int native_tile_video_write(void*ctx,unsigned plane,uint16_t column,uint8_t row,uint16_t tile,uint16_t attributes){
 struct NativeTileVideo*v=ctx;int i=local_column(v,plane,column);if(i<0||row>=32)return fail(v);
 struct NativeTileCell*c=&v->planes[plane].cells[i][row];
 if(v->source_cells)v->source_cells[plane][i][row]=0;
 if(c->tile!=tile||c->attributes!=attributes){c->tile=tile;c->attributes=attributes;v->planes[plane].dirty[i]|=1u<<row;v->revision++;}
 return 1;
}
static int source_write(void*ctx,unsigned plane,uint16_t column,uint8_t row,const uint16_t*source){
 struct NativeTileVideo*v=ctx;
 if(!source)return fail(v);
 if(!v->source_cells)return 1;
 if(v->world&&v->world->scene_runtime&&v->world->scene_runtime->program==15&&plane==0){
  unsigned image=v->world->scene_runtime->maps.planes[plane].image;
  if(image==39||image==41){
   const struct SceneImage*im=&scene_map_images[image];
   uintptr_t at=(uintptr_t)source,begin=(uintptr_t)im->cells;
   unsigned blank=image==39?14:22;
   if(at>=begin&&at<begin+im->width*im->height*4u&&((at-begin)/4u)%im->height==blank)
    source=im->cells+2*((((at-begin)/4u)/im->height%8)*im->height+blank-1);
  }
 }
 if(!native_tile_video_write(v,plane,column,row,source[0],source[1]))return fail(v);
 uintptr_t address=(uintptr_t)source;
 if(address<(uintptr_t)source_begin||address>=(uintptr_t)source_end){
  unsigned id;
  for(id=0;id<8;id++)if(address>=(uintptr_t)source_cache[id].begin&&address<(uintptr_t)source_cache[id].end){source_begin=source_cache[id].begin;source_end=source_cache[id].end;source_base=source_cache[id].base;goto found;}
  for(id=0;id<NATIVE_BG_REGION_COUNT;id++){
   const struct NativeBgRegion*r=&native_bg_regions[id];const uint16_t*begin=*native_bg_source_addresses[id];
   if(address>=(uintptr_t)begin&&address<(uintptr_t)(begin+2*r->width*r->height)){source_begin=begin;source_end=begin+2*r->width*r->height;source_base=r->start;break;}
  }
  if(id==NATIVE_BG_REGION_COUNT)return fail(v);
  unsigned cache=source_cache_next++&7;source_cache[cache].begin=source_begin;source_cache[cache].end=source_end;source_cache[cache].base=source_base;
 found:;
 }
 unsigned i=local_column(v,plane,column);v->source_cells[plane][i][row]=source_base+(source-source_begin)/2;return 1;
}
int native_tile_video_clear(void*ctx,unsigned plane,uint16_t column,uint8_t row){
 struct NativeTileVideo*v=ctx;int i=local_column(v,plane,column);if(i<0||row>=32)return fail(v);
 /* 51E74 intentionally leaves the palette/flip/bank/auto-animation word. */
 return native_tile_video_write(v,plane,column,row,0,v->planes[plane].cells[i][row].attributes);
}
int native_tile_video_fixed(void*ctx,uint8_t column,uint8_t row,uint16_t tile){
 struct NativeTileVideo*v=ctx;if(!v||v->fault||column>=40||row>=32)return fail(v);
 /* Keep ending subtitles, but expose the scenery beneath their black mask. */
 if(row>=26&&tile==0x20&&v->world&&v->world->scene_runtime&&
    (v->world->scene_runtime->program==14||v->world->scene_runtime->program==15))tile=0x300;
 if(v->fixed[column][row]!=tile){v->fixed[column][row]=tile;v->fixed_dirty[column]|=1u<<row;v->fixed_revision++;}
 return 1;
}
const struct NativeTileCell*native_tile_video_cell(const struct NativeTileVideo*v,unsigned plane,uint16_t column,uint8_t row){
 int i=local_column(v,plane,column);return i<0||row>=32?0:&v->planes[plane].cells[i][row];
}
int native_tile_video_object_layer(void*ctx,uint16_t priority,int forced,uint8_t*out){
 struct NativeTileVideo*v=ctx;(void)priority; /* Source priority sorts WITHIN its selected pool. */
 if(!v||v->fault||!v->world||!out||!v->world->video.pools.valid)return fail(v);
 *out=forced?1:2;return 1;
}
int native_tile_video_attach(struct NativeTileVideo*v,struct NativePlayWorld*w){
 if(!v||!w||w->fault||w->video.fault)return 0;
 /* A late attach would discard previously written scene/FIX state. */
 if(w->ticks||w->session||w->scene_runtime)return 0;
 *v=(struct NativeTileVideo){.world=w};
 for(unsigned i=0;i<380;i++)v->columns[i].hidden=1;
 for(unsigned p=0;p<4;p++)for(unsigned x=0;x<32;x++)v->planes[p].dirty[x]=0xffffffffu;
 for(unsigned x=0;x<40;x++)v->fixed_dirty[x]=0xffffffffu;
 w->video.context=v;w->video.column=native_tile_video_column;w->video.commit_columns=native_tile_video_columns;
 w->video.write_tile=native_tile_video_write;w->video.clear_tile=native_tile_video_clear;
 w->video.write_source_tile=source_write;
 w->video.hud_bottom_row=28;
 w->video.write_fixed=native_tile_video_fixed;return 1;
}
