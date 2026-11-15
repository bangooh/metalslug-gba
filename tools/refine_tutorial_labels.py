#!/usr/bin/env python3
"""Apply the C->R, D->L tutorial labels to the existing indexed assets."""
import json
import re
from pathlib import Path
from apply_hud_font_edit import decode, generate
from import_scaled_drawings import rows
from compile_m1_native_maps import body, numbers
from tutorial_button_labels import adapt

ROOT = Path(__file__).resolve().parents[1]
ART = {
    2427872: ('R', 0xB0, [
        '000000000000', '005111567000', '005111115700',
        '061000061100', '051000751500', '051555115700',
        '611005115000', '515000511700', '516000051500',
        '117000071100', '116000005110', '000000000000']),
    2427882: ('L', 0xB2, [
        '000000000000', '005110000000', '005110000000',
        '061000000000', '051000000000', '051000000000',
        '615000000000', '516000000000', '516000000000',
        '117000000000', '111111167000', '000000000000']),
}


def main():
    g = ROOT / 'generated'
    source = (g / 'scaled-drawing-data.c').read_text()
    maps, chunks = rows(source, 'native_scaled_maps'), rows(source, 'native_scaled_chunks')
    blob = (g / 'native-scaled-objects.4bpp').read_bytes()
    audit = {v['source']: v for v in json.loads((g / 'scaled-drawing-source.json').read_text())['map_audit']}
    palette_data = numbers(body((g / 'drawing-data.c').read_text(), 'native_source_palettes'))
    patch_path = ROOT / 'art/hud-font-edit/patch.json'
    patch = json.loads(patch_path.read_text())
    ids = set()
    for src, (letter, bank, new) in ART.items():
        m = audit[src]
        asset_id = f'tutorial-button:{m["id"]}'
        ids.add(asset_id)
        existing = next((a for a in patch['assets'] if a['id'] == asset_id), None)
        if existing is None:
            l, t, r, b = m['scaled_bounds']
            current = decode(blob, chunks, maps[m['id'] * 9], l, t, (b - t, r - l))
            old = [''.join(format(int(v), 'x') for v in row) for row in current]
            colors = palette_data[bank * 16:(bank + 1) * 16]
            palette = ['#' + ''.join(f'{((c >> shift) & 31) * 255 // 31:02x}' for shift in (0, 5, 10)) for c in colors]
            existing = {'id': asset_id, 'label': f'Tutorial / {letter}',
                        'group': 'Tutorial controls', 'original': old, 'before_current': old,
                        'target': {'kind': 'scaled-map', 'source': src, 'map_id': m['id']}, 'palette': palette}
            patch['assets'].append(existing)
        existing['rows'] = existing['after_padded'] = new
    patch_path.write_text(json.dumps(patch, ensure_ascii=False, indent=1) + '\n')
    generate(only_ids=ids)

    path = g / 'frontend-data.c'
    text = path.read_text()
    pattern = re.compile(r'(static const struct FrontendText text(\d+)\[\]=\{)(.*?)(\};)', re.S)
    changes = []
    def replace(match):
        values = [list(map(int, v.split(','))) for v in re.findall(r'\{([^{}]+)\}', match[3])]
        for _, index, old, new in adapt([values]):
            changes.append({'stream': int(match[2]), 'index': index, 'before': old, 'after': new})
        return match[1] + ','.join('{' + ','.join(map(str, v)) + '}' for v in values) + match[4]
    path.write_text(pattern.sub(replace, text))
    print(json.dumps({'tutorial_text_changes': changes}))


if __name__ == '__main__':
    main()
