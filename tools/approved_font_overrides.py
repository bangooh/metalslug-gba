"""Restore approved v17/v22 glyphs at build time, retaining live palette indices.

These are native-size indexed assets. Never resample them a second time.
Unlisted graphics, map origins, source codes and animation graphs stay intact.
"""
import hashlib
import json
from pathlib import Path
import numpy as np

D = Path(__file__).resolve().parents[1] / 'art/approved-fonts'

class ApprovedFonts:
    def __init__(self, revision=True):
        self.provenance = json.loads((D / 'provenance.json').read_text())
        for name, expected in self.provenance['files'].items():
            if hashlib.sha256((D / name).read_bytes()).hexdigest() != expected:
                raise ValueError('Approved font changed without review: ' + name)
        atlas = json.loads((D / 'fix-glyphs.json').read_text())
        glyphs = iter(atlas['glyphs'])
        self.fixed = {}
        for base, count in atlas['ranges']:
            for code in range(base, base + count):
                self.fixed[code] = self.pixels(next(glyphs))
        assert next(glyphs, None) is None and len(self.fixed) == 640
        # Apply the editable faces exactly as the approved packer did. The
        # atlas also contains the source HUD/result frame and icon cells.
        faces = json.loads((D / 'fonts.json').read_text())['fonts']
        for ch, glyph in faces['small_6x6']['glyphs'].items():
            self.fixed[0x314 if ch == '©' else 0x300 + ord(ch)] = self.pixels(glyph['rows'])
        for ch, glyph in faces['condensed_6x12']['glyphs'].items():
            code = 0x400 + (ord(ch) >> 4) * 32 + (ord(ch) & 15)
            pixels = self.pixels(glyph['rows'])
            self.fixed[code], self.fixed[code + 16] = pixels[:6], pixels[6:]
        for ch, glyph in faces['result_12x12']['glyphs'].items():
            code = 0xba0 if ch == 'P' else 0xb60 + (int(ch) // 8) * 32 + (int(ch) % 8) * 2
            pixels = self.pixels(glyph['rows'])
            for x, y, delta in [(0, 0, 0), (6, 0, 1), (0, 6, 16), (6, 6, 17)]:
                self.fixed[code + delta] = pixels[y:y+6, x:x+6]
        assert all(p.shape == (6, 6) for p in self.fixed.values())
        self.mission = json.loads((D / 'mission-font.json').read_text())['glyphs']
        self.maps = dict(zip(map(lambda v: int(v, 0), self.provenance['source_mission_maps']),
                             self.provenance['source_mission_characters'], strict=True))
        gameplay_path = D.parent / 'gameplay-text/glyphs.json'
        self.gameplay = json.loads(gameplay_path.read_text())
        self.gameplay_sha256 = hashlib.sha256(gameplay_path.read_bytes()).hexdigest()
        self.score_maps = {g['source']: g for g in self.gameplay['objects']}
        for code, rows in self.gameplay['fixed_cells'].items():
            code = int(code)
            assert code not in self.fixed
            self.fixed[code] = self.pixels(rows)
        clean = json.loads((D.parent / 'gameplay-text/hud-clean.json').read_text())
        for code, rows in clean['fixed_cells'].items():
            self.fixed[int(code)] = self.pixels(rows)
        display = json.loads((D.parent / 'gameplay-text/hud-display.json').read_text())
        for code, rows in display['fixed_cells'].items():
            self.fixed[int(code)] = self.pixels(rows)
        self.popup_maps = {g['source']: g for g in json.loads((D.parent / 'gameplay-text/popups.json').read_text())['objects']}
        # Source-style cleanup is authored at final GBA size, after legacy
        # overlays. It also repairs the old result8/9 FIX address assignment.
        self.revision = None
        self.mission_indexed = {}
        if revision:
            path = D.parent / 'ui133/overrides.json'
            revised = json.loads(path.read_text())
            for code, rows in revised['fixed_cells'].items():
                pixels = self.pixels(rows)
                assert pixels.shape == (6, 6)
                self.fixed[int(code)] = pixels
            self.popup_maps.update({g['source']: g for g in revised['objects']})
            self.score_maps.update({g['source']: g for g in revised.get('score_objects', [])})
            self.mission_indexed = revised.get('mission_indexed', {})
            self.revision = hashlib.sha256(path.read_bytes()).hexdigest()
        self.scores_applied = []
        self.applied = []
        # A thin original serif caption loses complete vertical stems under
        # point sampling. Keep this source-derived native-size repair separate
        # from the user-approved HUD, popups and mission lettering.
        path = D.parent / 'ending-fin/fin.json'
        self.ending = json.loads(path.read_text())
        self.ending_sha256 = hashlib.sha256(path.read_bytes()).hexdigest()
        assert self.ending['source'] == 0x25390e
        self.ending_applied = 0

    @staticmethod
    def pixels(rows):
        return np.array([[int(v, 16) for v in row] for row in rows], dtype=np.uint8)

    def drawing(self, source, reduced):
        if source == self.ending['source']:
            pixels = self.pixels(self.ending['rows'])
            assert pixels.shape == reduced.shape == (12, 36)
            assert hashlib.sha256(reduced.tobytes()).hexdigest() == self.ending['old_native_pixels_sha256']
            assert hashlib.sha256(pixels.tobytes()).hexdigest() == self.ending['native_pixels_sha256']
            self.ending_applied += 1
            return pixels
        if source in self.popup_maps:
            pixels = self.pixels(self.popup_maps[source]['rows'])
            assert pixels.shape == reduced.shape
            return pixels
        if source in self.score_maps:
            glyph = self.score_maps[source]
            pixels = self.pixels(glyph['rows'])
            assert pixels.shape == reduced.shape
            self.scores_applied.append({'source': source, 'glyph': glyph['character']})
            return pixels
        if source not in self.maps:
            return reduced
        ch = self.maps[source]
        pixels = self.pixels(self.mission_indexed[ch] if ch in self.mission_indexed else self.mission[ch]['rows'])
        h, w = pixels.shape
        if h > reduced.shape[0] or w > reduced.shape[1]:
            raise ValueError('Approved mission glyph exceeds source map: ' + ch)
        out = np.zeros_like(reduced)
        # Same upper-left placement and 1AA/1BE source index mapping as the
        # approved v22 packer; source movement/zoom/palette switches remain live.
        out[:h, :w] = pixels if ch in self.mission_indexed else np.array(self.provenance['mission_index_remap'], np.uint8)[pixels]
        self.applied.append({'source': source, 'glyph': ch})
        return out

    def report(self):
        assert len(self.applied) == len(self.maps) == 17
        assert len(self.scores_applied) == 10
        assert self.ending_applied == 1
        return dict(source_version=self.provenance['source_version'], fixed_cells=len(self.fixed),
                    ui_revision_sha256=self.revision,
                    approved_fixed_cells=640, gameplay_fixed_cells=len(self.gameplay['fixed_cells']),
                    score_maps=self.scores_applied, go_frames=len(self.gameplay['go_frames']),
                    gameplay_sha256=self.gameplay_sha256,
                    mission_maps=self.applied, runtime_conversion=False, indices_preserved=True,
                    ending_fin_override_sha256=self.ending_sha256,
                    provenance_sha256=hashlib.sha256((D / 'provenance.json').read_bytes()).hexdigest())
