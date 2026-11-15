#include "native_runtime.h"
#include "native_link.h"
static int fail(struct NativeRuntime*r){r->fault=1;return 0;}
uint8_t native_gba_player_buttons(uint16_t keys){
 return ((keys&(1u<<6))?PB_UP:0)|((keys&(1u<<7))?PB_DOWN:0)|
  ((keys&(1u<<5))?PB_LEFT:0)|((keys&(1u<<4))?PB_RIGHT:0)|
  ((keys&1)?PB_FIRE:0)|((keys&2)?PB_JUMP:0)|((keys&(1u<<8))?PB_GRENADE:0);
}
/* Hardware access only: callers must not invoke this in a host process. */
uint16_t native_gba_read_keys(void){return (uint16_t)(~*(volatile uint16_t*)0x04000130u)&0x3ff;}
int native_runtime_init(struct NativeRuntime*r,struct NativePlayWorld*w,struct NativeSession*s,const struct NativeRuntimeServices*v){
 if(!r||!w||!s||!v||w->session!=s||!s->ready||s->fault||w->fault||
  !v->audio.read_reply||!v->audio.send_byte||!v->commit||!v->prepare)return 0;
 *r=(struct NativeRuntime){.world=w,.session=s,.services=*v};source_input_reset(&r->input);return 1;
}
/*1E5E: audio precedes hold/cadence/busy checks; counters wrap at their
 * original widths. Source ready latch allows only one outstanding frame. */
int native_runtime_refresh(struct NativeRuntime*r,int present){
 if(!r||r->fault||!r->world||!r->session)return 0;
 r->refreshes++;r->vblanks++;
 if(r->link)native_link_vblank(r->link);
 if(!source_audio_vblank(&r->world->scene_runtime->audio,&r->services.audio))return fail(r);
 if(r->session->frame_hold){r->session->frame_hold--;r->waited++;return 1;}
 if(!present||(r->half_rate&&r->vblanks<=1)||r->ready){r->waited++;return 1;}
 int committed=r->services.commit(r->services.video_context,r->world);
 if(!committed)return fail(r);
 if(committed==2){r->waited++;return 1;}
 r->ready=1;return 1;
}
int native_runtime_vblank(struct NativeRuntime*r){return native_runtime_refresh(r,1);}
enum NativeFrameResult native_runtime_pump(struct NativeRuntime*r,const uint16_t keys[2],uint8_t credits){
 if(!r||r->fault||!r->world||!r->session||!keys)return NATIVE_FRAME_ERROR;
 if(r->link&&!r->link_consuming)return NATIVE_FRAME_ERROR;
 if(r->simulated)goto present;
 /* Overlap (local play only): start the next simulation tick without
  * waiting for the pending transfer. A frame hold still blocks it, as the
  * source pauses simulation there. Link play keeps the strict order. */
 int overlap=!r->link&&r->half_rate&&!r->session->frame_hold;
 if((!r->ready&&!overlap)||(r->half_rate&&r->vblanks<=1))return NATIVE_FRAME_WAIT;
 r->vblanks=0;
 if(r->services.before_simulate)r->services.before_simulate();
 uint8_t held[2]={native_gba_player_buttons(keys[0]),native_gba_player_buttons(keys[1])};
 uint8_t system=((keys[0]&8)?2:0)|((keys[1]&8)?8:0);
 if(!source_input_frame(&r->input,held,system,r->world->results.live_active)){fail(r);return NATIVE_FRAME_ERROR;}
 struct NativeSessionInput in={.system_edges=r->input.system_edges,.credits_added=credits,.raw_valid=1};
 for(unsigned i=0;i<2;i++){
  const struct SourceInputRecord*a=&r->input.active[i],*p=&r->input.raw[i];
  in.player[i]=(struct PlayerSourceInput){a->held,a->edges};in.raw_player[i]=(struct PlayerSourceInput){p->held,p->edges};
  in.edges[i]=p->edges;in.repeat[i]=p->repeat;in.start[i]=a->held;
  unsigned state=r->world->results.live_active[i];
  if((state==0||state==2)&&(in.system_edges&(i?8:2)))in.join_requests|=1u<<i;
 }
 uint32_t before=r->world->ticks;
 if(r->join_pending){
  for(unsigned i=0;i<2;i++)if(r->world->results.live_active[i]==1)r->join_pending&=(uint8_t)~(1u<<i);
  in.join_requests|=r->join_pending;
 }
 if(!native_session_frame(r->session,r->world,&in)){fail(r);return NATIVE_FRAME_ERROR;}
 int advanced=r->world->ticks!=before;
 if(advanced)r->half_rate=1;
 /* Pause modifies FIX/HUD too. A consumed opportunity must present those
  * writes even when simulation ticks stay unchanged. */
 if(r->services.after_simulate)r->services.after_simulate();
 r->simulated=1;r->simulated_result=(uint8_t)advanced;
present:
 if(!r->ready&&!(r->services.settled&&!r->link&&r->services.settled(r->services.video_context)))return NATIVE_FRAME_WAIT;
 r->simulated=0;advanced=r->simulated_result;
 if(!r->services.prepare(r->services.video_context,r->world)){fail(r);return NATIVE_FRAME_ERROR;}
 r->waited=0;r->ready=0;
 /* Publish completed staging immediately, so the next VBlank IRQ can
  * transfer it without a second VBlank merely to queue the request. */
 int queued=r->services.commit(r->services.video_context,r->world);
 if(!queued){fail(r);return NATIVE_FRAME_ERROR;}
 if(queued==1)r->ready=1;
 return advanced?NATIVE_FRAME_ADVANCED:NATIVE_FRAME_PAUSED;
}
