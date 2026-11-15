#include "map_view.h"
#include <assert.h>
#include <stdio.h>
static struct M1TileStream s;
static struct M1MapFrame f;
static uint16_t map[64*32];
int main(void){
 for(unsigned i=0;i<64*32;i++)map[i]=1+i%800;
 assert(m1_stream_init(&s,928));struct M1MapView v={map,64,32,0,0};
 assert(m1_map_prepare(&f,&s,&v,1));assert(s.upload_count>0);
 v.scroll_x=1;assert(m1_map_prepare(&f,&s,&v,1));assert(s.upload_count==0);assert(f.dirty_rows[0]==0);assert(f.scroll_x[0]==1);
 v.scroll_x=8;assert(m1_map_prepare(&f,&s,&v,1));assert(s.upload_count<=21);
 for(unsigned y=0;y<20;y++)for(unsigned x=0;x<30;x++)assert(s.owner[f.maps[0][y*32+x+1]]==map[y*64+x+1]);
 v.scroll_x=-1;v.scroll_y=-1;assert(m1_map_prepare(&f,&s,&v,1));assert(f.maps[0][31*32+31]==0);assert(f.scroll_x[0]==255);
 puts("PASS: subpixel scroll has zero uploads, tile-boundary scroll, ring addresses, negative clipping");
}
