"""Apply the reviewed FIX/opening/BIOS editor project without rebuilding actors.

The stored baseline prevents stale full-project JSON from undoing newer assets.
Opening cells are repacked losslessly; frame timing and unrelated maps stay put.
"""
from pathlib import Path
import hashlib
import json
import re
import runpy
import struct
import zlib
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / 'generated'


def read_json(path):
    return json.loads(path.read_text())


def save_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=1) + '\n')


def rows(asset):
    return asset.get('after_padded') or asset['rows']


def validate(value, width, height):
    assert len(value) == height and all(
        isinstance(row, str) and len(row) == width and re.fullmatch('[0-9a-f]+', row)
        for row in value), 'Invalid indexed glyph'


def unpack(name, fmt):
    return list(struct.iter_unpack(fmt, zlib.decompress(
        (GEN / f'opening-{name}.deflate').read_bytes(), -15)))


def update_opening():
    from presentation_logo_repairs import restore_opening_fonts
    atlas = zlib.decompress((GEN / 'opening-tiles.deflate').read_bytes(), -15)
    cells, maps = unpack('cells', '<hhH'), unpack('maps', '<HH')
    trace = read_json(ROOT / 'tools/opening_reference/trace.json')
    ids = {}
    for frame in trace['frames'][::2]:
        for draw in sorted(frame['draws'], key=lambda d: d['priority']):
            ids.setdefault(draw['map'], len(ids))
    assert len(ids) == len(maps)
    fonts = {ids[0x2519ea], ids[0x251ac6]}

    def content(raw, cs, ms, mid):
        first, count = ms[mid]
        return [(x, y, bytes(raw[t*72:(t+1)*72])) for x, y, t in cs[first:first+count]]

    new_atlas, new_cells, new_maps = restore_opening_fonts(atlas, cells, maps, ids)
    # Remove the fully erased earlier glyph cells before keeping the new ones.
    compact_cells, compact_maps = [], []
    for mid, (first, count) in enumerate(new_maps):
        start = len(compact_cells)
        for cell in new_cells[first:first+count]:
            tile = new_atlas[cell[2]*72:(cell[2]+1)*72]
            if mid not in fonts or any(tile):
                compact_cells.append(cell)
        compact_maps.append((start, len(compact_cells)-start))
    compact_atlas, lookup, indexed = bytearray(), {}, []
    for x, y, tid in compact_cells:
        raw = bytes(new_atlas[tid*72:(tid+1)*72])
        if raw not in lookup:
            lookup[raw] = len(compact_atlas)//72
            compact_atlas.extend(raw)
        indexed.append((x, y, lookup[raw]))
    for mid in range(len(maps)):
        if mid not in fonts:
            assert content(atlas, cells, maps, mid) == content(
                compact_atlas, indexed, compact_maps, mid), ('Opening map changed', mid)
    blocks = {'tiles': bytes(compact_atlas),
              'cells': b''.join(struct.pack('<hhH', *v) for v in indexed),
              'maps': b''.join(struct.pack('<HH', *v) for v in compact_maps)}
    arena = sum(len(v) for v in blocks.values()) + 1032 + sum(
        len(zlib.decompress((GEN / f'opening-{n}.deflate').read_bytes(), -15))
        for n in ['palettes', 'frames', 'palrefs'])
    assert arena <= 185228, ('Opening arena overflow', arena)
    header = (GEN / 'opening-replay-data.h').read_text()
    for name, value in blocks.items():
        (GEN / f'opening-{name}.deflate').write_bytes(zlib.compress(value, 9, wbits=-15))
        header = re.sub(r'OPENING_'+name.upper()+r'_BYTES=\d+',
                        f'OPENING_{name.upper()}_BYTES={len(value)}', header)
    header = re.sub(r'OPENING_TILE_COUNT=\d+',
                    f'OPENING_TILE_COUNT={len(compact_atlas)//72}', header)
    (GEN / 'opening-replay-data.h').write_text(header)
    runpy.run_path(str(ROOT / 'tools/build_opening_visibility.py'), run_name='__main__')
    runpy.run_path(str(ROOT / 'tools/build_title_from_opening.py'), run_name='__main__')
    return {'unrelated_maps_preserved': len(maps)-2, 'arena_bytes': arena}


def update_bios(before, after):
    path = ROOT / 'art/neogeo-boot/bios-replay.npz'
    with np.load(path) as capture:
        frames = capture['indices'].copy()
        palettes = capture['palettes'].copy()
    original = frames.copy()
    hits = []
    for text, top in [('MAX 330 MEGA', 81), ('PRO-GEAR SPEC', 94)]:
        left = (240-(len(text)*8-1))//2
        for offset, ch in enumerate(text):
            if ch not in after or after[ch] == before[ch]:
                continue
            old = np.array([[241 if c == '#' else 4095 for c in row] for row in before[ch]])
            new = np.array([[241 if c == '#' else 4095 for c in row] for row in after[ch]])
            x = left+offset*8
            matched = 0
            for frame in frames:
                region = frame[top:top+11, x:x+7]
                if np.array_equal(region, old):
                    region[:] = new
                    matched += 1
                else:
                    assert not np.any(region == 241) or np.array_equal(region, new), (ch, x, top)
            assert matched > 0, ('BIOS glyph not found', ch)
            hits.append({'character': ch, 'x': x, 'y': top, 'frames': matched})
    assert np.array_equal(frames[:56], original[:56]), 'Spinning logo changed'
    np.savez_compressed(path, indices=frames, palettes=palettes)
    runpy.run_path(str(ROOT / 'tools/build_neogeo_boot.py'), run_name='__main__')
    return hits


