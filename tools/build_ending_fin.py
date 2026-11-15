#!/usr/bin/env python3
"""Preserve the original FIN's one-pixel serif strokes at native75% size.

The source is a48x16 indexed map; point sampling loses the complete F/I
vertical stems. Project every source foreground pixel into its3/4 cell,
then retain the black offset shadow wherever no foreground lands. This is
a single caption override, not a change to approved HUD or sprite scaling.
"""
import hashlib
import json
import re
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw
from import_scaled_drawings import rows, scale, unpack_tiles

ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / 'generated'
ART = ROOT / 'art/ending-fin'
OUT = ROOT / 'build/ending-font'
SOURCE = 0x25390e
MAP = 5516


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    ledger = json.loads((GEN / 'drawing-source.json').read_text())
    source_bytes = (GEN / 'native-sprites.4bpp').read_bytes()
    expected = ledger['assets']['native-sprites.4bpp']
    assert digest(source_bytes) == (expected['sha256'] if isinstance(expected, dict) else expected)
    assert ledger['maps'][MAP]['source'] == SOURCE
    data = (GEN / 'drawing-data.c').read_text()
    maps, pieces = rows(data, 'native_draw_maps'), rows(data, 'native_draw_pieces')
    tiles = list(map(int, re.search(r'const uint16_t native_draw_tiles\[\]=\{([^}]+)\};', data)[1].split(',')))
    palette_text = re.search(r'const uint16_t native_source_palettes[^=]*=\{(.*?)\};', data, re.S)[1]
    palette = [list(map(int, row.split(','))) for row in re.findall(r'\{([^{}]+)\}', palette_text)][0x136]
    assert palette[1] == 0 and palette[15] == 32767
    raw = unpack_tiles(source_bytes)
    sprites = raw.reshape(-1, 2, 2, 8, 8).transpose(0, 1, 3, 2, 4).reshape(-1, 16, 16)
    first, count, attributes = maps[MAP]
    selected = pieces[first:first+count]
    assert len(selected) == 1 and selected[0][1:] == (-24, 32, 3, 1)
    source = np.zeros((16, 48), np.uint8)
    start = selected[0][0]
    for column in range(3):
        source[:, column*16:(column+1)*16] = sprites[((attributes >> 4) << 16) | tiles[start+column]]
    assert set(np.unique(source)) == {0, 1, 15}
    reduced = scale(source)
    native = np.zeros((12, 36), np.uint8)
    # Black shadow first; source foreground wins each downsampled cell.
    for value in (1, 15):
        y, x = np.where(source == value)
        native[y*3//4, x*3//4] = value
    source_rows = [''.join(format(int(v), 'x') for v in row) for row in source]
    native_rows = [''.join(format(int(v), 'x') for v in row) for row in native]
    def counts(pixels, width):
        return [int(np.count_nonzero(pixels[:, i*width:(i+1)*width] == 15)) for i in range(3)]
    report = dict(source=SOURCE, source_hex='0x25390e', map_id=MAP,
                  character='FIN', source_palette=0x136,
                  palette_indices=dict(transparent=0, black_shadow=1, white_foreground=15),
                  source_bounds=[-24, -32, 24, -16], native_bounds=[-18, -24, 18, -12],
                  method='Forward-project original white serif strokes at3/4; black offset shadow underneath; no new colours or runtime drawing.',
                  source_prom_sha256=ledger['prom_sha256'], source_sprite_sha256=digest(source_bytes),
                  source_map_sha256=ledger['maps'][MAP]['sha256'],
                  source_pixels_sha256=digest(source.tobytes()),
                  old_native_pixels_sha256=digest(reduced.tobytes()),
                  native_pixels_sha256=digest(native.tobytes()),
                  white_pixel_counts=dict(source=counts(source, 16), old_native=counts(reduced, 12), native=counts(native, 12)),
                  source_rows=source_rows, rows=native_rows)
    ART.mkdir(parents=True, exist_ok=True)
    OUT.mkdir(parents=True, exist_ok=True)
    (ART / 'fin.json').write_text(json.dumps(report, indent=2) + '\n')
    preview = Image.new('RGB', (520, 405), (29, 42, 51))
    labels = [('Original48x16', source), ('Previous36x12', reduced), ('Repaired36x12', native)]
    draw = ImageDraw.Draw(preview)
    for i, (label, pixels) in enumerate(labels):
        y = 8 + i*132
        draw.text((10, y), label, fill='white')
        rgb = np.zeros((*pixels.shape, 3), np.uint8)
        rgb[:] = (29, 42, 51)
        rgb[pixels == 1] = 0
        rgb[pixels == 15] = 255
        image = Image.fromarray(rgb)
        image.save(OUT / (('source', 'old', 'native')[i] + '.png'))
        preview.paste(image.resize((pixels.shape[1]*6, pixels.shape[0]*6), Image.Resampling.NEAREST), (10, y+22))
        preview.paste(image, (350, y+25))
    preview.save(OUT / 'fin-source-native-comparison.png')
    print(json.dumps({k: report[k] for k in ('source_hex', 'map_id', 'source_bounds', 'native_bounds', 'white_pixel_counts', 'native_pixels_sha256')}))


if __name__ == '__main__':
    main()
