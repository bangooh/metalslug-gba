#include "native_runtime.h"
#include "native_video.h"
#include "native_audio.h"
#include "native_link.h"
#include "native_frontend.h"
#include "native_opening.h"
#include "native_inflate.h"
#include "../generated/neogeo-boot.h"
#include "frontend_actors.h"
#include "gba_interrupts.h"
#include "native_snapshot.h"
#include "../m1_native/runtime.h"
#include <stddef.h>
#include "../generated/result-data.h"
#define R16(a) (*(volatile uint16_t*)(a))
/* All large arenas are static EWRAM. IRQ only counts VBlanks and services
 * serial/audio and queued DMA3; main owns simulation and staging buffers. */
static struct NativePlayWorld world;
static struct NativeVideo video;
static struct SceneRuntime scene;
static struct NativeSession session;
static struct NativeRuntime runtime;
/* Sample mixing repeatedly touches this small state; keep it on the
 * 32-bit internal bus with the mixer, instead of16-bit external RAM. */
static struct NativeAudio audio __attribute__((section(".iwram.audio_state")));
static struct NativeLink link;
static struct HighScoreRecord rankings[10];
static struct SourceAudioQueue menu_audio;
static volatile uint32_t vblank_count;
struct NativeAppStatus {uint32_t refreshes,frames,error;uint8_t screen,playing,linked,demo;};
volatile struct NativeAppStatus native_app_status;
static unsigned screen,selection,screen_age,playing,linked,demo,demo_index,presentation;
static uint16_t previous_keys;
static unsigned wire_active,frozen,lobby_role;
static unsigned opening_intro,cinematic;
static unsigned tutorial_start;
static uint32_t cinematic_started;
static uint32_t phase_started;
static void arm_link(void);
static void vblank(void*unused){(void)unused;R16(0x04000202)=1;vblank_count++;if(playing&&!frozen)native_video_irq_vblank(&video);}
static struct NativeSessionConfig config(unsigned players,unsigned character){
 return (struct NativeSessionConfig){.initial_players=players,.free_play=1,.allow_continue=1,
  .initial_lives=3,.time_setting=0,.difficulty=3,.blood_enabled=1,.control_layout=0,.first_character=character};
}
static void show(unsigned next,unsigned detail){
 screen=next;screen_age=0;native_app_status.screen=next;
 native_frontend_show((enum NativeFrontendScreen)next,selection,rankings,detail);
}
static void menu_music(void){
 source_audio_reset(&menu_audio);menu_audio.game_mode=2;menu_audio.game_sound_enabled=1;source_audio_game_command(&menu_audio,0x2b);
}
static void title(void){
 native_link_gba_stop();wire_active=frozen=lobby_role=0;playing=linked=demo=presentation=0;native_app_status.playing=native_app_status.linked=native_app_status.demo=0;
 native_audio_gba_init(&audio);
 opening_intro=cinematic=tutorial_start=0;menu_music();show(NFRONT_TITLE,0);arm_link();
}
static unsigned error_code(void){
 unsigned owner=world.fault?world.fault:session.fault?0x80|session.fault:video.fault?0x90:runtime.fault?0xa0:0;
 unsigned stage=video.backgrounds.fault?video.backgrounds.fault:video.objects.fault?0x10|video.objects.fault:video.fixed.fault?0x20|video.fixed.fault:video.palettes.fault?0x30:video.colors.fault?0x40:0;
 return owner<<24|(unsigned)world.weapons.fault<<16|stage<<8|(unsigned)(audio.fault?audio.fault:video.objects.frame.fault);
}
static void error(void){native_app_status.error=error_code();playing=0;native_app_status.playing=0;native_link_gba_stop();wire_active=frozen=0;linked=0;show(NFRONT_ERROR,native_app_status.error);}
static void latch_budget(void){uint16_t ime=R16(0x04000208);R16(0x04000208)=0;native_object_scene_latch(&video.objects);R16(0x04000208)=ime;}
static void link_offer_early(void){
 if(!linked||link.state!=NLINK_RUNNING||link.prepared)return;
 struct NativeLinkInput in={native_gba_read_keys(),0,world.scene_runtime->audio.reply,native_link_frame_checksum(&link,&world)};
 uint16_t ime=R16(0x04000208);R16(0x04000208)=0;int offered=native_link_offer(&link,&in);R16(0x04000208)=ime;
 if(offered)native_link_gba_poll();
}
static int begin(const struct NativeSessionConfig*c,const struct SourceRng*r,unsigned is_demo){
 presentation=NPRESENT_NONE;
 R16(0x04000000)=128;struct NativePlayServices services={0};
 if(!native_audio_gba_init(&audio)||!play_world_init(&world,&services,r)||!native_video_attach(&video,&world)||
  !native_session_begin(&session,&world,&scene,c))return 0;
 if(!scene_video_fixed_rect(&world.video,0,0,0x300,40,32,0))return 0;
 /* A linked round starts from source defaults on both cartridges. Local
  * persistent ranking history is merged after the synchronized round. */
 if(!linked&&c->initial_players!=3)for(unsigned i=0;i<10;i++)world.results.table[i]=rankings[i];
 struct NativeRuntimeServices outputs={.after_simulate=link_offer_early,.before_simulate=latch_budget,.settled=native_video_settled,.audio=native_audio_transport(&audio),.video_context=&video,
  .commit=native_video_commit,.prepare=native_video_prepare};
 if(!native_runtime_init(&runtime,&world,&session,&outputs))return 0;
 if(is_demo){
  source_input_mode(&runtime.input,demo_index);world.results.progress=demo_index?4:1;
  world.interactions.demo_mode=1;scene.audio.game_mode=1;scene.audio.bios_demo=1;
 }
 if(linked&&!native_runtime_attach_link(&runtime,&link))return 0;
 if(!native_video_prepare(&video,&world))return 0;
 playing=1;demo=is_demo;screen_age=0;native_app_status.playing=1;native_app_status.linked=linked;native_app_status.demo=demo;
 return 1;
}
static void solo(unsigned is_demo,unsigned character){
 struct SourceRng rng;source_random_seed(&rng,is_demo?0:(uint16_t)vblank_count);
 struct NativeSessionConfig c=config(1u<<character,character);linked=0;if(!begin(&c,&rng,is_demo))error();
}
static void present(unsigned kind){
 struct SourceRng rng;source_random_seed(&rng,(uint16_t)vblank_count);
 struct NativeSessionConfig c=config(1,0);linked=0;
 if(!begin(&c,&rng,0)||!frontend_actors_begin(&world,kind)||!native_video_prepare(&video,&world)){error();return;}
 presentation=kind;
}
static void start_opening(void){
 native_link_gba_stop();wire_active=playing=linked=demo=presentation=0;
 native_app_status.playing=native_app_status.linked=native_app_status.demo=0;
 native_audio_gba_init(&audio);source_audio_reset(&menu_audio);menu_audio.game_mode=2;menu_audio.game_sound_enabled=1;
 if(!native_opening_begin(&video,sizeof(video),&world,sizeof(world))){error();return;}
 cinematic=1;cinematic_started=vblank_count;
}
static void arm_link(void){
 struct SourceRng rng;source_random_seed(&rng,(uint16_t)vblank_count);
 struct NativeSessionConfig c=config(3,playing&&!demo&&!presentation?session.config.first_character:0);struct NativeLinkConfig wire;
 wire_active=native_link_config(&wire,&c,&rng,vblank_count)&&native_link_gba_init(&link,&wire);
 lobby_role=0;
}
static void phase(unsigned control){link.control=(uint8_t)control;phase_started=vblank_count;native_link_gba_poll();}
static void freeze_game(void){frozen=1;native_audio_gba_stop();phase_started=vblank_count;}
static void begin_game(unsigned character){
 if(wire_active&&link.agreed&&link.slot)return;
 unsigned paired=wire_active&&link.agreed&&link.state==NLINK_READY;
 if(wire_active&&!link.slot){
  uint16_t ime=R16(0x04000208);R16(0x04000208)=0;
  int selected=native_link_select_character(&link,character);R16(0x04000208)=ime;
  if(!selected){error();return;}
 }
 if(paired){
  if(!begin(&link.config.session,&link.config.rng,0)){error();return;}
  freeze_game();phase(NLCTRL_NEW);
 }else solo(0,character);
}
static void start_game(unsigned character){
 if(wire_active&&link.agreed&&link.slot)return;
 tutorial_start=character+1;present(NPRESENT_TUTORIAL);
}
static void finish_tutorial(void){
 unsigned character=tutorial_start-1;tutorial_start=0;
 begin_game(character);
}
static int restore_display(void){
 /* Only logical drawing/palette/map state crosses the cable. Pixel caches
  * and GPU staging are rebuilt from each cartridge's identical ROM. */
 video.world=&world;video.background_palette=(struct NativeBackgroundPaletteCache){0};
 video.fixed=(struct NativeFixedObjects){0};video.colors=(struct NativeVideoPalette){0};
 video.objects.frame=(struct NativeObjectFrame){0};
 video.hud_active=video.hud_blocked=video.fault=0;video.result_mode=video.result_loaded=video.result_visible=0;
 video.renderer_scene=255;video.renderer_palette_revision=0;
 m1_runtime_rebuild(&video);native_object_resident_reset();
 R16(0x04000000)=128;video.commit_pending=video.commit_complete=video.reveal_pending=video.reveal_hold=0;
 runtime.ready=runtime.vblanks=runtime.waited=0;
 return native_video_prepare(&video,&world);
}
static int transfer_world(void){
 struct NativeSnapshotRange ranges[12]={
  {&world,sizeof(world)},{&scene,sizeof(scene)},{&session,sizeof(session)},
  {&runtime,sizeof(runtime)},{&audio,sizeof(audio)},
  {&video.tiles,sizeof(video.tiles)},{&video.palettes,sizeof(video.palettes)},
  {&video.objects,offsetof(struct NativeObjectScene,frame)},
  {&video.objects.config,sizeof(video.objects)-offsetof(struct NativeObjectScene,config)},
  {&video.auto_frame,4}
 };
 unsigned count=10+m1_runtime_snapshot(&video,ranges+10);
 if(!native_snapshot_transfer(&link,ranges,count,&vblank_count,native_frontend_link_progress))return 0;
 return restore_display();
}
static void join_failed(void){
 unsigned slot=link.slot,reason=link.state;native_link_gba_stop();wire_active=0;
 if(!slot&&playing){
  if(link.control==NLCTRL_NEW&&!world.ticks){unsigned host=session.config.first_character;session.config.initial_players=1u<<host;world.results.live_active[host^1]=0;}
  runtime.link=0;linked=0;native_app_status.linked=0;
  /* The host now displays the same transfer screen as the guest. Restore
   * its overwritten GPU caches even when the peer leaves before transfer. */
  if(screen==NFRONT_LINK_SYNC&&!restore_display()){error();return;}
  frozen=0;
  native_audio_gba_resume(&audio);arm_link();
 }else{
  playing=linked=frozen=0;native_app_status.playing=native_app_status.linked=0;
  native_audio_gba_init(&audio);menu_music();show(NFRONT_LINK_ERROR,reason>NLINK_RUNNING?reason:NLINK_FAULT);
 }
}
static int activate_link(void){
 if(!native_link_start(&link)||!native_runtime_resume_link(&runtime,&link)||!native_audio_gba_resume(&audio))return 0;
 playing=linked=1;frozen=demo=presentation=0;
 native_app_status.playing=native_app_status.linked=1;native_app_status.demo=0;return 1;
}
/* Automatic listening is independent of menus and simulation. Only the
 * cable master starts a new game; a guest may interrupt its attract demo. */
