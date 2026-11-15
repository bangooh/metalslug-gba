#!/usr/bin/env python3
"""Lossless M1 base-map compiler. Output is NOT yet linked to gameplay.

Source 12px cells exist only here. Output: 8bpp 8px tiles, uint32 ROM
tile IDs and RGB555 palettes. ROM IDs are not hardware VRAM indices.
No recorded route, colour quantization or runtime fragment composition.
"""
import json, re, hashlib
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
G = ROOT / 'generated'
OUT = G / 'm1-native-maps'

def body(text, name):
    return re.search(r'\b' + name + r'\[[^;=]*\]\s*=\s*\{(.*?)\};', text, re.S)[1]

def numbers(text):
    return [int(x.strip(), 0) for x in text.replace('{', '').replace('}', '').split(',') if x.strip()]

def main():
    OUT.mkdir(exist_ok=True)
    scene = (G / 'scene-programs.c').read_text()
    commands = [(numbers(m[1]), int(m[2]), int(m[3]), int(m[4])) for m in
                re.finditer(r'\{\{([^{}]*)\},(\d+),(\d+),(\d+)\}', body(scene, 'scene_commands'))]
    descriptors = [numbers(x) for x in re.findall(r'\{([^{}]*)\}', body(scene, 'scene_descriptors'))]
    pairs = np.array(numbers(body(scene, 'scene_palette_pairs'))).reshape(-1, 2)
    bindings = np.full(256, -1, dtype=np.int32)
    for slot, pal, flags in np.array(numbers(body((G / 'session-data.c').read_text(), 'session_palettes'))).reshape(-1, 3):
        bindings[slot] = pal
    selected = {}; seen = set(); at = descriptors[0][0]
    while at not in seen:
        seen.add(at); args, nxt, op, plane = commands[at]
        if op == 10:
            for slot, pal in pairs[args[0]:args[0]+args[1]]:
                bindings[slot & 255] = -1 if pal == 65535 else 1024 + pal
        if op == 0:
            if args[0] in selected:
                assert np.array_equal(selected[args[0]], bindings), 'multiple palette states need explicit variants'
            selected[args[0]] = bindings.copy()
        at = nxt
    palettes = np.array(numbers(body((G / 'drawing-data.c').read_text(), 'native_source_palettes')), dtype=np.uint16).reshape(-1, 16)
    raw = np.fromfile(G / 'native-scaled-background-12x12.4bpp', np.uint8).reshape(-1, 12, 6)
    pixels = np.empty((len(raw), 12, 12), np.uint8)
    pixels[:, :, ::2] = raw & 15; pixels[:, :, 1::2] = raw >> 4
    ledger = json.loads((G / 'tile-video-source.json').read_text())
    rows = [r for r in ledger['records'] if r['file'] == 'scene-programs.c']
    reports = []
    for image_id, binding in sorted(selected.items()):
        row = rows[image_id]; w, h = row['width'] * 12, row['height'] * 12
        cells = np.array(numbers(body(scene, row['cells'])), dtype=np.uint32).reshape(row['width'], row['height'], 2)
        # -1 means transparent; opaque RGB555 black remains a real colour.
        canvas = np.full(((h+7)//8*8, (w+7)//8*8), -1, np.int32)
        for x in range(row['width']):
            for y in range(row['height']):
                tile, attr = map(int, cells[x, y]); idx = pixels[tile + ((attr >> 4) & 15) * 65536]
                idx = idx[::-1 if attr & 2 else 1, ::-1 if attr & 1 else 1]
                pal = int(binding[attr >> 8])
                if np.any(idx):
                    assert pal >= 0, ('unbound palette', image_id, attr >> 8)
                    canvas[y*12:y*12+12, x*12:x*12+12] = np.where(idx != 0, palettes[pal, idx], -1)
        colors = np.unique(canvas[canvas >= 0]); assert len(colors) <= 255
        indexed = np.where(canvas < 0, 0, np.searchsorted(colors, canvas)+1).astype(np.uint8)
        tile_images = indexed.reshape(indexed.shape[0]//8, 8, indexed.shape[1]//8, 8).transpose(0, 2, 1, 3)
        tiles = [bytes(64)]; lookup = {tiles[0]: 0}; ids = []
        for tile in tile_images.reshape(-1, 8, 8):
            key = tile.tobytes()
            if key not in lookup: lookup[key] = len(tiles); tiles.append(key)
            ids.append(lookup[key])
        ids = np.array(ids, dtype='<u4').reshape(tile_images.shape[:2])
        decoded = np.frombuffer(b''.join(tiles), np.uint8).reshape(-1, 8, 8)[ids].transpose(0, 2, 1, 3).reshape(indexed.shape)
        lut = np.concatenate(([-1], colors.astype(np.int32)))
        assert np.array_equal(lut[decoded], canvas), 'lossless roundtrip failed'
        palette = np.zeros(256, dtype='<u2'); palette[1:len(colors)+1] = colors
        stem = f'image-{image_id:02d}'
        for suffix, data in [('tiles.8bpp', b''.join(tiles)), ('map.u32', ids.tobytes()), ('palette.rgb555', palette.tobytes())]:
            (OUT / f'{stem}.{suffix}').write_bytes(data)
        # Conservative per-image viewport demand, not simultaneous-plane proof.
        peak = 0
        for y in range(max(1, ids.shape[0]-20)):
            for x in range(max(1, ids.shape[1]-30)):
                peak = max(peak, len(np.unique(ids[y:y+21, x:x+31])))
        reports.append(dict(image=image_id,width=w,height=h,map_width=ids.shape[1],map_height=ids.shape[0],colors=len(colors),tiles=len(tiles),max_viewport_tiles=peak,pixel_mismatches=0))
    report = dict(status='offline base maps only; not gameplay integrated',format='8x8 8bpp; uint32 ROM map IDs; RGB555',quantized=False,images=reports,source_scene_sha256=hashlib.sha256(scene.encode()).hexdigest(),pending=['shared visible-region palette','VRAM streaming','column deformation','destruction states','OBJ/HUD integration','Neo Geo scene comparison','GBA performance'])
    (OUT / 'manifest.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report))

if __name__ == '__main__': main()
