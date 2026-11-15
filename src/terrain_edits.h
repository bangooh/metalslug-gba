#ifndef TERRAIN_EDITS_H
#define TERRAIN_EDITS_H
#include <stdint.h>
#define TERRAIN_EDITS 256
struct TerrainEdit {uint16_t key;uint8_t value,pad;};
struct TerrainEdits {struct TerrainEdit slots[TERRAIN_EDITS];uint16_t count,fault;};
extern struct TerrainEdits terrain_edits;
void terrain_edits_reset(void);
unsigned terrain_cell(unsigned index);
int terrain_stamp(int x,int y,unsigned width,unsigned height,const uint8_t*column_cells);
#endif
