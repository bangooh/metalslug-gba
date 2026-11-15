#include "terrain_edits.h"
#include "../generated/assets.h"
struct TerrainEdits terrain_edits;
void terrain_edits_reset(void){terrain_edits=(struct TerrainEdits){0};}
unsigned terrain_cell(unsigned index){
 if(!terrain_edits.count)return terrain_cells[index];
 unsigned key=index+1,at=(key*40503u)&255;
 for(unsigned n=0;n<TERRAIN_EDITS;n++,at=(at+1)&255){
  const struct TerrainEdit*e=&terrain_edits.slots[at];
  if(!e->key)return terrain_cells[index];if(e->key==key)return e->value;
 }
 return terrain_cells[index];
}
static int write_cell(unsigned index,unsigned value){
 unsigned key=index+1,at=(key*40503u)&255;
 for(unsigned n=0;n<TERRAIN_EDITS;n++,at=(at+1)&255){
  struct TerrainEdit*e=&terrain_edits.slots[at];
  if(!e->key||e->key==key){
   if(!e->key){if(value==terrain_cells[index])return 1;terrain_edits.count++;}
   *e=(struct TerrainEdit){(uint16_t)key,(uint8_t)value,0};return 1;
  }
 }
 terrain_edits.fault=1;return 0;
}
/* $043FAC/$043F5E column-major 8px collision cells, translated to world
 * coordinates. This changes collision bytes only, never background pixels. */
int terrain_stamp(int x,int y,unsigned width,unsigned height,const uint8_t*cells){
 int cx=x>>3,cy=y>>3;
 for(unsigned col=0;col<width;col++)for(unsigned row=0;row<height;row++){
  int xx=cx+(int)col,yy=cy+(int)row;
  if(xx<0||yy<0||xx>=CELL_W||yy>=WORLD_H/8)continue;
  if(!write_cell((unsigned)(yy*CELL_W+xx),cells[col*height+row]))return 0;
 }
 return 1;
}
