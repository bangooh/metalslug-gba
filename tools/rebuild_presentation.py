#!/usr/bin/env python3
"""Recapture original boot/opening and apply v191 authored presentation edits.

Requires staged pinned upstream, numpy, Pillow and unicorn. No capture cache
is consumed. Other game/background catalogs remain separate build inputs.
"""
import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--bios', type=Path, required=True)
    p.add_argument('--work', type=Path, default=ROOT/'build/presentation-source')
    a = p.parse_args()
    source, bios, work = a.source.resolve(), a.bios.resolve(), a.work.resolve()
    assert (source/'build/mslug_prom.bin').is_file(), 'Stage original game ROM first'
    assert bios.is_file(), 'Original neogeo.zip is required'
    work.mkdir(parents=True, exist_ok=True)
    def link(target, path):
        path.parent.mkdir(parents=True, exist_ok=True)
        if path.is_symlink() or path.exists():
            assert path.resolve() == target, f'Unexpected workspace entry: {path}'
        else:
            path.symlink_to(target)
    link(source, work/'upstream')
    link(ROOT, work/'src/metal-slug-native')
    link(bios, work/'scene-fixes/reference/neogeo.zip')
    capture = work/'scene-fixes/boot-animation'
    capture.mkdir(parents=True, exist_ok=True)
    def run(path, *args):
        subprocess.run([sys.executable, str(path), *map(str,args)], cwd=work, check=True)
    for name in ('capture-original-workspace.py', 'render-original-workspace.py'):
        run(ROOT/'tools/neogeo_boot_reference'/name)
    raw, refined = capture/'raw.npz', capture/'refined.npz'
    indices = np.load(capture/'indexframes.npy')
    palettes = np.load(capture/'paletteframes.npy')
    assert indices.shape == (165,160,240) and palettes.shape == (165,4096)
    np.savez_compressed(raw, indices=indices, palettes=palettes)
    art = ROOT/'art/presentation-refined'
    run(art/'refine_neogeo.py', raw, refined, art/'bios-font.json')
    with np.load(refined) as z:
        frames, colors = z['indices'].copy(), z['palettes'].copy()
    # Keep the authored v191 glyph reveal timing; this is timing metadata,
    # not a retained pixel capture or an opaque binary delta.
    for glyph in json.loads((art/'bios-reveal.json').read_text())['glyphs']:
        x,y,first = glyph['x'],glyph['y'],glyph['first_frame']
        assert 56 <= first < len(frames)
        region = frames[56:first,y:y+11,x:x+7]
        region[region == 241] = 4095
    (ROOT/'art/neogeo-boot').mkdir(parents=True, exist_ok=True)
    np.savez_compressed(ROOT/'art/neogeo-boot/bios-replay.npz', indices=frames, palettes=colors)
    run(ROOT/'tools/build_neogeo_boot.py')
    opening = work/'opening-work'
    opening.mkdir(exist_ok=True)
    for path in (ROOT/'tools/opening_reference').glob('*.py'):
        shutil.copy2(path, opening/path.name)
    run(opening/'oracle.py')
    run(opening/'pack.py')
    run(ROOT/'tools/build_opening_background.py', source)
    run(ROOT/'tools/build_title_from_opening.py')
    print('Original boot/opening recaptured and v191 presentation edits applied.')

if __name__ == '__main__':
    main()
