#ifndef NATIVE_SOURCE_VISIT_INLINE_H
#define NATIVE_SOURCE_VISIT_INLINE_H
#include "source_drawing.h"
#include "../generated/drawing-data.h"
/* Capture specialization of the source visitor. Constant sinks let the ARM
 * compiler remove the per-part indirect call and temporary placement copies.
 * Keep the ordinary visitor as an independently compiled differential oracle. */
static inline int16_t native_source_inline_word(int x){return (int16_t)(uint16_t)x;}
static inline __attribute__((always_inline)) int native_source_visit_inline(const struct NativeDrawing*d,unsigned flags,const struct NativeSourceDrawPose*p,NativeSourcePartSink sink,void*ctx,uint16_t*count){
 if(!p||!sink||!count)return 0;*count=0;if(!d)return 1;
 unsigned stride=p->reflection?2:1;
 if(p->gba_priority>3||d->count*stride>65535u-p->sequence)return 0;
 int alternate=p->direct||(flags&1);
 if(p->direct&&d->count!=1)return 0; /*05CA2A is one raw map.*/
 for(unsigned i=0;i<d->count;i++){
  const struct NativeDrawPart*q=&native_draw_parts[d->first_part+i];if(q->map==65535)continue;
  /*028EC6: ordinary DRAW flag2 is suppressed by source entity12.bit3. */
  if(!alternate&&(q->flags&4)&&(p->display_flags&8))continue;
  unsigned flip=(p->flip^(alternate?0:q->flags))&3;
  int dx=alternate||q->flags&8?0:q->x,dy=alternate||q->flags&8?0:q->y_up;
  unsigned priority=alternate||q->priority==65535?p->priority:q->priority;
  /*5B232 sorts unsigned(priority,queue-offset). NeoGeo higher indices
   *cover lower ones; GBA uses the opposite OAM order. */
  struct NativeObjectPlacement place={
   .x=native_source_inline_word(p->x+((flip&1)?-dx:dx)),.y=native_source_inline_word(p->y-((flip&2)?-dy:dy)),
   .order=~((priority<<16)|(p->sequence+*count+stride-1)),.palette=p->palette,
   .flip=flip,.priority=p->gba_priority,.zoom_x=p->zoom_x,.zoom_y=p->zoom_y};
  /*028F6C ordinary part gate precedes splash dispatch. Y-up is the packed
   * sprite anchor, not world ground Y. Alternate1E has no such gate. */
  if(!alternate&&(place.x<=-256||place.x>=512||native_source_inline_word(496-place.y)<=0||native_source_inline_word(496-place.y)>=768))continue;
  if(p->reflection){
   struct NativeObjectPlacement reflected=place;
   reflected.y=native_source_inline_word(2*p->reflection_y-place.y+2);reflected.flip^=2;reflected.zoom_y=127;
   reflected.order=~((priority<<16)|(p->sequence+*count));
   if(!sink(ctx,q->map,&reflected))return 0;
  }
  if(!sink(ctx,q->map,&place))return 0;*count+=stride;
 }
 return 1;
}
#endif
