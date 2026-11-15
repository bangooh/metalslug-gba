#include "source_strip_budget.h"
#include "../generated/drawing-data.h"
#include "../generated/strip-plans.inc"
/* Exact counts for consecutive columns in the source 32-column ring.
 * Full-scale admission needs counts only; no per-column iteration. */
static const uint8_t visible_columns[32][32]={
 {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,20,20,20,20,20,20,20,20,20,20,20},
 {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,19,19,19,19,19,19,19,19,19,19,19,20},
 {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,18,18,18,18,18,18,18,18,18,18,18,19,20},
 {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,17,17,17,17,17,17,17,17,17,17,17,18,19,20},
 {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,16,16,16,16,16,16,16,16,16,16,16,17,18,19,20},
 {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,15,15,15,15,15,15,15,15,15,15,15,16,17,18,19,20},
 {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,14,14,14,14,14,14,14,14,14,14,14,15,16,17,18,19,20},
 {0,1,2,3,4,5,6,7,8,9,10,11,12,13,13,13,13,13,13,13,13,13,13,13,13,14,15,16,17,18,19,20},
 {0,1,2,3,4,5,6,7,8,9,10,11,12,12,12,12,12,12,12,12,12,12,12,12,13,14,15,16,17,18,19,20},
 {0,1,2,3,4,5,6,7,8,9,10,11,11,11,11,11,11,11,11,11,11,11,11,12,13,14,15,16,17,18,19,20},
 {0,1,2,3,4,5,6,7,8,9,10,10,10,10,10,10,10,10,10,10,10,10,11,12,13,14,15,16,17,18,19,20},
 {0,1,2,3,4,5,6,7,8,9,9,9,9,9,9,9,9,9,9,9,9,10,11,12,13,14,15,16,17,18,19,20},
 {0,1,2,3,4,5,6,7,8,8,8,8,8,8,8,8,8,8,8,8,9,10,11,12,13,14,15,16,17,18,19,20},
 {0,1,2,3,4,5,6,7,7,7,7,7,7,7,7,7,7,7,7,8,9,10,11,12,13,14,15,16,17,18,19,20},
 {0,1,2,3,4,5,6,6,6,6,6,6,6,6,6,6,6,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20},
 {0,1,2,3,4,5,5,5,5,5,5,5,5,5,5,5,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20},
 {0,1,2,3,4,4,4,4,4,4,4,4,4,4,4,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20},
 {0,1,2,3,3,3,3,3,3,3,3,3,3,3,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20},
 {0,1,2,2,2,2,2,2,2,2,2,2,2,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20},
 {0,1,1,1,1,1,1,1,1,1,1,1,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20},
 {0,0,0,0,0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20},
 {0,0,0,0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21},
 {0,0,0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,21},
 {0,0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,21,21},
 {0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,21,21,21},
 {0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,21,21,21,21},
 {0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,21,21,21,21,21},
 {0,0,0,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,21,21,21,21,21,21},
 {0,0,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,21,21,21,21,21,21,21},
 {0,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,21,21,21,21,21,21,21,21},
 {0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,21,21,21,21,21,21,21,21,21},
 {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,21,21,21,21,21,21,21,21,21,21}
};
static int fault(struct NativeSourceStripBudget*q){if(q)q->fault=1;return -1;}
static int floor256(int32_t n){return n>=0?n/256:-((-n+255)/256);}
void native_source_strips_begin(struct NativeSourceStripBudget*q){if(q)*q=(struct NativeSourceStripBudget){0};}
__attribute__((section(".iwram.render_hot"),noinline,long_call)) int native_source_strips_map(struct NativeSourceStripBudget*q,unsigned id,const struct NativeObjectPlacement*p,int forced){
 if(!q||q->fault||!p||id>=NATIVE_DRAW_MAP_COUNT||p->flip>3)return fault(q);
 /*05A9E6 performs these admission checks before inspecting any map piece. */
 if(q->records>=380){q->record_drops++;return 0;}
 if(q->keys>=210){q->key_drops++;return 0;}
 if(p->zoom_x==255){
  unsigned x=(unsigned)p->x&511;
  unsigned row=strip_plans[strip_plan_ids[id]][((p->flip&1)<<4)|(x&15)];
  unsigned admitted=strip_rows[row][x>>4];
  if(!admitted){q->empty_maps++;return 0;}
  if(admitted>380u-q->records)admitted=380u-q->records;
  q->records+=admitted;q->strips[forced!=0]+=admitted;q->keys++;return 1;
 }
 const struct NativeDrawMap*m=&native_draw_maps[id];unsigned before=q->records;
 if(!m->count||m->first_piece+m->count>NATIVE_DRAW_PIECE_COUNT)return fault(q);
 for(unsigned i=0;i<m->count;i++){
  const struct NativeDrawPiece*c=&native_draw_pieces[m->first_piece+i];
  if(!c->width||!c->height||c->height>32)return fault(q);
  /* At full source scale every strip is16px. Clipping depends only on
   * the five-bit column, so avoid per-strip zoom/fraction arithmetic. */
  if(p->zoom_x==255){
   unsigned flip=p->flip&1;
   int offset=flip?(int16_t)(uint16_t)~c->x:c->x;
   unsigned column=((uint16_t)(p->x+offset-(flip?16:0))>>4)&31;
   unsigned width=c->width,tail=width&31;
   if(flip)column=(column+1-tail)&31;
   unsigned admitted=(width>>5)*21+visible_columns[column][tail];
   if(admitted>380u-q->records)admitted=380u-q->records;
   q->records+=admitted;q->strips[forced!=0]+=admitted;
   if(q->records==380)goto done;
   continue;
  }
  /*05AB34/05AE00 NOT.W precedes scaling; reflected X increments its
   * fraction while ordinary X subtracts it. Zero-width columns vanish. */
  int offset=(p->flip&1)?(int16_t)(uint16_t)~c->x:c->x;
  int32_t product=(int32_t)offset*((int32_t)p->zoom_x+1);
  uint16_t x=(uint16_t)(p->x+floor256(product));
  unsigned fraction=(uint8_t)product,deduct=(15-(p->zoom_x&15))*16;
  for(unsigned col=0;col<c->width;col++){
   unsigned width=(p->zoom_x>>4)+1;
   if(p->flip&1){unsigned sum=fraction+deduct;width-=sum>255;fraction=(uint8_t)sum;}
   else{width-=fraction<deduct;fraction=(uint8_t)(fraction-deduct);}
   if(!width)continue;
   if(p->flip&1)x=(uint16_t)(x-width);
   unsigned wrapped=x&511;
   /* Original packed comparisons:A000 <= X<<7|rows < F800 is rejected.
    * The left margin is16 even for a narrower zoomed strip. */
   if(wrapped<320||wrapped>=496){
    q->records++;q->strips[forced!=0]++;
    if(q->records==380)goto done;
   }
   if(!(p->flip&1))x=(uint16_t)(x+width);
  }
 }
done:
 if(q->records==before){q->empty_maps++;return 0;}
 q->keys++;return 1;
}
int native_source_strips_cached(struct NativeSourceStripBudget*q,unsigned strips,int forced){
 if(!q||q->fault)return fault(q);
 if(q->records>=380){q->record_drops++;return 0;}
 if(q->keys>=210){q->key_drops++;return 0;}
 if(!strips){q->empty_maps++;return 0;}
 if(strips>380u-q->records)strips=380u-q->records;
 q->records+=strips;q->strips[forced!=0]+=strips;q->keys++;return 1;
}
int native_source_strips_free(const struct NativeSourceStripBudget*q,const struct SceneSpritePools*p,uint16_t out[2]){
 if(!q||q->fault||!p||!p->valid||!out||p->lower_end>p->upper_begin||p->upper_begin>p->forced_begin||p->forced_begin>380)return 0;
 unsigned limit[2]={p->upper_begin-p->lower_end,380-p->forced_begin};
 /*05B400 saturates independently at each range boundary. Sort direction
  * changes WHICH strips survive, but not the final number of free strips. */
 for(unsigned i=0;i<2;i++)out[i]=q->strips[i]<limit[i]?limit[i]-q->strips[i]:0;
 return 1;
}