def main():
    base = read_json(ROOT / 'art/font-infinity-base.json')
    project_path = ROOT / 'art/user-font-infinity-project.json'
    project = read_json(project_path)
    assert project['format'] == 'metalslug-font-project' and project['base'] == base['base']
    assert not project['palette_overrides'], 'Palette changes require separate shared-palette review'
    assert len(project['tiles']) == len(base['tiles']) == 4096
    assert set(project['assets']) == set(base['assets'])
    for tile in project['tiles']:
        validate(tile, 6, 6)
    changed = [i for i in range(4096) if project['tiles'][i] != base['tiles'][i]]
    edited_assets = []
    for key, asset in project['assets'].items():
        old = base['assets'][key]
        assert asset['target'] == old['target'], key
        validate(rows(asset), len(rows(old)[0]), len(rows(old)))
        if rows(asset) != rows(old):
            assert asset['target']['kind'] in ('approved-font', 'bios-font'), key
            edited_assets.append(key)

    fixed_path = GEN / 'native-scaled-fixed-6x6.4bpp'
    fixed = bytearray(fixed_path.read_bytes())
    for i in range(4096):
        actual = [''.join(f'{fixed[i*32+y*4+x//2] >> (4*(x%2)) & 15:x}'
                          for x in range(6)) for y in range(6)]
        assert actual in (base['tiles'][i], project['tiles'][i]), ('Stale FIX base', hex(i))
    fonts_path = ROOT / 'art/approved-fonts/fonts.json'
    fonts = read_json(fonts_path)
    bios_path = ROOT / 'art/presentation-refined/bios-font.json'
    bios = read_json(bios_path)
    bios_before = dict(bios)
    opening_changed = False
    for key in edited_assets:
        asset = project['assets'][key]
        target, value = asset['target'], rows(asset)
        if target['kind'] == 'approved-font':
            glyph = fonts['fonts'][target['font']]['glyphs'][target['character']]
            assert glyph['rows'] in (rows(base['assets'][key]), value), ('Stale opening font', key)
            opening_changed |= glyph['rows'] != value
            glyph['rows'] = value
        else:
            assert set(''.join(value)) <= {'0', 'f'}, key
            value = [r.replace('0', '.').replace('f', '#') for r in value]
            old = [r.replace('0', '.').replace('f', '#') for r in rows(base['assets'][key])]
            assert bios[target['character']] in (old, value), ('Stale BIOS font', key)
            bios[target['character']] = value

    patch_path = ROOT / 'art/hud-font-edit/patch.json'
    patch = read_json(patch_path)
    by_id = {a['id']: a for a in patch['assets']}
    fragments = bytearray((GEN / 'native-fix-fragments.u16').read_bytes())
    for code in changed:
        value = project['tiles'][code]
        pixels = [[int(c, 16) for c in row] for row in value]
        padded = [row+[0, 0] for row in pixels]+[[0]*8, [0]*8]
        fixed[code*32:(code+1)*32] = bytes(row[x] | row[x+1] << 4
                                         for row in padded for x in range(0, 8, 2))
        words = [sum(pixels[y*2+dy][x*2+dx] << (4*(dy*2+dx))
                     for dy in range(2) for dx in range(2)) for y in range(3) for x in range(3)]
        fragments[code*18:(code+1)*18] = struct.pack('<9H', *words)
        key = f'fixed:{code}'
        if key not in by_id:
            entry = {'id': key, 'label': f'User font {code:03X}', 'group': 'User font',
                     'target': {'kind': 'fixed', 'code': code},
                     'original': base['tiles'][code], 'before_current': base['tiles'][code]}
            patch['assets'].append(entry)
            by_id[key] = entry
        by_id[key]['rows'] = by_id[key]['after_padded'] = value
    fixed_path.write_bytes(fixed)
    (GEN / 'native-fix-fragments.u16').write_bytes(fragments)
    # The runtime fragment table includes older, intentional pixel-only fixes.
    # Keep their visibility authoritative when an untouched atlas tile differs.
    visible = [sum(any(fragments[i*18:(i+1)*18]) << (i%32)
                   for i in range(b*32, b*32+32)) for b in range(128)]
    (GEN / 'native-fix-visible.inc').write_text(','.join(hex(v)+'u' for v in visible)+'\n')
    save_json(patch_path, patch)
    opening = None
    if opening_changed:
        save_json(fonts_path, fonts)
        provenance_path = ROOT / 'art/approved-fonts/provenance.json'
        provenance = read_json(provenance_path)
        provenance['files']['fonts.json'] = hashlib.sha256(fonts_path.read_bytes()).hexdigest()
        save_json(provenance_path, provenance)
        opening = update_opening()
    bios_hits = []
    if bios != bios_before:
        bios_hits = update_bios(bios_before, bios)
        save_json(bios_path, bios)
    report = {'base': project['base'], 'project_sha256': hashlib.sha256(project_path.read_bytes()).hexdigest(),
              'fixed_tiles': changed, 'fixed_pixels': sum(a != b for i in changed
                   for r, s in zip(base['tiles'][i], project['tiles'][i]) for a, b in zip(r, s)),
              'glyph_assets': edited_assets, 'opening': opening, 'bios_replacements': bios_hits,
              'palette_overrides': 0}
    save_json(GEN / 'font-infinity-apply.json', report)
    ledger_path = GEN / 'scaled-drawing-source.json'
    ledger = read_json(ledger_path)
    for name in ledger['files']:
        if name in ('native-scaled-fixed-6x6.4bpp', 'native-fix-fragments.u16', 'native-fix-visible.inc'):
            p = GEN / name
            ledger['files'][name] = {'bytes': p.stat().st_size, 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
    ledger['user_font_infinity'] = report
    save_json(ledger_path, ledger)
    print(json.dumps(report))


if __name__ == '__main__':
    main()
