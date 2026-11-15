/* Timer-0 Direct Sound A, DMA1 FIFO interrupts. The producer stays active
 * during long game frames. Audio owns no simulation state and cannot use the
 * gameplay state from its interrupt handler. */
#include <stdint.h>
#include "audio.h"
#include "../generated/audio_data.h"
#include "../generated/audio_ima_fast.h"
#define R16(a) (*(volatile uint16_t*)(a))
#define R32(a) (*(volatile uint32_t*)(a))
#define DMA_CONTROL 0xf6400000u
#define BLOCK 16u
#define RING 256u
#define VOICES 3u
extern const unsigned char audio_music[],audio_music_tail[],audio_effects[];
/* Guard mirrors the first 128 bytes so DMA can pass a wrap during a long
 * DMA3 transfer. Timer1 counts consumed samples even while CPU IRQs wait. */
/* Independent IRQ stack avoids overlapping the existing main/IWRAM stack. */
unsigned audio_irq_stack[128] __attribute__((aligned(8)));
static signed char ring[RING+128] __attribute__((aligned(4)));
static volatile unsigned head,tail;
static uint16_t commands[16];
static unsigned cursor,ready,sample_origin;
struct Music {const struct AudioAsset*asset;unsigned position;int predictor,index,previous;unsigned phase;};
static struct Music music;
struct Voice {const struct AudioAsset*asset;unsigned position,remaining,priority;int predictor,index;};
static struct Voice voices[VOICES];
/* IRQs, samples, maximum IRQ cycles, queue overflow, unknown command,
 * clipped samples, music command, last unsupported command. */
volatile unsigned g_audio_debug[10];
static unsigned cycles(void){unsigned h,l;do{h=R16(0x0400010c);l=R16(0x04000108);}while(h!=R16(0x0400010c));return h<<16|l;}
static const struct AudioAsset*find(unsigned command){
 unsigned lo=0,hi=AUDIO_ASSET_COUNT;while(lo<hi){unsigned mid=(lo+hi)/2;if(audio_assets[mid].command<command)lo=mid+1;else hi=mid;}
 return lo<AUDIO_ASSET_COUNT&&audio_assets[lo].command==command?&audio_assets[lo]:0;
}
static void select_command(unsigned command){
 if(!command){music.asset=0;g_audio_debug[6]=0;for(unsigned i=0;i<VOICES;i++)voices[i].remaining=0;return;}
 if(command==0xffff)return;
 if(command&0x8000){command&=0x7fff;for(unsigned i=0;i<VOICES;i++)if(voices[i].asset&&voices[i].asset->command==command)voices[i].remaining=0;return;}
 const struct AudioAsset*a=find(command);if(!a){g_audio_debug[4]++;g_audio_debug[7]=command;return;}
 if(command<0x1000){music=(struct Music){.asset=a};g_audio_debug[6]=command;return;}
 for(unsigned i=0;i<VOICES;i++)if(voices[i].remaining&&voices[i].asset==a&&a->samples>8192)return;
 unsigned slot=VOICES;
 for(unsigned i=0;i<VOICES;i++)if(!voices[i].remaining){slot=i;break;}
 if(slot==VOICES){
  /* Never let a repeated gunshot evict a mission/weapon announcement. */
  slot=0;for(unsigned i=1;i<VOICES;i++)if(voices[i].priority<voices[slot].priority||(voices[i].priority==voices[slot].priority&&voices[i].remaining<voices[slot].remaining))slot=i;
  if(voices[slot].priority>a->priority)return;
 }
 voices[slot]=(struct Voice){a,0,a->samples,a->priority,0,0};
}
static inline __attribute__((always_inline)) int music_sample(struct Music*m){
 const struct AudioAsset*a=m->asset;if(!a)return 0;
 m->phase+=7;if(m->phase<8)return (m->previous*(8-(int)m->phase)+m->predictor*(int)m->phase)>>12;
 m->phase-=8;m->previous=m->predictor;
 if(m->position==a->samples){
  if(a->loop==0xffffffffu){m->asset=0;return 0;}
  m->position=a->loop;m->predictor=a->loop_predictor;m->index=a->loop_index;
 }
 unsigned p=m->position++,off=a->offset+(p>>1),n=off<0x100000?audio_music[off]:audio_music_tail[off-0x100000];n=(p&1)?n&15:n>>4;
 int v=m->predictor,step=m->index;
 unsigned packed=audio_ima_fast[step*16+n];v+=(int32_t)(packed<<14)>>14;
 if(v>32767)v=32767;else if(v< -32768)v=-32768;
 m->predictor=v;m->index=packed>>18;
 return (m->previous*(8-(int)m->phase)+v*(int)m->phase)>>12; /* 7168 -> 8192 Hz; half-volume. */
}
/* Process one voice across the block: decoder state stays in registers and
 * inactive voices are checked once, while retaining the exact sample sum. */
