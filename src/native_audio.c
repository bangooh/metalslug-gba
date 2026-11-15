#include "native_audio.h"
#include "../generated/audio_ima_fast.h"
#include "../generated/native-audio-tempo.h"
#include "../generated/native-audio-pcm.h"
#include "../generated/native-audio-runtime.h"
#define AUDIO_FAST __attribute__((section(".iwram.audio"),noinline,long_call))
_Static_assert(sizeof(struct NativeAudio)<2048,"audio state RAM budget");
static int fail(struct NativeAudio*a,unsigned code){if(a&&!a->fault)a->fault=(uint8_t)code;return 0;}
static const struct NativeAudioAsset*find(unsigned command){
 unsigned lo=0,hi=NATIVE_AUDIO_ASSET_COUNT;
 while(lo<hi){unsigned mid=(lo+hi)/2;if(native_audio_assets[mid].command<command)lo=mid+1;else hi=mid;}
 return lo<NATIVE_AUDIO_ASSET_COUNT&&native_audio_assets[lo].command==command?&native_audio_assets[lo]:0;
}
void native_audio_reset(struct NativeAudio*a,uint32_t silence){
 if(!a)return;*a=(struct NativeAudio){.mixed_samples=silence,.volume=65535,.volume_step=65535};
}
int native_audio_send(void*ctx,uint8_t value){
 struct NativeAudio*a=ctx;if(!a||a->fault)return 0;uint32_t n=a->command_write;
 if(n-a->command_read>=NAUDIO_COMMANDS)return fail(a,NAUDIO_QUEUE);
 a->commands[n&(NAUDIO_COMMANDS-1)]=value;__asm__ volatile("":::"memory");a->command_write=n+1;return 1;
}
int native_audio_reply(void*ctx,uint8_t*out){struct NativeAudio*a=ctx;if(!a||a->fault||!out)return 0;*out=a->reply;return 1;}
struct SourceAudioTransport native_audio_transport(struct NativeAudio*a){return (struct SourceAudioTransport){a,native_audio_reply,native_audio_send};}
static void stop_id(struct NativeAudio*a,uint16_t id){
 if(!id)return;for(unsigned i=0;i<NAUDIO_VOICES;i++)if(a->voices[i].asset&&a->voices[i].command==id)a->voices[i].asset=0;
}
static int event(struct NativeAudio*a,unsigned value){
 if(a->reply_write-a->reply_read>=NAUDIO_REPLIES)return fail(a,NAUDIO_REPLY_QUEUE);
 unsigned i=a->reply_write&(NAUDIO_REPLIES-1);a->replies[i]=(struct NativeAudioReply){a->mixed_samples,(uint8_t)value};a->reply_write++;return 1;
}
static struct NativeAudioVoice new_voice(const struct NativeAudioAsset*p,unsigned id,unsigned priority){
 unsigned offset=native_audio_pcm_offsets[p-native_audio_assets];
 return (struct NativeAudioVoice){.asset=p,.samples=p->samples,.data=offset==UINT32_MAX?native_audio_runtime_data+native_audio_runtime_offsets[p-native_audio_assets]:(const uint8_t*)native_audio_pcm+offset,.index=offset==UINT32_MAX?0:255,
  .next_event=p->event_count?native_audio_events[p->first_event].sample:UINT32_MAX,.command=(uint16_t)id,.priority=(uint8_t)priority};
}
static int start(struct NativeAudio*a,unsigned id,unsigned catalog){
 const struct NativeAudioAsset*p=find(catalog);
 if(!p){
  /* Null authored table entries are real no-ops. Out-of-table commands
   * are explicit errors rather than invented silent audio assets. */
  if((catalog>=0x1020&&catalog<=0x10ff)||(catalog>=0x1120&&catalog<=0x1140))return 1;
  return fail(a,NAUDIO_DATA);
 }
 if(!p->samples||p->offset+(p->samples+1)/2>NATIVE_AUDIO_BYTES||p->loop_index>88||
  (p->loop!=0xffffffffu&&p->loop>=p->samples))return fail(a,NAUDIO_DATA);
 unsigned ai=p-native_audio_assets,pcm=native_audio_pcm_offsets[ai],ima=native_audio_runtime_offsets[ai];
 if(pcm==UINT32_MAX){if(ima>NATIVE_AUDIO_RUNTIME_BYTES||(p->samples+1)/2>NATIVE_AUDIO_RUNTIME_BYTES-ima)return fail(a,NAUDIO_DATA);}
 else if(pcm>NATIVE_AUDIO_PCM_BYTES||p->samples>NATIVE_AUDIO_PCM_BYTES-pcm)return fail(a,NAUDIO_DATA);
 if(p->music){
  a->tempo_base=(int8_t)native_audio_initial_tempo[p-native_audio_assets];
  a->tempo_target=a->tempo_base;a->tempo=(uint16_t)(uint8_t)a->tempo_base<<8;a->tempo_step=0;
  a->music_phase=0;a->music_rate=65536;a->music_sample=0;
  a->volume_step=native_audio_fade_steps[a->music_fade];a->music_fade=0;a->fade_out=0;a->volume=0;
  a->music=new_voice(p,id,0);return 1;
 }
 for(unsigned i=0;i<4;i++)stop_id(a,p->stop[i]);
 unsigned priority=255;for(unsigned i=0;i<p->track_count&&i<8;i++)if(p->priority[i]<priority)priority=p->priority[i];
 /* Decide group admission before retiring any member of that group. */
 for(unsigned i=0;i<NAUDIO_VOICES;i++){
  const struct NativeAudioVoice*v=&a->voices[i];
  if(v->asset&&p->group&&v->asset->group==p->group&&v->priority<priority){a->rejected++;return 1;}
 }
 unsigned free=NAUDIO_VOICES,victim=NAUDIO_VOICES;
 for(unsigned i=0;i<NAUDIO_VOICES;i++){
  struct NativeAudioVoice*v=&a->voices[i];
  if(!v->asset){free=i;continue;}
  if(p->group&&v->asset->group==p->group){
   v->asset=0;a->preempted++;free=i;continue;
  }
  if(v->priority>=priority&&(victim==NAUDIO_VOICES||v->priority>=a->voices[victim].priority))victim=i;
 }
 if(free==NAUDIO_VOICES){if(victim==NAUDIO_VOICES){a->rejected++;return 1;}free=victim;a->preempted++;}
 /* Whole premixed sounds share NAUDIO_VOICES GBA voices. Header stop/group/priority
  * rules are retained; partial source-track stealing cannot be reproduced
  * by premixed samples and remains an explicit documented adaptation. */
 a->voices[free]=new_voice(p,id,priority);return 1;
}
static void stop_effects(struct NativeAudio*a){for(unsigned i=0;i<NAUDIO_VOICES;i++)a->voices[i].asset=0;}
static int command(struct NativeAudio*a,uint8_t c){
 if(c<32){
  if(c!=15)a->type=c;
  switch(c){
   case 2:return start(a,2,2); /* Original special music sequence. */
   case 4:a->music.asset=0;stop_effects(a);return 1;
   case 5:a->music.asset=0;return 1;
   case 6:stop_effects(a);return 1;
   case 12:a->stop_pending=1;return 1;
   case 15:return start(a,15,15); /* Original special SFX; preserves type. */
   default:return 1;
  }
 }
 if(a->type>=16){
  unsigned id=((unsigned)a->type<<8)|c;
  if(a->stop_pending){a->stop_pending=0;stop_id(a,id);return 1;}
  return start(a,id,((a->type==17?17u:16u)<<8)|c);
 }
 switch(a->type){
  case 0:return start(a,c,c);
  case 7:a->param=(uint8_t)(c-32);return 1;
  case 8:{
   /*4F69 log-domain scale; saturate the nine-bit sum before inverse. */
   unsigned sum=native_audio_curve[0][c>>1]+native_audio_curve[0][(uint8_t)(a->tempo_base+128)];
   unsigned index=sum<192?0:sum>447?255:sum-192;
   a->tempo_target=(int8_t)(native_audio_curve[1][index]-128);a->tempo_step=a->param;return 1;
  }
  case 9:a->music_fade=(uint8_t)(c-32);return 1;
  case 10:case 11:a->fade_out=a->type==10;a->volume_step=native_audio_fade_steps[c-32];return 1;
  default:return 1;
 }
}
/* Events and loop boundaries are rare. Keep metadata reads outside the
 * per-sample path, while retaining the original event order/timestamps. */
