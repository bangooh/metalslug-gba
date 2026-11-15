"""Apply reviewed indexed pixels after scaling, before hardware tile packing."""
import hashlib
import json
from pathlib import Path
import numpy as np

R = Path(__file__).resolve().parents[1]


def pixels(rows):
    return np.array([[int(c, 16) for c in row] for row in rows], dtype=np.uint8)


class ReviewedAssets:
    def __init__(self, fonts):
        path = R / 'art/review-v18/overrides.json'
        self.data = json.loads(path.read_text())
        self.sha256 = hashlib.sha256(path.read_bytes()).hexdigest()
        self.hud = json.loads((R/'art/hud153/fixed.json').read_text())['fixed_cells']
        self.maps = {}
        self.seen = set()
        mission = {ch: source for source, ch in fonts.maps.items()}
        for asset in self.data['assets']:
            target = asset['target']
            kind = target['kind']
            if kind in ('scaled-map', 'score', 'popup'):
                source = target['source']
            elif kind == 'mission':
                source = mission[target['character']]
            elif kind == 'fin':
                source = fonts.ending['source']
            elif kind == 'fixed':
                code = target['code']
                # Non-overridden source cells are checked in fixed().
                continue
            else:
                continue
            assert source not in self.maps
            self.maps[source] = asset

    def drawing(self, source, baseline, origin, map_id):
        asset = self.maps.get(source)
        if asset is None:
            return baseline
        target = asset['target']
        old, new = pixels(asset['original']), pixels(asset['rows'])
        if target['kind'] == 'scaled-map':
            assert target['map_id'] == map_id and target['origin'] == origin
        if target['kind'] == 'mission':
            h, w = old.shape
            original = np.zeros_like(baseline)
            original[:h, :w] = old
            result = np.zeros_like(baseline)
            result[:h, :w] = new
        else:
            original, result = old, new
        assert np.array_equal(original, baseline), ('Review baseline mismatch', asset['id'])
        assert result.shape == baseline.shape
        self.seen.add(asset['id'])
        return result

    def fixed(self, code, baseline):
        asset = self.fixed_assets.get(code)
        if asset is None:
            return pixels(self.hud[str(code)]) if str(code) in self.hud else baseline
        assert np.array_equal(pixels(asset['original']), baseline), asset['id']
        self.seen.add(asset['id'])
        return pixels(self.hud[str(code)]) if str(code) in self.hud else pixels(asset['rows'])

    def prepare(self, pack):
        self.fixed_assets = {a['target']['code']: a for a in self.data['assets']
                             if a['target']['kind'] == 'fixed'}
        for kind, name, stride, key in [('menu', 'native-ui-font.4bpp', 32, 'code'),
                                      ('approved-player', 'player.tiles', 2048, 'frame')]:
            path = R / 'generated' / name
            raw = bytearray(path.read_bytes())
            for asset in self.data['assets']:
                target = asset['target']
                if target['kind'] != kind:
                    continue
                start = target[key] * stride
                old, new = pack(pixels(asset['original'])), pack(pixels(asset['rows']))
                assert len(old) == len(new) == stride
                assert raw[start:start+stride] in (old, new), asset['id']
                raw[start:start+stride] = new
                self.seen.add(asset['id'])
            path.write_bytes(raw)
        path = R / 'generated/approved-player-source.json'
        report = json.loads(path.read_text())
        report['approved_sha256'] = hashlib.sha256((R / 'generated/player.tiles').read_bytes()).hexdigest()
        report['review_revision'] = self.data['revision']
        path.write_text(json.dumps(report, indent=2) + '\n')

    def report(self):
        expected = {a['id'] for a in self.data['assets']}
        assert self.seen == expected, sorted(expected - self.seen)
        return {'revision': self.data['revision'], 'overrides_sha256': self.sha256,
                'project_sha256': self.data['project_sha256'],
                'applied_assets': len(self.seen), 'changed_counts': self.data['changed_counts'],
                'palette_indices_preserved': True, 'origins_preserved': True}
