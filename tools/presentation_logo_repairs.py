"""Offline presentation-only repairs; no runtime filtering or extra frame work."""
import json
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]

def neogeo_palettes(source):
    """Keep settled-logo edge contrast continuous through its captured tint."""
    result = source.copy()
    for index, target in ((1, 30), (241, 31), (242, 30), (243, 26), (244, 4)):
        end = int(source[-1, index])
        channel = end & 31
        assert channel > 0 and channel == (end >> 5 & 31) == (end >> 10 & 31)
        # The rotating logo reuses these palette slots for different surfaces.
        # Contrast remapping is only valid after the logo settles (frame 56).
        for frame in range(56, len(source)):
            value = int(source[frame, index])
            channels = [(value >> shift & 31) for shift in (0, 5, 10)]
            # The settled logo briefly takes on a blue tint while SNK appears.
            # Skipping those colored entries restored the old blurry edge
            # levels for five frames. Apply the same gain to each channel so
            # the tint remains animated without switching contrast recipes.
            adjusted = [min(31, (v * target + channel // 2) // channel)
                        for v in channels]
            result[frame, index] = sum(v << shift for v, shift in
                                       zip(adjusted, (0, 5, 10)))
    return result

def nazca_palette(palette):
    """Only the final NAZCA black-on-white palette; keep colored intro phases."""
    out = list(palette)
    assert out[5] in (13 * 1057, 5 * 1057)
    assert out[6] in (24 * 1057, 29 * 1057)
    out[5], out[6] = 5 * 1057, 29 * 1057
    return out

def restore_opening_fonts(atlas, cells, maps, mapids):
    fonts = json.loads((ROOT / 'art/approved-fonts/fonts.json').read_text())['fonts']
    specs = {
        mapids[0x2519ea]: ('subtitle_proportional_14', 'SUPER VEHICLE-001', (80, 5, 210, 22), (82, 6), (228, 80)),
        mapids[0x251ac6]: ('trademark_9', 'TM', (198, 110, 216, 122), (199, 112), (24, 134)),
    }
    atlas = bytearray(atlas)
    newcells, newmaps = [], []

    def tile(a):
        index = len(atlas) // 72
        atlas.extend((a[:, ::2] | a[:, 1::2] << 4).tobytes())
        return index

    for mid, (begin, count) in enumerate(maps):
        start = len(newcells)
        if mid not in specs:
            newcells.extend(cells[begin:begin + count])
        else:
            name, text, (x0, y0, x1, y1), (gx, gy), (dx, dy) = specs[mid]
            for x, y, tid in cells[begin:begin + count]:
                raw = np.frombuffer(atlas[tid*72:(tid+1)*72], np.uint8).reshape(12, 6)
                a = np.empty((12, 12), np.uint8)
                a[:, ::2], a[:, 1::2] = raw & 15, raw >> 4
                yy, xx = np.indices(a.shape)
                mask = (xx+x+dx >= x0) & (xx+x+dx < x1) & (yy+y+dy >= y0) & (yy+y+dy < y1)
                if np.any(a[mask]):
                    a[mask] = 0
                    if not a.any():
                        continue
                    tid = tile(a)
                newcells.append((x, y, tid))
            font = fonts[name]
            width = sum(font['glyphs'][ch]['advance'] for ch in text)
            pixels = np.zeros((font['height'], width), np.uint8)
            cursor = 0
            for ch in text:
                glyph = font['glyphs'][ch]
                a = np.array([[int(v, 16) for v in row] for row in glyph['rows']], np.uint8)
                dst = pixels[:, cursor:cursor+glyph['width']]
                np.copyto(dst, a, where=a != 0)
                cursor += glyph['advance']
            assert gx >= x0 and gy >= y0 and gx+width <= x1 and gy+font['height'] <= y1
            for y in range(0, pixels.shape[0], 12):
                for x in range(0, pixels.shape[1], 12):
                    a = np.zeros((12, 12), np.uint8)
                    part = pixels[y:y+12, x:x+12]
                    a[:part.shape[0], :part.shape[1]] = part
                    if a.any():
                        newcells.append((gx-dx+x, gy-dy+y, tile(a)))
        newmaps.append((start, len(newcells)-start))
    # Drop superseded cells' tiles and share identical glyph tiles. The opening
    # borrows a fixed video arena, so retaining obsolete tiles is not harmless.
    compact, lookup, result = bytearray(), {}, []
    for x, y, tid in newcells:
        raw = bytes(atlas[tid*72:(tid+1)*72])
        if raw not in lookup:
            lookup[raw] = len(compact) // 72
            compact.extend(raw)
        result.append((x, y, lookup[raw]))
    return compact, result, newmaps