static __attribute__((noinline)) int voice_boundary(struct NativeAudio*a,struct NativeAudioVoice*v){
 const struct NativeAudioAsset*p=v->asset;
 if(v->position==v->samples){
  if(p->loop==0xffffffffu){v->asset=0;return 0;}
  v->position=p->loop;v->predictor=p->loop_predictor;if(v->index!=255)v->index=p->loop_index;v->event=0;
  while(v->event<p->event_count&&native_audio_events[p->first_event+v->event].sample<p->loop)v->event++;
 }
 while(v->event<p->event_count){
  const struct NativeAudioEvent*e=&native_audio_events[p->first_event+v->event];if(e->sample>v->position)break;
  if(!event(a,e->value))return 0;v->event++;
 }
 v->next_event=v->event<p->event_count?native_audio_events[p->first_event+v->event].sample:UINT32_MAX;return 1;
}
static __attribute__((always_inline)) inline int voice(struct NativeAudio*a,struct NativeAudioVoice*v){
 if(!v->asset)return 0;
 if((v->position==v->samples||v->position>=v->next_event)&&!voice_boundary(a,v))return 0;
 unsigned pos=v->position++;
 if(v->index==255)return ((const int8_t*)v->data)[pos]*256;
 unsigned n=v->data[pos>>1];n=(pos&1)?n&15:n>>4;
 unsigned packed=audio_ima_fast[(unsigned)v->index*16+n];int sample=v->predictor+((int32_t)(packed<<14)>>14);
 if(sample>32767)sample=32767;if(sample< -32768)sample=-32768;v->predictor=(int16_t)sample;v->index=packed>>18;return sample;
}
/* v169: out-of-line ordered path for boundaries/events/IMA SFX tails.
 * Lives in ROM (Thumb): rare per sample, and IWRAM has no spare room. */
