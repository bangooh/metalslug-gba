"""Audit the font-only delta against the saved pre-change conversion ledger."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np

R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
p.add_argument('--before',required=True,type=Path,help='Directory containing the114 scaled ledger and FIX atlas')
p.add_argument('--legacy-fixed',type=Path,help='Optional released v17/v30 reference-fix-fonts.bin')
a=p.parse_args()
G=R/'generated'
before=json.loads((a.before/'scaled-drawing-source.json').read_text())
after=json.loads((G/'scaled-drawing-source.json').read_text())
fonts=after['approved_font_overrides']
changed={x['source'] for x,y in zip(after['map_audit'],before['map_audit']) if x['pixel_sha256']!=y['pixel_sha256']}
assert changed=={x['source'] for x in fonts['mission_maps']+fonts['score_maps']}
old=np.frombuffer((a.before/'native-scaled-fixed-6x6.4bpp').read_bytes(),np.uint8).reshape(4096,32)
new=np.frombuffer((G/'native-scaled-fixed-6x6.4bpp').read_bytes(),np.uint8).reshape(4096,32)
changed_fixed=set(np.flatnonzero((old!=new).any(axis=1)))
gameplay=json.loads((R/'art/gameplay-text/glyphs.json').read_text())
allowed=set(range(0x300,0x380))|set(range(0x400,0x500))|set(range(0xb00,0xc00))|set(map(int,gameplay['fixed_cells']))
assert changed_fixed<=allowed
assert before['files']['native-scaled-background-12x12.4bpp']['sha256']==after['files']['native-scaled-background-12x12.4bpp']['sha256']
assert before['approved_player_sha256']==after['approved_player_sha256']
raw=new.reshape(4096,8,4);pix=np.empty((4096,8,8),np.uint8)
pix[:,:,::2]=raw&15;pix[:,:,1::2]=raw>>4
legacy=bytearray()
for base,count in [(0x300,128),(0x400,256),(0xb00,256)]:
    for im in pix[base:base+count,:6,:6]: legacy.extend((im[:,::2]|im[:,1::2]<<4).tobytes())
if a.legacy_fixed: assert legacy==a.legacy_fixed.read_bytes()
# All linked FIX fragments must reconstruct the authored cells exactly,
# including cells that were not selected for a change.
fragments=np.frombuffer((G/'native-fix-fragments.u16').read_bytes(),dtype='<u2').reshape(4096,3,3)
for y in range(6):
    for x in range(6):
        decoded=(fragments[:,y//2,x//2]>>(4*((y%2)*2+x%2)))&15
        assert np.array_equal(decoded,pix[:,y,x])
print(json.dumps(dict(changed_object_maps=len(changed),unchanged_object_maps=9391-len(changed),
                     changed_fixed_cells=len(changed_fixed),fragment_roundtrip_pixels=4096*36,
                     legacy_fix_sha256=hashlib.sha256(legacy).hexdigest(),
                     legacy_binary_compared=bool(a.legacy_fixed),background_and_player_unchanged=True),indent=2))
