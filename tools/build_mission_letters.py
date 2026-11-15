"""Mission banner letters on one construction (user review 2026-11-12).

Usage: python3 tools/build_mission_letters.py  -> updates art/hud-font-edit/patch.json

Every glyph is drawn as an ink mask (15 rows) and then dressed the same way
as the hand-edited M: a white outline ring, a dark border ring, a highlight on
the top and left edge of every stroke, and the M's row colour bands.
 - vertical strokes are 4 px of ink everywhere (outline excluded), bars 3 rows;
 - N is point-symmetric (the mask equals itself turned 180 degrees);
 - S, O, A follow the rounder shapes of the original Neo Geo letters.
M stays exactly as the user drew it. A and R are separate maps; they sit on the
same rows as the others (2 columns in, which the spacing pass absorbs).
"""
import json
from pathlib import Path
import numpy as np

R = Path(__file__).resolve().parents[1]
PATCH = R / 'art/hud-font-edit/patch.json'

BAND = ['99', 'de', 'dd', 'dd', 'dc', 'cc', 'cb', 'bc', 'bb', 'bb', 'aa', 'aa', 'a9', '9a', '99']

G = {
 'I': ["########", "########", "..####..", "..####..", "..####..", "..####..", "..####..",
       "..####..", "..####..", "..####..", "..####..", "..####..", "########", "########",
       "########"],
 'O': ["...#########...", ".#############.", "###############", "#####.....#####", "####.......####",
       "####.......####", "####.......####", "####.......####", "####.......####", "####.......####",
       "####.......####", "#####.....#####", "###############", ".#############.", "...#########..."],
 'C': ["...#########...", ".#############.", "###############", "#####.....#####", "####.......####",
       "####...........", "####...........", "####...........", "####...........", "####...........",
       "####.......####", "#####.....#####", "###############", ".#############.", "...#########..."],
 'S': ["...#########...", ".#############.", "###############", "#####.....#####", "####.......####",
       "#####..........", ".###########...", "..###########..", "...###########.", "..........#####",
       "####.......####", "#####.....#####", "###############", ".#############.", "...#########..."],
 'P': ["############..", "#############.", "##############", "####......####", "####......####",
       "####......####", "##############", "#############.", "############..", "####..........",
       "####..........", "####..........", "####..........", "####..........", "####.........."],
 'R': ["############..", "#############.", "##############", "####......####", "####......####",
       "####......####", "##############", "#############.", "############..", "####...####...",
       "####....####..", "####....####..", "####.....####.", "####......####", "####......####"],
 'L': ["####.........."] * 12 + ["##############"] * 3,
 'E': ["##############"] * 3 + ["####.........."] * 3 + ["###########..."] * 3 +
      ["####.........."] * 3 + ["##############"] * 3,
 'T': ["##############"] * 3 + [".....####....."] * 12,
 '!': ["####"] * 10 + ["...."] * 2 + ["####"] * 3,
 '1': ["...####.", "..#####.", ".######."] + ["...####."] * 9 + ["########"] * 3,
 '2': ["...#########...", ".#############.", "###############", "####.......####", "...........####",
       ".........######", ".......#######.", "....#########..", "..########.....", ".#######.......",
       "######.........", "####...........", "###############", "###############", "###############"],
 '3': ["..###########.", ".#############", "##############", "####......####", "..........####",
       ".....#########", ".....########.", ".....#########", "..........####", "..........####",
       "..........####", "####......####", "##############", ".#############", "..###########."],
 '4': ["........#######", ".......########", "......#########", ".....#####.####", "....#####..####",
       "...#####...####", "..#####....####", ".#####.....####", "###############", "###############",
       "###############", "...........####", "...........####", "...........####", "...........####"],
 '5': ["##############"] * 3 + ["####.........."] * 2 +
      ["############..", "#############.", "##############"] + ["..........####"] * 3 +
      ["####......####", "##############", ".#############", "..###########."],
 '6': ["...#########...", ".#############.", "###############", "####.......####", "####...........",
       "####...........", "#############..", "##############.", "###############", "####.......####",
       "####.......####", "####.......####", "###############", ".#############.", "...#########..."],
}