static __attribute__((noinline)) int voice_slow(struct NativeAudio*a,struct NativeAudioVoice*v){return voice(a,v);}
static void tempo_tick(struct NativeAudio*a){
 int current=(int8_t)(a->tempo>>8),target=a->tempo_target;
 if(current==target)return;
 if(!a->tempo_step)a->tempo=((uint16_t)(uint8_t)target<<8)|(a->tempo&255);
 else{
  /*4D94: arithmetic halves, one's-complement magnitude, signed Q8.8
   * accumulator. Keep the fractional byte when target changes. */
  int difference=(target>>1)-(current>>1);
  if(!difference)a->tempo_step=0;
  else{
   unsigned sign=difference<0?255:0;
   unsigned magnitude=(uint8_t)difference^sign;
   unsigned sum=native_audio_curve[0][magnitude]+native_audio_curve[0][a->tempo_step];
   unsigned index=sum>=256?sum&255:0;
   int delta=sign?-(int)native_audio_curve[1][index]-1:native_audio_curve[1][index];
   a->tempo=(uint16_t)(a->tempo+delta*16);
  }
 }
 current=(int8_t)(a->tempo>>8);
 a->music_rate=((unsigned)(current+129)<<16)/(unsigned)(a->tempo_base+129);
}
/* v169: register-resident block mixer. Output, event timestamps and all
 * state are bit-identical to v168 (tests/audio_mixer_169_equivalence.c).
 * Timer/tempo/volume, the music decoder and counters stay in locals; they
 * are written back before any call that can observe them (voice boundary
 * events read mixed_samples, tempo_tick reads/writes the struct). */
