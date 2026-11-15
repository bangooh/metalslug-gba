#ifndef NATIVE_AUDIO_H
#define NATIVE_AUDIO_H
#include "source_audio.h"
#include "../generated/native-audio-data.h"
enum {NAUDIO_VOICES=4,NAUDIO_COMMANDS=32,NAUDIO_REPLIES=64,
 NAUDIO_RATE=NATIVE_AUDIO_SAMPLE_RATE,
 NAUDIO_TIMER_PERIOD=(16777216u+NAUDIO_RATE/2)/NAUDIO_RATE,
 NAUDIO_TIMER_RELOAD=65536u-NAUDIO_TIMER_PERIOD};
_Static_assert(NAUDIO_TIMER_PERIOD>0&&NAUDIO_TIMER_PERIOD<=65536,"audio timer range");
enum NativeAudioFault {NAUDIO_OK,NAUDIO_QUEUE,NAUDIO_DATA,NAUDIO_TEMPO,NAUDIO_REPLY_QUEUE,NAUDIO_UNDERRUN};
struct NativeAudioVoice {
 const struct NativeAudioAsset*asset;
 uint32_t position,samples,next_event;
 const uint8_t*data;
 int16_t predictor;
 uint16_t command,event;
 uint8_t index,priority;
};
struct NativeAudioReply {uint32_t sample;uint8_t value;};
struct NativeAudio {
 struct NativeAudioVoice music,voices[NAUDIO_VOICES];
 struct NativeAudioReply replies[NAUDIO_REPLIES];
 uint8_t commands[NAUDIO_COMMANDS];
 volatile uint32_t command_read,command_write;
 uint32_t reply_read,reply_write,mixed_samples,played_samples,timer_phase,clipped,preempted,rejected;
 uint16_t volume,volume_step;
 uint32_t music_phase,music_rate;
 uint16_t tempo;
 int8_t tempo_target,tempo_base;
 uint8_t tempo_step;
 int16_t music_sample;
 uint8_t type,stop_pending,music_fade,fade_out,param,toggle;
 volatile uint8_t reply;
 volatile uint8_t fault;
};
/* Caller serializes mix/audible. send_byte is single-producer, IRQ-safe when
 * the mixer is the only consumer. Publish counters AFTER writing entries. */
void native_audio_reset(struct NativeAudio*,uint32_t initial_silence_samples);
int native_audio_send(void*,uint8_t);
int native_audio_reply(void*,uint8_t*);
int native_audio_mix(struct NativeAudio*,int8_t*,unsigned samples);
int native_audio_audible(struct NativeAudio*,uint32_t played_samples);
struct SourceAudioTransport native_audio_transport(struct NativeAudio*);
/* Hardware owns DMA1/Timer0+1+3. Caller installs native_audio_gba_irq in its
 * shared dispatcher. It never replaces another subsystem's IRQ handler. */
int native_audio_gba_init(struct NativeAudio*);
int native_audio_gba_resume(struct NativeAudio*);
void native_audio_gba_irq(void);
void native_audio_gba_stop(void);
#endif