def mask_n(w=14, h=15, stem=4):
    m = np.zeros((h, w), bool)
    m[:, :stem] = m[:, w - stem:] = True
    span = w - stem
    for r in range(h):
        x = int(np.floor(r * span / (h - 1) + 0.5))
        m[r, x:x + stem] = True
    return m | m[::-1, ::-1]                     # point symmetric


def mask_a(w=14, h=15, leg=4):
    m = np.zeros((h, w), bool)
    top = (w - leg) // 2
    for r in range(h):
        x = int(np.floor(top - r * top / (h - 1) + 0.5))
        m[r, x:x + leg] = True
        m[r, w - x - leg:w - x] = True
    m[9:11, :] |= np.logical_or.accumulate(m[9:11], 1) & np.logical_or.accumulate(m[9:11, ::-1], 1)[:, ::-1]
    return m


def to_mask(rows):
    return np.array([[c == '#' for c in r] for r in rows])


def dress(ink):
    """ink mask -> glyph rows in the M scheme (outline, border, highlights, bands)."""
    h, w = ink.shape
    pad = np.zeros((h + 4, w + 4), bool)
    pad[2:-2, 2:-2] = ink
    def grow(m):
        out = m.copy()
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                out |= np.roll(np.roll(m, dy, 0), dx, 1)
        return out
    line = grow(pad) & ~pad
    border = grow(pad | line) & ~(pad | line)
    g = np.zeros(pad.shape, int)
    g[border] = 1
    g[line] = 15
    for y, x in zip(*np.nonzero(pad)):
        r = y - 2
        if not pad[y - 1, x] or not pad[y, x - 1]:
            g[y, x] = 9
        else:
            g[y, x] = int(BAND[r][x % 2], 16)
    return g


def place(g, dy, dx, shape=(24, 24)):
    out = np.zeros(shape, int)
    out[dy:dy + g.shape[0], dx:dx + g.shape[1]] = g
    return [''.join('%x' % v for v in row) for row in out]


def build(shapes=None):
    masks = {k: to_mask(v) for k, v in G.items()}
    masks['N'] = mask_n()
    masks['A'] = mask_a()
    for k, m in masks.items():
        assert m.shape[0] == 15, k
    assert np.array_equal(masks['N'], masks['N'][::-1, ::-1])
    glyphs = {}
    for k, m in masks.items():
        g = dress(m)
        shape = (shapes or {}).get(k, (24, 24))
        glyphs[k] = place(g, 0, 2, shape) if k in 'AR' else place(g, 0, 0, shape)
    return glyphs


SOURCES = {'A': 2441928, 'R': 2441944}

if __name__ == '__main__':
    import sys
    sys.path.insert(0, str(R / 'tools'))
    cur = json.load(open(sys.argv[1])) if len(sys.argv) > 1 else None
    p0 = json.loads(PATCH.read_text())
    shapes = {}
    for a in p0['assets']:
        if a['group'] == 'MISSION':
            shapes[a['label']] = (len(a['before_current']), len(a['before_current'][0]))
    for ch, v in (cur or {}).items():
        shapes.setdefault(ch, (len(v['rows']), len(v['rows'][0])))
    glyphs = build(shapes)
    p = json.loads(PATCH.read_text())
    M = [a for a in p['assets'] if a['id'] == 'mission:M'][0]
    for ch, rows in glyphs.items():
        kind = 'map' if ch in SOURCES else 'mission'
        aid = ('mission-map:' + ch) if kind == 'map' else 'mission:' + ch
        old = [a for a in p['assets'] if a['id'] == aid]
        before = old[0]['before_current'] if old else cur[ch]['rows']
        target = {'kind': 'map', 'source': SOURCES[ch]} if kind == 'map' else {'kind': 'mission', 'character': ch}
        a = dict(id=aid, label=ch, group='MISSION', target=target, palette=M['palette'],
                 before_current=before, after_padded=rows, original=before, rows=rows)
        p['assets'] = [x for x in p['assets'] if x['id'] != aid] + [a]
    PATCH.write_text(json.dumps(p, ensure_ascii=False, indent=1))
    print('mission glyphs written:', ''.join(sorted(glyphs)))