static __attribute__((noinline)) int mix_ordered(struct NativeAudio*a,int8_t*out,unsigned count,const int8_t*const*pcm,unsigned pcm_count,struct NativeAudioVoice*const*active,unsigned active_count){
 enum {TIMER_LIMIT=144u*333u*NAUDIO_RATE};
 struct NativeAudioVoice*m=&a->music;
 uint32_t timer=a->timer_phase,phase=a->music_phase,rate=a->music_rate,mixed=a->mixed_samples,clipped=0;
 int gain=a->volume>>8,sample=a->music_sample;
 /* Music decoder state: valid while `limit` > position (no boundary due). */
 uint32_t pos=m->position,limit=0;const uint8_t*data=m->data;int pred=m->predictor;unsigned idx=m->index;
#define MUSIC_LIMIT() (limit=m->asset?(m->samples<m->next_event?m->samples:m->next_event):0)
#define MUSIC_SAVE() do{m->position=pos;m->predictor=(int16_t)pred;m->index=(uint8_t)idx;}while(0)
#define MUSIC_LOAD() do{pos=m->position;data=m->data;pred=m->predictor;idx=m->index;MUSIC_LIMIT();}while(0)
 MUSIC_LIMIT();
 for(unsigned i=0;i<count;i++){
  timer+=8000000u;
  if(timer>=TIMER_LIMIT){
   timer-=TIMER_LIMIT;
   tempo_tick(a);rate=a->music_rate;
   if(a->fade_out)a->volume=a->volume>a->volume_step?a->volume-a->volume_step:0;
   else {unsigned v=a->volume+a->volume_step;a->volume=v>65535?65535:v;}
   gain=a->volume>>8;
  }
  phase+=rate;
  while(phase>=65536){
   phase-=65536;
   if(pos<limit&&m->asset){
    unsigned at=pos++;
    if(idx==255)sample=((const int8_t*)data)[at]*256;
    else{
     unsigned n=data[at>>1];n=(at&1)?n&15:n>>4;
     unsigned packed=audio_ima_fast[idx*16+n];int x=pred+((int32_t)(packed<<14)>>14);
     if(x>32767)x=32767;if(x< -32768)x=-32768;pred=(int16_t)x;idx=packed>>18;sample=pred;
    }
   }else{
    /* Boundary, event, loop or no asset: ordered reference path. */
    MUSIC_SAVE();a->mixed_samples=mixed;
    sample=voice_slow(a,m);
    MUSIC_LOAD();
    if(a->fault)return 0;
   }
  }
  int mix=m->asset?sample:0;
  mix=(mix*gain)>>8;
  _Static_assert(NAUDIO_VOICES==4,"update PCM summation when voice budget changes");
  switch(pcm_count){
   case 4:mix+=pcm[3][i]*256; /* fall through */
   case 3:mix+=pcm[2][i]*256; /* fall through */
   case 2:mix+=pcm[1][i]*256; /* fall through */
   case 1:mix+=pcm[0][i]*256; /* fall through */
   default:break;
  }
  if(active_count){
   a->mixed_samples=mixed;
   for(unsigned j=0;j<active_count;j++)if(active[j]->asset)mix+=voice_slow(a,active[j]);
   if(a->fault){MUSIC_SAVE();return 0;}
  }
  mix>>=9;
  if(mix>127){mix=127;clipped++;}if(mix< -128){mix=-128;clipped++;}
  out[i]=(int8_t)mix;mixed++;
 }
 MUSIC_SAVE();
#undef MUSIC_LIMIT
#undef MUSIC_SAVE
#undef MUSIC_LOAD
 a->timer_phase=timer;a->music_phase=phase;a->music_rate=rate;a->mixed_samples=mixed;a->clipped+=clipped;a->music_sample=(int16_t)sample;
 return 1;
}
/* v169: two mixers, bit-identical to v168 (tests/audio_mixer_169_equivalence.c).
 * - Common case (no SFX voice needs the ordered per-sample path): separate
 *   passes for music, PCM SFX and clip/store with locals in registers.
 *   Music is the only event source then, so event order is unchanged.
 * - Otherwise mix_ordered() (ROM) keeps the per-sample interleaving. */
