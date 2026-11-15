"""Compare every editor asset with decoded final GBA data, without palettes."""
import argparse
import hashlib
import json
from collections import Counter
from pathlib import Path
import numpy as np
from import_scaled_drawings import rows, unpack_tiles
from approved_font_overrides import ApprovedFonts
from reviewed_assets import pixels

R = Path(__file__).resolve().parents[1]
G = R / 'generated'


def main():
    p = argparse.ArgumentParser()
    p.add_argument('project', type=Path)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    project = json.loads(args.project.read_text())
    source = json.loads((G / 'scaled-drawing-source.json').read_text())
    text = (G / 'scaled-drawing-data.c').read_text()
    maps = rows(text, 'native_scaled_maps')
    chunks = rows(text, 'native_scaled_chunks')
    blob = (G / 'native-scaled-objects.4bpp').read_bytes()
    audit = {m['source']: m for m in source['map_audit']}
    fonts = ApprovedFonts()
    missions = {ch: src for src, ch in fonts.maps.items()}
    fixed = unpack_tiles((G / 'native-scaled-fixed-6x6.4bpp').read_bytes())
    menu = unpack_tiles((G / 'native-ui-font.4bpp').read_bytes())
    player = (G / 'player.tiles').read_bytes()
    fragments = np.frombuffer((G / 'native-fix-fragments.u16').read_bytes(), dtype='<u2').reshape(4096, 9)
    counts = Counter()
    for a in project['assets']:
        t = a['target']
        kind = t['kind']
        expected = pixels(a['rows'])
        if kind == 'fixed':
            actual = fixed[t['code'], :6, :6]
            decoded = np.zeros((6, 6), np.uint8)
            for i, word in enumerate(fragments[t['code']]):
                yy, xx = divmod(i, 3)
                decoded[yy*2:yy*2+2, xx*2:xx*2+2] = np.array([(int(word) >> (4*j)) & 15 for j in range(4)]).reshape(2, 2)
            assert np.array_equal(decoded, expected), ('FIX fragments', a['id'])
        elif kind == 'menu':
            actual = menu[t['code']]
        elif kind == 'approved-player':
            i = t['frame'] * 2048
            actual = unpack_tiles(player[i:i+2048]).reshape(8, 8, 8, 8).transpose(0, 2, 1, 3).reshape(64, 64)
        else:
            src = missions[t['character']] if kind == 'mission' else fonts.ending['source'] if kind == 'fin' else t['source']
            m = audit[src]
            left, top, right, bottom = m['scaled_bounds']
            actual = np.zeros((bottom-top, right-left), np.uint8)
            first, count = maps[m['id'] * 9]
            for offset, x, y, w, h, shape, size in chunks[first:first+count]:
                tile = unpack_tiles(blob[offset:offset+w*h//2]).reshape(h//8, w//8, 8, 8).transpose(0, 2, 1, 3).reshape(h, w)
                x -= left
                y -= top
                ch, cw = min(h, actual.shape[0]-y), min(w, actual.shape[1]-x)
                dst = actual[y:y+ch, x:x+cw]
                np.copyto(dst, tile[:ch, :cw], where=tile[:ch, :cw] != 0)
            assert hashlib.sha256(actual.tobytes()).hexdigest() == m['pixel_sha256']
            if kind == 'mission':
                h, w = expected.shape
                padded = np.zeros_like(actual)
                padded[:h, :w] = expected
                expected = padded
        assert np.array_equal(actual, expected), a['id']
        counts[kind] += 1
    report = dict(project_sha256=hashlib.sha256(args.project.read_bytes()).hexdigest(),
                  verified_assets=sum(counts.values()), by_kind=dict(counts),
                  exact_indexed_pixel_match=True, fixed_fragments_verified=True,
                  runtime_or_visual_approval=False)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
