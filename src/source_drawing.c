#include "source_drawing.h"
#include "../generated/drawing-data.h"
static int16_t word(int x){return (int16_t)(uint16_t)x;}
static int floor256(int n){return n>=0?n/256:-((-n+255)/256);}
void native_source_anchor(struct NativeSourceDrawPose*p,const struct PlayerTransform*t,int32_t cx,int32_t cy,int direct){
 if(!p||!t)return;
 p->x=word((t->x>>8)-cx);p->y=word((direct?496:497)-word(transform_y_up(t)+cy));
}
void native_source_reflection(struct NativeSourceDrawPose*p,unsigned scene,int16_t sx,int16_t sy,uint8_t raw){
 if(!p)return;p->reflection=scene==0&&sx<=0x780&&raw==0x3a;p->reflection_y=word(343-sy);
}
int native_source_blink(const struct Animation*a,uint8_t damage,uint8_t hit,uint8_t phase,uint8_t*movement){
 if(!a||!movement)return 0;if(a->drawing_flags&1||!(damage&1))return 1;
 if(!(hit&65)&&!(phase&2))return 0;*movement|=128;return 1;
}
void native_source_project(struct NativeSourceDrawPose*p,int16_t px,int16_t py){
 if(!p)return;
 p->x=word(px+floor256((int)word(p->x-px)*((int)p->zoom_x+1)));
 /* Inverted source Y-up uses floor before conversion to the Y-down view. */
 p->y=word(py-floor256((int)word(py-p->y)*((int)p->zoom_y+1)));
}
int native_source_visit(const struct Animation*a,const struct NativeSourceDrawPose*p,NativeSourcePartSink sink,void*ctx,uint16_t*count){
 if(!a||!p||!sink||!count)return 0;*count=0;const struct NativeDrawing*d;
 if(!native_drawing_resolve(a,&d))return 0;
 return native_source_visit_resolved(d,a->drawing_flags,p,sink,ctx,count);
}
int native_source_visit_resolved(const struct NativeDrawing*d,unsigned flags,const struct NativeSourceDrawPose*p,NativeSourcePartSink sink,void*ctx,uint16_t*count){
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
   .x=word(p->x+((flip&1)?-dx:dx)),.y=word(p->y-((flip&2)?-dy:dy)),
   .order=~((priority<<16)|(p->sequence+*count+stride-1)),.palette=p->palette,
   .flip=flip,.priority=p->gba_priority,.zoom_x=p->zoom_x,.zoom_y=p->zoom_y};
  /*028F6C ordinary part gate precedes splash dispatch. Y-up is the packed
   * sprite anchor, not world ground Y. Alternate1E has no such gate. */
  if(!alternate&&(place.x<=-256||place.x>=512||word(496-place.y)<=0||word(496-place.y)>=768))continue;
  if(p->reflection){
   struct NativeObjectPlacement reflected=place;
   reflected.y=word(2*p->reflection_y-place.y+2);reflected.flip^=2;reflected.zoom_y=127;
   reflected.order=~((priority<<16)|(p->sequence+*count));
   if(!sink(ctx,q->map,&reflected))return 0;
  }
  if(!sink(ctx,q->map,&place))return 0;*count+=stride;
 }
 return 1;
}
static int object_part(void*ctx,unsigned map,const struct NativeObjectPlacement*p){return native_object_map(ctx,map,p);}
int native_source_drawing(struct NativeObjectFrame*f,const struct Animation*a,const struct NativeSourceDrawPose*p){
 if(!f||f->fault||f->ready)return 0;uint16_t count;
 if(native_source_visit(a,p,object_part,f,&count))return 1;
 if(!f->fault)f->fault=NOBJ_METADATA;return 0;
}
