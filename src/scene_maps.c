#include "scene_maps.h"
static void collision_clear(struct SceneMaps*m){for(unsigned i=0;i<sizeof(m->collision);i++)m->collision[i]=0;}
static int fail(struct SceneMaps*m){m->fault=1;return 0;}
static int16_t word(int32_t v){return (int16_t)(uint16_t)v;}
static uint16_t integer(uint32_t v){return v>>16;}
static uint16_t column(const struct SceneTileView*v,unsigned i){return (v->first_column+i+1)&1023;}
int scene_map_hide(struct SceneMaps*m,struct SceneVideoWorld*v,unsigned id){
 if(id>=4||!v->column)return fail(m);m->planes[id].dirty=0;v->native_visible&=(uint8_t)~(1u<<id);
 for(unsigned i=0;i<32;i++)if(!v->column(v->context,id,column(&v->views[id],i),0,0,1))return fail(m);return 1;
}
int scene_map_clear(struct SceneMaps*m,struct SceneVideoWorld*v,unsigned id){
 if(id>=4||!v->write_tile)return fail(m);
 if(v->native_clear_plane)v->native_clear_plane(v->native_context,id);
 if(!v->native_maps)for(unsigned i=0;i<32;i++)for(unsigned y=0;y<32;y++)if(!v->write_tile(v->context,id,column(&v->views[id],i),y,0,0))return fail(m);
 if(m->planes[id].collision!=SCENE_NO_IMAGE)collision_clear(m);return 1;
}
int scene_maps_load(struct SceneMaps*m,struct SceneVideoWorld*v,const struct SceneDescriptor*d){
 if(!m||!v||!d||d->first+d->count>43||d->count>4)return m?fail(m):0;
 m->ending=d==&scene_descriptors[14]||d==&scene_descriptors[15];
 unsigned count0=0,count1=0,seen=0;
 for(unsigned i=0;i<d->count;i++){
  const struct SceneInitialPlane*r=&scene_initial_planes[d->first+i];
  if(r->plane>=4||r->pool>1||(seen&(1u<<r->plane)))return fail(m);seen|=1u<<r->plane;
  if(!r->pool)count0+=32;else count1+=32;
 }
 if(count0+count1+d->allocation_mode>380)return fail(m);
 unsigned pool[2]={380-d->allocation_mode-count0,0};
 v->pools=(struct SceneSpritePools){count1,pool[0],380-d->allocation_mode,1};
 if(v->sprite_reset&&!v->sprite_reset(v->sprite_context,&v->pools))return fail(m);
 /* $1390E hides the entire 380-column range before rebuilding the pools.
  * plane=4 denotes a global column, outside any current logical view. */
 v->native_visible=0;
 if(!v->column)return fail(m);for(unsigned i=1;i<=380;i++)if(!v->column(v->context,SCENE_TILE_PLANE_COUNT,i,0,0,1))return fail(m);
 for(unsigned i=0;i<4;i++){m->planes[i].image=SCENE_NO_IMAGE;v->views[i].valid=0;}
 for(unsigned i=0;i<d->count;i++){
  const struct SceneInitialPlane*r=&scene_initial_planes[d->first+i];unsigned id=r->plane;struct SceneMapPlane*p=&m->planes[id];struct SceneTileView*t=&v->views[id];
  *p=(struct SceneMapPlane){.image=SCENE_NO_IMAGE,.collision=SCENE_NO_IMAGE,.controls=SCENE_NO_IMAGE,.rect_x=r->x,.rect_y=r->y,.width=r->width,.height=r->height,.scale_x=256,.scale_y=256};
  /* $51ABE does not clear the previous column-Y bytes. */
  t->base_x=t->base_y=0;t->first_column=pool[r->pool];pool[r->pool]+=32;t->valid=1;
  if(!scene_map_hide(m,v,id)||!scene_map_clear(m,v,id))return 0;
 }
 collision_clear(m);return 1;
}
void scene_map_cursor(struct SceneMaps*m,struct SceneVideoWorld*v,unsigned id,uint8_t flags,uint8_t y){
 struct SceneMapPlane*p=&m->planes[id];p->cursor_flags=flags&63;p->cursor_y=(-(y+v->views[id].base_y))&31;p->controls=SCENE_NO_IMAGE;
}
int scene_map_bind(struct SceneMaps*m,struct SceneVideoWorld*v,unsigned id,unsigned image,unsigned collision,uint16_t x,uint16_t y){
 if(id>=4||image>=sizeof(scene_map_images)/sizeof(*scene_map_images)||(collision!=SCENE_NO_IMAGE&&(id||collision>=sizeof(scene_collision_images)/sizeof(*scene_collision_images)))||!v->views[id].valid)return fail(m);
 struct SceneMapPlane*p=&m->planes[id];struct SceneTileView*t=&v->views[id];
 t->base_x=(((uint16_t)(integer(p->x)-x)>>4)+t->base_x)&31;t->base_y=(((uint16_t)(integer(p->y)-y)>>4)+t->base_y)&31;
 p->x=((uint32_t)x<<16)|(p->x&65535);p->y=((uint32_t)y<<16)|(p->y&65535);p->cached_x=p->cached_y=0;p->image=image;p->collision=collision;scene_map_cursor(m,v,id,32,0);return 1;
}
/* $51BA8 changes the current table pointer even on the two sentinel exits. */
static int controls(struct SceneMaps*m,struct SceneVideoWorld*v,unsigned id,uint16_t x){
 struct SceneMapPlane*p=&m->planes[id];if(p->controls==SCENE_NO_IMAGE)return 1;if(p->controls>=23)return fail(m);
 const struct SceneColumnControls*t=&scene_column_controls[p->controls];unsigned i=p->control_index;
 for(unsigned budget=0;budget<t->count;budget++){
  if(i+1>=t->count)return fail(m);uint16_t lo=t->rows[i].threshold,hi=t->rows[i+1].threshold;
  if(x<lo){if(!lo){p->control_index=i;return 1;}if(!i)return fail(m);i--;}
  else if(x>=hi){if(hi==65535){p->control_index=i;return 1;}i++;}
  else{uint16_t value=t->rows[i].value;p->cursor_flags=value>>8;p->cursor_y=(-(int)((uint8_t)value+v->views[id].base_y))&31;p->control_index=i;return 1;}
 }return fail(m);
}
static int clip(uint16_t*dest,uint16_t*source,uint16_t*size,uint16_t limit){
 uint16_t end=*source+*size;if(limit<*source){if(end>*source)return 0;*dest-=*source;*source=0;}if(limit<end)end=limit;*size=end-*source;return *size!=0;
}
int scene_map_probe(struct SceneMaps*m,struct SceneVideoWorld*v,unsigned id,unsigned axis){
 if(id>=4||axis>2)return fail(m);struct SceneMapPlane*p=&m->planes[id];if(p->image>=sizeof(scene_map_images)/sizeof(*scene_map_images))return fail(m);const struct SceneImage*im=&scene_map_images[p->image];struct SceneTileView*t=&v->views[id];
 uint16_t x=word(integer(p->x)+p->rect_x)>>4,y=word(integer(p->y)+p->rect_y)>>4,width=p->width,height=p->height;
 if(axis==1){if(x==(uint16_t)p->cached_x)return 1;p->cached_x=word(x);if(p->dx>=0)x+=width-1;width=1;}
 else if(axis==2){if(y==(uint16_t)p->cached_y)return 1;p->cached_y=word(y);if(p->dy>=0)y+=height-1;height=1;}
 else{p->cached_x=word(x);p->cached_y=word(y);}
 unsigned extend=m->ending&&!id&&p->image==34?2:0;
 uint16_t sx=x,sy=y;if(!clip(&x,&sx,&width,im->width)||!clip(&y,&sy,&height,im->height+extend))return 1;
 if(!v->write_tile)return fail(m);
 for(unsigned i=0;i<width;i++){
  uint16_t dx=x+i;unsigned col=(dx+t->base_x)&31;if(!controls(m,v,id,dx))return 0;t->column_y[col]=p->cursor_y;p->column_flags[col]=p->cursor_flags;
  unsigned row=(uint8_t)(y+t->base_y+t->column_y[col]);
  if(!v->native_maps)for(unsigned j=0;j<height;j++){
   unsigned source_row=sy+j;
   if(extend&&source_row>=im->height)source_row=im->height-2+(source_row-im->height)%2;
   const uint16_t*source=im->cells+2*((sx+i)*im->height+source_row);
   if(v->write_source_tile){if(!v->write_source_tile(v->context,id,column(t,col),(row+j)&31,source))return fail(m);}
   else if(!v->write_tile(v->context,id,column(t,col),(row+j)&31,source[0],source[1]))return fail(m);
  }
 }
 if(p->collision!=SCENE_NO_IMAGE){
  if(id||p->collision>=sizeof(scene_collision_images)/sizeof(*scene_collision_images))return fail(m);const struct SceneCollisionImage*ci=&scene_collision_images[p->collision];if(ci->width!=im->width||ci->height!=im->height)return fail(m);
  /* All authored collision images fit signed16 source offsets. The
   * importer enforces that condition rather than changing address wrap. */
  for(unsigned i=0;i<width;i++)for(unsigned j=0;j<height;j++){
   unsigned source=((sx+i)*im->height+sy+j)*4,dest=(((x+i+t->base_x)<<7)&0xf80)|(((y+j+t->base_y)<<2)&0x7c);
   for(unsigned k=0;k<4;k++)m->collision[dest+k]=ci->cells[source+k];
  }
 }return 1;
}
int scene_map_commit(struct SceneMaps*m,struct SceneVideoWorld*v,unsigned id){
 if(id>=4)return fail(m);struct SceneMapPlane*p=&m->planes[id];if(!p->dirty)return 1;if(!v->column)return fail(m);struct SceneTileView*t=&v->views[id];
 /* GBA ROM maps consume plane visibility and their own scroll values.
  * Source strip coordinates belong only to the legacy presentation path. */
 if(v->native_maps){
  if(!t->valid||t->first_column>348)return fail(m);
  v->native_visible|=1u<<id;return 1;
 }
 v->native_visible|=1u<<id;
 uint16_t u=integer(p->x)+t->base_x*16,origin_x=(uint32_t)(uint16_t)-u<<7,origin_y=(integer(p->y)+t->base_y*16)<<7;
 unsigned left=((u>>4)-1)&31,right=(left+22)&31,i=(left+1)&31;uint16_t x=(i<<11)+origin_x;
 if(v->commit_columns)return v->commit_columns(v->context,id,origin_x,origin_y,left,p->column_flags)?1:fail(m);
 if(!v->column(v->context,id,column(t,left),0,0,1))return fail(m);
 for(;i!=right;i=(i+1)&31,x+=2048){uint16_t y=(t->column_y[i]<<11)+origin_y;y|=p->column_flags[i];if(!v->column(v->context,id,column(t,i),x,y,0))return fail(m);}
 if(!v->column(v->context,id,column(t,right),0,0,1))return fail(m);return 1;
}
int scene_maps_step(struct SceneMaps*m,struct SceneVideoWorld*v,int16_t dx,int16_t dy){
 for(unsigned id=0;id<4;id++){struct SceneMapPlane*p=&m->planes[id];if(p->image==SCENE_NO_IMAGE)continue;uint32_t oldx=p->x,oldy=p->y;p->x+=(uint32_t)((int32_t)dx*p->scale_x)<<8;p->y+=(uint32_t)((int32_t)dy*p->scale_y)<<8;p->dx=word(integer(p->x)-integer(oldx));p->dy=word(integer(p->y)-integer(oldy));if((p->flags&1)&&!scene_map_probe(m,v,id,1))return 0;if((p->flags&2)&&!scene_map_probe(m,v,id,2))return 0;if(!scene_map_commit(m,v,id))return 0;}return 1;
}
static unsigned cell_index(const struct SceneMaps*m,const struct SceneVideoWorld*v,uint16_t x,uint16_t y){
 x+=integer(m->planes[0].x)+v->views[0].base_x*16;y=integer(m->planes[0].y)+v->views[0].base_y*16-y;
 return ((x&0x1f0)<<3)+((y&0x1f8)>>2)+((x&8)>>3);
}
uint8_t scene_map_cell(const struct SceneMaps*m,const struct SceneVideoWorld*v,uint16_t x,uint16_t y){return m->collision[cell_index(m,v,x,y)];}
void scene_map_write_cell(struct SceneMaps*m,const struct SceneVideoWorld*v,uint16_t x,uint16_t y,uint8_t value){m->collision[cell_index(m,v,x,y)]=value;}
