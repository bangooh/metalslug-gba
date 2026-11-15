"""Index existing sparse animation records by image row; never edit pixels."""
from pathlib import Path
import argparse
import bisect
import re


def tables(root):
    generated = root / 'generated'
    source = (generated / 'background-animation158.h').read_text()
    for mission in range(1, 7):
        body = re.search(r'auto_tiles_m%d\[\]=\{(.*?)\n\};' % mission, source, re.S).group(1)
        keys = [int(x) for x in re.findall(r'\{(\d+)u,', body)]
        assert keys == sorted(keys) and len(keys) < 65536
        metadata = (generated / ('m%d-assets.c' % mission)).read_text()
        body = re.search(r'm%d_images\[\]=\{(.*?)\};' % mission, metadata, re.S).group(1)
        images = [[int(n) for n in re.findall(r'-?\d+', row)] for row in re.findall(r'\{([^{}]*)\}', body)]
        for key in keys:
            image, cell = key >> 24, key & 0xffffff
            assert image < len(images) and cell < images[image][1] * images[image][2]
        offsets, rows = [], []
        for image, (_, width, height, *_) in enumerate(images):
            offsets.append(len(rows))
            rows.extend(bisect.bisect_left(keys, (image << 24) + y * width) for y in range(height + 1))
        assert len(rows) < 65536
        yield mission, keys, images, offsets, rows


def build(root, output=None):
    output = output or root / 'generated/background-animation-row-index.h'
    text = '/* Derived row bounds for background-animation158.h; no pixel changes. */\n'
    for mission, _, _, offsets, rows in tables(root):
        text += 'static const uint16_t auto_row_offsets_m%d[]={%s};\n' % (mission, ','.join(map(str, offsets)))
        text += 'static const uint16_t auto_rows_m%d[]={%s};\n' % (mission, ','.join(map(str, rows)))
    text += 'struct AutoRowSet {const uint16_t*rows,*images;};\n'
    text += 'static const struct AutoRowSet auto_row_sets[]={' + ','.join('{auto_rows_m%d,auto_row_offsets_m%d}' % (m, m) for m in range(1, 7)) + '};\n'
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.exists() or output.read_text() != text:
        output.write_text(text)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    build(args.root, args.output)
