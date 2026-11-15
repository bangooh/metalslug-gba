#!/usr/bin/env python3
"""Compile editable player/menu masters without reading generated binaries.

Player originals and their exact 35 bindings are already in review-v18.
The menu master retains the checkpoint's other glyphs; it is authored input,
not an asset claimed to have been extracted from the game ROM.
"""
import hashlib
import json
from pathlib import Path

import numpy as np
from import_scaled_drawings import pack
from import_approved_players import main as bind_players

ROOT = Path(__file__).resolve().parents[1]


def main():
    generated = ROOT / 'generated'
    generated.mkdir(exist_ok=True)
    review = json.loads((ROOT / 'art/review-v18/overrides.json').read_text())
    players = sorted((a for a in review['assets']
                      if a['target']['kind'] == 'approved-player'),
                     key=lambda a: a['target']['frame'])
    if [a['target']['frame'] for a in players] != list(range(35)):
        raise ValueError('Expected the complete 35-frame approved player master')

    def compile_rows(rows, shape):
        if len(rows) != shape[0] or any(len(row) != shape[1] for row in rows):
            raise ValueError('Invalid indexed master dimensions')
        pixels = np.array([[int(c, 16) for c in row] for row in rows], dtype=np.uint8)
        return pack(pixels)

    raw = b''.join(compile_rows(a['original'], (64, 64)) for a in players)
    if hashlib.sha256(raw).hexdigest() != 'ca7febc19eeda7cf62af36f036cdb91f290bf8be8ac279344e03bf269c74be32':
        raise ValueError('Approved original master fingerprint mismatch')
    (generated / 'player.tiles').write_bytes(raw)
    bind_players()
    actual = json.loads((generated / 'approved-player-source.json').read_text())['bindings']
    if actual != [a['target']['binding'] for a in players]:
        raise ValueError('Regenerated player program bindings differ from reviewed masters')
    # The scaled importer first unwinds the approved edit chain, then reapplies
    # it. Feed it the final authored player state, not an intermediate stage.
    raw = bytearray(b''.join(compile_rows(a['rows'], (64, 64)) for a in players))
    for stage in ('player-animation-fixes', 'player-solid-fixes',
                  'player-twin-fixes', 'player-face-fixes'):
        for asset in json.loads((ROOT / 'art' / stage / 'patch.json').read_text())['assets']:
            if asset['target']['kind'] != 'approved-player':
                continue
            offset = asset['target']['frame'] * 2048
            if raw[offset:offset + 2048] != compile_rows(asset['original'], (64, 64)):
                raise ValueError('Player master stage mismatch: ' + asset['id'])
            raw[offset:offset + 2048] = compile_rows(asset['rows'], (64, 64))
    (generated / 'player.tiles').write_bytes(raw)
    glyphs = json.loads((ROOT / 'art/native-menu-font.json').read_text())['glyphs']
    if len(glyphs) != 96:
        raise ValueError('Expected 96 menu glyphs')
    (generated / 'native-ui-font.4bpp').write_bytes(
        b''.join(compile_rows(rows, (8, 8)) for rows in glyphs))
    print('Rebuilt 35 approved player masters/bindings and 96 menu glyphs.')


if __name__ == '__main__':
    main()
