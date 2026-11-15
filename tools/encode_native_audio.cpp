// Offline mono IMA encoder. Save decoder state immediately BEFORE loop sample.
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
static const int steps[]={7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767};
static const int changes[]={-1,-1,-1,-1,2,4,6,8};
int main(int argc,char**argv){
 if(argc!=4)return 2;FILE*in=fopen(argv[1],"rb"),*out=fopen(argv[2],"wb");if(!in||!out)return 3;
 unsigned loop=strtoul(argv[3],0,10),pos=0;int pred=0,index=0,lp=0,li=0,pending=-1,c;
 while((c=fgetc(in))!=EOF){int high=fgetc(in);if(high==EOF)return 4;int value=(int16_t)(c|(high<<8));
  if(pos==loop){lp=pred;li=index;}int delta=value-pred,code=delta<0?8:0;if(delta<0)delta=-delta;int step=steps[index],v=step>>3;
  if(delta>=step){code|=4;delta-=step;v+=step;}if(delta>=(step>>1)){code|=2;delta-=step>>1;v+=step>>1;}if(delta>=(step>>2)){code|=1;v+=step>>2;}
  pred+=(code&8)?-v:v;if(pred>32767)pred=32767;if(pred< -32768)pred=-32768;
  index+=changes[code&7];if(index<0)index=0;if(index>88)index=88;
  if(pending<0)pending=code;else{fputc((pending<<4)|code,out);pending=-1;}pos++;
 }
 if(pending>=0)fputc(pending<<4,out);fclose(in);if(fclose(out))return 5;
 printf("{\"samples\":%u,\"loop_predictor\":%d,\"loop_index\":%d}\n",pos,lp,li);return 0;
}
