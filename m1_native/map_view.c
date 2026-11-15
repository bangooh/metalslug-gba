#include "map_view.h"
static int floor8(int32_t n){return n>=0?n/8:-1-(int)((-(int64_t)n-1)/8);}
int m1_map_prepare(struct M1MapFrame*f,struct M1TileStream*s,const struct M1MapView*v,unsigned layers){
 if(!f||!s||!v||!layers||layers>M1_BG_PLANES)return 0;
 for(unsigned l=0;l<layers;l++)if(!v[l].map||!v[l].width||!v[l].height)return 0;
 unsigned n=0;
 for(unsigned l=0;l<layers;l++){
  int ox=floor8(v[l].scroll_x),oy=floor8(v[l].scroll_y);
  for(unsigned y=0;y<21;y++)for(unsigned x=0;x<31;x++){
   int mx=ox+(int)x,my=oy+(int)y;
   f->refs[n++]=(mx<0||my<0||(unsigned)mx>=v[l].width||(unsigned)my>=v[l].height)?0:v[l].map[(unsigned)my*v[l].width+(unsigned)mx];
  }
 }
 if(!m1_stream_plan(s,f->refs,n))return 0;
 n=0;
 for(unsigned l=0;l<layers;l++){
  int ox=floor8(v[l].scroll_x),oy=floor8(v[l].scroll_y);
  f->dirty_rows[l]=0;
  for(unsigned y=0;y<21;y++)for(unsigned x=0;x<31;x++){
   unsigned row=(unsigned)(oy+(int)y)&31,at=row*32+((unsigned)(ox+(int)x)&31),id=m1_stream_find(s,f->refs[n++]);
   if(f->maps[l][at]!=id){f->maps[l][at]=id;f->dirty_rows[l]|=1u<<row;}
  }
  f->scroll_x[l]=(uint32_t)v[l].scroll_x&255;f->scroll_y[l]=(uint32_t)v[l].scroll_y&255;
 }
 f->layers=layers;f->ref_count=n;return 1;
}
