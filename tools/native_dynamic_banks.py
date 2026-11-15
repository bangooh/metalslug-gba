"""Offline 4bpp banks that never merge independently animated source palettes.

Each bank keeps source-slot identity. Runtime copies/interpolates only its 15
colors; it never searches colors or repacks a tile.
"""
import numpy as np
from prepare_native_background import rgb, colors, centers

def compile_banks(hist, pts, semantic, groups):
    domain = semantic[1:] // 16
    previous = None
    for iteration in range(16):
        banks = []; bindings = []
        for bank in range(12):
            weights = hist[groups == bank].sum(0)
            active = np.unique(domain[weights > 0])
            if not len(active):
                banks.append(previous[0][bank]); bindings.append(previous[1][bank]); continue
            assert len(active) <= 15
            counts = {int(d): 1 for d in active}
            # Keep small populations useful; favor color-rich source domains.
            importance = {int(d): float(np.sqrt(weights[domain == d].sum())) for d in active}
            for _ in range(15-len(active)):
                eligible = [int(d) for d in active if counts[int(d)] < np.count_nonzero((domain == d) & (weights > 0))]
                if not eligible: break
                chosen = max(eligible, key=lambda d: importance[d] / counts[d])
                counts[chosen] += 1
            row = []; tags = []
            for d in active:
                mask = (domain == d) & (weights > 0)
                positions = np.flatnonzero(mask)
                samples = pts[mask]; ww = weights[mask]
                reps = rgb(colors(centers(samples, ww, counts[int(d)], 20)))
                for rep in reps:
                    if d:
                        index = positions[np.argmin(((samples-rep)**2).sum(1))]
                        row.append(pts[index]); tags.append(int(semantic[index+1]))
                    else: row.append(rep); tags.append(0)
            while len(row) < 15: row.append(row[-1]); tags.append(tags[-1])
            banks.append(np.array(row)); bindings.append(np.array(tags))
        banks = np.array(banks); bindings = np.array(bindings, dtype=np.uint16)
        dist = ((pts[:,None,None]-banks[None])**2).sum(3)
        dist = np.where(domain[:,None,None] == bindings[None]//16, dist, 1e12)
        errors = dist.min(2).T
        new = np.argmin(hist @ errors.T, axis=1)
        assert np.max((hist * errors[new]).sum(1)) < 1e11, 'unrepresented source palette'
        previous = banks, bindings
        if np.array_equal(groups, new): break
        groups = new
    return groups, banks, dist, bindings
