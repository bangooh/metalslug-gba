#include "scene_video.h"
static int fail(struct SceneVideoWorld*w){if(w)w->fault=1;return 0;}
/* $05DA56 increments tiles across columns and by16 across rows; $05DA9C
 * repeats one tile. Fixed-layer writes outside40x32 cells are discarded. */
int scene_video_fixed_rect(struct SceneVideoWorld*w,int column,int row,uint16_t tile,unsigned width,unsigned height,int atlas){
 if(!w||!w->write_fixed||!width||!height)return fail(w);
 for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++){int index=column*32+row+(int)(x*32+y);if(index>=0&&index<1280&&!w->write_fixed(w->context,index>>5,index&31,(uint16_t)(tile+(atlas?x+16*y:0))))return fail(w);}return 1;
}
int scene_video_patch(struct SceneVideoWorld*w,const struct SceneTilePatch*p){
 if(!w||!p||!p->cells||p->plane>=SCENE_TILE_PLANE_COUNT||!p->width||!p->height||!w->write_tile)return fail(w);
 if(w->native_patch&&!w->native_patch(w->native_context,p))return fail(w);
 const struct SceneTileView*v=&w->views[p->plane];if(!v->valid)return fail(w);
 if(w->native_maps&&w->native_patch)return 1;
 unsigned source=0;uint16_t x=(uint16_t)(p->x+v->base_x),base_y=(uint16_t)(p->y+v->base_y);
 for(unsigned i=0;i<p->width;i++,x++){
  unsigned col=x&31;uint16_t y=(uint16_t)((base_y&0xff00)|(uint8_t)(base_y+v->column_y[col]));
  uint16_t column=(uint16_t)(v->first_column+col+1)&1023;
  for(unsigned j=0;j<p->height;j++,y++){
   uint16_t tile=p->cells[source++],attributes=p->cells[source++];
   if(w->write_source_tile){if(!w->write_source_tile(w->context,p->plane,column,(uint8_t)(y&31),p->cells+source-2))return fail(w);}
   else if(!w->write_tile(w->context,p->plane,column,(uint8_t)(y&31),tile,attributes))return fail(w);
  }
 }
 return 1;
}

/* $051E74 clears only tile words; attributes are retained. */
int scene_video_clear(struct SceneVideoWorld*w,unsigned plane,int16_t x,int16_t y,uint16_t width,uint16_t height){
 if(!w||plane>=SCENE_TILE_PLANE_COUNT||!width||!height||!w->clear_tile)return fail(w);if(w->native_clear&&!w->native_clear(w->native_context,plane,x,y,width,height))return fail(w);const struct SceneTileView*v=&w->views[plane];if(!v->valid)return fail(w);
 if(w->native_maps&&w->native_clear)return 1;
 uint16_t sx=(uint16_t)(x+v->base_x),sy=(uint16_t)(y+v->base_y);
 for(unsigned i=0;i<width;i++,sx++){unsigned col=sx&31;uint16_t row=(uint16_t)((sy&0xff00)|(uint8_t)(sy+v->column_y[col])),column=(v->first_column+col+1)&1023;for(unsigned j=0;j<height;j++,row++)if(!w->clear_tile(w->context,plane,column,(uint8_t)(row&31)))return fail(w);}return 1;
}
