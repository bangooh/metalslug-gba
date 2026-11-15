"""Generate source-cell identity bindings for the new native stage catalogs."""
from pathlib import Path
import json
R=Path(__file__).resolve().parents[1]
h=['/* Generated from authored patch identities, not gameplay traces. */']
for m in range(3,7):
 b=json.loads((R/f'generated/m{m}-cartridge/bindings.json').read_text())
 h.append(f'#include "../generated/m{m}-assets.h"')
 for t in sorted({r[0] for r in b['patches'] if r[0]!="m5_turret_death_patch"}):h.append(f'#include "../generated/{t.removesuffix("_patches")}-programs.h"')
 h.append('#include "../generated/m5-programs.h"')
 h.append(f'static const struct StagePatchBinding campaign{m}_bindings[]={{'+','.join(('{&m5_turret_death_patch,%d,%d}'%(image,n) if t=='m5_turret_death_patch' else '{&%s[%d],%d,%d}'%(t,i,image,n)) for n,(t,i,image) in enumerate(b['patches']))+'};')
(R/'m1_native/campaign_bindings.inc').write_text('\n'.join(h)+'\n')
