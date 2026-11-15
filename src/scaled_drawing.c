#include "scaled_drawing.h"
#include "../generated/scaled-drawing-data.h"
#include "../generated/drawing-data.h"
#include "../generated/bridge-horizontal.inc"
#include "../generated/blast-packed.inc"
const struct NativeScaledMap*native_scaled_bridge_map(unsigned n){return n>=9153&&n<=9154?&bridge_horizontal_maps[n-9153]:0;}
const struct NativeScaledMap*native_scaled_map(unsigned n){
 return native_scaled_zoom_map(n,255,255);
}
const struct NativeScaledMap*native_scaled_dense_map(unsigned n){
 if(n>=NATIVE_SCALED_SOURCE_MAP_COUNT)return 0;
 const struct NativeScaledMap*b=blast_packed(n);if(b)return b;
 return native_dense_maps[n].count?&native_dense_maps[n]:native_scaled_map(n);
}
const struct NativeScaledMap*native_scaled_zoom_map(unsigned n,uint8_t x,uint8_t y){
 if(x==255&&y==255){const struct NativeScaledMap*b=blast_packed(n);if(b)return b;}
 unsigned vx=x<2?x+1:0,vy=y<2?y+1:0;
 return n<NATIVE_SCALED_SOURCE_MAP_COUNT?&native_scaled_maps[n*NATIVE_SCALED_VARIANTS+vy*3+vx]:0;
}
const struct NativeScaledChunk*native_scaled_chunk(unsigned n){
 return n<NATIVE_SCALED_CHUNK_COUNT?&native_scaled_chunks[n]:0;
}
const uint8_t*native_scaled_pixels(const struct NativeScaledChunk*c){
 if(!c)return 0;unsigned bytes=(unsigned)c->width*c->height/2;
 if(c->byte_offset>NATIVE_SCALED_PIXEL_BYTES||bytes>NATIVE_SCALED_PIXEL_BYTES-c->byte_offset)return 0;
 return native_scaled_sprite_pixels+c->byte_offset;
}
int native_scaled_drawing_visit(const struct Animation*a,NativeScaledSink sink,void*ctx){
 const struct NativeDrawing*d;if(!native_drawing_resolve(a,&d))return 0;if(!d)return 1;
 for(unsigned i=0;i<d->count;i++){
  const struct NativeDrawPart*p=&native_draw_parts[d->first_part+i];if(p->map==65535)continue;
  const struct NativeScaledMap*m=native_scaled_map(p->map);if(!m||!sink)return 0;
  if(m->first_chunk>NATIVE_SCALED_CHUNK_COUNT||m->count>NATIVE_SCALED_CHUNK_COUNT-m->first_chunk)return 0;
  if(!sink(ctx,p,m))return 0;
 }
 return 1;
}
