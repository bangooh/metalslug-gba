#include "source_input.h"
#include "../generated/runtime-data.h"
_Static_assert(sizeof(struct SourceInputRecord)==6,"source input record");
void source_input_reset(struct SourceInput*s){*s=(struct SourceInput){.repeat_delay=16,.repeat_interval=2};}
void source_input_mode(struct SourceInput*s,unsigned index){
 s->raw[0]=s->raw[1]=(struct SourceInputRecord){0};
 s->block=s->cursor=0;s->elapsed=0;s->replay_held[0]=s->replay_held[1]=0;
 s->mode=index<2?SOURCE_INPUT_REPLAY:SOURCE_INPUT_PHYSICAL;s->replay=index<2?runtime_demo[index]:0;
}
/*253BE: return one ready toggle, or -1 for a wait/end. A full delay byte
 * advances the cursor even when the following nonzero byte delays completion. */
static int next(struct SourceInput*s){
 unsigned at=s->cursor;if(at>=512){s->mode=SOURCE_INPUT_DONE;return -1;}
 unsigned value=s->replay[at],limit=255;if(at==s->block){limit=15;value&=15;}
 if(value!=s->elapsed)return -1;
 s->elapsed=0;at++;
 if(value==limit){if(at>=512){s->mode=SOURCE_INPUT_DONE;return -1;}if(s->replay[at]){s->cursor=at;return -1;}at++;}
 if(s->block>=512){s->fault=1;return -1;}
 unsigned bit=s->replay[s->block]>>4;s->cursor=s->block=at;return bit;
}
static void record(struct SourceInput*s,struct SourceInputRecord*r,uint8_t held){
 r->previous=r->held;r->held=held;r->edges=(r->previous^held)&held;
 if(r->edges){r->repeat=r->edges;r->timer=s->repeat_delay;}
 else if(!--r->timer){r->repeat=held;r->timer=s->repeat_interval;}
 else r->repeat=0;
}
int source_input_frame(struct SourceInput*s,const uint8_t held[2],uint8_t system,const uint8_t live[2]){
 if(!s||!held||!live||s->fault||s->mode>SOURCE_INPUT_DONE)return 0;
 uint8_t selected[2]={held[0],held[1]};
 if(s->mode==SOURCE_INPUT_REPLAY){
  if(!s->replay){s->fault=1;return 0;}
  int bit;while((bit=next(s))>=0)s->replay_held[(unsigned)bit>>3]^=1u<<(bit&7);
  s->elapsed++;selected[0]=s->replay_held[0];selected[1]=s->replay_held[1];
 }else if(s->mode==SOURCE_INPUT_DONE)selected[0]=selected[1]=0;
 for(unsigned i=0;i<2;i++){
  record(s,&s->raw[i],selected[i]);
  if(live[i]==3)s->active[i]=(struct SourceInputRecord){0};
  else if(live[i])s->active[i]=s->raw[i]; /* status0 leaves snapshot unchanged */
 }
 s->system_edges=(s->system^system)&system;s->system=system;return !s->fault;
}
