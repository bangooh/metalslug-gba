#include "../m1_native/runtime.h"
#include "../generated/m1-assets.h"
#include "native_video.h"
#include "object_scanline.h"
#include "../generated/ending-background-palette.inc"
#include "play_world.h"
#include "scene_runtime.h"
#include "session.h"
#include "hud_bg.h"
#include "result_background.h"
#include "native_background_assets.h"
#include "../generated/native-bg-assets.h"
#include "../generated/render-data.h"
#define R16(a) (*(volatile uint16_t*)(a))
#ifdef NATIVE_PROFILE
volatile uint32_t native_profile_ticks[10];
#define PROFILE_BEGIN() uint16_t profile_start=0,profile_last=0;R16(0x0400010a)=0;R16(0x04000108)=0;R16(0x0400010a)=0x81
#define PROFILE_SPLIT(n) do{uint16_t now=R16(0x04000108);native_profile_ticks[n]+=(uint16_t)(now-profile_last);profile_last=now;}while(0)
#define PROFILE_END() do{native_profile_ticks[6]+=(uint16_t)(R16(0x04000108)-profile_start);native_profile_ticks[7]++;R16(0x0400010a)=0;}while(0)
#else
#define PROFILE_BEGIN() ((void)0)
#define PROFILE_SPLIT(n) ((void)0)
#define PROFILE_END() ((void)0)
#endif
static int fail(struct NativeVideo*v){if(v){v->fault=1;if(v->world)v->world->video.fault=1;}return 0;}
int native_video_attach(struct NativeVideo*v,struct NativePlayWorld*w){
 if(!v||!w||w->fault)return 0;*v=(struct NativeVideo){.world=w,.renderer_scene=255};v->backgrounds.depth4=1;
 m1_runtime_load(0);
 if(!native_tile_video_attach(&v->tiles,w)||!native_palette_attach(&v->palettes,w))return fail(v);
 v->palettes.native_backgrounds=1;v->tiles.source_cells=native_background_4(&v->backgrounds)->source_cells;native_object_resident_reset();
 /* E8E overrides5CACE's centre with X240,Y-up488. Source-screen Y-down
  * anchor is496-488=8; no camera-world displacement belongs in this pivot. */
 struct NativeObjectSceneConfig config={.context=&v->tiles,.layer=native_tile_video_object_layer,
  .tile_budget=1024,.pivot_x=240,.pivot_y=8,.projection_valid=1};
 if(!native_object_scene_attach(&v->objects,w,&v->palettes,&config))return fail(v);
 m1_runtime_attach(v);return 1;
}
/* Count cells, including hidden cells, as distinct tiles: a conservative
 * capacity bound that needs no new storage or palette approximation. */
