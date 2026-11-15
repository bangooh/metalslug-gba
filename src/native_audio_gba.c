#include "native_audio.h"
#include "gba_interrupts.h"
#include <stddef.h>
#define R16(a) (*(volatile uint16_t*)(a))
#define R32(a) (*(volatile uint32_t*)(a))
enum {RING=256,BLOCK=32,GUARD=128,AUDIO_IRQ=1u<<6};
static int8_t ring[RING+GUARD] __attribute__((aligned(4)));
static struct NativeAudio* volatile engine;
static unsigned cursor,pending_wrap;
static uint16_t previous_timer;
static uint32_t played;
static void audio_irq(void*ctx){(void)ctx;native_audio_gba_irq();}
void native_audio_gba_stop(void){
 uint16_t ime=R16(0x04000208);R16(0x04000208)=0;
 R16(0x04000102)=0;R16(0x04000106)=0;R16(0x0400010e)=0;R32(0x040000c4)=0;
 R16(0x04000200)&=(uint16_t)~(AUDIO_IRQ|512);R16(0x04000202)=AUDIO_IRQ|512;engine=0;
 native_irq_set(6,0,0);native_irq_set(9,0,0);R16(0x04000208)=ime;
}
static int start_audio(struct NativeAudio*a,int preserve){
 if(!a)return 0;uint16_t ime=R16(0x04000208);R16(0x04000208)=0;native_audio_gba_stop();
 /* FIFO initially contains16 zeros, followed by256 initial ring zeros.
  * Reply timestamps describe audible samples, not IRQ decode times. */
 if(preserve){a->reply_read=a->reply_write=0;a->mixed_samples=RING+16;a->played_samples=a->timer_phase=0;a->fault=0;}
 else native_audio_reset(a,RING+16);
 engine=a;cursor=pending_wrap=played=0;previous_timer=0;
 native_irq_set(6,audio_irq,0);
 for(unsigned i=0;i<sizeof(ring);i++)ring[i]=0;
 R16(0x04000088)=0x200;R16(0x04000084)=0x80;R16(0x04000080)=0;R16(0x04000082)=0x0b04;
 for(unsigned i=0;i<4;i++)R32(0x040000a0)=0;
 R16(0x04000104)=0;R16(0x04000106)=0x84;
 R32(0x040000bc)=(uintptr_t)ring;R32(0x040000c0)=0x040000a0;
 R16(0x04000202)=AUDIO_IRQ|512;R16(0x04000200)|=AUDIO_IRQ;R32(0x040000c4)=0xb6400000u;
 /* Timer1 still counts actual output samples. Timer3/64 wakes the mixer
  * every32 samples; Timer2 remains exclusively owned by the link cable. */
 R16(0x0400010c)=NAUDIO_TIMER_RELOAD;
 R16(0x04000100)=NAUDIO_TIMER_RELOAD;R16(0x04000102)=0x80;
 R16(0x0400010c)=65536u-(NAUDIO_TIMER_PERIOD*40u+32u)/64u;
 R16(0x0400010e)=0xc1;R16(0x0400010c)=65536u-NAUDIO_TIMER_PERIOD/2;
 R16(0x04000208)=ime;return 1;
}
int native_audio_gba_init(struct NativeAudio*a){return start_audio(a,0);}
int native_audio_gba_resume(struct NativeAudio*a){return start_audio(a,1);}
/* Called by the shared BIOS IRQ dispatcher. Serial/VBlank IF bits are not
 * acknowledged here. DMA3 remains available to video; Timer2 untouched. */
__attribute__((target("thumb"),optimize("O2"),section(".iwram.audio_irq"),noinline)) void native_audio_gba_irq(void){
 if(!(R16(0x04000202)&AUDIO_IRQ))return;R16(0x04000202)=AUDIO_IRQ;
 struct NativeAudio*a=engine;if(!a)return;
 unsigned now=R16(0x04000104);played+=(uint16_t)(now-previous_timer);previous_timer=(uint16_t)now;
 if(!native_audio_audible(a,played)){native_audio_gba_stop();return;}
 unsigned fetched=((now>>4)+1)&4095,current=fetched>>1,blocks=(current-cursor)&2047;
 if(blocks>4){a->fault=NAUDIO_UNDERRUN;native_audio_gba_stop();return;}
 if((cursor&7)+blocks>=8)pending_wrap=1;
 if(pending_wrap){
  /* FIFO requests and Timer1 cascade occur at the same sample boundary.
   * Rebase in samples7..11 of a burst. The FIFO internal4-sample word
   * can retain its phase across resets; this window covers that latency.
   * Keep Timer0 running so the playback/mixer clocks never drift. */
  unsigned phase=now&15;
  if((phase<7||phase>11)&&(fetched&15)>=6){do{now=R16(0x04000104);phase=now&15;}while(phase<7||phase>11);}
  if(phase>=7&&phase<=11){
   now=R16(0x04000104);fetched=((now>>4)+1)&4095;current=fetched>>1;blocks=(current-cursor)&2047;
   if(blocks>4){a->fault=NAUDIO_UNDERRUN;native_audio_gba_stop();return;}
   R32(0x040000c4)=0;R32(0x040000bc)=(uintptr_t)(ring+(fetched&15)*16);R32(0x040000c4)=0xb6400000u;pending_wrap=0;
  }
 }
 while(blocks--){unsigned at=(cursor&7)*BLOCK;
  if(!native_audio_mix(a,ring+at,BLOCK)){native_audio_gba_stop();return;}
  if(at<GUARD)for(unsigned i=0;i<BLOCK;i++)ring[RING+at+i]=ring[at+i];cursor=(cursor+1)&2047;
 }
}
