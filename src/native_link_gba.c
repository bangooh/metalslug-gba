#include "native_link.h"
#include "gba_interrupts.h"
#define R16(a) (*(volatile uint16_t*)(uintptr_t)(a))
enum {MAGIC=0x4d53,PACE_TICKS=128,LOBBY_TICKS=512}; /* Longer HELLO verification needs a wider service interval. */
static void (*bulk_receive)(void*,const uint16_t*,unsigned);
static void*bulk_context;
static struct NativeLink* volatile connection;
static uint16_t transmit[2][NLINK_WORDS],receive[1][NLINK_WORDS];
static uint8_t lengths[2];
static volatile uint32_t wire_errors;
static volatile uint8_t active,pending,pending_valid;
static volatile uint8_t leave_sent;
static uint8_t position,in_position,in_length,slot;
static uint8_t role_ready,ready_samples,ready_si;
static uint16_t ready_line;
static void transfer_irq(void*ctx){
 (void)ctx;R16(0x04000202)=32;
 if(!connection||!role_ready||slot||connection->state>NLINK_RUNNING)return;
 R16(0x04000108)=65536-((connection->state==NLINK_RUNNING||bulk_receive)?PACE_TICKS:LOBBY_TICKS);
 uint16_t status=R16(0x04000128);
 if(!(R16(0x04000202)&128)&&!(status&128)&&(status&8))R16(0x04000128)=0x6083;
}
/* Only serialized state invalidates the published packet. Recomputing CRC
 * on every idle poll used to delay starting the next individual SIO word. */
static struct {uint32_t config,frame,received;unsigned flags;} published;
static uint8_t published_valid;