static __attribute__((noinline,section(".iwram.audio_fill"),target("arm"))) void fill(unsigned at,unsigned count){
 int mix[BLOCK];struct Music m=music;
 for(unsigned n=0;n<count;n++)mix[n]=music_sample(&m);
 music=m;
 for(unsigned i=0;i<VOICES;i++){
  struct Voice*v=&voices[i];unsigned length=v->remaining;if(!length)continue;
  const struct AudioAsset*a=v->asset;unsigned output=0;
  while(output<count&&v->remaining){unsigned length=v->remaining;if(length>count-output)length=count-output;unsigned pos=v->position;
   if(!a->format){const signed char*p=(const signed char*)audio_effects+a->offset+pos;for(unsigned n=0;n<length;n++)mix[output+n]+=p[n];}
   else {int value=v->predictor,index=v->index;const unsigned char*p=audio_effects+a->offset;
    for(unsigned j=0;j<length;j++){unsigned n=p[(pos+j)>>1];n=((pos+j)&1)?n&15:n>>4;
     unsigned packed=audio_ima_fast[index*16+n];value+=(int32_t)(packed<<14)>>14;
     if(value>32767)value=32767;else if(value< -32768)value=-32768;
     index=packed>>18;mix[output+j]+=value>>8;
    }v->predictor=value;v->index=index;
   }v->position+=length;v->remaining-=length;output+=length;
   if(!v->remaining&&a->loop!=0xffffffffu){v->position=a->loop;v->remaining=a->samples-a->loop;v->predictor=a->loop_predictor;v->index=a->loop_index;}
  }
 }
 for(unsigned n=0;n<count;n++){int sample=mix[n];
  if(sample>127){sample=127;g_audio_debug[5]++;}else if(sample< -128){sample=-128;g_audio_debug[5]++;}
  ring[at+n]=(signed char)sample;
 }
}
/* Called in ARM state by the BIOS IRQ dispatcher, which saves r0-r3/r12/LR.
 * Only acknowledge DMA1; serial IF bit 7 belongs to the polling link driver. */
static void audio_service(void){
 if(!(R16(0x04000202)&0x200))return;
 unsigned start=cycles();R16(0x04000202)=0x200;
 /* FIFO prefetch phase is measured on its first request, rather than
  * assuming how its internal working word contributes to the threshold. */
 if(!g_audio_debug[0])sample_origin=R16(0x04000104);
 unsigned current=((((R16(0x04000104)-sample_origin)&65535)>>4)+1)&4095,blocks=(current-cursor)&4095;
 if((cursor&15)+blocks>=16){
  /* Preserve the sample timer phase while restarting the DMA source. */
  R16(0x04000102)=0;unsigned phase=R16(0x04000100);R32(0x040000c4)=0;
  current=((((R16(0x04000104)-sample_origin)&65535)>>4)+1)&4095;blocks=(current-cursor)&4095;
  R32(0x040000bc)=(unsigned)(ring+(current&15)*BLOCK);R32(0x040000c4)=DMA_CONTROL;
  R16(0x04000100)=phase;R16(0x04000102)=0x80;R16(0x04000100)=0xf800;
 }
 if(blocks>g_audio_debug[8])g_audio_debug[8]=blocks;
 if(blocks>8){g_audio_debug[9]++;blocks=8;cursor=(current-8)&4095;}
 while(tail!=head){unsigned command=commands[tail&15];tail++;select_command(command);}
 for(unsigned b=0;b<blocks;b++){
  unsigned used=(cursor&15)*BLOCK;fill(used,BLOCK);
  if(used<128)for(unsigned n=0;n<BLOCK;n++)ring[RING+used+n]=ring[used+n];
  cursor=(cursor+1)&4095;
 }
 g_audio_debug[0]++;g_audio_debug[1]+=blocks*BLOCK;
 unsigned dt=cycles()-start;if(dt>g_audio_debug[2])g_audio_debug[2]=dt;
}
static __attribute__((target("arm"))) void audio_irq(void){audio_service();}
void audio_command(unsigned command){
 if(!ready)return;
 unsigned h=head;if(h-tail==16){g_audio_debug[3]++;return;}
 commands[h&15]=(uint16_t)command;__asm__ volatile("" ::: "memory");head=h+1;
}
void audio_scene(unsigned scene){
 audio_command(0);if(scene==1)audio_command(0x2b);
}
__attribute__((target("arm"))) void audio_init(void){
#ifdef AUDIO_DIAGNOSTIC_DISABLED
 return;
#endif
 R16(0x04000208)=0;R16(0x04000102)=0;R32(0x040000c4)=0;
 R16(0x04000088)=0x200;R16(0x04000084)=0x80;R16(0x04000080)=0;R16(0x04000082)=0x0b04;
 for(unsigned i=0;i<128;i++)audio_irq_stack[i]=0xc0decafe;
 for(unsigned i=0;i<sizeof ring;i++)ring[i]=0;cursor=head=tail=0;ready=1;
 for(unsigned i=0;i<8;i++)R32(0x040000a0)=0; /* Start with a full FIFO. */
 R16(0x04000106)=0;R16(0x04000104)=0;R16(0x04000106)=0x84;
 R32(0x03007ffc)=(unsigned)audio_irq;
 R32(0x040000bc)=(unsigned)ring;R32(0x040000c0)=0x040000a0;
 R16(0x04000202)=0x200;R16(0x04000200)=0x200;
 R32(0x040000c4)=DMA_CONTROL;
 R16(0x04000100)=0xf800; /* 16777216 / 2048 = 8192 Hz */
 R16(0x04000102)=0x80;
 __asm__ volatile("mrs r0, cpsr\n\tbic r0, r0, #0x80\n\tmsr cpsr_c, r0" ::: "r0","memory");
 R16(0x04000208)=1;
}
