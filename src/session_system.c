#include "session_internal.h"
#include "../generated/runtime-data.h"
static uint16_t hud_position(const struct NativePlayWorld*w,uint16_t at){
 /* Only the lower credits/LEVEL row moves. Pause remains on source row28;
  * world geometry, ordinary FIX art and every other text row stay source-based. */
 unsigned row=w->video.hud_bottom_row;
 return row&&row<32&&(at&31)==29?(uint16_t)((at&~31u)|row):at;
}
static int bytes(struct NativePlayWorld*w,uint16_t at,const uint8_t*p,unsigned n){
 at=hud_position(w,at);
 for(unsigned i=0;i<n;i++)if(!result_rect(&w->weapons,at+32*i,0x2300|p[i],1,1,0))return session_fail(w);return 1;
}
static int blank(struct NativePlayWorld*w,uint16_t at,unsigned n){return result_rect(&w->weapons,hud_position(w,at),0x2320,n,1,0);}
static int credit(struct NativePlayWorld*w,uint16_t at){
 uint8_t text[10];for(unsigned i=0;i<10;i++)text[i]=runtime_credits[i];
 unsigned n=w->session->credits;if(n<2)text[6]=32;text[8]=48+n/10;text[9]=48+n%10;return bytes(w,at,text,10);
}
/*52050/5223A. Keep source layout/flags; display the explicit native shared
 * balance in both regional banks. Free play affects eligibility, not digits. */
int session_hud_global(struct NativePlayWorld*w){
 struct NativeSession*s=w->session;unsigned options=s->config.hud_options;
 if(!s->config.arcade_mode||!(options&2)){
  if(s->hud_flags&2){if(!blank(w,0x1d,16)||!blank(w,0x2fd,17))return session_fail(w);s->hud_flags&=(uint8_t)~3;}
  else if(s->hud_flags&1){
   if(!blank(w,0x1d,3)||!blank(w,0x19d,4)||!blank(w,0x2fd,4)||!blank(w,0x49d,4))return session_fail(w);
   if(!credit(w,0x7d))return 0;
   if(!credit(w,0x37d))return 0;
  }
 }
 if(s->config.arcade_mode&&!(options&1)){
  if(s->hud_flags&8){if(!blank(w,0x21d,8))return session_fail(w);s->hud_flags&=(uint8_t)~12;}
  else if(s->hud_flags&4){uint8_t rank=0x31+s->config.difficulty;
   if(!bytes(w,0x21d,runtime_level,6)||!bytes(w,0x2dd,&rank,1))return 0;
  }
 }
 return 1;
}
/*13C3C: home pause gates the ENTIRE logical frame, not player motion.
 * Input/repeat and VBlank audio still run. First entry draws next opportunity. */
int session_pause(struct NativePlayWorld*w,uint8_t edge){
 struct NativeSession*s=w->session;struct SourceAudioQueue*q=&w->scene_runtime->audio;
 if(s->config.arcade_mode||!s->pause_allowed)return 1;
 unsigned mask=(w->results.live_active[0]==1?2:0)|(w->results.live_active[1]==1?8:0);
 if(!s->paused){if(edge&mask){s->paused=1;s->pause_blink=0;source_audio_game_command(q,0x10e0);}return 1;}
 if(edge&mask){
  s->paused=0;if(!bytes(w,0x25c,runtime_pause_blank,5))return 0;
  source_audio_stop_command(q,0x10e0);source_audio_game_command(q,0x10e1);return 1;
 }
 unsigned phase=s->pause_blink++&31;
 if(!phase)return bytes(w,0x25c,runtime_pause,5);
 if(phase==24)return bytes(w,0x25c,runtime_pause_blank,5);return 1;
}