static void serial_irq(void*ctx){(void)ctx;native_link_gba_irq();}
static void publish_locked(void){
 if(bulk_receive)return;
 struct NativeLink*l=connection;
 unsigned flags=l->state|l->prepared<<8|l->received_valid<<9|l->leave<<10|l->agreed<<11|l->control<<16;
 if(published_valid&&published.config==l->config_hash&&published.frame==l->frame&&published.received==l->received_frame&&published.flags==flags)return;
 uint16_t packet[NLINK_WORDS];unsigned count=native_link_packet(l,packet);if(!count)return;
 published.config=l->config_hash;published.frame=l->frame;published.received=l->received_frame;published.flags=flags;published_valid=1;
 unsigned next=active^1;
 for(unsigned i=0;i<count;i++)transmit[next][i]=packet[i];lengths[next]=(uint8_t)count;pending=(uint8_t)next;
 __asm__ volatile("":::"memory");pending_valid=1;
}
static void publish(void){uint16_t ime=R16(0x04000208);R16(0x04000208)=0;__asm__ volatile("":::"memory");publish_locked();__asm__ volatile("":::"memory");R16(0x04000208)=ime;}
void native_link_gba_stop(void){
 uint16_t ime=R16(0x04000208);R16(0x04000208)=0;
 R16(0x04000200)&=(uint16_t)~160;R16(0x04000128)=0;R16(0x0400010a)=0;R16(0x04000202)=128;
 connection=0;bulk_receive=0;bulk_context=0;role_ready=ready_samples=0;native_irq_set(7,0,0);native_irq_set(5,0,0);R16(0x04000208)=ime;
}
int native_link_gba_init(struct NativeLink*l,const struct NativeLinkConfig*c){
 if(!l||!c)return 0;uint16_t ime=R16(0x04000208);R16(0x04000208)=0;native_link_gba_stop();
 native_link_diagnostic=(struct NativeLinkDiagnostic){0};
 R16(0x04000134)=0;R16(0x04000128)=0x6003;native_link_diagnostic.sio_initial=R16(0x04000128);slot=0;role_ready=ready_samples=0;ready_line=65535;native_link_diagnostic.local_slot=255;native_link_diagnostic.peer_slot=255;
 if(!native_link_init(l,slot,c)){R16(0x04000208)=ime;return 0;}connection=l;
 active=pending=pending_valid=position=in_position=in_length=leave_sent=0;wire_errors=0;
 published_valid=0;
 lengths[0]=(uint8_t)native_link_packet(l,transmit[0]);R16(0x0400012a)=transmit[0][0];
 R16(0x04000108)=65536-LOBBY_TICKS;native_irq_set(5,transfer_irq,0);R16(0x04000202)=32;
 /* L2: no clock start until all units are ready and SI is stable. */
 native_irq_set(7,serial_irq,0);R16(0x04000202)=128;R16(0x04000200)|=128;R16(0x04000208)=ime;return 1;
}
#if defined(__arm__)
__attribute__((target("arm")))
#endif
void native_link_gba_irq(void){
 if(!(R16(0x04000202)&128))return;
 struct NativeLink*l=connection;if(!l){R16(0x04000202)=128;return;}
 /* Diablo coop_wire receive_sample: BUSY makes ERROR undefined. Snapshot
  * all completed registers together before interpreting status or data.
  * In an IRQ, discard a stale notification while BUSY; its new completion
  * will raise IF again. Framed retransmission resynchronizes at MAGIC. */
 uint16_t status=R16(0x04000128);
 if(status&128){R16(0x04000202)=128;in_position=0;return;}
 native_link_diagnostic.sio_last=status;
 uint16_t value=R16(slot?0x04000120:0x04000122);
 uint16_t third=R16(0x04000124),fourth=R16(0x04000126),after=R16(0x04000128);
 R16(0x04000202)=128;
 if((after&128)||((status^after)&0x70)){in_position=0;return;}
 if(!role_ready){R16(0x0400012a)=MAGIC;return;}
 unsigned actual_id=(status>>4)&3;
 if(!(status&64)&&actual_id<2&&actual_id!=slot&&!l->agreed&&l->state==NLINK_HELLO){
  slot=(uint8_t)actual_id;l->slot=slot;native_link_diagnostic.local_slot=slot;
  position=in_position=in_length=active=pending=pending_valid=0;published_valid=0;
  lengths[0]=(uint8_t)native_link_packet(l,transmit[0]);
  if(slot){R16(0x0400010a)=0;R16(0x04000200)&=(uint16_t)~32;}
  else{R16(0x04000200)|=32;R16(0x0400010a)=0xc1;}
  R16(0x0400012a)=transmit[0][0];return;
 }
 if((status&64)||actual_id!=slot||third!=65535||fourth!=65535){
  /* Failed wire words do not advance the peer packet parser. Resync at the
   * next magic word; absence of complete packets eventually times out. */
  in_position=0;wire_errors++;
 }else{
  unsigned row=0;
  if(!in_position){if(value==MAGIC){receive[row][0]=value;in_position=1;}}
  else{
   int valid=1;
   if(in_position==1){if(value<4||value>NLINK_WORDS){in_position=0;valid=0;}else in_length=(uint8_t)value;}
   if(valid)receive[row][in_position++]=value;
   if(valid&&in_position==in_length){
    /* Validate and acknowledge complete input while the main thread renders.
     * Main input offer/take hold IME while updating the shared link state. */
    if(bulk_receive)bulk_receive(bulk_context,receive[row],in_length);
    else native_link_receive(l,receive[row],in_length);
    publish_locked();in_position=0;
   }
  }
 }
 if(++position>=lengths[active]){
  if(!bulk_receive&&((lengths[active]==8||lengths[active]==6)?(transmit[active][2]&32):(transmit[active][3]&2)))leave_sent=1;
  position=0;
  if(pending_valid){active=pending;pending_valid=0;}
  else if(bulk_receive&&!slot&&transmit[active][2]==NLINK_BULK_DATA){
   /* Wait for ACK using a short idle frame. Repeating a full64-word block
    * otherwise delays every next block by another entire wire packet. */
   transmit[active][0]=MAGIC;transmit[active][1]=lengths[active]=4;transmit[active][2]=0x5350;
   transmit[active][3]=native_link_crc(transmit[active],3);
  }
 }
 R16(0x0400012a)=transmit[active][position];
}
int native_link_gba_leave_complete(void){return leave_sent;}
void native_link_gba_send(const uint16_t*p,unsigned n){
 if(!p||n<4||n>NLINK_WORDS)return;
 uint16_t ime=R16(0x04000208);R16(0x04000208)=0;
 unsigned next=active^1;for(unsigned i=0;i<n;i++)transmit[next][i]=p[i];lengths[next]=(uint8_t)n;
 pending=(uint8_t)next;__asm__ volatile("":::"memory");pending_valid=1;R16(0x04000208)=ime;
}
void native_link_gba_bulk(void (*receive_hook)(void*,const uint16_t*,unsigned),void*ctx){
 uint16_t ime=R16(0x04000208);R16(0x04000208)=0;
 bulk_context=ctx;bulk_receive=receive_hook;
 if(!receive_hook){published_valid=0;publish_locked();}
 R16(0x04000208)=ime;
}
int native_link_gba_poll(void){
 struct NativeLink*l=connection;if(!l)return 0;
 uint16_t ime=R16(0x04000208);R16(0x04000208)=0;
 if(role_ready&&!l->agreed&&l->state==NLINK_HELLO){
  uint16_t status=R16(0x04000128);
  /* An idle listener may have chosen a provisional slave role before a
   * cable was inserted. Re-evaluate it without waiting15s for timeout. */
  if(!(status&128)&&(!(status&8)||((status>>2)&1)!=slot)){
   role_ready=ready_samples=0;ready_line=65535;native_link_diagnostic.local_slot=255;
   R16(0x0400010a)=0;R16(0x04000200)&=(uint16_t)~32;
  }
 }
 if(!role_ready){
  uint16_t status=R16(0x04000128),line=R16(0x04000006);native_link_diagnostic.sio_last=status;
  if((status&0x88)!=8){ready_samples=0;ready_line=65535;}
  else if(line!=ready_line){
   uint8_t si=(status&4)?1:0;ready_line=line;
   if(!ready_samples||si!=ready_si){ready_si=si;ready_samples=1;}else if(ready_samples<8)ready_samples++;
   if(ready_samples>=8){
    slot=ready_si;l->slot=slot;native_link_diagnostic.local_slot=slot;
    active=pending=pending_valid=position=in_position=in_length=0;published_valid=0;
    lengths[0]=(uint8_t)native_link_packet(l,transmit[0]);R16(0x0400012a)=transmit[0][0];role_ready=1;
    if(!slot){R16(0x04000202)=32;R16(0x04000200)|=32;R16(0x04000108)=65536-LOBBY_TICKS;R16(0x0400010a)=0xc1;}
   }
  }
  if(!role_ready){R16(0x04000208)=ime;return l->state<=NLINK_RUNNING;}
 }
 l->bad_packets+=wire_errors;wire_errors=0;R16(0x04000208)=ime;
 publish();
 if(l->state>NLINK_RUNNING)return 0;

 return 1;
}
