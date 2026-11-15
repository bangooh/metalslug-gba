"""Apply the reviewed F only, aligning its ink to the existing mission grid."""
from pathlib import Path
import copy
import hashlib
import json
import sys

from apply_hud_font_edit import generate
from import_scaled_drawings import rows

ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / 'generated'


def references():
    text = (GEN / 'scaled-drawing-data.c').read_text()
    chunks = rows(text, 'native_scaled_chunks')
    pixels = (GEN / 'native-scaled-objects.4bpp').read_bytes()
    result = {}
    for name in ('native_scaled_maps', 'native_dense_maps'):
        for i, (first, count) in enumerate(rows(text, name)):
            digest = hashlib.sha256()
            for offset, x, y, w, h, shape, size in chunks[first:first+count]:
                digest.update(str((x, y, w, h, shape, size)).encode())
                digest.update(pixels[offset:offset+w*h//2])
            result[name, i] = digest.hexdigest()
    return result


def main():
    upload = json.loads(Path(sys.argv[1]).read_text())
    assert upload['base'] == 'MetalSlug-v171-graphics-fix1-ending-fonts'
    entry = copy.deepcopy(next(a for a in upload['assets'] if a['id'] == 'mission-map:F'))
    assert entry['target']['source'] == 0x2543a4
    assert not upload['palette_overrides']
    original = entry['rows']
    assert len(original) == 24 and all(len(r) == 24 for r in original)
    ink = [(x, y) for y, r in enumerate(original) for x, c in enumerate(r) if c != '0']
    dx, dy = min(x for x, y in ink), min(y for x, y in ink)
    aligned = [r[dx:]+'0'*dx for r in original[dy:]]+['0'*24]*dy
    assert sorted(c for r in original for c in r if c != '0') == sorted(c for r in aligned for c in r if c != '0')
    entry['rows'] = entry['after_padded'] = aligned
    entry['alignment'] = {'shift_gba_pixels': [-dx, -dy], 'ink_height': 19}
    before = references()
    fixed_paths = ['native-scaled-fixed-6x6.4bpp', 'native-fix-fragments.u16']
    fixed_hash = {n: hashlib.sha256((GEN/n).read_bytes()).hexdigest() for n in fixed_paths}
    patch_path = ROOT / 'art/hud-font-edit/patch.json'
    patch = json.loads(patch_path.read_text())
    patch['assets'] = [a for a in patch['assets'] if a['id'] != entry['id']] + [entry]
    patch_path.write_text(json.dumps(patch, ensure_ascii=False, indent=2)+'\n')
    generate(only_ids={entry['id']})
    after = references()
    changed = [key for key in before if before[key] != after[key]]
    assert all((name == 'native_scaled_maps' and index//9 == 5536) or
               (name == 'native_dense_maps' and index == 5536) for name, index in changed), changed
    assert fixed_hash == {n: hashlib.sha256((GEN/n).read_bytes()).hexdigest() for n in fixed_paths}
    report = {'applied': [entry['id']], 'deferred': [a['id'] for a in upload['assets'] if a['id'] != entry['id']],
              'shift_gba_pixels': [-dx, -dy], 'changed_map_references': changed,
              'unrelated_map_references_preserved': len(before)-len(changed), 'fixed_assets_unchanged': True,
              'uploaded_sha256': hashlib.sha256(Path(sys.argv[1]).read_bytes()).hexdigest()}
    out = ROOT/'validation-final-font-unified'
    out.mkdir(exist_ok=True)
    (out/'F-apply.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
