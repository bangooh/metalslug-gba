#!/usr/bin/env python3
"""Port-original HUD glyphs (own design, not traced from any source font).

* Small text font used by PUSH START / INSERT COIN / CONTINUE / GAME OVER /
  PLEASE WAIT / TIME UP: 6x12 cells (two stacked 6x6 fixed tiles).
  Ink is 5x10 with 1 px vertical stems and single row bars, a 1 px drop shadow
  (index 1) to the right/below, and one shared row gradient.
* Mission timer digits: 12x12 cells (2x2 fixed tiles). Ink 9x10 with 3 px
  stems and 2 row bars, full 1 px outline (index 1), gradient 8/5/2.

Writes/updates `fixed` entries in art/hud-font-edit/patch.json. Existing
entries keep their `before_current`; new ones take the current tile.
Usage: python3 tools/build_hud_glyphs.py [--preview out.png]
"""
import argparse, json, sys
from pathlib import Path
import numpy as np
R = Path(__file__).resolve().parents[1]
G = R / 'generated'

LETTERS = json.loads((R / 'art/ui-font-cleanup/small-masks.json').read_text())
LETTER_RAMP = [15,15,15,8,8,7,7,6,6,5]

DIGITS = {
 0: [".#######.","#########","###...###","###...###","###...###","###...###","###...###","###...###","#########",".#######."],
 1: ["...###...","..####...",".#####...","...###...","...###...","...###...","...###...","...###...","#########","#########"],
 2: [".#######.","#########","......###","......###",".########","########.","###......","###......","#########","#########"],
 3: [".#######.","#########","......###","......###","..#######","..#######","......###","......###","#########",".#######."],
 4: ["###...###","###...###","###...###","###...###","#########","#########","......###","......###","......###","......###"],
 5: ["#########","#########","###......","###......","########.","#########","......###","......###","#########","########."],
 6: [".########","#########","###......","###......","########.","#########","###...###","###...###","#########",".#######."],
 7: ["#########","#########","......###","......###",".....###.","....###..","...###...","...###...","...###...","...###..."],
 8: [".#######.","#########","###...###","###...###",".#######.",".#######.","###...###","###...###","#########",".#######."],
 9: [".#######.","#########","###...###","###...###","#########",".########","......###","......###","#########","########."],
}
DIGIT_RAMP = [8,8,8,5,5,5,5,2,2,2]

def mask(rows): return np.array([[c == '#' for c in r] for r in rows], bool)

def letter_cell(rows):
    m = mask(rows); assert m.shape == (10, 5), rows
    out = np.zeros((12, 6), np.uint8)
    for y in range(10): out[y, :5][m[y]] = LETTER_RAMP[y]
    ink = np.zeros((12, 6), bool); ink[:10, :5] = m
    sh = np.zeros_like(ink)
    sh[:, 1:] |= ink[:, :-1]; sh[1:, :] |= ink[:-1, :]; sh[1:, 1:] |= ink[:-1, :-1]
    out[sh & ~ink] = 1
    return out

def digit_cell(rows):
    m = mask(rows); assert m.shape == (10, 9), rows
    out = np.zeros((12, 12), np.uint8); ink = np.zeros((12, 12), bool)
    ink[1:11, 1:10] = m
    for y in range(10): out[1 + y, 1:10][m[y]] = DIGIT_RAMP[y]
    ring = np.zeros_like(ink)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            ring |= np.roll(np.roll(ink, dy, 0), dx, 1)
    out[ring & ~ink] = 1
    return out

def letter_codes(ch):
    c = ord(ch); a = 0x400 + ((c & 0xF0) << 1) + (c & 15); return [a, a + 16]

TIME_DIGITS = [25522,25520,25502,25500,25498,25496,25494,25492,25490,25488]
def digit_codes(d):
    c = TIME_DIGITS[d] & 0xFFF; return [c, c + 1, c + 16, c + 17]

