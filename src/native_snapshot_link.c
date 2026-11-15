#include "native_snapshot.h"
#include "native_link.h"
enum {MAGIC=0x4d53};
struct NativeSnapshotStats native_snapshot_stats;
struct Transfer {
 struct NativeLink*link;
 volatile uint8_t pending,finished;
 uint16_t received[NLINK_WORDS];
 unsigned count;
 void (*progress)(unsigned);
};
static uint32_t get32(const uint16_t*p){return p[0]|((uint32_t)p[1]<<16);}
static void put32(uint16_t*p,uint32_t v){p[0]=(uint16_t)v;p[1]=(uint16_t)(v>>16);}
static void receive(void*ctx,const uint16_t*p,unsigned n){
 struct Transfer*t=ctx;
 if(n<7||p[0]!=MAGIC||p[1]!=n||native_link_crc(p,n-1)!=p[n-1])return;
 if(t->link->slot&&n==58&&p[2]==NLINK_PROTOCOL&&p[3]==0&&get32(p+12)==t->link->config.epoch&&(p[56]>>1)==NLCTRL_COMMIT){t->finished=1;return;}
 if(p[2]!=(t->link->slot?NLINK_BULK_DATA:NLINK_BULK_ACK)||get32(p+3)!=t->link->config.epoch||t->pending)return;
 for(unsigned i=0;i<n;i++)t->received[i]=p[i];t->count=n;__asm__ volatile("":::"memory");t->pending=1;
}
static void send(uint16_t*p,unsigned n){p[0]=MAGIC;p[1]=(uint16_t)n;p[n-1]=native_link_crc(p,n-1);native_link_gba_send(p,n);}
static void ack(uint16_t*p,const struct NativeLink*l,uint16_t sequence,const struct NativeSnapshot*s){
 p[2]=NLINK_BULK_ACK;put32(p+3,l->config.epoch);p[5]=sequence;put32(p+6,s->position);put32(p+8,s->hash);send(p,11);
}
static int guest(struct Transfer*t,struct NativeSnapshot*s,volatile uint32_t*clock){
 uint16_t packet[NLINK_WORDS];uint16_t next=0;uint32_t expected_hash=0,progress=*clock;
 ack(packet,t->link,65535,s);
 for(;;){
  if(t->pending){
   const uint16_t*p=t->received;unsigned n=t->count;int ok=1;
   if(n>=12&&p[5]==next&&get32(p+6)==s->words){
    expected_hash=get32(p+8);ok=native_snapshot_decode(s,p+10,n-11);
    if(ok){next++;progress=*clock;if(t->progress)t->progress(s->position*100u/s->words);}
   }
   __asm__ volatile("":::"memory");t->pending=0;
   if(!ok)return 0;
   ack(packet,t->link,(uint16_t)(next-1),s);
  }
  if(s->position==s->words&&s->hash!=expected_hash)return 0;
  if(t->finished)return s->position==s->words&&s->hash==expected_hash;
  if((uint32_t)(*clock-progress)>=NLINK_TIMEOUT)return 0;
 }
}
static int host(struct Transfer*t,struct NativeSnapshot*s,volatile uint32_t*clock){
 struct NativeSnapshotEncoder encoder;native_snapshot_encoder(&encoder,s);
 uint32_t hash=native_snapshot_hash(s);uint16_t packet[NLINK_WORDS],sequence=0;
 do{
  unsigned n=native_snapshot_encode(&encoder,packet+10,NLINK_WORDS-11);if(!n)return 0;
  packet[2]=NLINK_BULK_DATA;put32(packet+3,t->link->config.epoch);packet[5]=sequence;put32(packet+6,s->words);put32(packet+8,hash);send(packet,n+11);
  native_snapshot_stats.encoded_bytes+=n*2;native_snapshot_stats.packets++;
  uint32_t progress=*clock,retry=progress;
  for(;;){
   if(t->pending){
    const uint16_t*p=t->received;int accepted=t->count==11&&p[5]==sequence&&get32(p+6)==encoder.state.position;
    uint32_t received_hash=get32(p+8);__asm__ volatile("":::"memory");t->pending=0;
    if(accepted){if(encoder.state.position==s->words&&received_hash!=hash)return 0;if(t->progress)t->progress(encoder.state.position*100u/s->words);break;}
   }
   if((uint32_t)(*clock-progress)>=NLINK_TIMEOUT)return 0;
   if((uint32_t)(*clock-retry)>=8){send(packet,n+11);retry=*clock;native_snapshot_stats.retries++;}
  }
  sequence++;
 }while(encoder.state.position<s->words);
 return 1;
}
int native_snapshot_transfer(struct NativeLink*l,const struct NativeSnapshotRange*r,unsigned n,volatile uint32_t*clock,void (*progress)(unsigned)){
 struct NativeSnapshot state;if(!l||!clock||l->state!=NLINK_READY||!native_snapshot_init(&state,r,n))return 0;
 native_snapshot_stats=(struct NativeSnapshotStats){.raw_bytes=state.words*2};uint32_t started=*clock;
 struct Transfer t={.link=l,.progress=progress};native_link_gba_bulk(receive,&t);
 int result=l->slot?guest(&t,&state,clock):host(&t,&state,clock);
 if(result&&!l->slot)l->control=NLCTRL_COMMIT;
 native_link_gba_bulk(0,0);l->age=0;native_snapshot_stats.refreshes=*clock-started;return result;
}
