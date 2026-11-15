"""Describe connected native code separately from execution/fidelity evidence."""
import json

APPROVED_BINDING = '35 exact Marco body/arm graph pairs; approved player.tiles unchanged; other poses use source graphics'
PALETTE_GAP = 'Original48-slot shared palette allocation, three-entry history, reference release/reuse and exhaustion semantics'
STRIP_GAP = 'Source partial-map strip prefix and alternating pool-overflow/previous-SCB-fill pixel selection'
AUDIO_LIMITS = [
 'GBA6000Hz nominal mono, one premixed music voice plus4 effects; IMA with exact short-SFX PCM cache. Timer6000.435Hz; source YM channel stealing and stereo differ.',
 'Commands07/08 use extracted M1 tempo curves and envelope timing; premixed resampling changes pitch with tempo.',
 'Captured internal timing/random timbre are baked into each asset; live per-YM-channel synthesis is not implemented.',
 '299 captured replies and11 observed loop states; audible-time replies connected, integrated listening/timing not verified.',
]

def read(g, name):
 return json.loads((g/name).read_text())

def apply_current_integration(status, g, version):
 build=read(g,'native-build.json')
 if build['version']!=version:
  raise RuntimeError('Recompile/link the current VERSION before saving its integration status')
 drawing=read(g,'drawing-source.json')['counts']
 status['last_verified_executable_scope']=status['executable_scope']
 status['executable_scope_note']='Historical0.5.43 execution evidence only; not the current default build target'
 status['default_build_target']='tools/build.py -> build_native.py -> native_main -> NativeRuntime -> NativeSession -> NativePlayWorld'
 status['last_generated_rom_version']='0.5.89-wip'
 status['implementation_only'].extend([
  '0.5.98 real startup/serialized main/VBlank/audio IRQ/SIO IRQ; unified Runtime/Session/PlayWorld/video/audio/link and return path',
  'All4 source BGs ->4Mode0 BGs with exact255opaqueRGB555 palette,896shared tiles,physical-priority ordering and source auto-animation clock; FIX/HUD -> shared OBJ',
  'Source380strip/210key admission and previous committed free counts; partial overflow pixel selection remains a fidelity gap',
  'Approved35Marco composites conditionally replace paired body/arm draws; source asset SHA unchanged',
  'Original logo pieces, demo streams, ranking actors/text, tutorial with actual player/Slug and scripted inputs; native GBA title/link/controls/error/return menus',
  'All192audio assets connected to DMA/Timers; external07/08 source curve/envelope translated to premixed resampling',
  'Three explicit native argument corrections for59/120/318; original inherited-register semantics remain unproved',
  'Actual GBA2P handshake/frame barrier/input/reply/CRC/checksum/timeout path, packet-complete cancel; no hardware execution',
  'ARM-Werror compilation and final cartridge ELF link only; no newROM/gameplay/emulator/regression/performance tests',
 ])
 status['native_integration']=dict(version=version,connected=True,complete_port=False,
  target=build['target'],build_record='generated/native-build.json',
  units=build['units'],elf_bytes=build['elf_bytes'],elf_sha256=build['elf_sha256'],fingerprint=build['fingerprint'],
  stages=['startup/IRQ/input','main/frontend','Runtime/Session/PlayWorld','scene/entities/collisions/results/endings','background/OBJ/FIX/HUD/palettes/DMA3','audio/DMA1/Timers','2P/SIO/Timer2/barrier'],
  verification=build['validation'],gameplay_verified=False,two_devices_verified=False,new_rom_generated=False)
 status['session'].update(physical_main_connected=True,completion='Native main merges RAM ranking records, shows return, then title',
  remaining=['Fullcampaign/results/endings execution verification','Optional debug scene/language/configuration UI','Persistent ranking save'])
 status['runtime'].update(physical_main_connected=True,hardware_backends_implemented=True,
  threading='Main serializes world/preparation/DMA3; IRQ counts VBlank and services serial7/audioDMA9; separate main/IRQ stacks',
  remaining=['Integrated execution/timing verification','Source fidelity gaps listed in pending'])
 status['graphics'].update(programs=drawing['programs'],drawings=drawing['drawings'],maps=drawing['maps'],
  pixel_conversion='Offline nearest150percent then50percent ->240x168; crop4top/4bottom; all source map/BG/FIX output connected',
  physical_renderer_complete=False,physical_renderer_connected=True,
  remaining=[PALETTE_GAP,STRIP_GAP,'Approved combat.tiles overlay binding beyond35 player.tiles composites','Runtime capacity/timing/playability verification'])
 status['graphics']['scaled'].update(background_and_fixed='All4Mode0BGs plusFIX/HUD viaOBJ, serialized DMA3',approved_player_override_binding=APPROVED_BINDING)
 status['object_video'].update(source_resolver_connected=True,actual_world_collector=True,physical_main_connected=True,
  frame_storage_max_bytes=4352,source_free_strip_producer=True,
  source_strip_accounting='380records/210keys, horizontal source clipping, independent source pools, previous successful commit free count',
  layer_mapping='Physical source BG ordering; FIX0/forcedOBJ1/upperBG1/ordinaryOBJ2/lowerBG3',
  palette_packing='Share exact used nibble colors without quantization/reindexing',
  commit='Atomic forcedblank DMA3; release in VBlank, defer late reveal to next VBlank; timing not measured',
  remaining=[PALETTE_GAP,STRIP_GAP,'OAM/VRAM/palette/affine capacity and performance verification'])
 status['palettes'].update(hardware_main_connected=True,
  current_policy='Per-entity serial/generation retained RGB state, private interpolation and global fade; exact used-color hardware packing',
  backdrop='Source paletteFF entryF -> GBA backdrop index0',remaining=[PALETTE_GAP])
 status['tile_video'].update(background_compositor=True,main_connected=True,source_free_strip_producer=True,
  source_auto_clock_producer=True,fixed_frame_bytes=32772,
  fix_backend='Greedy exact4bppOBJ composition, shared128OAM/1024tiles/16palettes with actors',
  bg_backend='4Mode0BGs, shared8968bpp tiles,screenblocks28..31, exact255opaqueRGB555 colors',
  limitations=['Capacity excess faults explicitly; busy scenes not verified','Source partial strip overflow pixels differ','VBlank transfer/runtime composition performance not measured'])
 status['audio'].update(physical_main_connected=True,tempo_07_08=True,tempo_source='generated/native-audio-tempo-source.json',
  limitations=AUDIO_LIMITS,remaining=['Integrated listening/reply/timing/underrun verification','Per-YM-channel/source-random-timbre fidelity'])
 status['link'].update(physical_main_connected=True,frontend_connected=True,
  cancel='Wait until outbound LEAVE packet completes, or existing180VBlank timeout; no fabricated peer input',
  remaining=['Two GBA handshake/fullcampaign/disconnect/audio/IRQ timing validation','Broaden semantic checksum beyond currently covered actor fields'])
 status['frontend']=dict(connected=True,source='generated/frontend-source.json',
  native_menus=['title','controls','link wait/error/retry/cancel','error','return'],
  original_actors=['logo','two demo streams','tutorial player/Slug/input/icons/text/drop','ranking'],
  ranking='10records retained across rounds in RAM; linked round starts from identical defaults and merges local history afterward',
  tutorial='JP/EN selected by region; all4language text data extracted; language menu not exposed',gameplay_verified=False)
 status['exception_corrections']=dict(native_implemented=3,original_residual_semantics_verified=0,
  research_record='generated/exception-input-research.json',research_record_scope='Historical static residual-register analysis; resolved0of3 still valid for original semantics',
  cases={
   '59':'Native uses existing one RNG call result&3 as release selector; research override remains. Original inherited D1 not reproduced.',
   '120':'Native passes radius1 from caller D0 to steering branch and preserves movement/timer tail. Original inherited D5 not reproduced.',
   '318':'Native explicitly passes caller rectangle[-24,24,-4,8] and point[0,0] to recovery probe. Original inherited A3/A4 not reproduced.',
  })
 status['pending']=[PALETTE_GAP,STRIP_GAP,
  'Remaining approved combat overlay bindings and original shrink-mask sampling differences',
  '59D1/120D5/318A3A4 original residual-register fidelity; native corrections already connected',
  '106F4B wider indexed consumer/remaining alternate-source semantic comparisons',
  'Audio per-YM-channel stealing/pitch-preserving tempo/random-timbre fidelity',
  'Fullcampaign/results/endings/audio/capacity/performance and realtwo-device verification after implementation',
  'Optional persistent rankings and language/configuration/debug menus']
 status['not_connected']=['Exact original48palette resource lifecycle','Exact original overflow-strip pixel selection','Remaining approved combat overlays']
 status['current_status_source']='tools/integration_status.py; earlier implementation_only entries are historical milestones'

 # Execution evidence must match this exact cartridge, never a prior build.
 evidence=g/f'play-validation-{version}.json'
 if evidence.exists():
  checked=read(g,evidence.name)
  if checked['rom_sha256']!=build.get('rom_sha256'):raise RuntimeError('Play report is from a different ROM')
  scope=checked['verified_campaign_scope']
  status.update(workflow='User authorized continuing implementation and input-only emulator/play/regression/performance validation',
   verified_rom_version=version,last_generated_rom_version=version,new_rom_published=True,executable_scope=scope,
   executable_scope_note='Current partial input-only mGBA execution; full campaign and physical hardware not verified')
  status['implementation_only'][-1]='Historical0.5.98 compilation-only milestone superseded by matching0.5.99 input-only execution evidence'
  status['native_integration'].update(execution_verified=True,gameplay_verified=False,execution_scope=scope,
   new_rom_generated=True,verification=f'generated/{evidence.name}',two_devices_verified=False)
  status['runtime'].update(threading='Main owns simulation/staging; VBlank IRQ consumes immutable queued DMA3 frames; serial/audio IRQ share dispatcher',
   remaining=['Background composition FPS and forcedblank transfer overflow','Full campaign and real two-device verification'])
  status['graphics']['remaining']=[PALETTE_GAP,STRIP_GAP,'Additional approved combat overlays','Measured~4FPS and visible white transfer frames']
  status['object_video'].update(commit='Immutable staged frame queued to next VBlank IRQ; late transfer reveal retained; visible white frames remain',
   palette_packing='Actors preserve nibble indices; mutable FIX pixels reindex exactRGB into compatible actor palette holes without quantization')
  status['tile_video'].update(bg_backend='4Mode0BGs with exact-key/pixel cache, retained logical palette mappings, dirty/auto revision checks and incremental uploads',
   limitations=['Forcedblank DMA overflow visible','~4FPS measured in M1','Full busy-scene/2P capacity unverified',STRIP_GAP])
  status['audio'].update(catalog_validation=checked['catalog_decode'],actual_output=checked['audible_output'],
   remaining=['Full campaign listening/reply timing and physical hardware','Per-YM-channel/source-random-timbre fidelity'])
  if 'audio_settings' in checked:status['audio'].update(checked['audio_settings'])
  status['frontend'].update(execution_verified_paths=['logo/title','firstdemo1200ticks/ranking','tutorial1411ticks/game','ranking/title','pause/resume','gameover/title','no-peer link cancel'],
   limitations=['Second demo stream unverified','Native RETURN pixels may be brief under accumulated refreshes','Rankings RAM-only'])
  status['pending']=['PRIORITY: fix measured~4FPS and forcedblank white frames','M1 helicopter defeat/clear then allcampaign/results/endings/physical2P validation']+status['pending']
  status['current_execution_evidence']=f'generated/{evidence.name}'
  performance=checked.get('performance')
  if performance:
   status['native_integration']['performance']=performance
   status['runtime']['remaining']=['Occasional full-cache VRAM transfer overflow','Full campaign and real two-device verification']
   status['graphics']['remaining']=[PALETTE_GAP,STRIP_GAP,'Additional approved combat overlays','White transfer frames and heavier campaign-scene profiling']
   status['object_video']['palette_packing']=checked.get('object_palette_policy','Cached FIX prefix with dedicated original-index palettes; capacity fallback uses prior exactRGB actor-first packing')
   status['tile_video'].update(fixed_frame_bytes=checked.get('fixed_frame_bytes',32908),shared_palette_bytes=2054,
    bg_backend=performance.get('bg_backend','Persistent896tile cache, exact three-way keys, protected front references, world-aligned circular maps, edge-owner checks, dirty uploads, stable logical palette indices with exactRGB capacity fallback'),
    limitations=[f"{performance['white_frames']} all-white LCD frames in {performance['game_lcd_frames']} recorded game frames",f"{performance['m1_active']['fps']:.2f} FPS in the recorded M1 interval; not a full-campaign average",'Full busy-scene/2P capacity unverified',STRIP_GAP])
   status['frontend']['limitations']=['Second demo scene4 fails at tick112; same error reproduced in0.5.99','Native RETURN pixels may be brief under accumulated refreshes','Rankings RAM-only']
   status['pending'][0]=checked.get('priority','PRIORITY: second-demo scene4 tick112 enemy fault; remaining full-cache white transfers and slower scrolling windows')
   if 'visual_tradeoffs' in checked:
    status['graphics']['intentional_performance_adaptations']=checked['visual_tradeoffs']
    status['graphics']['scaled']['background_and_fixed']='Up to three nearest active source BG planes; persistent two-plane pressure limit; exact retained BG pixels; FIX/HUD via OBJ'
    status['palettes']['current_policy']=checked['object_palette_policy']
    status['object_video']['conditional_palette_approximation']=True
    status['tile_video']['decorative_auto_animation_enabled']=False
    status['audio']['pcm_cache']=checked.get('pcm_cache')
    status['runtime']['remaining']=['Reach nominal30FPS across heavy scenes','Remaining all-white transfer frames','Full campaign and physical two-device validation']
   if checked.get('m1_clear_verified'):
    status['pending'][1]='M1 clear and M2 entry observed; validate M2 clear onward, every boss, results/endings and physical2P; helicopter defeat remains separate'
    status['session']['remaining'][0]='M2 clear onward, remaining results/endings execution verification'
   if 'frontend_verified_paths' in checked:status['frontend']['execution_verified_paths']=checked['frontend_verified_paths']
   if 'implemented_optimizations' in checked:
    status['object_video']['capture_storage_bytes']=checked['memory']['object_scene_bytes']
    status['object_video']['performance_optimizations']=checked['implemented_optimizations']
   status['implementation_only'][-1]=f'Historical0.5.98 compile-only milestone superseded by matching{version} input-only execution evidence'

   if 'renderer_features' in checked:
    status['tile_video']['shared_palette_bytes']=2056
    status['tile_video']['reference_optimizations']=checked['renderer_features']
    status['graphics']['scaled']['background_and_fixed']='75percent4+4 crop; conditional exact4bpp+BG HUD; measured route predominantly8bpp+OBJ HUD; full stage/region precompiler incomplete'
    if 'offline_region_compiler' in checked:
     status['tile_video']['offline_region_compiler']=checked['offline_region_compiler']
     status['graphics']['scaled']['background_and_fixed']='75percent4+4 crop; conditional4bpp/BG HUD; all authored source interiors compiled offline, ROM integration and dynamic seams pending'

  if checked.get('offline_region_compiler',{}).get('format')=='joint-index-regional-palettes':
   status['native_integration'].update(experimental=True,target_fps=30,target_met=False)
   status['graphics']['pixel_conversion']='Offline75percent; complete authored BG windows/maps jointly quantized into stable4bpp indices and14regional RGB palettes'
   status['graphics']['scaled'].update(quantized=True,background_and_fixed='Nearest two BG planes, permanent4bpp; BG3HUD320tiles with exact HUD RGB. Dynamic boundary fragments still assembled; all source regions compiled, no missing-data fallback.')
   status['tile_video'].update(quantized=True,exact_color_allocator=False,permanent_bg4=True,permanent_bg_hud=True,fix_backend='BG3 with320resident tiles and banks12..15',bg_backend=checked['performance']['bg_backend'],limitations=['Dynamic boundary4quarter assembly remains','Aggregate FPS lower than109; nominal30 not achieved','Four white gameplay LCD frames in final36000LCD run','Fullcampaign/real2P unverified',STRIP_GAP])
   status['object_video']['reuse']='Immutable ROM-pointer/count registry retains physical VRAM positions across absent frames; OAM tile remap, compact layout on fragmentation'
   status['palettes']['background_policy']='14precompiled regional12x15 palettes, common colour indices; local BG interpolation frozen, uniform global BG fade uses hardware'
   status['runtime']['remaining']=['Resolve measured M2 FPS regression and reach nominal30','Remaining four all-white transfer frames','Fullcampaign and physical two-device validation']
