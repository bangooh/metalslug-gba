#include "native_link.h"
#include "../generated/native-link-build.h"
volatile struct NativeLinkDiagnostic native_link_diagnostic;
enum {CHECKSUM_PERIOD=10};
enum {MAGIC=0x4d53,HELLO_WORDS=58,FRAME_WORDS=20,FAST_WORDS=8};
#define crc native_link_crc
uint16_t native_link_crc(const uint16_t*p,unsigned n){
 static const uint16_t table[16]={0,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef};
 uint16_t c=65535;for(unsigned i=0;i<n;i++)for(unsigned byte=0;byte<2;byte++){
  c^=(uint16_t)((p[i]>>(byte*8))&255)<<8;
  c=(uint16_t)((c<<4)^table[c>>12]);c=(uint16_t)((c<<4)^table[c>>12]);
 }return c;
}
static void put32(uint16_t*p,uint32_t n){p[0]=(uint16_t)n;p[1]=(uint16_t)(n>>16);}
static uint32_t get32(const uint16_t*p){return p[0]|((uint32_t)p[1]<<16);}
static uint32_t hash_word(uint32_t h,uint32_t n){for(unsigned i=0;i<4;i++){h^=(n>>(8*i))&255;h*=16777619u;}return h;}
static void pack_config(const struct NativeLinkConfig*c,uint16_t*p){
 for(unsigned i=0;i<4;i++)put32(p+i*2,c->build[i]);put32(p+8,c->epoch);
 for(unsigned i=0;i<32;i++)p[10+i]=c->rng.words[i];p[42]=c->rng.head;
 const struct NativeSessionConfig*s=&c->session;
 p[43]=s->initial_players|s->initial_credits<<8;p[44]=s->free_play|s->allow_continue<<8;
 p[45]=s->initial_lives|s->time_setting<<8;p[46]=s->timing_variant<<8; /* Low byte reserved; no regional setting. */
 p[47]=s->difficulty|s->control_layout<<8;p[48]=s->blood_enabled|s->arcade_mode<<8;p[49]=s->hud_options|s->first_character<<8;
}
static void unpack_config(struct NativeLinkConfig*c,const uint16_t*p){
 for(unsigned i=0;i<4;i++)c->build[i]=get32(p+2*i);c->epoch=get32(p+8);
 for(unsigned i=0;i<32;i++)c->rng.words[i]=p[10+i];c->rng.head=p[42];
 c->session=(struct NativeSessionConfig){p[43]&255,p[43]>>8,p[44]&255,p[44]>>8,p[45]&255,p[45]>>8,0,p[46]>>8,p[47]&255,p[47]>>8,p[48]&255,p[48]>>8,(uint8_t)p[49],p[49]>>8};
}
static uint32_t config_hash(const struct NativeLinkConfig*c){uint16_t p[50];pack_config(c,p);uint32_t h=2166136261u;for(unsigned i=0;i<50;i++)h=hash_word(h,p[i]);return h;}
static int valid_config(const struct NativeLinkConfig*c){
 return c&&c->rng.head<64&&!(c->rng.head&1)&&c->session.initial_players&&c->session.initial_players<=3&&c->session.initial_credits<=99&&
  c->session.free_play<=1&&c->session.allow_continue<=1&&c->session.difficulty<8&&c->session.initial_lives&&
  c->session.time_setting<=3&&c->session.control_layout<=5&&
  c->session.blood_enabled<=1&&c->session.arcade_mode<=1&&c->session.first_character<=1;
}
int native_link_init(struct NativeLink*l,unsigned slot,const struct NativeLinkConfig*c){
 if(!l||slot>1||!valid_config(c))return 0;*l=(struct NativeLink){.config=*c,.slot=slot,.config_hash=config_hash(c)};return 1;
}
int native_link_config(struct NativeLinkConfig*c,const struct NativeSessionConfig*s,const struct SourceRng*r,uint32_t epoch){
 if(!c||!s||!r)return 0;*c=(struct NativeLinkConfig){.session=*s,.rng=*r,.epoch=epoch};
 const uint32_t build[4]=NATIVE_LINK_BUILD;for(unsigned i=0;i<4;i++)c->build[i]=build[i];return valid_config(c);
}
int native_link_select_character(struct NativeLink*l,unsigned character){
 if(!l||l->slot||l->state>NLINK_READY||l->control!=NLCTRL_IDLE||character>1)return 0;
 if(l->config.session.first_character==character)return 1;
 l->config.session.first_character=(uint8_t)character;
 l->config_hash=config_hash(&l->config);
 /* The guest must acknowledge the new choice before the start barrier. */
 l->agreed=0;l->state=NLINK_HELLO;l->age=0;return 1;
}
int native_link_start(struct NativeLink*l){if(!l||l->state!=NLINK_READY||!l->agreed)return 0;l->state=NLINK_RUNNING;return 1;}
void native_link_leave(struct NativeLink*l){if(l&&l->state<=NLINK_RUNNING)l->leave=1;}
void native_link_vblank(struct NativeLink*l){
 if(!l||l->state>NLINK_RUNNING)return;if(++l->age>=NLINK_TIMEOUT)l->state=NLINK_LOST;
}
unsigned native_link_packet(const struct NativeLink*l,uint16_t p[NLINK_WORDS]){
 if(!l||!p)return 0;unsigned n=l->state==NLINK_HELLO||l->state==NLINK_READY?HELLO_WORDS:FRAME_WORDS;
 if(n==FRAME_WORDS&&l->frame){
  n=(l->frame%CHECKSUM_PERIOD)?6:FAST_WORDS;p[0]=MAGIC;p[1]=n;
  p[2]=NLINK_PROTOCOL|l->slot<<4|(l->leave||l->state>NLINK_RUNNING)<<5|l->prepared<<6|l->received_valid<<7|(l->frame&15)<<8|(l->received_frame&15)<<12;
  p[3]=l->local.keys;p[4]=l->local.credits|l->local.audio_reply<<8;if(n==FAST_WORDS)put32(p+5,l->local.checksum);p[n-1]=crc(p,n-1);return n;
 }
 for(unsigned i=0;i<n;i++)p[i]=0;p[0]=MAGIC;p[1]=(uint16_t)n;p[2]=NLINK_PROTOCOL;p[3]=(uint16_t)(l->slot|(l->leave?2:0));
 if(n==HELLO_WORDS){pack_config(&l->config,p+4);put32(p+54,l->config_hash);p[56]=l->agreed|(l->control<<1);}
 else{
  put32(p+4,l->config.epoch);put32(p+6,l->config_hash);put32(p+8,l->frame);put32(p+10,l->received_frame);
  p[12]=l->prepared|(l->received_valid?2:0);p[13]=l->local.keys;p[14]=l->local.credits|l->local.audio_reply<<8;
  put32(p+15,l->local.checksum);p[17]=l->state;
 }
 p[n-1]=crc(p,n-1);return n;
}
static int mismatch(struct NativeLink*l,unsigned state){l->state=(uint8_t)state;return 0;}
static int reject(struct NativeLink*l,unsigned code,uint32_t expected,uint32_t actual){native_link_diagnostic.code=code;native_link_diagnostic.expected=expected;native_link_diagnostic.actual=actual;return mismatch(l,NLINK_MISMATCH);}
int native_link_receive(struct NativeLink*l,const uint16_t*p,unsigned n){
 if(!l||!p||l->state>NLINK_RUNNING)return 0;
 native_link_diagnostic.local_slot=l->slot;native_link_diagnostic.words=n;
 /* The guest's first stream ACK proves it has installed its receiver. */
 if(n==11&&p[0]==MAGIC&&p[1]==n&&p[2]==NLINK_BULK_ACK){
  if(!l->slot&&l->control==NLCTRL_STREAM&&get32(p+3)==l->config.epoch&&crc(p,n-1)==p[n-1]){l->peer_control=NLCTRL_STREAM;l->age=0;}return 1;
 }
 uint16_t expanded[FRAME_WORDS];int checked=0;
 if(n==FAST_WORDS||n==6){
  if(p[0]!=MAGIC||p[1]!=n||crc(p,n-1)!=p[n-1]){l->bad_packets++;return 1;}
  /* Frame zero uses the full epoch/config packet, so compact packets can
   * only follow a completed, fingerprint-checked handshake and barrier. */
  if(!l->agreed||l->state!=NLINK_RUNNING)return 1;
  if((p[2]&15)!=NLINK_PROTOCOL)return reject(l,1,NLINK_PROTOCOL,p[2]&15);
  for(unsigned i=0;i<FRAME_WORDS;i++)expanded[i]=0;
  expanded[0]=MAGIC;expanded[1]=FRAME_WORDS;expanded[2]=p[2]&15;expanded[3]=(p[2]>>4)&3;
  put32(expanded+4,l->config.epoch);put32(expanded+6,l->config_hash);
  put32(expanded+8,l->frame+(int32_t)((((p[2]>>8)-l->frame+8)&15)-8));
  put32(expanded+10,l->frame+(int32_t)((((p[2]>>12)-l->frame+8)&15)-8));
  expanded[12]=(p[2]>>6)&3;expanded[13]=p[3];expanded[14]=p[4];expanded[15]=n==FAST_WORDS?p[5]:0;expanded[16]=n==FAST_WORDS?p[6]:0;expanded[17]=NLINK_RUNNING;
  if(n!=((get32(expanded+8)%CHECKSUM_PERIOD)?6:FAST_WORDS))return reject(l,6,(get32(expanded+8)%CHECKSUM_PERIOD)?6:FAST_WORDS,n);
  checked=1;p=expanded;n=FRAME_WORDS;
 }
 if((n!=HELLO_WORDS&&n!=FRAME_WORDS)||p[0]!=MAGIC||p[1]!=n||(!checked&&crc(p,n-1)!=p[n-1])){l->bad_packets++;return 1;}
 native_link_diagnostic.peer_slot=p[3]&1;
 if(p[2]!=NLINK_PROTOCOL)return reject(l,1,NLINK_PROTOCOL,p[2]);
 if((p[3]&1)==l->slot)return reject(l,2,l->slot^1,p[3]&1);
 if(n==HELLO_WORDS){
  struct NativeLinkConfig peer;unpack_config(&peer,p+4);
  for(unsigned i=0;i<4;i++)if(peer.build[i]!=l->config.build[i])return reject(l,3,l->config.build[i],peer.build[i]);
  if(!valid_config(&peer)||config_hash(&peer)!=get32(p+54)){l->bad_packets++;return 1;}
  if(p[3]&2)return mismatch(l,NLINK_LEFT);
  if(l->state==NLINK_RUNNING){if(peer.epoch!=l->config.epoch)return reject(l,4,l->config.epoch,peer.epoch);return 1;}
  int changed=!l->agreed;
  if(l->slot){l->config=peer;l->config_hash=get32(p+54);l->agreed=1;l->state=NLINK_READY;}
  else if(get32(p+54)==l->config_hash&&(p[56]&1)){l->agreed=1;l->state=NLINK_READY;}
  l->peer_control=(uint8_t)(p[56]>>1);
  if((changed&&l->agreed)||l->state==NLINK_READY)l->age=0;return 1;
 }
 if(get32(p+4)!=l->config.epoch)return reject(l,4,l->config.epoch,get32(p+4));
 if(get32(p+6)!=l->config_hash)return reject(l,5,l->config_hash,get32(p+6));
 if(l->state==NLINK_HELLO&&!l->slot){l->agreed=1;l->state=NLINK_READY;}
 if(p[3]&2||p[17]>NLINK_RUNNING)return mismatch(l,NLINK_LEFT);
 if(p[17]==NLINK_RUNNING)l->peer_running=1;
 uint32_t number=get32(p+8),ack=get32(p+10);int32_t delta=(int32_t)(number-l->frame);
 if(delta>1||delta< -1){l->bad_packets++;return 1;}
 int progressed=0;
 if(p[12]&2){
  if((int32_t)(ack-l->frame)>0)return mismatch(l,NLINK_DESYNC);
  if(!l->ack_valid||(int32_t)(ack-l->peer_ack)>0){l->peer_ack=ack;l->ack_valid=1;progressed=1;}
 }
 if((p[12]&1)&&delta>=0){
  if(p[13]&~1023u)return mismatch(l,NLINK_FAULT);
  struct NativeLinkPeerFrame*f=&l->peer[number&1];struct NativeLinkInput in={p[13],p[14]&255,p[14]>>8,get32(p+15)};
  if(f->valid&&f->number==number&&(f->input.keys!=in.keys||f->input.credits!=in.credits||f->input.audio_reply!=in.audio_reply||f->input.checksum!=in.checksum))return mismatch(l,NLINK_DESYNC);
  if(!f->valid||f->number!=number)progressed=1;
  *f=(struct NativeLinkPeerFrame){.input=in,.number=number,.valid=1};
  if(!l->received_valid||(int32_t)(number-l->received_frame)>=0){l->received_frame=number;l->received_valid=1;}
 }
 if(progressed)l->age=0;return 1;
}
int native_link_offer(struct NativeLink*l,const struct NativeLinkInput*in){
 if(!l||!in||l->state!=NLINK_RUNNING||in->keys&~1023u)return 0;
 if(l->prepared)return 1;l->local=*in;l->prepared=1;return 1;
}
int native_link_take(struct NativeLink*l,struct NativeLinkInput out[2]){
 if(!l||!out||l->state!=NLINK_RUNNING||l->leave)return -1;
 struct NativeLinkPeerFrame*f=&l->peer[l->frame&1];
 if(!l->prepared||!f->valid||f->number!=l->frame||!l->ack_valid||l->peer_ack!=l->frame)return 0;
 if(l->local.checksum!=f->input.checksum){l->state=NLINK_DESYNC;return -1;}
 out[l->slot]=l->local;out[l->slot^1]=f->input;f->valid=0;l->prepared=0;l->frame++;return 1;
}
uint32_t native_link_world_checksum(const struct NativePlayWorld*w){
 /* Semantic state only: no C padding, stack bytes or native pointers. This
  * is a divergence guard, not a proof that every private actor field agrees. */
 if(!w)return 0;uint32_t h=2166136261u;h=hash_word(h,w->ticks);h=hash_word(h,w->rng.head);
 for(unsigned i=0;i<32;i++)h=hash_word(h,w->rng.words[i]);
 h=hash_word(h,w->entities.active_count);h=hash_word(h,w->enemies.mission_time);
 if(w->session){const struct NativeSession*s=w->session;h=hash_word(h,s->state|s->credits<<8|s->paused<<16|s->transition_hold<<24);
  for(unsigned i=0;i<2;i++)h=hash_word(h,s->player[i].state|s->player[i].lives<<8);}
 if(w->scene_runtime){const struct SceneRuntime*s=w->scene_runtime;h=hash_word(h,s->camera.x);h=hash_word(h,s->camera.y);h=hash_word(h,s->command|s->scene<<16);
  for(unsigned i=0;i<sizeof(s->maps.collision);i++){h^=s->maps.collision[i];h*=16777619u;}}
 for(unsigned i=1;i<ENTITY_CAPACITY;i++){
  const struct NativeEntity*e=&w->entities.nodes[i];if(!e->active)continue;h=hash_word(h,i|e->generation<<16);h=hash_word(h,e->kind|e->deleting<<8);
  const struct PlayerTransform*t=0;const struct Animation*a=0;
  if(e->kind==ENTITY_PLAYER||e->kind==ENTITY_PLAYER_ARM){const struct PlayerActionLayer*p=e->kind==ENTITY_PLAYER?&e->payload.player.actor:&e->payload.arm;t=&p->transform;a=&p->animation;h=hash_word(h,(uint16_t)p->hit.hp);}
  else {const struct WeaponActor*p=entity_weapon_actor((struct NativeEntity*)e);if(p){t=&p->transform;a=&p->animation;h=hash_word(h,(uint16_t)p->hit.hp|p->state<<16);}}
  if(t){h=hash_word(h,(uint32_t)t->x);h=hash_word(h,(uint32_t)t->y);h=hash_word(h,t->facing|t->priority<<8);}
  if(a)h=hash_word(h,a->cursor|a->delay<<16|a->freeze<<24);
 }
 return h;
}

uint32_t native_link_frame_checksum(const struct NativeLink*l,const struct NativePlayWorld*w){return l->frame%CHECKSUM_PERIOD?0:native_link_world_checksum(w);}
