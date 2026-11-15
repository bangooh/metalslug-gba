#!/usr/bin/env python3
"""Repair only the three pistol-ammunition FIX cells, in indexed pixels."""
import json
import struct
from pathlib import Path
from compile_m1_native_maps import body, numbers

ROOT = Path(__file__).resolve().parents[1]
CODES = range(0x3F0, 0x3F3)
MASK = (
    '.####....####.',
    '##..##..##..##',
    '##...####...##',
    '##..##..##..##',
    '.####....####.',
)
RAMP = (15, 15, 5, 3, 2)


def refine():
    # Preserve the 18x6 cell, x=1..14 ink bounds, palette E and 1px shadow.
    pixels = [[0] * 18 for _ in range(6)]
    for y, row in enumerate(MASK):
        for x, value in enumerate(row, 1):
            if value == '#':
                pixels[y + 1][x + 1] = 1
    for y, row in enumerate(MASK):
        for x, value in enumerate(row, 1):
            if value == '#':
                pixels[y][x] = RAMP[y]

    fixed_path = ROOT / 'generated/native-scaled-fixed-6x6.4bpp'
    fragments_path = ROOT / 'generated/native-fix-fragments.u16'
    fixed = bytearray(fixed_path.read_bytes())
    fragments = bytearray(fragments_path.read_bytes())
    patch_path = ROOT / 'art/hud-font-edit/patch.json'
    patch = json.loads(patch_path.read_text())
    lookup = {a['id']: a for a in patch['assets']}
    colors = numbers(body((ROOT / 'generated/drawing-data.c').read_text(),
                          'native_source_palettes'))[1449 * 16:1450 * 16]
    palette = ['#' + ''.join(f'{((color >> shift) & 31) * 255 // 31:02x}'
                            for shift in (0, 5, 10)) for color in colors]
    for index, code in enumerate(CODES):
        rows = [row[index * 6:index * 6 + 6] for row in pixels]
        hexrows = [''.join(format(v, 'x') for v in row) for row in rows]
        old = [''.join(format((fixed[code * 32 + y * 4 + x // 2]
                              >> (4 * (x % 2))) & 15, 'x')
                       for x in range(6)) for y in range(6)]
        asset_id = f'fixed:{code}'
        if asset_id not in lookup:
            entry = {'id': asset_id, 'label': f'Infinity / {index + 1}',
                     'group': 'HUD ammunition', 'original': old,
                     'before_current': old, 'target': {'kind': 'fixed', 'code': code},
                     'palette': palette}
            patch['assets'].append(entry)
        else:
            entry = lookup[asset_id]
        entry['palette'] = palette
        entry['rows'] = entry['after_padded'] = hexrows
        padded = [row + [0, 0] for row in rows] + [[0] * 8, [0] * 8]
        fixed[code * 32:(code + 1) * 32] = bytes(
            row[x] | (row[x + 1] << 4) for row in padded for x in range(0, 8, 2))
        words = [sum(rows[y * 2 + dy][x * 2 + dx] << (4 * (dy * 2 + dx))
                     for dy in range(2) for dx in range(2))
                 for y in range(3) for x in range(3)]
        fragments[code * 18:(code + 1) * 18] = struct.pack('<9H', *words)
    fixed_path.write_bytes(fixed)
    fragments_path.write_bytes(fragments)
    patch_path.write_text(json.dumps(patch, ensure_ascii=False, indent=1) + '\n')
    print('Updated FIX 0x3f0..0x3f2; existing visibility bits remain set.')


if __name__ == '__main__':
    refine()
