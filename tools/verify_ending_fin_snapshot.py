#!/usr/bin/env python3
"""Verify FIN from captured GBA OAM/VRAM/palette and actual console images.

This does not emulate gameplay or infer a caption from its screenshot. It
decodes the nominated, committed 4bpp OBJ entries and compares every indexed
pixel to the source-derived asset. Separately it checks that every opaque
pixel survives in the actual composite, and locates the unscaled source
caption in an actual Neo Geo frame. The snapshot must be commit-coherent.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SIZES = (((8, 8), (16, 16), (32, 32), (64, 64)),
         ((16, 8), (32, 8), (32, 16), (64, 32)),
         ((8, 16), (8, 32), (16, 32), (32, 64)))


def digest(data):
    return hashlib.sha256(data).hexdigest()


def rows(value):
    return np.array([[int(c, 16) for c in row] for row in value], dtype=np.uint8)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snapshot', type=Path, required=True)
    parser.add_argument('--screen', type=Path, required=True)
    parser.add_argument('--source-screen', type=Path, required=True)
    parser.add_argument('--objects', type=int, nargs='+', default=[0, 1])
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    spec = json.loads((ROOT / 'art/ending-fin/fin.json').read_text())
    expected, source = rows(spec['rows']), rows(spec['source_rows'])
    vram = (args.snapshot / 'vram.bin').read_bytes()
    oam_bytes = (args.snapshot / 'oam.bin').read_bytes()
    palette_bytes = (args.snapshot / 'pal.bin').read_bytes()
    oam = struct.unpack('<512H', oam_bytes)
    palette = struct.unpack('<512H', palette_bytes)
    dispcnt = struct.unpack_from('<H', (args.snapshot / 'io.bin').read_bytes())[0]
    assert dispcnt & 0x40, 'Expected GBA 1D OBJ tile addressing'
    parts = []
    for index in args.objects:
        a0, a1, a2, _ = oam[index * 4:index * 4 + 4]
        assert not (a0 & 0x3f00), 'FIN must be ordinary non-affine 4bpp OBJ'
        shape, size = a0 >> 14, a1 >> 14
        assert shape < 3
        width, height = SIZES[shape][size]
        x, y = a1 & 511, a0 & 255
        x -= 512 if x >= 256 else 0
        y -= 256 if y >= 160 else 0
        tile, bank = a2 & 1023, a2 >> 12
        pixels = np.zeros((height, width), dtype=np.uint8)
        for py in range(height):
            for px in range(width):
                position = 0x10000 + tile * 32 + (py // 8 * (width // 8) + px // 8) * 32 + py % 8 * 4 + px % 8 // 2
                value = (vram[position] >> (px % 2 * 4)) & 15
                if value:
                    colour = palette[256 + bank * 16 + value]
                    assert colour in (0, 32767), (index, value, colour)
                    pixels[py, px] = 1 if colour == 0 else 15
        if a1 & 0x1000:
            pixels = pixels[:, ::-1]
        if a1 & 0x2000:
            pixels = pixels[::-1, :]
        parts.append((x, y, pixels))
    x0, y0 = min(p[0] for p in parts), min(p[1] for p in parts)
    x1 = max(x + p.shape[1] for x, y, p in parts)
    y1 = max(y + p.shape[0] for x, y, p in parts)
    decoded = np.zeros((y1 - y0, x1 - x0), dtype=np.uint8)
    # Lower OAM index wins if two opaque pixels overlap.
    for x, y, pixels in reversed(parts):
        destination = decoded[y-y0:y-y0+pixels.shape[0], x-x0:x-x0+pixels.shape[1]]
        destination[pixels != 0] = pixels[pixels != 0]
    padded = np.zeros_like(decoded)
    h, w = expected.shape
    padded[:h, :w] = expected
    hardware_mismatches = int(np.count_nonzero(decoded != padded))
    assert hardware_mismatches == 0, hardware_mismatches
    actual_hash = digest(decoded[:h, :w].tobytes())
    assert actual_hash == spec['native_pixels_sha256']
    screen = np.array(Image.open(args.screen).convert('RGB'))
    crop = screen[y0:y0+h, x0:x0+w]
    white, black = expected == 15, expected == 1
    white_bad = int(np.count_nonzero(np.any(crop[white] != 255, axis=1)))
    black_bad = int(np.count_nonzero(np.any(crop[black] != 0, axis=1)))
    assert white_bad == black_bad == 0, (white_bad, black_bad)
    original = np.array(Image.open(args.source_screen).convert('RGB'))
    sh, sw = source.shape
    source_white, source_black = source == 15, source == 1
    matches = []
    for y in range(original.shape[0] - sh + 1):
        for x in range(original.shape[1] - sw + 1):
            candidate = original[y:y+sh, x:x+sw]
            if np.all(candidate[source_white] == 255) and np.all(candidate[source_black] == 0):
                matches.append((x, y))
    assert len(matches) == 1, matches
    sx, sy = matches[0]
    source_crop = original[sy:sy+sh, sx:sx+sw]
    report = dict(snapshot=str(args.snapshot), screen=str(args.screen),
                  source_screen=str(args.source_screen), oam_indices=args.objects,
                  gba_bounds=[x0, y0, x0+w, y0+h], source_bounds=[sx, sy, sx+sw, sy+sh],
                  canonical_pixel_sha256=actual_hash,
                  hardware_pixel_mismatches=hardware_mismatches,
                  white_pixels=int(np.count_nonzero(white)), white_composite_mismatches=white_bad,
                  shadow_pixels=int(np.count_nonzero(black)), shadow_composite_mismatches=black_bad,
                  source_white_pixels=int(np.count_nonzero(source_white)),
                  source_shadow_pixels=int(np.count_nonzero(source_black)),
                  inputs={str(path): digest(path.read_bytes()) for path in
                          [args.snapshot/'vram.bin', args.snapshot/'oam.bin', args.snapshot/'pal.bin',
                           args.screen, args.source_screen]},
                  scope='Read-only FIN caption at one committed frame; background appearance is not validated by this check.')
    (args.output / 'fin-hardware.json').write_text(json.dumps(report, indent=2) + '\n')
    preview = Image.new('RGB', (520, 360), (29, 42, 51))
    draw = ImageDraw.Draw(preview)
    for y, title, pixels in [(12, 'Actual Neo Geo: 48 x 16', source_crop),
                             (196, 'Actual GBA: 36 x 12; all repaired pixels retained', crop)]:
        draw.text((12, y), title, fill='white')
        image = Image.fromarray(pixels)
        preview.paste(image.resize((image.width * 8, image.height * 8), Image.Resampling.NEAREST), (12, y+24))
        preview.paste(image, (420, y+24))
    preview.save(args.output / 'fin-actual-source-gba.png')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