static int service_link(void){
 if(!wire_active)return 0;
 if(!native_link_gba_poll()){
  if(frozen){join_failed();return 1;}
  if(linked){unsigned reason=link.state;native_link_gba_stop();wire_active=playing=linked=0;native_app_status.playing=native_app_status.linked=0;show(NFRONT_LINK_ERROR,reason);return 1;}
  /* A passive listener has no partner yet, or a peer was unplugged. */
  arm_link();if(!playing&&screen==NFRONT_LINK_WAIT)show(NFRONT_TITLE,0);return 0;
 }
 if(linked)return 0;
 if(frozen&&(uint32_t)(vblank_count-phase_started)>=NLINK_TIMEOUT){join_failed();return 1;}
 if(!link.agreed)return 0;
 if(!frozen&&link.control==NLCTRL_IDLE){
  unsigned role=link.slot+1;
  if((!playing||demo||presentation)&&role!=lobby_role){
   playing=demo=presentation=0;native_app_status.playing=native_app_status.demo=0;
   native_audio_gba_init(&audio);menu_music();lobby_role=role;
   show(link.slot?NFRONT_LINK_WAIT:NFRONT_TITLE,1);
  }
 }
 if(link.slot){
  if(link.control==NLCTRL_IDLE&&link.peer_control==NLCTRL_NEW){
   if(!begin(&link.config.session,&link.config.rng,0)){error();return 1;}
   freeze_game();phase(NLCTRL_DONE);
  }else if(link.control==NLCTRL_IDLE&&link.peer_control==NLCTRL_JOIN){
   freeze_game();playing=demo=presentation=0;native_app_status.playing=native_app_status.demo=0;
   show(NFRONT_LINK_SYNC,0);phase(NLCTRL_ACCEPT);
  }else if(link.control==NLCTRL_ACCEPT&&link.peer_control==NLCTRL_STREAM){
   /* GPU memory remains local, so the progress screen stays visible. */
   link.control=NLCTRL_STREAM;
   if(!transfer_world()){join_failed();return 1;}
   phase(NLCTRL_DONE);return 1;
  }
  if(link.control==NLCTRL_DONE&&link.peer_running){if(!activate_link())error();return 1;}
 }else{
  if(link.control==NLCTRL_IDLE&&playing&&!demo&&!presentation&&session.state==SESSION_PLAY&&(runtime.ready||video.commit_complete)&&!video.commit_pending){freeze_game();show(NFRONT_LINK_SYNC,0);phase(NLCTRL_JOIN);}
  else if(link.control==NLCTRL_JOIN&&link.peer_control==NLCTRL_ACCEPT)phase(NLCTRL_STREAM);
  else if(link.control==NLCTRL_STREAM&&link.peer_control==NLCTRL_STREAM){
   if(!transfer_world()){join_failed();return 1;}
   phase(NLCTRL_COMMIT);return 1;
  }
  if((link.control==NLCTRL_NEW||link.control==NLCTRL_COMMIT)&&link.peer_control==NLCTRL_DONE){if(!activate_link())error();return 1;}
 }
 return 0;
}
static void merge_rankings(void){
 if(!linked){for(unsigned i=0;i<10;i++)rankings[i]=world.results.table[i];return;}
 for(unsigned i=0;i<10;i++){
  const struct HighScoreRecord*r=&world.results.table[i];int duplicate=0;
  for(unsigned j=0;j<10;j++)if(rankings[j].packed_score==r->packed_score&&rankings[j].name[0]==r->name[0]&&rankings[j].name[1]==r->name[1]&&rankings[j].name[2]==r->name[2])duplicate=1;
  if(duplicate)continue;unsigned at=0;while(at<10&&rankings[at].packed_score>r->packed_score)at++;if(at==10)continue;
  for(unsigned j=9;j>at;j--)rankings[j]=rankings[j-1];rankings[at]=*r;
 }
 for(unsigned i=0;i<10;i++){rankings[i].owner=0;rankings[i].rank=i+1;}
}
static void menu_input(uint16_t keys,uint16_t edges){
 (void)keys;
 if(screen==NFRONT_TITLE){
  native_frontend_title_tick(screen_age);
  if(edges&12)start_game(native_title_character(edges));
  else if(edges&1)start_game(0);
  else if(edges&2)show(NFRONT_CONTROLS,0);
  else if(edges&0x300)present(NPRESENT_RANKING);
  else if(screen_age>=600&&(!wire_active||!link.agreed)){solo(1,0);demo_index^=1;}
 }else if(screen==NFRONT_CONTROLS){if(edges&9)present(NPRESENT_TUTORIAL);else if(edges&2)title();}
 else if(screen==NFRONT_LINK_WAIT){(void)edges;}
 else if(screen==NFRONT_LINK_ERROR){if(edges&11)title();}
 else if(screen==NFRONT_RETURN){if(screen_age>=30||edges&11)title();}
 else if(screen==NFRONT_RANKING){if(screen_age>=600||edges&11)title();}
 else if(edges&11)title();
}
/* Original BIOS palette/tile replay; borrows the idle video arena. */
static void neogeo_boot(void){
 _Static_assert(38400+NEOGEO_TILES_BYTES+NEOGEO_CHANGES_BYTES+NEOGEO_PALETTES_BYTES<=sizeof(video),"boot arena");
 R16(0x04000000)=128;m1_runtime_load(2);
 uint8_t *pixels=(void*)&video,*tiles=pixels+38400;
 uint16_t *changes=(void*)(tiles+NEOGEO_TILES_BYTES);
 uint16_t *palettes=(void*)((uint8_t*)changes+NEOGEO_CHANGES_BYTES);
 if(!native_inflate(neogeo_tiles_packed,tiles,NEOGEO_TILES_BYTES,0)||
    !native_inflate(neogeo_changes_packed,changes,NEOGEO_CHANGES_BYTES,0)||
    !native_inflate(neogeo_palettes_packed,palettes,NEOGEO_PALETTES_BYTES,0))return;
 for(unsigned i=0;i<38400;i++)pixels[i]=0;
 R16(0x04000020)=R16(0x04000026)=256;R16(0x04000022)=R16(0x04000024)=0;
 *(volatile uint32_t*)0x04000028=*(volatile uint32_t*)0x0400002c=0;
 R16(0x04000050)=R16(0x04000054)=0;
 unsigned event=0,shown=~0u,sounded=0,page=0;uint32_t started=vblank_count,seen=~0u;
 while(vblank_count-started<NEOGEO_DURATION){
  if(seen==vblank_count)continue;seen=vblank_count;unsigned age=seen-started;
  if(!sounded&&age>=10){native_audio_send(&audio,2);native_audio_gba_resume(&audio);sounded=1;}
  if(age>=16&&(native_gba_read_keys()&15))break;
  unsigned frame=age/2;if(frame>=NEOGEO_FRAMES)frame=NEOGEO_FRAMES-1;
  if(frame==shown)continue;shown=frame;
  while(event<NEOGEO_EVENTS&&changes[0]<=frame*2){
   unsigned count=changes[1];changes+=2;
   while(count--){
    unsigned cell=*changes++,tile=*changes++,left=(cell%30)*8,top=(cell/30)*8;
    const uint8_t *src=tiles+tile*32;
    for(unsigned y=0;y<8;y++)for(unsigned x=0;x<4;x++){
     unsigned v=*src++;pixels[(top+y)*240+left+x*2]=v&15;pixels[(top+y)*240+left+x*2+1]=v>>4;
    }
   }event++;
  }
  page^=1;volatile uint32_t *dma=(volatile uint32_t*)0x040000d4;
  dma[2]=0;dma[0]=(uintptr_t)pixels;dma[1]=0x06000000+page*0xa000;dma[2]=0x84000000u|9600;while(dma[2]&0x80000000u){}
  while(R16(0x04000006)>=160){}while(R16(0x04000006)<160){}
  for(unsigned i=0;i<16;i++)((volatile uint16_t*)0x05000000)[i]=palettes[frame*16+i];
  R16(0x04000000)=0x404|(page<<4);
 }
 native_audio_gba_stop();R16(0x04000000)=128;
}

