#include "session_internal.h"

int session_fail(struct NativePlayWorld*w){if(w->session)w->session->fault=1;w->fault=14;return 0;}
int session_input_allowed(const struct NativePlayWorld*w){return !w->enemies.scripted_input.system_freeze&&!w->enemies.siege_phase;}
int session_credit_ready(const struct NativeSession*s){return s->config.free_play||s->credits;}
static int clear_fixed(struct NativePlayWorld*w){return scene_video_fixed_rect(&w->video,0,0,0x300,40,32,0);}
static int palettes(struct NativePlayWorld*w){
 if(!w->video.set_palette)return session_fail(w);
 for(unsigned i=0;i<SESSION_PALETTE_COUNT;i++){const uint16_t*v=session_palettes[i];if(!w->video.set_palette(w->video.palette_context,v[0],v[1],v[2]))return session_fail(w);}return 1;
}
static void reset_fade(struct NativePlayWorld*w){w->video.fade=(__typeof__(w->video.fade)){0};w->enemies.scene_flags[0]=0;}
static int clear_children(struct NativePlayWorld*w,struct EntityRef ref){struct NativeEntity*e=entity_get(&w->entities,ref);if(!e)return session_fail(w);entity_delete_children(&w->entities,e);return !w->entities.fault;}
/*1D3C preserves player-controller state/lives;1CD4 also stops controllers.
 * Stable fixed groups survive both operations, unlike their actor children. */
static int cleanup(struct NativePlayWorld*w,int full){
 struct NativeSession*s=w->session;struct SceneRuntime*r=w->scene_runtime;
 if(!mission_event_stop(&w->weapons,w->weapons.enemy_group))return session_fail(w);
 struct EntityRef groups[]={r->group,w->weapons.enemy_group,w->weapons.projectile_group,
  w->player_groups[0],w->player_groups[1],w->hits.slug,s->controllers[0],s->controllers[1],s->boot};
 for(unsigned i=0;i<sizeof(groups)/sizeof(groups[0]);i++)if(!clear_children(w,groups[i]))return 0;
 r->active=r->driver.kind=0;
 for(unsigned i=0;i<2;i++){
  w->weapons.players[i]=0;w->hits.players[i]=w->arms[i]=(struct EntityRef){ENTITY_NONE,0};w->death_routes[i]=w->action_fault[i]=0;
  if(full){s->player[i].state=PC_STOP;s->player[i].entry=1;}
 }
 slug_body_reset(&w->slug,w->hits.slug.index);target_ring_reset(&w->targets);terrain_platform_reset(&w->terrain.platforms);
 w->enemies.scripted_input.active=0;w->events.enabled=0;return 1;
}
static void joins(struct NativePlayWorld*w){
 struct NativeSession*s=w->session;
 /* Explicit native policy. P1 gets the last shared credit if both request
  * it on the same frame; start/repeat/action streams remain independent. */
 for(unsigned i=0;i<2;i++)if((s->input.join_requests&(1u<<i))&&
  (w->results.live_active[i]==0||w->results.live_active[i]==2)&&
  !s->player[i].join_blocked&&session_input_allowed(w)&&session_credit_ready(s)){
  if(!s->config.free_play)s->credits--;w->results.live_active[i]=1;
 }
}
static int begin_mission(struct NativePlayWorld*w){
 struct NativeSession*s=w->session;struct ResultWorld*r=&w->results;unsigned scene=session_scenes[r->progress];
 r->mission=scene;r->level=r->progress;r->mission_valid=1;
 source_audio_reset(&w->scene_runtime->audio);reset_fade(w);
 if(!scene_driver_begin(w->scene_runtime,w,(enum SceneDriverEntry)scene,scene,s->config.timing_variant))return 0;
 if(!scene_fade_spawn(&w->weapons,s->boot,1,1,0)||!session_ui_timer(w)||!play_world_start_events(w,scene))return 0;
 w->enemies.scripted_player_lock=w->enemies.damage_enabled=255;
 if(!session_ui_start(w)||!clear_fixed(w)||!scene_text_clear_border(&w->video,0))return 0;
 /*29588 and469E2(param0D) initialization outputs.*/
 w->slug_narrow_bounds=255;w->enemies.continue_count=13;w->enemies.palette_shutdown=0;
 w->slug_player_query_enabled=1;w->slug_riders_enabled=1;
 w->weapons.grenade_body_collision=0;w->input_freeze=60;s->pause_allowed=1;
 s->hud_flags=5;if(!session_hud_global(w))return 0;
 for(unsigned i=0;i<2;i++)if(s->player[i].state==PC_STOP){s->player[i].state=PC_INIT;s->player[i].entry=1;}
 s->state=SESSION_PLAY;return 1;
}
/*1A70: continue waits precede active-player and completion tests. The hold
 * byte is cleared only at1ADC, so a pending respawn cannot lose its hold. */