AUDIO_FAST int native_audio_mix(struct NativeAudio*a,int8_t*out,unsigned count){
 if(!a||a->fault||!out||count>256)return 0;
 while(a->command_read!=a->command_write){unsigned i=a->command_read&(NAUDIO_COMMANDS-1);if(!command(a,a->commands[i]))return 0;a->command_read++;}
 struct NativeAudioVoice*active[NAUDIO_VOICES];unsigned active_count=0;
 const int8_t*pcm[NAUDIO_VOICES];unsigned pcm_count=0;
 struct NativeAudioVoice*ima[NAUDIO_VOICES];unsigned ima_count=0;
 for(unsigned j=0;j<NAUDIO_VOICES;j++)if(a->voices[j].asset){
  struct NativeAudioVoice*v=&a->voices[j];
  /* No boundary inside this block: positions..+count stay below both the
   * end and the next event, so the voice cannot emit events or stop. */
  int clear=v->position<=v->samples&&count<=v->samples-v->position&&v->position<v->next_event&&count<=v->next_event-v->position;
  if(clear&&v->index==255){pcm[pcm_count++]=(const int8_t*)v->data+v->position;v->position+=count;}
  else if(clear)ima[ima_count++]=v;
  else active[active_count++]=v;
 }
 if(active_count){
  /* Rare (an SFX ends, loops or reaches an event in this block): keep the
   * exact per-sample interleaving of all event sources. */
  for(unsigned k=0;k<ima_count;k++)active[active_count++]=ima[k];
  return mix_ordered(a,out,count,pcm,pcm_count,active,active_count);
 }
 enum {TIMER_LIMIT=144u*333u*NAUDIO_RATE};
 int32_t acc[256];
 struct NativeAudioVoice*m=&a->music;
 uint32_t timer=a->timer_phase,phase=a->music_phase,rate=a->music_rate,mixed=a->mixed_samples;
 int gain=a->volume>>8,sample=a->music_sample;
 uint32_t pos=m->position,limit=m->asset?(m->samples<m->next_event?m->samples:m->next_event):0;
 const uint8_t*data=m->data;int pred=m->predictor;unsigned idx=m->index;
 /* Pass 1: timer/tempo/volume and music. */
 for(unsigned i=0;i<count;i++){
  timer+=8000000u;
  if(timer>=TIMER_LIMIT){
   timer-=TIMER_LIMIT;
   tempo_tick(a);rate=a->music_rate;
   if(a->fade_out)a->volume=a->volume>a->volume_step?a->volume-a->volume_step:0;
   else {unsigned v=a->volume+a->volume_step;a->volume=v>65535?65535:v;}
   gain=a->volume>>8;
  }
  phase+=rate;
  while(phase>=65536){
   phase-=65536;
   if(pos<limit){
    unsigned at=pos++;
    if(idx==255)sample=((const int8_t*)data)[at]*256;
    else{
     unsigned n=data[at>>1];n=(at&1)?n&15:n>>4;
     unsigned packed=audio_ima_fast[idx*16+n];int x=pred+((int32_t)(packed<<14)>>14);
     if(x>32767)x=32767;if(x< -32768)x=-32768;pred=x;idx=packed>>18;sample=pred;
    }
   }else{
    m->position=pos;m->predictor=(int16_t)pred;m->index=(uint8_t)idx;a->mixed_samples=mixed+i;
    sample=voice_slow(a,m);if(a->fault)return 0;
    pos=m->position;data=m->data;pred=m->predictor;idx=m->index;
    limit=m->asset?(m->samples<m->next_event?m->samples:m->next_event):0;
   }
  }
  acc[i]=m->asset?(sample*gain)>>8:0;
 }
 m->position=pos;m->predictor=(int16_t)pred;m->index=(uint8_t)idx;
 a->timer_phase=timer;a->music_phase=phase;a->music_rate=rate;a->music_sample=(int16_t)sample;
 /* Pass 2: boundary-free IMA SFX, one voice at a time. */
 for(unsigned k=0;k<ima_count;k++){
  struct NativeAudioVoice*v=ima[k];uint32_t p=v->position;const uint8_t*d=v->data;int q=v->predictor;unsigned ix=v->index;
  for(unsigned i=0;i<count;i++,p++){
   unsigned nib=d[p>>1];nib=(p&1)?nib&15:nib>>4;
   unsigned packed=audio_ima_fast[ix*16+nib];int x=q+((int32_t)(packed<<14)>>14);
   if(x>32767)x=32767;if(x< -32768)x=-32768;q=x;ix=packed>>18;acc[i]+=q;
  }
  v->position=p;v->predictor=(int16_t)q;v->index=(uint8_t)ix;
 }
 /* Pass 3: PCM SFX, one voice at a time. */
 for(unsigned k=0;k<pcm_count;k++){const int8_t*src=pcm[k];for(unsigned i=0;i<count;i++)acc[i]+=src[i]*256;}
 /* Pass 4: headroom, clip, store. */
 uint32_t clipped=0;
 for(unsigned i=0;i<count;i++){
  int x=acc[i]>>9;
  if(x>127){x=127;clipped++;}if(x< -128){x=-128;clipped++;}
  out[i]=(int8_t)x;
 }
 a->clipped+=clipped;a->mixed_samples=mixed+count;
 return 1;
}
int native_audio_audible(struct NativeAudio*a,uint32_t played){
 if(!a||a->fault)return 0;a->played_samples=played;
 while(a->reply_read!=a->reply_write){
  const struct NativeAudioReply*r=&a->replies[a->reply_read&(NAUDIO_REPLIES-1)];
  if((int32_t)(played-r->sample)<0)break;
  if(r->value==255)a->reply=0;else{a->toggle++;a->reply=(uint8_t)(128|(r->value<<1)|(a->toggle&1));}
  a->reply_read++;
 }
 return 1;
}
