#!/usr/bin/env python3
"""Verify a committed FIN snapshot against independently sampled source tiles.

Reads an existing same-ROM mGBA snapshot. This is a graphics diagnostic, not
evidence that the campaign was played through without fixture injection.
"""
import argparse
import hashlib
import json
import re
import struct
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rgb555(colours):
    channels = np.stack([(colours >> shift) & 31 for shift in (0, 5, 10)], axis=-1)
    return ((channels << 3) | (channels >> 2)).astype(np.uint8)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--cc', required=True)
    ap.add_argument('--run', type=Path, required=True)
    ap.add_argument('--snapshot', type=Path, required=True)
    ap.add_argument('--screen', type=Path, required=True)
    ap.add_argument('--source-screen', type=Path, required=True)
    ap.add_argument('--comparison-note', default='Background/style comparison; caption animation phases are not synchronized.')
    ap.add_argument('--output', type=Path, required=True)
    a = ap.parse_args()
    run, snapshot, out = a.run.resolve(), a.snapshot.resolve(), a.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    state = json.loads((snapshot / 'snapshot-state.json').read_text())
    assert state['commit_boundary'] == 1, 'A coherent commit snapshot is required'
    specs = {
        'planes': ('video', 'NativeVideo', 'tiles.planes'),
        'columns': ('video', 'NativeVideo', 'tiles.columns'),
        'slots': ('video', 'NativeVideo', 'palettes.slots'),
        'bg': ('video', 'NativeVideo', 'backgrounds'),
        'views': ('world', 'NativePlayWorld', 'video.views'),
        'camera': ('scene', 'SceneRuntime', 'camera'),
        'native_camera': ('scene', 'SceneRuntime', 'native_camera_x'),
        'program': ('scene', 'SceneRuntime', 'program'),
        'scene_planes': ('scene', 'SceneRuntime', 'maps.planes'),
    }
    meta = {k: f'offsetof(struct {t},{f})' for k, (_, t, f) in specs.items()}
    meta.update({
        'view_size': 'sizeof(struct SceneTileView)',
        'view_first': 'offsetof(struct SceneTileView,first_column)',
        'view_valid': 'offsetof(struct SceneTileView,valid)',
        'column_size': 'sizeof(struct NativeTileColumn)',
        'plane_size': 'sizeof(struct NativeTilePlane)',
        'palette_size': 'sizeof(struct NativePaletteState)',
        'palette_source': 'offsetof(struct NativePaletteState,source)',
        'bg_count': 'offsetof(struct NativeBackgroundFrame,count)',
        'bg_layers': 'offsetof(struct NativeBackgroundFrame,map_count)',
        'bg_enable': 'offsetof(struct NativeBackgroundFrame,enable)',
        'bg_control': 'offsetof(struct NativeBackgroundFrame,control)',
        'bg_scroll_x': 'offsetof(struct NativeBackgroundFrame,scroll_x)',
        'bg_scroll_y': 'offsetof(struct NativeBackgroundFrame,scroll_y)',
        'bg_profile': 'offsetof(struct NativeBackgroundFrame,native_profile)',
        'bg4_maps': 'offsetof(struct NativeBackground4,maps)',
        'bg4_colours': 'offsetof(struct NativeBackground4,physical_colors)',
        'scene_plane_size': 'sizeof(struct SceneMapPlane)',
        'scene_plane_image': 'offsetof(struct SceneMapPlane,image)',
        'bg_fade_control': 'offsetof(struct NativeBackgroundFrame,fade_control)',
        'bg_fade_level': 'offsetof(struct NativeBackgroundFrame,fade_level)',
    })
    (out / 'layout.c').write_text(
        '#include "native_video.h"\n#include "play_world.h"\n#include "scene_runtime.h"\n#include <stddef.h>\n'
        'const unsigned offsets[]={' + ','.join(meta.values()) + '};\n')
    subprocess.run([a.cc, '-mcpu=arm7tdmi', '-marm', '-Isrc', '-c',
                    str(out / 'layout.c'), '-o', str(out / 'layout.o')], cwd=ROOT, check=True)
    subprocess.run([a.cc.replace('gcc', 'objcopy'), '-O', 'binary', '-j', '.rodata',
                    str(out / 'layout.o'), str(out / 'layout.bin')], check=True)
    off = dict(zip(meta, struct.unpack('<' + 'I' * len(meta), (out / 'layout.bin').read_bytes())))
    nm = subprocess.check_output([a.cc.replace('gcc', 'nm'), '-n', str(run / 'native.elf')], text=True)
    symbols = {l.split()[-1]: int(l.split()[0], 16) for l in nm.splitlines() if len(l.split()) >= 3}
    ram = (snapshot / 'ewram.bin').read_bytes()
    vram = (snapshot / 'vram.bin').read_bytes()
    pal_bytes = (snapshot / 'pal.bin').read_bytes()
    pal = np.frombuffer(pal_bytes, '<u2')
    addr = {k: symbols[s] - 0x02000000 + off[k] for k, (s, _, _) in specs.items()}
    u16 = lambda at: struct.unpack_from('<H', ram, at)[0]
    bg = addr['bg']
    profile, layers = ram[bg + off['bg_profile']], ram[bg + off['bg_layers']]
    camera = [v >> 16 for v in struct.unpack_from('<2I', ram, addr['camera'])]
    native_camera = list(struct.unpack_from('<2h', ram, addr['native_camera']))
    program = ram[addr['program']]
    source_image = u16(addr['scene_planes'] + 2 * off['scene_plane_size'] + off['scene_plane_image'])
    fade = [u16(bg + off['bg_fade_control']), u16(bg + off['bg_fade_level'])]
    assert camera == [832, 120] and program == 14 and source_image == 50 and fade == [0, 0]
    count, enable = u16(bg + off['bg_count']), u16(bg + off['bg_enable'])
    assert profile == 12 and layers == 2, (profile, layers)
    tile_bad = sum(x != y for x, y in zip(ram[bg:bg + count * 32], vram[:count * 32]))
    staged_palette = bg + 32768 + off['bg4_colours']
    palette_bad = sum(x != y for x, y in zip(ram[staged_palette:staged_palette + 384], pal_bytes[:384]))
    sy, sx = np.indices((160, 240))
    actual, maps_bad = [], 0
    for layer in range(layers):
        control = u16(bg + off['bg_control'] + layer * 2)
        assert enable & (256 << layer)
        x = (sx + u16(bg + off['bg_scroll_x'] + layer * 2)) & 255
        y = (sy + u16(bg + off['bg_scroll_y'] + layer * 2)) & 255
        map_address = ((control >> 8) & 31) * 2048
        entries = np.frombuffer(vram, '<u2', 1024, map_address).reshape(32, 32)[y // 8, x // 8]
        staged = bg + 32768 + off['bg4_maps'] + layer * 2048
        cpu = np.frombuffer(ram, '<u2', 1024, staged).reshape(32, 32)[y // 8, x // 8]
        maps_bad += int(np.count_nonzero(entries != cpu))
        px = np.where(entries & 1024, 7 - x % 8, x % 8)
        py = np.where(entries & 2048, 7 - y % 8, y % 8)
        addresses = (entries & 1023) * 32 + py * 4 + px // 2
        indices = (np.frombuffer(vram, np.uint8)[addresses] >> ((px & 1) * 4)) & 15
        colours = pal[(entries >> 12) * 16 + indices]
        actual.append((indices, colours, entries >> 12))

    text = (ROOT / 'generated/drawing-data.c').read_text()
    match = re.search(r'native_source_palettes\[[^;=]*\]\s*=\s*\{(.*?)\};', text, re.S)
    source_pal = np.array([int(n.strip(), 0) for n in match[1].replace('{', '').replace('}', '').split(',') if n.strip()], np.uint16).reshape(-1, 16)
    source_binding = u16(addr['slots'] + 22 * off['palette_size'] + off['palette_source'])
    assert source_binding == 1145, source_binding
    expected_bank = source_pal[1145].copy()
    expected_bank[0] = 0
    bank_bad = int(np.count_nonzero(pal[176:192] != expected_bank))
    raw = np.fromfile(ROOT / 'generated/native-scaled-background-12x12.4bpp', np.uint8).reshape(-1, 12, 6)
    source = np.empty((len(raw), 12, 12), np.uint8)
    source[:, :, ::2], source[:, :, 1::2] = raw & 15, raw >> 4
    plane = 2
    first = u16(addr['views'] + plane * off['view_size'] + off['view_first'])
    assert ram[addr['views'] + plane * off['view_size'] + off['view_valid']]
    cells = np.ndarray((32, 32, 2), dtype='<u2', buffer=ram, offset=addr['planes'] + plane * off['plane_size'])
    expected = np.zeros((160, 240), np.uint8)
    used_source_palettes = set()
    for col in range(32):
        cx, cy, hidden = struct.unpack_from('<HHB', ram, addr['columns'] + (first + col) * off['column_size'])
        if hidden:
            continue
        x = (cx >> 7) & 511
        if x >= 496:
            x -= 512
        elif x >= 320:
            continue
        left, top = x * 3 // 4, (496 - ((cy >> 7) & 511)) * 3 // 4 - 4
        height = cy & 63
        height = height if 0 < height <= 32 else 32
        for xx in range(max(left, 0), min(left + 12, 240)):
            dy = (np.arange(160) - top) % 384
            rows = dy // 12
            tile = cells[col, rows, 0].astype(np.uint32)
            attr = cells[col, rows, 1].astype(np.uint32)
            tid = tile + (((attr >> 4) & 15) << 16)
            px = np.where(attr & 1, 11 - (xx - left), xx - left)
            py = np.where(attr & 2, 11 - dy % 12, dy % 12)
            values = source[tid, py, px]
            valid = (dy < height * 12) & (values != 0)
            used_source_palettes.update(map(int, (attr >> 8)[valid]))
            expected[:, xx] = np.where(valid, values, 0)
    assert used_source_palettes == {22}, used_source_palettes
    indices, colours, banks = actual[1]
    index_bad = int(np.count_nonzero(indices != expected))
    rgb_bad = int(np.count_nonzero(colours[expected != 0] != expected_bank[expected[expected != 0]]))
    visible_bank_bad = int(np.count_nonzero(banks[expected != 0] != 11))
    front_opaque = int(np.count_nonzero(actual[0][0]))
    Image.fromarray(rgb555(expected_bank[expected])).save(out / 'fin-source-background.png')
    Image.fromarray(rgb555(colours)).save(out / 'fin-gpu-background.png')
    original = Image.open(a.source_screen).convert('RGB')
    screen = Image.open(a.screen).convert('RGB')
    comparison = Image.new('RGB', (704, 286), (29, 42, 51))
    draw = ImageDraw.Draw(comparison)
    draw.text((12, 10), 'Actual Neo Geo (unscaled)', fill='white')
    comparison.paste(original, (12, 30))
    draw.text((360, 10), 'Actual GBA R (unscaled)', fill='white')
    comparison.paste(screen, (360, 30))
    draw.text((360, 202), 'FIN starfield: source indices and RGB555 exact', fill='white')
    draw.text((360, 220), '38,400 screen pixels checked', fill='white')
    draw.text((12, 264), a.comparison_note, fill='white')
    comparison.save(out / 'fin-actual-source-gba-background.png')
    report = dict(status='Actual same-ROM committed FIN hardware verified',
                  scope='Targeted diagnostic fixture; no assertion of full campaign completion.',
                  comparison_note=a.comparison_note,
                  snapshot_state=state, rom_sha256=digest(run / 'rom.gba'), profile=profile,
                  source_plane=plane, source_slot=22, source_palette=1145, hardware_bank=11,
                  source_camera=camera, native_camera=native_camera, program=program,
                  source_image=source_image, fade_control_and_level=fade,
                  checked_screen_pixels=38400, opaque_source_pixels=int(np.count_nonzero(expected)),
                  source_index_mismatches=index_bad, source_rgb555_mismatches=rgb_bad,
                  exact_palette_entry_mismatches=bank_bad, visible_bank_mismatches=visible_bank_bad,
                  front_layer_opaque_pixels=front_opaque, displayed_map_pixel_mismatches=maps_bad,
                  cached_tile_byte_mismatches=tile_bad, checked_cached_tile_bytes=count * 32,
                  staged_palette_byte_mismatches=palette_bad,
                  inputs={str(p): digest(p) for p in [run / 'rom.gba', run / 'native.elf', snapshot / 'ewram.bin', snapshot / 'vram.bin', snapshot / 'pal.bin', a.screen, a.source_screen]})
    (out / 'fin-starfield-hardware.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    assert not (index_bad or rgb_bad or bank_bad or visible_bank_bad or front_opaque or maps_bad or tile_bad or palette_bad)


if __name__ == '__main__':
    main()
