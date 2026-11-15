#include "native_runtime.h"
#include "native_link.h"
static unsigned lock(void){
#if defined(__arm__)
 volatile uint16_t*p=(void*)0x04000208;unsigned v=*p;*p=0;__asm__ volatile("":::"memory");return v;
#else
 return 0;
#endif
}
static void unlock(unsigned v){
#if defined(__arm__)
 __asm__ volatile("":::"memory");*(volatile uint16_t*)0x04000208=(uint16_t)v;
#else
 (void)v;
#endif
}
int native_runtime_attach_link(struct NativeRuntime*r,struct NativeLink*l){
 if(!r||!l||r->fault||r->link||l->state!=NLINK_RUNNING||r->world->ticks||l->frame)return 0;
 /* Session must already have been begun from the agreed config/RNG. Do
  * not overwrite a live simulation to conceal a handshake mismatch. */
 for(unsigned i=0;i<32;i++)if(r->world->rng.words[i]!=l->config.rng.words[i])return 0;
 if(r->world->rng.head!=l->config.rng.head)return 0;
 const struct NativeSessionConfig*a=&r->session->config,*b=&l->config.session;
 if(a->initial_players!=b->initial_players||a->initial_credits!=b->initial_credits||a->free_play!=b->free_play||a->allow_continue!=b->allow_continue||
  a->initial_lives!=b->initial_lives||a->time_setting!=b->time_setting||a->timing_variant!=b->timing_variant||
  a->difficulty!=b->difficulty||a->control_layout!=b->control_layout||a->blood_enabled!=b->blood_enabled||a->arcade_mode!=b->arcade_mode||a->hud_options!=b->hud_options||a->first_character!=b->first_character)return 0;
 r->link=l;return 1;
}
enum NativeFrameResult native_runtime_link_pump(struct NativeRuntime*r,uint16_t keys,uint8_t credits){
 if(!r||!r->link||r->fault)return NATIVE_FRAME_ERROR;struct NativeLink*l=r->link;
 if(l->state!=NLINK_RUNNING||l->leave)return NATIVE_FRAME_ERROR;
 if(!r->ready||(r->half_rate&&r->vblanks<=1))return NATIVE_FRAME_WAIT;
 if(!l->prepared){
  struct NativeLinkInput in={keys,credits,r->world->scene_runtime->audio.reply,native_link_frame_checksum(l,r->world)};
  unsigned ime=lock();int offered=native_link_offer(l,&in);unlock(ime);if(!offered)return NATIVE_FRAME_ERROR;
 }
 struct NativeLinkInput pair[2];unsigned ime=lock();int available=native_link_take(l,pair);unlock(ime);
 if(available<0)return NATIVE_FRAME_ERROR;if(!available)return NATIVE_FRAME_WAIT;
 /* Source replies affect ending/gameplay state. Both machines consume the
  * host's latched audible reply at this same logical opportunity. */
 r->world->scene_runtime->audio.reply=pair[0].audio_reply;
 unsigned host=r->session->config.first_character;
 uint16_t both[2];both[host]=pair[0].keys;both[host^1]=pair[1].keys;unsigned coins=pair[0].credits+pair[1].credits;
 if(coins>255)coins=255;r->link_consuming=1;
 enum NativeFrameResult result=native_runtime_pump(r,both,(uint8_t)coins);r->link_consuming=0;return result;
}
int native_runtime_resume_link(struct NativeRuntime*r,struct NativeLink*l){
 /* Called only after a fingerprint-checked new game or verified complete
  * frozen snapshot. Frame zero still checks semantic state before input. */
 if(!r||!l||!r->world||!r->session||r->fault||r->world->fault||r->session->fault||r->link||l->state!=NLINK_RUNNING||!l->agreed||l->frame)return 0;
 unsigned guest=r->session->config.first_character^1;
 r->link=l;r->join_pending=r->world->results.live_active[guest]==1?0:(1u<<guest);
 r->ready=0;r->vblanks=0;r->waited=0;return 1;
}
