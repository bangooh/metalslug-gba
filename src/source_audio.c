#include "source_audio.h"
/* $00212E resets queue/replies/mute and immediately enqueues literal4. */
void source_audio_reset(struct SourceAudioQueue*q){q->read=q->write=q->reply=q->previous_reply=q->suppress_effects=0;q->last_command=255;q->ready=1;source_audio_byte(q,4);}
/* $002152 writes before checking full; on overflow WRITE is unchanged. */
void source_audio_byte(struct SourceAudioQueue*q,uint8_t byte){
 if(byte<32){if(byte==q->last_command)return;q->last_command=byte;}
 q->bytes[q->write]=byte;unsigned next=(q->write+1)&31;
 if(next==q->read)q->last_command=255;else q->write=next;
}
/* $00219C, distinct from the extra $002352 game-mode guard. */
void source_audio_command(struct SourceAudioQueue*q,uint16_t command){
 if(command>=0x2000||(uint8_t)command<32)return;unsigned bank=command&0xff00;
 if(bank==0x100||bank==0x200||bank==0x300)return;
 if((bank>=0x1000||!bank)&&q->bios_demo&&q->demo_sound_off&&q->game_mode==1)return;
 if(q->suppress_effects&&command>=0x1000)return;
 source_audio_byte(q,command>>8);source_audio_byte(q,command);
}
void source_audio_game_command(struct SourceAudioQueue*q,uint16_t command){if(q->game_mode!=1||q->game_sound_enabled)source_audio_command(q,command);}
/*2222 emits0C even if the following219C command is filtered. */
void source_audio_stop_command(struct SourceAudioQueue*q,uint16_t command){source_audio_byte(q,12);source_audio_command(q,command);}
int source_audio_take(struct SourceAudioQueue*q,uint8_t*byte){if(q->read==q->write)return 0;*byte=q->bytes[q->read];q->read=(q->read+1)&31;return 1;}
int8_t source_audio_reply_frame(struct SourceAudioQueue*q){uint8_t raw=q->reply;if(raw==q->previous_reply)return -1;q->previous_reply=raw;return raw&128?(int8_t)((raw-128)>>1):-1;}
void source_audio_force(struct SourceAudioQueue*q,int enabled){q->force_request=!!enabled;}
int source_audio_vblank(struct SourceAudioQueue*q,const struct SourceAudioTransport*t){
 if(!q||!t||!t->read_reply||!t->send_byte)return 0;
 if(q->force_request){if(!t->send_byte(t->context,15))return 0;q->force_request=0;return 1;}
 uint8_t reply;if(!t->read_reply(t->context,&reply))return 0;q->reply=reply;
 if(q->read!=q->write){if(!t->send_byte(t->context,q->bytes[q->read]))return 0;q->read=(q->read+1)&31;}
 return 1;
}