static int background_can_unprune(const struct NativeBackgroundFrame*f){
 unsigned layers=(1u<<f->map_count)-1;
 if(!f->culling||f->fault||!f->count||((f->enable>>8)&layers)!=layers)return 0;
 unsigned cells=1,limit=native_background_capacity(f)-64;
 for(unsigned l=0;l<f->map_count;l++){
  unsigned rows=(160+(f->scroll_y[l]&7)+7)/8,cols=(240+(f->scroll_x[l]&7)+7)/8;
  for(unsigned y=0;y<rows;y++)for(unsigned x=0;x<cols;x++){
   unsigned at=(((f->scroll_y[l]>>3)+y)&31)*32+(((f->scroll_x[l]>>3)+x)&31);
   if(f->maps[l][at]&&++cells>limit)return 0;
  }
 }
 return 1;
}
int native_video_prepare(void*ctx,struct NativePlayWorld*w){
 struct NativeVideo*v=ctx;if(!v||v->fault||w!=v->world||w->video.fault)return fail(v);
 PROFILE_BEGIN();
 /* Event requests are latched until a complete frame can be published. */
 v->flash_pending=!!(w->weapons.shotgun_flash||w->enemies.scene_flags[1]);
 unsigned bg_transfer=0;
 if(native_result_background_prepare(v)){v->background_palette.base=0;if(v->fault)return fail(v);goto objects;}
 /* NeoGeo backdrop is palette FF entry F (colour RAM FFF), including
  * the same source global fade as the other palette slots. */
 const uint16_t*backdrop=native_palette_slot(&v->palettes,255);
 if(!v->bg4_replanned&&!(w->ticks&127)&&background_can_unprune(&v->backgrounds)){
  v->backgrounds.culling=3;
 }
 unsigned scene=w->scene_runtime?w->scene_runtime->program:255;
 unsigned profile=v->backgrounds.native_profile&127;
 /* Authored native maps use cartridge palettes, not legacy profile lookup. */
 if(!v->bg4_replanned&&(v->renderer_scene!=scene||v->renderer_palette_revision!=v->palettes.binding_revision))profile=native_bg_profile(&v->palettes);
 /* Ending B uses three source planes (including the rear waterfall cliff).
  * BG3 remains reserved for FIX text; never silently discard the rear plane. */
 if(v->renderer_scene!=scene){
  v->renderer_scene=scene;v->backgrounds.count=0;v->backgrounds.culling=0;v->backgrounds.layer_limit=(scene==14||scene==15)?3:2;
 }
 /* Ending-specific exact banks can assign the same source fragment to a
  * different physical bank. Do not retain a boundary tile from an older
  * regional palette profile when bindings change inside the same scene. */
 unsigned render_profile=profile|((scene==14||scene==15)&&(profile==0||profile==1||profile==4||profile==5||profile==12)?128:0);
 if(!v->bg4_replanned&&v->backgrounds.native_profile!=render_profile)v->backgrounds.count=0;
 v->backgrounds.native_profile=render_profile;v->backgrounds.depth4=1;
 PROFILE_SPLIT(0);
 int was_hud=v->hud_active;
 v->hud_active=!v->bg4_replanned&&native_hud_prepare((struct NativeHudFrame*)v->fixed.pixels,&v->tiles,&v->palettes,was_hud);
 if(!v->bg4_replanned){v->fixed.cached=v->fixed.suffix=0;v->fixed.bytes=0;}
 if(!v->hud_active&&!v->bg4_replanned)return fail(v);
 v->renderer_palette_revision=v->palettes.binding_revision;
 PROFILE_SPLIT(8);
 unsigned span=((struct NativeHudFrame*)v->fixed.pixels)->tile_span;
 unsigned hud=span==64?3:span==128?1:2;
 if(v->backgrounds.hud_reserved!=hud){v->backgrounds.hud_reserved=hud;v->backgrounds.map_dirty=15;for(unsigned i=0;i<4;i++)v->backgrounds.map_rows[i]=0xffffffffu;}
 if(!v->bg4_replanned)native_background_cache_storage((uint8_t*)v->fixed.pixels+24576,v->fixed.storage_generation);
 v->backgrounds.palette_revision=v->palettes.fixed_revision;
 v->colors.fault=v->colors.ready=0;v->colors.stable=0;v->colors.count=v->colors.limit=192;
 const struct ScenePaletteFade*fade=&v->palettes.fade;
 int hardware_fade=fade->mode&&fade->red==fade->green&&fade->red==fade->blue;
 v->backgrounds.fade_control=hardware_fade?((fade->mode==255?0x80:0xc0)|(v->bg4_replanned?15:7)):0;
 v->backgrounds.fade_level=hardware_fade?(fade->red*16+15)/31:0;
 const uint16_t*cartridge_palette=v->bg4_replanned?m1_runtime_palette():0;
 const uint16_t*bindings=v->bg4_replanned?m1_runtime_palette_bindings():0;
 if(v->bg4_replanned)native_palette_background(&v->background_palette,&v->palettes,cartridge_palette,bindings,v->colors.colors);
 else {v->background_palette.base=0;for(unsigned bank=0;bank<12;bank++)for(unsigned i=0;i<16;i++){
  unsigned color=(scene==14||scene==15)?ending_background_palette[profile][bank][i]:native_bg_native_palettes[profile][bank][i];
  if(fade->mode&&!hardware_fade){unsigned shaded=0;for(unsigned shift=0;shift<15;shift+=5){unsigned c=(color>>shift)&31,level=shift==0?fade->red:shift==5?fade->green:fade->blue;unsigned change=native_color_ramp[fade->mode==255?31-c:c][level];shaded|=(fade->mode==255?c+change:c-change)<<shift;}color=shaded;}
  v->colors.colors[bank*16+i]=color;
 }}
 v->colors.colors[0]=backdrop?backdrop[15]:0;
 PROFILE_SPLIT(1);
 int background=0;
 if(v->bg4_replanned)background=m1_runtime_prepare(v);
 else for(unsigned attempt=0;attempt<6;attempt++){
  background=native_background_prepare(&v->backgrounds,&v->tiles,&v->palettes,&v->colors,0,0);
  if(background)break;
  if(v->backgrounds.fault!=NBG_TILES)break;
  if(v->backgrounds.culling!=1)v->backgrounds.culling=v->backgrounds.culling?1:2;
  else if(v->backgrounds.map_count>2&&scene!=14&&scene!=15){v->backgrounds.layer_limit=2;v->backgrounds.culling=1;}
  else break;
  v->backgrounds.count=0;
 }
 PROFILE_SPLIT(2);
 if(!background)return fail(v);
 if(v->bg4_replanned&&v->hud_active){
  if(!native_hud_prepare_limit((struct NativeHudFrame*)v->fixed.pixels,&v->tiles,&v->palettes,was_hud,256)){v->hud_active=0;v->hud_blocked=1;v->fixed.cached=v->fixed.suffix=0;v->fixed.bytes=0;}
 }
 if(v->bg4_replanned&&was_hud&&!v->hud_active){v->fixed.cached=v->fixed.suffix=0;v->fixed.bytes=0;}

 if(v->hud_active)native_hud_layout((struct NativeHudFrame*)v->fixed.pixels,v->backgrounds.map_count);
 v->colors.ready=1;
 for(unsigned plane=0;plane<4;plane++)for(unsigned col=0;col<32;col++)v->tiles.planes[plane].dirty[col]=0;
 if(v->bg4_replanned)m1_runtime_prestage_background(v);
 bg_transfer=v->bg4_replanned?m1_runtime_heavy_transfer_bytes(v):0;
objects:
 for(unsigned attempt=0;attempt<5;attempt++){
  native_object_begin(&v->objects.frame,v->objects.config.tile_budget);
  v->objects.frame.dense_layout=attempt>=2?(attempt>=3?3:1):0;
  /* Retain the same existing scenery approximation budget when HUD is omitted. */
  if(v->bg4_replanned&&!v->hud_active)v->objects.frame.palette_tolerance=31;
  /* Port extra (v167): attempt 4 omits the OBJ HUD for this frame only.
   * A frame without HUD is preferable to stopping the game with
   * 0x9000_18_04 (object hardware: tile pressure). The FIX cache stays
   * valid, so the HUD returns on the next prepared frame. */
  if(v->bg4_replanned&&!v->hud_active&&attempt<4){v->fixed.suffix=0;if(!native_fixed_refresh(&v->fixed,&v->objects.frame,&v->tiles,&v->palettes))return fail(v);if(!native_fixed_compact(&v->fixed,&v->objects.frame))return fail(v);v->objects.frame.palette_tolerance=31;}
  if(attempt>=2&&v->bg4_replanned&&!v->hud_active&&w->weapons.scene_number==5){
   unsigned offset=(2u*v->fixed.bytes+31u)&~31u;
   /* FIX pixels occupy at most two spans; metadata is above byte24576. */
   if(offset<24576)native_object_bridge_scratch((uint8_t*)v->fixed.pixels+offset,24576-offset);
  }
  if(native_object_scene_append_budgeted(&v->objects,attempt>=2?3:attempt)){
   /* Capacity in tiles/OAM alone misses hardware scanline exhaustion.
    * M3/M6 collapse effects may hide structural chunks despite fitting RAM.
    * Ask the existing retry path to reduce cosmetics before publishing. */
   int pressure=0;
   if(attempt<2&&(w->weapons.scene_number==3||w->weapons.scene_number==5)){
    unsigned draw=0;while(draw<v->objects.count&&!v->objects.draws[draw].cosmetic)draw++;
    pressure=draw<v->objects.count&&native_object_scanline_overflow(&v->objects.frame,1210);
   }
   if(!pressure)break;
   v->objects.frame.fault=NOBJ_CAPACITY;v->objects.fault=NOS_HARDWARE;
  }
  unsigned fault=v->objects.frame.fault;
  int hud_fallback=v->bg4_replanned&&!v->hud_active;
  if(attempt==4||(attempt==3&&!hud_fallback)||(attempt==2&&fault!=NOBJ_TILES&&!hud_fallback)||v->objects.fault!=NOS_HARDWARE||(fault!=NOBJ_TILES&&fault!=NOBJ_CAPACITY&&fault!=NOBJ_PALETTES))return fail(v);
  /* Keep all simulation updates. First retry omits sparse smoke/debris;
   * the next retry uses dense ROM layouts and may omit visual explosions.
   * OAM and palette pressure need this retry too, not only tile pressure.
   * Only a remaining tile shortage enables exact horizontal row clipping;
   * it may spend spare OAM but still fails explicitly if OAM is exhausted.
   * Players, live enemies, projectiles, pickups and platforms stay visible. */
  v->objects.fault=0;v->objects.budget_retries++;
  /* The sparse-only retry cannot reclaim OAM or tiles if the captured
   * queue contains no sparse cosmetics. This occurs in the M6 airship
   * burst: repeating the entire full-detail build costs another ~69ms.
   * Keep the dense fallback and all source strip/gameplay accounting;
   * do not carry a pressure tier into the next frame. Palette failures
   * retain their original retry sequence because bank reuse can differ. */
  if(attempt==0&&(fault==NOBJ_TILES||fault==NOBJ_CAPACITY)){
   unsigned draw=0;
   while(draw<v->objects.count&&!(v->objects.draws[draw].cosmetic&ENTITY_COSMETIC_SPARSE))draw++;
   if(draw==v->objects.count)attempt=1;
  }
 }
 if(!native_object_finish(&v->objects.frame))return fail(v);
 PROFILE_SPLIT(3);
 if(!native_object_resident_stage(&v->objects.frame))return fail(v);
 if(bg_transfer)m1_runtime_prestage_objects(v,bg_transfer);
 PROFILE_SPLIT(4);
 if(!v->bg4_replanned)native_background_stage_palette(&v->backgrounds,&v->colors);native_object_stage_palette(&v->objects.frame);
 if(v->hud_active)native_hud_stage_palette((struct NativeHudFrame*)v->fixed.pixels);
 for(unsigned x=0;x<40;x++)v->tiles.fixed_dirty[x]=0;
 PROFILE_SPLIT(5);PROFILE_END();
 w->weapons.shotgun_flash=0;w->enemies.scene_flags[1]=0;
 return 1;
}
int native_video_settled(void*ctx){struct NativeVideo*v=ctx;if(!v||v->fault||v->commit_pending||!v->commit_complete)return 0;v->commit_complete=0;return 1;}
int native_video_commit(void*ctx,struct NativePlayWorld*w){
 struct NativeVideo*v=ctx;
 if(!v||v->fault||w!=v->world)return fail(v);
 if(v->commit_complete){v->commit_complete=0;return 1;}
 if(v->commit_pending)return 2;
 if(!v->backgrounds.ready||!v->colors.ready||!v->objects.frame.ready)return fail(v);
 /* Publish only after staging is complete. Main retains these buffers until
  * IRQ completion is acknowledged; prepare cannot race the DMA readers. */
 __asm__ volatile("":::"memory");v->commit_pending=1;return 2;
}
static void transfer(struct NativeVideo*v){
 R16(0x04000000)|=128;
 if(!v->result_mode)R16(0x04000000)&=(uint16_t)~7;
 if(!(v->result_mode?native_result_background_commit(v):v->bg4_replanned?m1_runtime_commit(v):native_background_commit(&v->backgrounds,&v->colors))||!native_object_scene_commit(&v->objects)){fail(v);return;}
 if(v->hud_active){native_hud_commit((struct NativeHudFrame*)v->fixed.pixels);v->hud_commits++;}
 /* The loss GAME OVER card is entirely background/FIX. Do not expose
  * combat OBJ graphics during this presentation-only scene. The completed
  * campaign's separate PEACE result actor must remain visible. */
 if(v->world->session&&v->world->session->state==SESSION_GAMEOVER&&
    v->world->results.progress<6)R16(0x04000000)&=(uint16_t)~0x1000;
 R16(0x04000050)=v->backgrounds.fade_control;R16(0x04000054)=v->backgrounds.fade_level;
 if(v->flash_pending){R16(0x04000050)=0xbf;R16(0x04000054)=16;v->flash_pending=0;v->flash_active=1;}
 if(v->backgrounds.depth4)v->bg4_commits++;
 v->fixed.upload_pending=0;
 v->commits++;
 if(R16(0x04000004)&1){R16(0x04000000)&=(uint16_t)~128;v->reveal_pending=0;}
 else{v->reveal_pending=1;v->late_transfers++;}
 __asm__ volatile("":::"memory");v->commit_pending=0;v->commit_complete=1;
}
void native_video_vblank(struct NativeVideo*v){
 if(!v||v->fault)return;
 /* SCOP_MODE writes the HIGH byte of106EAC. The low byte is cleared at
  * frame initialization; its bit3 is therefore not the scene mode bit3.
  * Reloaded speedN advances the hardware auto counter everyN+1 VBlanks. */
 if(v->auto_wait)v->auto_wait--;
 else{v->auto_frame++;v->auto_wait=v->world&&v->world->scene_runtime?v->world->scene_runtime->mode:0;}
}
void native_video_irq_vblank(struct NativeVideo*v){
 if(!v||v->fault)return;
 /* One visible LCD frame, even when rendering the next game frame is slow.
  * A late transfer must first be revealed before its flash can expire. */
 if(v->flash_active&&!v->reveal_pending){
  R16(0x04000050)=v->backgrounds.fade_control;R16(0x04000054)=v->backgrounds.fade_level;
  v->flash_active=0;
 }
 v->reveal_hold=0;
 if(v->reveal_pending&&(R16(0x04000004)&1)){
  R16(0x04000000)&=(uint16_t)~128;v->reveal_pending=0;v->reveal_hold=1;
 }
 if(v->commit_pending&&!v->reveal_pending&&!v->reveal_hold)transfer(v);
}
