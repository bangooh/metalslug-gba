#include "native_inflate.h"
#include <stdint.h>
#ifdef __arm__
#define HOT __attribute__((section(".openinghot")))
#else
#define HOT
#endif
struct Bits {const uint8_t *p;unsigned bits,n;};
struct Tree {uint16_t counts[16],symbols[288];};
static HOT unsigned take(struct Bits *b,unsigned n){
 while(b->n<n){b->bits|=(unsigned)*b->p++<<b->n;b->n+=8;}
 unsigned v=b->bits&((1u<<n)-1);b->bits>>=n;b->n-=n;return v;
}
static HOT int tree(struct Tree *t,const uint8_t *lens,unsigned n){
 unsigned offsets[16];for(unsigned i=0;i<16;i++)t->counts[i]=0;
 for(unsigned i=0;i<n;i++){if(lens[i]>15)return 0;t->counts[lens[i]]++;}
 offsets[1]=0;for(unsigned i=1;i<15;i++)offsets[i+1]=offsets[i]+t->counts[i];
 for(unsigned i=0;i<n;i++)if(lens[i])t->symbols[offsets[lens[i]]++]=i;return 1;
}
static HOT int symbol(struct Bits *b,const struct Tree *t){
 unsigned code=0,first=0,index=0;
 for(unsigned n=1;n<=15;n++){
  code|=take(b,1);unsigned count=t->counts[n];if(code<first+count)return t->symbols[index+code-first];
  index+=count;first=(first+count)<<1;code<<=1;
 }return -1;
}
static HOT void fast_tree(const struct Tree *t,uint16_t fast[512]){
 for(unsigned i=0;i<512;i++)fast[i]=0;
 unsigned code=0,index=0;
 for(unsigned n=1;n<=9;n++){
  for(unsigned j=0;j<t->counts[n];j++){
   unsigned v=code+j,rev=0;for(unsigned k=0;k<n;k++){rev=(rev<<1)|(v&1);v>>=1;}
   for(unsigned k=rev;k<512;k+=1u<<n)fast[k]=(n<<9)|t->symbols[index+j];
  }
  index+=t->counts[n];code=(code+t->counts[n])<<1;
 }
}
static HOT int fast_symbol(struct Bits *b,const struct Tree *t,const uint16_t fast[512]){
 while(b->n<9){b->bits|=(unsigned)*b->p++<<b->n;b->n+=8;}
 unsigned v=fast[b->bits&511];if(!v)return symbol(b,t);
 b->bits>>=v>>9;b->n-=v>>9;return v&511;
}
static HOT void put(uint8_t *out,unsigned at,unsigned v,int vram){
 if(vram){volatile uint16_t *d=(volatile uint16_t*)(out+(at&~1u));unsigned s=(at&1)*8;*d=(*d&~(255u<<s))|(v<<s);}
 else out[at]=v;
}
HOT int native_inflate(const unsigned char *source,void *output,size_t bytes,int vram){
 static const uint16_t length_base[]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
 static const uint8_t length_extra[]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
 static const uint8_t order[]={16,17,18,0,8,7,9,6,10,5,11,4,10+2,3,13,2,14,1,15};
 struct Bits b={source,0,0};struct Tree lit,dist;uint8_t lengths[320];uint16_t fast[512];uint8_t *out=output;unsigned pos=0,final;
 do{
  final=take(&b,1);unsigned kind=take(&b,2);if(kind==3)return 0;
  if(!kind){
   b.bits=b.n=0;unsigned len=take(&b,16),inv=take(&b,16);if((len^inv)!=65535||len>bytes-pos)return 0;
   while(len--)put(out,pos++,take(&b,8),vram);continue;
  }
  if(kind==1){
   for(unsigned i=0;i<288;i++)lengths[i]=i<144?8:i<256?9:i<280?7:8;
   tree(&lit,lengths,288);for(unsigned i=0;i<32;i++)lengths[i]=5;tree(&dist,lengths,32);
  }else{
   unsigned nl=take(&b,5)+257,nd=take(&b,5)+1,nc=take(&b,4)+4;if(nl>286||nd>32)return 0;
   for(unsigned i=0;i<19;i++)lengths[i]=0;for(unsigned i=0;i<nc;i++)lengths[order[i]]=take(&b,3);
   if(!tree(&lit,lengths,19))return 0;
   for(unsigned i=0;i<nl+nd;){
    int s=symbol(&b,&lit);if(s<0)return 0;
    if(s<16){lengths[i++]=s;continue;}
    unsigned value=0,count;if(s==16){if(!i)return 0;value=lengths[i-1];count=take(&b,2)+3;}
    else if(s==17)count=take(&b,3)+3;else count=take(&b,7)+11;
    if(count>nl+nd-i)return 0;while(count--)lengths[i++]=value;
   }
   if(!tree(&lit,lengths,nl)||!tree(&dist,lengths+nl,nd))return 0;
  }
  fast_tree(&lit,fast);
  for(;;){
   int s=fast_symbol(&b,&lit,fast);if(s<0)return 0;if(s==256)break;
   if(s<256){if(pos>=bytes)return 0;put(out,pos++,s,vram);continue;}
   if(s>285)return 0;unsigned index=s-257,len=length_base[index]+take(&b,length_extra[index]);
   int d=symbol(&b,&dist);if(d<0||d>29)return 0;
   unsigned extra=d<4?0:((unsigned)d/2-1),base=d<4?(unsigned)d+1:((2u+(d&1))<<extra)+1;
   unsigned distance=base+take(&b,extra);if(distance>pos||len>bytes-pos)return 0;
   while(len--){unsigned value=out[pos-distance];put(out,pos++,value,vram);}
  }
 }while(!final);return pos==bytes;
}