static int mission_wait(struct NativePlayWorld*w){
 struct NativeSession*s=w->session;uint8_t a=w->results.live_active[0],b=w->results.live_active[1];
 s->hud_flags=5;if(!session_hud_global(w))return 0;
 if(a==2||b==2)return 0;
 if(a!=1&&b!=1){
  s->player[0].join_blocked=s->player[1].join_blocked=1;
  w->enemies.scripted_player_lock=255;
  source_audio_game_command(&w->scene_runtime->audio,0x27);
  struct NativeEntity*boot=entity_get(&w->entities,s->boot);if(!boot)return session_fail(w);
  boot->payload.group.command=255;if(!banner_white_flash(&w->weapons,s->boot))return session_fail(w);
  s->pause_allowed=0;s->state=SESSION_LOST;return 0;
 }
 if(s->transition_hold||w->enemies.scripted_player_lock){s->transition_hold=0;return 0;}return 1;
}
static void boot_tick(struct EntityPool*p,struct NativeEntity*e,void*user){
 (void)p;struct NativePlayWorld*w=user;struct NativeSession*s=w->session;
 if(w->fault||!s||s->fault)return;
 if(s->state==SESSION_PRESENTATION)return;
 if(w->enemies.scripted_input.system_freeze)w->enemies.scripted_input.system_freeze--;
 if(w->input_freeze)w->input_freeze--;
 joins(w);
 w->video.start_prompt=s->config.arcade_mode?(session_credit_ready(s)?3:0):4;
 w->video.start_prompt_valid=1;
 /* Original FE0 immediately advances through nonwaiting boot operations. */
 for(unsigned budget=0;budget<12;budget++){
  switch(s->state){
  case SESSION_MISSION_BEGIN:
   if(w->results.progress>=6){s->state=SESSION_FINAL_BEGIN;continue;}
   if(!begin_mission(w))session_fail(w);return;
  case SESSION_PLAY:
   if(!mission_wait(w))return;
   w->enemies.scripted_player_lock=255;
   if(!mission_results_spawn(&w->weapons,s->boot)||!scene_text_clear_border(&w->video,0)){session_fail(w);return;}
   s->state=SESSION_RESULTS;continue;
  case SESSION_RESULTS:
   w->enemies.scripted_input.system_freeze=2;
   w->input_freeze=2;
   if(!mission_wait(w))return;
   w->results.progress++;results_snapshot(&w->results);
   if(!cleanup(w,0)){session_fail(w);return;}
   s->state=SESSION_MISSION_BEGIN;continue;
  case SESSION_FINAL_BEGIN:
   s->pause_allowed=0;
   if(!clear_fixed(w)||!scene_text_clear_border(&w->video,0)||!palettes(w)){session_fail(w);return;}
   reset_fade(w);e->payload.group.phase=0;w->enemies.scripted_player_lock=255;
   w->results.mission=w->terrain.scene=w->weapons.scene_number=w->scene_runtime->scene=13;
   if(!results_spawn(&w->weapons,s->boot,RESULT_ENDING)){session_fail(w);return;}
   s->state=SESSION_FINAL;continue;
  case SESSION_FINAL:
   w->enemies.scripted_input.system_freeze=2;
   if(!e->payload.group.phase)return;
   if(!cleanup(w,0)){session_fail(w);return;}
   s->state=SESSION_ENDING_BEGIN;continue;
  case SESSION_ENDING_BEGIN:{
   struct ResultWorld*r=&w->results;
   if(!r->summary_valid){session_fail(w);return;}
   /*5174C tests BOTH completion snapshots. Continue totals are not tested
    * by1604; a no-continue ending criterion here would be invented. */
   s->ending=r->active[0]!=1||r->active[1]!=1?SCENE_DRIVER_ENDING_B:
    SCENE_DRIVER_ENDING_D;
   unsigned scene=s->ending==SCENE_DRIVER_ENDING_B?11:12;
   r->mission=scene;r->level=255;reset_fade(w);w->enemies.scripted_player_lock=255;
   if(!clear_fixed(w)||!scene_text_clear_border(&w->video,0)||
    !scene_driver_begin(w->scene_runtime,w,s->ending,scene,s->config.timing_variant)||
    !play_world_start_events(w,scene)){session_fail(w);return;}
   s->state=SESSION_ENDING;continue;
  }
  case SESSION_ENDING:
   w->enemies.scripted_input.system_freeze=2;
   w->input_freeze=2;
   if(s->transition_hold||w->enemies.scripted_player_lock){s->transition_hold=0;return;}
   if(!cleanup(w,1)){session_fail(w);return;}
   s->state=SESSION_GAMEOVER_BEGIN;continue;
  case SESSION_LOST:
   w->enemies.scripted_input.system_freeze=2;
   w->input_freeze=2;s->hud_flags=5;if(!session_hud_global(w))return;
   if(e->payload.group.command)return;
   if(!cleanup(w,1)||!palettes(w)){session_fail(w);return;}
   s->state=SESSION_GAMEOVER_BEGIN;continue;
  case SESSION_GAMEOVER_BEGIN:
   s->pause_allowed=0;
   if(!clear_fixed(w)){session_fail(w);return;}reset_fade(w);
   w->results.mission=w->results.level=255;s->player[0].join_blocked=s->player[1].join_blocked=1;
   source_audio_byte(&w->scene_runtime->audio,6);
   /*46682 renders program12 once with a stationary camera. Subsequent
    * frames run only its fades and timed acknowledgement. */
   if(!scene_runtime_load(w->scene_runtime,w,12,255)){session_fail(w);return;}
   w->scene_runtime->follow.valid=0;
   if(!scene_runtime_frame(w->scene_runtime,w)){session_fail(w);return;}
   w->scene_runtime->active=0;w->enemies.scripted_player_lock=255;
   if(!scene_fade_spawn(&w->weapons,s->boot,3,1,0)||
    (w->results.progress>=6&&!results_spawn(&w->weapons,s->boot,RESULT_PEACE))||
    !scene_text_clear_border(&w->video,0)){session_fail(w);return;}
   s->panel_clock=120;s->gameover_phase=0;s->state=SESSION_GAMEOVER;return;
  case SESSION_GAMEOVER:
   if(--s->panel_clock)return;
   if(!s->gameover_phase){s->gameover_phase=1;s->panel_clock=60;if(!scene_fade_spawn(&w->weapons,s->boot,2,0,0))session_fail(w);return;}
   w->enemies.scripted_player_lock=0;
   if(!clear_children(w,s->boot)||!palettes(w)){session_fail(w);return;}
   source_audio_reset(&w->scene_runtime->audio);s->state=SESSION_RETURN_TITLE;return;
  case SESSION_RETURN_TITLE:return;
  default:session_fail(w);return;
  }
 }
 session_fail(w);
}
int native_session_begin(struct NativeSession*s,struct NativePlayWorld*w,struct SceneRuntime*r,const struct NativeSessionConfig*c){
 if(!s||!w||!r||!c||w->fault||w->session||w->scene_runtime||w->ticks||
  !c->initial_players||(c->initial_players&~3)||!c->initial_lives||c->time_setting>3||c->difficulty>7||c->control_layout>5||c->first_character>1)return 0;
 *s=(struct NativeSession){.config=*c,.credits=c->initial_credits>99?99:c->initial_credits,.state=SESSION_MISSION_BEGIN};
 *r=(struct SceneRuntime){0};w->session=s;
 struct EntityRef root=entity_ref(&w->entities.nodes[0]);
 /* Prepend P1, P2, scene, boot: execution is boot -> scene -> P2 -> P1. */
 for(unsigned i=0;i<2;i++){
  struct NativeEntity*g=entity_spawn(&w->entities,root,ENTITY_GROUP,ENTITY_FIRST_CHILD,session_player_tick);if(!g)return session_fail(w);
  entity_reserve(g);s->controllers[i]=entity_ref(g);s->player[i].state=PC_STOP;
  w->results.live_active[i]=(c->initial_players>>i)&1;
 }
 if(!scene_runtime_attach(r,w))return session_fail(w);
 struct NativeEntity*g=entity_spawn(&w->entities,root,ENTITY_GROUP,ENTITY_FIRST_CHILD,boot_tick);if(!g)return session_fail(w);
 entity_reserve(g);s->boot=entity_ref(g);w->results.progress=0;
 r->audio.game_mode=2;r->audio.game_sound_enabled=1;
 r->audio.bios_demo=!!c->arcade_mode;
 w->interactions.rank=c->difficulty;w->interactions.rank_valid=1;w->interactions.demo_mode=0;
 w->control_mode[0]=w->control_mode[1]=w->slug_layout=c->control_layout;
 w->enemies.grenade_mode=3;w->blood_enabled=c->blood_enabled;
 if(!palettes(w))return 0;s->ready=1;return 1;
}
int native_session_frame(struct NativeSession*s,struct NativePlayWorld*w,const struct NativeSessionInput*in){
 if(!s||!w||!in||w->session!=s||!s->ready||s->fault||w->fault)return 0;
 s->input=*in;
 unsigned credits=s->credits+in->credits_added;s->credits=credits>99?99:credits;
 for(unsigned i=0;i<2;i++){w->inputs[i]=in->player[i];w->raw_inputs[i]=in->raw_player[i];w->results.edges[i]=in->edges[i];w->results.repeat[i]=in->repeat[i];w->results.start[i]=in->start[i];}
 w->raw_inputs_valid=in->raw_valid;
 w->results.input_valid=w->results.start_valid=1;
 if(!session_pause(w,in->system_edges))return 0;
 if(s->paused&&s->pause_allowed&&!s->config.arcade_mode){s->input.join_requests=s->input.credits_added=0;return 1;}
 for(unsigned i=0;i<2;i++)play_world_cheat_input(w,i,in->edges[i]); /* port extra: Konami code */
 int ok=play_world_tick(w);s->input.join_requests=s->input.credits_added=0;
 return ok&&!s->fault;
}
