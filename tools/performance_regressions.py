#!/usr/bin/env python3
"""Host differential checks for the native renderer/audio performance changes."""
import argparse, json, subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--cc', default='cc')
    p.add_argument('--only', nargs='+', help='Run selected relevant checks')
    a = p.parse_args()
    out = ROOT/'build/performance-regressions'
    out.mkdir(parents=True, exist_ok=True)
    common = ['-O2', '-Wno-attributes', '-ffunction-sections', '-fdata-sections', '-Isrc']
    checks = {
        'fixed-incremental146': ['tests/fixed_incremental146.c','src/object_video.c','src/tile_video_assets.S'],
        'fixed-compact-format146': ['tests/fixed_compact_format146.c','src/object_video.c','src/tile_video_assets.S'],
        'native-inherit-hud146': ['tests/native_inherit_hud146.c','m1_native/tile_stream.c'],
        'fixed-palette-lut146': ['tests/fixed_palette_lut146.c','src/object_video.c','src/palette_video.c','src/scaled_drawing.c','generated/object-masks.c','src/tile_video_assets.S'],
        'source-visit145': ['tests/source_visit_inline145.c','src/source_drawing.c','generated/drawing-data.c'],
        'native-bg-palette': ['tests/native_background_palette_cache.c','src/palette_video.c','generated/render-data.c'],
        'object-retry145': ['tests/native_object_retry145.c'],
        # v143's exact PCM16-cache oracle is historical: the production
        # cache now uses PCM8. Compare bounded output error and exact
        # logical/event timing against the self-contained IMA decoder.
        'audio-pcm8': ['tests/audio_pcm8_145.c', 'src/native_audio.c', 'tests/reference/native_audio_099.c', 'generated/native-audio-data.c', 'generated/native-audio-tempo.c', 'src/native_audio_assets.S'],
        'object-free-extent': ['tests/object_free_extent.c'],
        'placement-groups': ['tests/placement_groups.c','src/scene_placements.c','generated/scene-placements.c','generated/village-programs.c'],
        'native-plane-visibility': ['tests/native_plane_visibility.c','src/scene_maps.c','src/tile_video.c'],
        'column-batch': ['tests/column_batch_equivalence.c','src/tile_video.c'],
        'fx-collision': ['tests/fx_collision.c','src/hit_scene.c','src/hitbox.c'],
        'fx-compiled': ['tests/fx_compiled_equivalence.c','src/fx_animation.c','src/animation.c','generated/boss-programs.c'],
        'hud-budget': ['tests/native_hud_budget.c','src/hud_bg.c','src/tile_video_assets.S'],
        'strip-exhaustive': ['tests/strip_budget_exhaustive.c','src/source_strip_budget.c','tests/reference/source_strip_budget_101.c','generated/drawing-data.c'],
        'object-map-fast': ['tests/object_map_fast_equivalence.c', 'src/scaled_drawing.c', 'generated/scaled-drawing-data.c', 'generated/object-masks.c', 'src/scaled_drawing_assets.S'],
        'object-residency-order': ['tests/object_residency_order.c'],
        'palette-identity': ['tests/video_palette_identity.c','tests/reference/video_palette_106.c','src/video_palette.c','src/background_video.c'],
        'strip-budget': ['tests/strip_budget_equivalence.c', 'src/source_strip_budget.c',
                         'tests/reference/source_strip_budget_101.c', 'generated/drawing-data.c'],
        'capture': ['tests/object_capture_equivalence.c','src/source_drawing.c','src/source_strip_budget.c'],
        'object-pressure': ['tests/object_palette_pressure.c'],
        'renderer': ['tests/render_cache_equivalence.c', 'src/background_video.c',
                     'tests/reference/background_video_099.c', 'tests/reference/fixed_object_104.c', 'src/background_tile.c',
                     'src/video_palette.c', 'src/fixed_object.c', 'src/object_video.c',
                     'generated/object-masks.c', 'src/tile_video_assets.S'],
        'bg-unprune': ['tests/background_unprune_108.c', 'src/background_video.c',
                     'tests/reference/background_video_099.c', 'src/background_tile.c',
                     'src/video_palette.c', 'src/fixed_object.c', 'src/object_video.c',
                     'generated/object-masks.c', 'src/tile_video_assets.S'],
        'bg-recycle': ['tests/background_recycled_origin.c', 'src/background_video.c',
                       'tests/reference/background_video_099.c', 'tests/reference/fixed_object_104.c',
                       'src/background_tile.c', 'src/video_palette.c', 'src/fixed_object.c',
                       'src/object_video.c', 'generated/object-masks.c', 'src/tile_video_assets.S'],
        'reference-formats': ['tests/reference_formats_106.c', 'src/background_video.c', 'src/background_video_gba.c',
                     'tests/reference/background_video_099.c', 'tests/reference/fixed_object_104.c', 'src/background_tile.c',
                     'src/video_palette.c', 'src/fixed_object.c', 'src/object_video.c', 'src/hud_bg.c',
                     'generated/object-masks.c', 'src/tile_video_assets.S'],
        'palette-worklist': ['tests/palette_worklist_equivalence.c','src/palette_video.c','tests/reference/palette_video_099.c','generated/render-data.c'],
        'palette': ['tests/palette_cache_equivalence.c', 'src/palette_video.c',
                    'tests/reference/palette_video_099.c', 'generated/render-data.c'],
        'audio-catalog': ['tests/native_audio_catalog.c', 'src/native_audio.c',
                          'generated/native-audio-data.c', 'generated/native-audio-tempo.c',
                          'src/native_audio_assets.S'],
        'integration': ['tests/native_regressions.c', 'src/hit_scene.c', 'src/hitbox.c',
                        'src/background_video.c', 'src/video_palette.c', 'src/object_video.c'],
    }
    if a.only:
        unknown = set(a.only)-set(checks)
        if unknown: p.error("Unknown checks: "+str(sorted(unknown)))
        checks = {k:v for k,v in checks.items() if k in a.only}
    results = {}
    for name, units in checks.items():
        exe = out/name
        subprocess.run([a.cc, *common, *units, '-Wl,--gc-sections,-z,noexecstack'+(',--wrap=native_palette_dirty' if name=='reference-formats' else ''),
                        '-o', str(exe)], cwd=ROOT, check=True)
        extra = [str(ROOT/'tests/fixtures/m1-bg-before-tick3491-104.bin')] if name == 'bg-recycle' else []
        if name == 'bg-unprune': extra = [str(ROOT/'tests/fixtures/reference-107-unprune-25000.bin')]
        if name == 'fixed-incremental146': extra = [str(ROOT/'tests/fixtures'/f) for f in ['m5-fixed-before-tick57248-145.bin','m2-fixed-before-tick22259-146.bin','m6-fixed-before-tick77547-146.bin','m6-fixed-before-tick66407-146B.bin']]
        if name == 'reference-formats': extra = [str(ROOT/f'tests/fixtures/reference-105-{n}.bin') for n in [6428,6930,8810,25000]]
        result = subprocess.check_output([str(exe), *extra], cwd=ROOT, text=True)
        results[name] = result.strip()
        print(result, end='', flush=True)
    (out/'results.json').write_text(json.dumps(results, indent=2)+'\n')

if __name__ == '__main__':
    main()