def tiles():
    """code -> 6x6 array"""
    t = {}
    for ch, rows in LETTERS.items():
        cell = letter_cell(rows); top, bot = letter_codes(ch)
        t[top] = cell[:6]; t[bot] = cell[6:]
    for d, rows in DIGITS.items():
        cell = digit_cell(rows); a, b, c, e = digit_codes(d)
        t[a] = cell[:6, :6]; t[b] = cell[:6, 6:]; t[c] = cell[6:, :6]; t[e] = cell[6:, 6:]
    return t

def current_tile(code):
    b = np.frombuffer((G / 'native-scaled-fixed-6x6.4bpp').read_bytes()[code*32:code*32+32], np.uint8).reshape(8, 4)
    t = np.zeros((8, 8), np.uint8); t[:, 0::2] = b & 15; t[:, 1::2] = b >> 4
    return t[:6, :6]

def hexrows(a): return [''.join('%x' % v for v in r) for r in a]

def preview(path, t):
    from PIL import Image
    pal = {0:(40,60,90),1:(10,10,10),2:(160,60,0),5:(230,140,20),8:(255,230,80),
           6:(150,150,160),7:(190,190,200),15:(255,255,255)}
    text = ["PUSH START","INSERT COIN","CONTINUE?","GAME OVER","PLEASE WAIT","TIME UP",
            "ABCDEFGHIJKLM","NOPQRSTUVWXYZ"]
    W = 6 * 14 + 4; H = 12 * len(text) + 16 + 16
    img = np.zeros((H, W, 3), np.uint8); img[:] = pal[0]
    for li, s in enumerate(text):
        for i, ch in enumerate(s):
            if ch == ' ': continue
            a, b = letter_codes(ch); cell = np.vstack([t[a], t[b]])
            for y in range(12):
                for x in range(6):
                    if cell[y, x]: img[2 + li*12 + y, 2 + i*6 + x] = pal.get(int(cell[y, x]), (255, 0, 255))
    y0 = 2 + len(text) * 12 + 2
    for i, d in enumerate([0,1,2,3,4,5,6,7,8,9]):
        a, b, c, e = digit_codes(d); cell = np.vstack([np.hstack([t[a], t[b]]), np.hstack([t[c], t[e]])])
        if i >= 7: continue
        for y in range(12):
            for x in range(12):
                if cell[y, x]: img[y0 + y, 2 + i*12 + x] = pal.get(int(cell[y, x]), (255, 0, 255))
    y1 = y0 + 13
    for i, d in enumerate([7,8,9]):
        a, b, c, e = digit_codes(d); cell = np.vstack([np.hstack([t[a], t[b]]), np.hstack([t[c], t[e]])])
        for y in range(12):
            for x in range(12):
                if cell[y, x] and y1 + y < H: img[y1 + y, 2 + i*12 + x] = pal.get(int(cell[y, x]), (255, 0, 255))
    Image.fromarray(img).resize((W*6, H*6), Image.NEAREST).save(path)

def main():
    p = argparse.ArgumentParser(); p.add_argument('--preview'); p.add_argument('--no-write', action='store_true'); a = p.parse_args()
    t = tiles()
    if a.preview: preview(a.preview, t)
    if a.no_write: return
    path = R / 'art/hud-font-edit/patch.json'; d = json.loads(path.read_text())
    byid = {x['id']: x for x in d['assets']}
    template = next(x for x in d['assets'] if x['target']['kind'] == 'fixed')
    for code, arr in sorted(t.items()):
        aid = 'fixed:%d' % code; rows = hexrows(arr)
        if aid in byid:
            e = byid[aid]
        else:
            cur = hexrows(current_tile(code))
            e = {'id': aid, 'label': '%X' % code, 'group': 'HUD · port glyphs', 'rows': rows, 'original': cur,
                 'target': {'kind': 'fixed', 'code': code}, 'palette': template['palette'], 'before_current': cur}
            d['assets'].append(e); byid[aid] = e
        e['rows'] = rows; e['after_padded'] = rows; e['group'] = 'HUD · port glyphs'
    path.write_text(json.dumps(d, ensure_ascii=False, indent=1) + '\n')
    print('fixed glyph tiles', len(t))

if __name__ == '__main__': main()