int main(void){
 R16(0x04000204)=0x4317; /* ROM wait states and prefetch, same as legacy GBA entry. */
 R16(0x04000000)=128;native_irq_install();native_irq_set(0,vblank,0);R16(0x04000004)=8;R16(0x04000202)=1;R16(0x04000200)|=1;
 for(unsigned i=0;i<10;i++)rankings[i]=result_defaults[i];
 if(!native_audio_gba_init(&audio)){show(NFRONT_ERROR,1);for(;;){}}
 native_irq_enable();neogeo_boot();opening_intro=1;present(NPRESENT_LOGO);uint32_t seen=vblank_count;
 for(;;){
  if(service_link())seen=vblank_count;
  uint32_t now=vblank_count;
  if(seen!=now){
   unsigned present=(R16(0x04000004)&1)!=0;
   do{
    seen++;native_app_status.refreshes++;screen_age++;
    if(wire_active&&!linked)native_link_vblank(&link);
    if(playing&&!frozen){native_video_vblank(&video);if(!native_runtime_refresh(&runtime,present&&seen==now)){error();break;}}
    else if(!frozen){struct SourceAudioTransport t=native_audio_transport(&audio);if(!source_audio_vblank(&menu_audio,&t)&&screen!=NFRONT_ERROR){error();break;}}
   }while(seen!=now);
   uint16_t keys=native_gba_read_keys(),edges=keys&~previous_keys;previous_keys=keys;
   if(frozen){}
   else if(cinematic){if(edges&15)title();}
   else if(playing&&presentation&&(edges&11)){
    unsigned kind=presentation,opening=opening_intro;
    if(kind==NPRESENT_TUTORIAL&&tutorial_start&&(edges&9))finish_tutorial();
    else{title();if(!opening&&kind==NPRESENT_TUTORIAL&&(edges&9))solo(0,0);}
   }
   else if(playing&&demo&&(edges&15)){title();if(edges&13)start_game(native_title_character(edges));}
   else if(!playing)menu_input(keys,edges);
  }
  if(cinematic){if(native_opening_frame(vblank_count-cinematic_started,&menu_audio))title();}
  if(playing&&!frozen){
   uint16_t local_keys=native_gba_read_keys(),keys[2]={0,0};keys[session.config.first_character]=local_keys;
   enum NativeFrameResult result=linked?native_runtime_link_pump(&runtime,local_keys,0):native_runtime_pump(&runtime,keys,0);
   if(result==NATIVE_FRAME_ERROR){if(linked&&link.state>NLINK_RUNNING){unsigned reason=link.state;playing=linked=0;native_link_gba_stop();wire_active=0;show(NFRONT_LINK_ERROR,reason);}else error();continue;}
   if(result==NATIVE_FRAME_ADVANCED){native_app_status.frames++;
    if(presentation&&frontend_actors_done(&world)){
     unsigned kind=presentation,opening=opening_intro;
     if(opening&&kind==NPRESENT_LOGO)start_opening();
     else if(kind==NPRESENT_TUTORIAL&&tutorial_start)finish_tutorial();
     else{title();if(!opening&&kind==NPRESENT_TUTORIAL)solo(0,0);}
    }
    else if(demo&&(runtime.input.mode==SOURCE_INPUT_DONE||world.ticks>=1200||session.state==SESSION_RETURN_TITLE)){title();present(NPRESENT_RANKING);}
    else if(!demo&&session.state==SESSION_RETURN_TITLE){merge_rankings();title();}
   }
  }
 }
}
