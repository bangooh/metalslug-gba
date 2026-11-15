#!/usr/bin/env python3
"""Exact 4bpp starfield for ending profile12; preserve every other profile.

Run after legacy pruning and before native-background packing. The previous
joint-profile quantizer assigned blue gradient tiles to profile12's black
banks. Only images48/49/50 use this profile. Bank11 becomes source slot22;
old bank11 pixels in companion images migrate losslessly to black bank0.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import numpy as np
from compile_m1_native_maps import body, numbers

R = Path(__file__).resolve().parents[1]
G = R / 'generated'
P = G / 'native-background'


def mapbytes(values):
    return np.stack([values & 255, values >> 8 & 255, values >> 16 & 255], -1).astype('uint8').tobytes()


def packed_size(tiles, maps):
    source = np.frombuffer(tiles, np.uint8).reshape(-1, 8, 4)
    block_bytes = 0
    for at in range(0, len(source), 32768):
        a = source[at:at + 32768]
        q = np.stack([a[:, :4, :2], a[:, :4, 2:], a[:, 4:, :2], a[:, 4:, 2:]], axis=1).reshape(-1, 8)
        block_bytes += len(np.unique(q, axis=0)) * 8
    pages = {maps[at:at + 24].ljust(24, b'\0') for at in range(0, len(maps), 24)}
    return block_bytes + len(source) * 8 + ((len(source) + 32767) // 32768) * 4 + len(pages) * 24 + ((len(maps) + 23) // 24) * 4 + len(source)


def repair(write=False, pack=False):
    manifest = json.loads((P / 'manifest.json').read_text())
    rows = manifest['rows']
    scene_regions = [i for i, row in enumerate(rows) if row['file'] == 'scene-programs.c']
    selected = [scene_regions[i] for i in (48, 49, 50)]
    assert [i for i, row in enumerate(rows) if 12 in row['profiles']] == selected
    assert all(rows[i]['profiles'] == [12] for i in selected)
    metadata = (G / 'native-bg-assets.c').read_text()
    regions = [numbers(row) for row in re.findall(r'\{([^{}]+)\}', body(metadata, 'native_bg_regions'))]
    variants = [numbers(row) for row in re.findall(r'\{([^{}]+)\}', body(metadata, 'native_bg_variants'))]
    raw_maps = np.fromfile(P / 'maps.u24', np.uint8).reshape(-1, 3)
    maps = raw_maps[:, 0].astype(np.uint32) | raw_maps[:, 1].astype(np.uint32) << 8 | raw_maps[:, 2].astype(np.uint32) << 16
    old_maps = maps.copy()
    old_tiles = (P / 'tiles.4bpp').read_bytes()
    old_banks = (P / 'tile-banks.u8').read_bytes()
    tiles = [old_tiles[i:i + 32] for i in range(0, len(old_tiles), 32)]
    banks = list(old_banks)
    lookup = {bytes([bank]) + pixels: i for i, (bank, pixels) in enumerate(zip(banks, tiles))}
    palettes = np.fromfile(P / 'palettes.rgb555', '<u2').reshape(14, 12, 16)
    old_palettes = palettes.copy()
    original = np.array(numbers(body((G / 'drawing-data.c').read_text(), 'native_source_palettes')), np.uint16).reshape(-1, 16)
    exact = original[1145].copy()
    exact[0] = 0
    assert not palettes[12, 0].any()
    assert not palettes[12, 11].any() or np.array_equal(palettes[12, 11], exact)

    def intern(pixels, bank):
        key = bytes([bank]) + pixels
        if key not in lookup:
            lookup[key] = len(tiles)
            tiles.append(pixels)
            banks.append(bank)
        return lookup[key]

    allowed = np.zeros(len(maps), bool)
    migrated = 0
    for rid in selected[:2]:
        region = regions[rid]
        assert region[5] == 1
        first, profile, pad = variants[region[1]]
        assert profile in (12, 65535)
        length = rows[rid]['map_width'] * rows[rid]['map_height']
        allowed[first:first + length] = True
        for at in range(first, first + length):
            old = int(maps[at])
            if banks[old] != 11:
                continue
            raw = np.frombuffer(tiles[old], np.uint8)
            # Both banks are exactly RGB0 in this profile. Canonicalizing
            # nontransparent nibbles preserves opacity while deduplicating.
            canonical = ((raw & 15 != 0).astype('u1') | ((raw >> 4 != 0).astype('u1') << 4)).tobytes()
            assert all(bool((a >> shift) & 15) == bool((z >> shift) & 15) for a, z in zip(tiles[old], canonical) for shift in (0, 4))
            maps[at] = intern(canonical, 0)
            migrated += 1

    rid = selected[2]
    scene = (G / 'scene-programs.c').read_text()
    cells = np.array(numbers(body(scene, 'map_cells50')), np.uint16).reshape(32, 15, 2)
    assert np.all(cells[:, :, 1] >> 8 == 22)
    raw = np.fromfile(G / 'native-scaled-background-12x12.4bpp', np.uint8).reshape(-1, 12, 6)
    pixels = np.empty((len(raw), 12, 12), np.uint8)
    pixels[:, :, ::2] = raw & 15
    pixels[:, :, 1::2] = raw >> 4
    canvas = np.zeros((180, 384), np.uint8)
    for x in range(32):
        for y in range(15):
            tile, attr = map(int, cells[x, y])
            tile |= ((attr >> 4) & 15) << 16
            canvas[y * 12:y * 12 + 12, x * 12:x * 12 + 12] = pixels[tile, ::-1 if attr & 2 else 1, ::-1 if attr & 1 else 1]
    windows = np.lib.stride_tricks.sliding_window_view(canvas, (8, 8))[::4, ::4].reshape(-1, 64)
    packed = windows[:, ::2] | windows[:, 1::2] << 4
    first, profile, pad = variants[regions[rid][1]]
    assert regions[rid][5] == 1 and len(packed) == rows[rid]['map_width'] * rows[rid]['map_height']
    allowed[first:first + len(packed)] = True
    for i, pix in enumerate(packed):
        maps[first + i] = intern(pix.tobytes(), 11) if pix.any() else 0
    palettes[12, 11] = exact
    # Decode every phase window independently, including all 4px seams.
    for i, source in enumerate(windows):
        tile = int(maps[first + i]);raw = np.frombuffer(tiles[tile], 'u1')
        decoded = np.empty(64, 'u1');decoded[::2] = raw & 15;decoded[1::2] = raw >> 4
        assert np.array_equal(decoded, source)
        assert np.array_equal(palettes[12, banks[tile]][decoded], exact[source])
    assert np.array_equal(maps[~allowed], old_maps[~allowed])
    assert np.array_equal(palettes[:12], old_palettes[:12]) and np.array_equal(palettes[13:], old_palettes[13:])
    assert np.array_equal(palettes[12, :11], old_palettes[12, :11])
    new_tiles = b''.join(tiles)
    new_maps = mapbytes(maps)
    before_bytes = packed_size(old_tiles, raw_maps.tobytes())
    after_bytes = packed_size(new_tiles, new_maps)
    report = dict(regions=selected, images=[48, 49, 50], profile=12, exact_source_palette=1145, source_slot=22,
                  exact_bank=11, exact_windows=len(windows), compared_pixels=int(windows.size),
                  source_canvas_sha256=hashlib.sha256(canvas).hexdigest(), companion_black_entries_migrated=migrated,
                  tiles_before=len(old_banks), tiles_after=len(banks), added_tiles=len(banks) - len(old_banks),
                  packed_linked_bytes_before=before_bytes, packed_linked_bytes_after=after_bytes,
                  packed_linked_delta=after_bytes - before_bytes, other_profile_palettes_unchanged=True,
                  other_region_maps_unchanged=True, original_tile_prefix_unchanged=True,
                  starfield_palette_index_roundtrip=True, companion_rgb_alpha_preserved=True)
    if write:
        assert after_bytes - before_bytes < 24000, 'Leave room below32MiB for code changes'
        (P / 'tiles.4bpp').write_bytes(new_tiles)
        (P / 'tile-banks.u8').write_bytes(bytes(banks))
        (P / 'maps.u24').write_bytes(new_maps)
        (P / 'palettes.rgb555').write_bytes(palettes.astype('<u2').tobytes())
        (P / 'ending-starfield.json').write_text(json.dumps(report, indent=2) + '\n')
        if pack:
            from pack_native_background import main as repack
            repack()
    print(json.dumps(report))
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    repair(write=args.apply, pack=args.apply)
