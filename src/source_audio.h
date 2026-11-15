#ifndef NATIVE_SOURCE_AUDIO_H
#define NATIVE_SOURCE_AUDIO_H
#include <stdint.h>
/* $108184..$1081AC: byte queue and BIOS/game sound gates. The platform
 * supplies policy inputs and drains bytes; no sound asset fallback. */
struct SourceAudioQueue {
 uint8_t bytes[32],read,write,last_command;
 uint8_t reply,previous_reply,ready,force_request;
 /* $10FD82,$D00046,$10FDAF,$1081AA,$10FD8D respectively. */
 uint8_t bios_demo,demo_sound_off,game_mode,suppress_effects,game_sound_enabled;
};
void source_audio_reset(struct SourceAudioQueue*);
void source_audio_byte(struct SourceAudioQueue*,uint8_t);
void source_audio_command(struct SourceAudioQueue*,uint16_t);
void source_audio_game_command(struct SourceAudioQueue*,uint16_t);
void source_audio_stop_command(struct SourceAudioQueue*,uint16_t);
int source_audio_take(struct SourceAudioQueue*,uint8_t*);
struct SourceAudioTransport {
 void *context;
 int (*read_reply)(void*,uint8_t*);
 int (*send_byte)(void*,uint8_t);
};
/*226A: exactly one command byte per VBlank; forced0F skips reply and queue.
 * This is a byte protocol adapter, not a PCM/Z80 backend or fake reply timer. */
int source_audio_vblank(struct SourceAudioQueue*,const struct SourceAudioTransport*);
void source_audio_force(struct SourceAudioQueue*,int);
/*2242: emit a decoded event only when the raw reply changes with bit7 set;
 * unchanged/low replies returnFF. Called once per frame, not per consumer. */
int8_t source_audio_reply_frame(struct SourceAudioQueue*);
#endif
