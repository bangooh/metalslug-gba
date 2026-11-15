#include "tile_stream.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct M1TileStream s, unique;
int main(void){
 assert(m1_stream_init(&s,8));
 uint16_t a[]={0,1,2,3,1,2};assert(m1_stream_plan(&s,a,6));assert(s.upload_count==3);
 uint16_t previous=m1_stream_find(&s,2);assert(m1_stream_plan(&s,a,6));assert(s.upload_count==0);
 uint16_t b[]={2,3,4,5,6,7};assert(m1_stream_plan(&s,b,6));assert(s.upload_count==4);assert(m1_stream_find(&s,2)==previous);
 for(unsigned i=0;i<6;i++)assert(s.owner[m1_stream_find(&s,b[i])]==b[i]);
 uint16_t owners[8];memcpy(owners,s.owner,sizeof owners);
 uint16_t excess[]={1,2,3,4,5,6,7,8};assert(!m1_stream_plan(&s,excess,8));assert(!memcmp(owners,s.owner,sizeof owners));
 uint16_t invalid[]={16384};assert(!m1_stream_plan(&s,invalid,1));assert(!memcmp(owners,s.owner,sizeof owners));
 assert(m1_stream_init(&s,896));assert(m1_stream_init(&unique,896));unsigned seed=17;
 for(unsigned trial=0;trial<5000;trial++){
  uint16_t refs[700];for(unsigned i=0;i<700;i++){seed=seed*1664525u+1013904223u;refs[i]=(seed>>8)%M1_ROM_TILES;}
  unsigned count=0;unsigned char seen[M1_ROM_TILES/8]={0};
  for(unsigned i=0;i<700;i++)if(refs[i]&&!(seen[refs[i]>>3]&(1u<<(refs[i]&7)))){
   seen[refs[i]>>3]|=1u<<(refs[i]&7);unique.uploads[count++].rom_tile=refs[i];
  }
  assert(m1_stream_plan(&s,refs,700));
  assert(m1_stream_plan_unique(&unique,count));
  assert(s.upload_count==unique.upload_count);
  assert(!memcmp(s.uploads,unique.uploads,s.upload_count*sizeof(s.uploads[0])));
  assert(!memcmp(s.owner,unique.owner,sizeof(s.owner)));
  assert(!memcmp(s.hash,unique.hash,sizeof(s.hash)));
  for(unsigned i=0;i<700;i++){assert(s.owner[m1_stream_find(&s,refs[i])]==refs[i]);assert(m1_stream_find(&s,refs[i])==m1_stream_find(&s,refs[i]));}
  for(unsigned i=0;i<s.capacity;i++)if(s.owner[i]!=65535)assert(m1_stream_find(&s,s.owner[i])==i);
 }
 puts("PASS: reuse, pinning, eviction, over-budget, invalid-ID transactions and 5000 unique-planner equivalence trials");
}
