#include "native_snapshot.h"
#include <stddef.h>
static uint16_t*address(struct NativeSnapshot*s,uint32_t at){
 for(unsigned i=0;i<2;i++)if(at>=s->cache_base[i]&&at<s->cache_end[i])return s->cache[i]+at-s->cache_base[i];
 uint32_t base=0;
 for(unsigned i=0;i<s->count;i++){
  uint32_t end=base+s->ranges[i].bytes/2;
  if(at<end){s->cache[1]=s->cache[0];s->cache_base[1]=s->cache_base[0];s->cache_end[1]=s->cache_end[0];
   s->cache[0]=s->ranges[i].data;s->cache_base[0]=base;s->cache_end[0]=end;return s->cache[0]+at-base;}
  base=end;
 }return 0;
}
static uint16_t read_word(struct NativeSnapshot*s,uint32_t at){return *(volatile uint16_t*)address(s,at);}
static uint32_t hash_word(uint32_t h,uint16_t v){h=(h^(v&255))*16777619u;return (h^(v>>8))*16777619u;}
int native_snapshot_init(struct NativeSnapshot*s,const struct NativeSnapshotRange*r,unsigned n){
 if(!s||!r||!n)return 0;*s=(struct NativeSnapshot){.ranges=r,.count=n,.hash=2166136261u};
 for(unsigned i=0;i<n;i++){if(!r[i].data||((uintptr_t)r[i].data&1)||(r[i].bytes&1)||r[i].bytes>0x80000)return 0;s->words+=r[i].bytes/2;}
 return s->words&&s->words<=0x40000;
}
uint32_t native_snapshot_hash(struct NativeSnapshot*s){uint32_t h=2166136261u;for(uint32_t i=0;i<s->words;i++)h=hash_word(h,read_word(s,i));return h;}
void native_snapshot_encoder(struct NativeSnapshotEncoder*e,const struct NativeSnapshot*s){e->state=*s;for(unsigned i=0;i<256;i++)e->recent[i]=0xffffffffu;}
static unsigned bucket(struct NativeSnapshot*s,uint32_t p){return (read_word(s,p)*33u^read_word(s,p+1)*7u^read_word(s,p+2))&255;}
static void advance(struct NativeSnapshotEncoder*e,unsigned n){
 while(n--){uint32_t p=e->state.position++;if(p+2<e->state.words)e->recent[bucket(&e->state,p)]=p;}
}
static unsigned match(struct NativeSnapshotEncoder*e,unsigned*distance){
 struct NativeSnapshot*s=&e->state;uint32_t p=s->position;unsigned n=0;*distance=0;
 if(!read_word(s,p)){while(p+n<s->words&&n<16384&&!read_word(s,p+n))n++;if(n>=3)return n;}
 if(p+2>=s->words)return 0;uint32_t old=e->recent[bucket(s,p)];
 if(old>=p)return 0;
 n=0;while(n<4096&&p+n<s->words&&read_word(s,p+n)==read_word(s,old+n))n++;
 if(n<(p-old>256?8u:3u))return 0;*distance=p-old;return n;
}
unsigned native_snapshot_encode(struct NativeSnapshotEncoder*e,uint16_t*out,unsigned capacity){
 if(!e||!out||capacity<4)return 0;unsigned used=0;
 while(e->state.position<e->state.words&&used+1<capacity){
  unsigned distance,n=match(e,&distance);
  if(n){
   if(distance&&(distance>256||n>65)){
    if(capacity-used<4)break;out[used++]=65535;out[used++]=(uint16_t)n;out[used++]=(uint16_t)distance;out[used++]=(uint16_t)(distance>>16);
   }else out[used++]=(uint16_t)(distance?(0xc000u|((n-3)<<8)|(distance-1)):(0x8000u|(n-1)));
   if(!distance&&n>2){e->recent[0]=e->state.position+n-3;e->state.position+=n-2;advance(e,2);}else advance(e,n);
  }
  else{
   unsigned command=used++,start=used;
   do{out[used++]=read_word(&e->state,e->state.position);advance(e,1);}
   while(used<capacity&&e->state.position<e->state.words&&!match(e,&distance));
   out[command]=(uint16_t)(used-start-1);
  }
 }
 return used;
}
int native_snapshot_decode(struct NativeSnapshot*s,const uint16_t*data,unsigned count){
 if(!s||!data)return 0;unsigned i=0;
 while(i<count){
  uint16_t token=data[i++];unsigned n,distance=0;
  if(token==65535){if(count-i<3)return 0;n=data[i++];distance=data[i]|((uint32_t)data[i+1]<<16);i+=2;if(n<3||!distance||distance>s->position)return 0;}
  else if(token<0x8000){n=token+1;if(n>count-i)return 0;}
  else if(token<0xc000)n=(token&0x3fff)+1;
  else{n=((token>>8)&63)+3;distance=(token&255)+1;if(distance>s->position)return 0;}
  if(n>s->words-s->position)return 0;
  while(n--){uint16_t v=token<0x8000?data[i++]:distance?read_word(s,s->position-distance):0;
   *(volatile uint16_t*)address(s,s->position++)=v;s->hash=hash_word(s->hash,v);}
 }
 return 1;
}
