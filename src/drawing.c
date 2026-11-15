#include "drawing.h"
#include "../generated/drawing-data.h"
static const struct NativeDrawingSet*sets[32] __attribute__((section(".iwram.drawing_cache")));
int native_drawing_resolve(const struct Animation*a,const struct NativeDrawing**out){
 if(!a||!out)return 0;*out=0;if(a->drawing==65535)return 1;
 if(!a->drawing_program)return 0;
 uintptr_t key=(uintptr_t)a->drawing_program;unsigned slot=((key>>2)^(key>>9))&31;
 const struct NativeDrawingSet*s=sets[slot];
 if(!s||s->program!=a->drawing_program){
  unsigned i;for(i=0;i<NATIVE_DRAWING_SET_COUNT;i++)if(native_drawing_sets[i].program==a->drawing_program)break;
  if(i==NATIVE_DRAWING_SET_COUNT)return 0;s=&native_drawing_sets[i];sets[slot]=s;
 }
  if(a->drawing>=s->count)return 0;const struct NativeDrawing*d=&s->drawings[a->drawing];
  if(d->first_part>NATIVE_DRAW_PART_COUNT||d->count>NATIVE_DRAW_PART_COUNT-d->first_part)return 0;
  *out=d;return 1;
}
int native_drawing_visit(const struct Animation*a,NativeDrawSink sink,void*ctx){
 const struct NativeDrawing*d;if(!native_drawing_resolve(a,&d))return 0;if(!d)return 1;
 for(unsigned i=0;i<d->count;i++){
  const struct NativeDrawPart*p=&native_draw_parts[d->first_part+i];if(p->map==65535)continue;
  if(p->map>=NATIVE_DRAW_MAP_COUNT||!sink)return 0;const struct NativeDrawMap*m=&native_draw_maps[p->map];
  if(m->first_piece>NATIVE_DRAW_PIECE_COUNT||m->count>NATIVE_DRAW_PIECE_COUNT-m->first_piece)return 0;
  for(unsigned j=0;j<m->count;j++){
   const struct NativeDrawPiece*q=&native_draw_pieces[m->first_piece+j];unsigned n=(unsigned)q->width*q->height;
   if(q->first_tile>NATIVE_DRAW_TILE_COUNT||n>NATIVE_DRAW_TILE_COUNT-q->first_tile)return 0;
   if(!sink(ctx,p,m,q,&native_draw_tiles[q->first_tile]))return 0;
  }
 }
 return 1;
}
const uint8_t*native_sprite_tile(uint32_t n){return n<NATIVE_SPRITE_TILE_COUNT?native_sprite_pixels+n*128:0;}
const uint8_t*native_fixed_tile(unsigned n){return n<NATIVE_FIXED_TILE_COUNT?native_fixed_pixels+n*32:0;}
const uint16_t*native_palette(unsigned n){return n<NATIVE_PALETTE_COUNT?native_source_palettes[n]:0;}
