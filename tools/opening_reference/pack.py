import sys,zlib,struct,json
from pathlib import Path
import numpy as np
sys.path.insert(0,'upstream/assets/tools');import sprites as S
p=S.PRom();tiles=S.Tiles();g=Path('src/metal-slug-native/generated')
# Read deterministic generator tables.
sys.path.insert(0,'opening-work');import build as b
atlas=bytearray()
for tid in b.tileids:
 a=tiles.get(tid);ix=(np.arange(12)*4+2)//3;a=a[ix[:,None],ix];atlas+=(a[:,::2]|a[:,1::2]<<4).tobytes()
sys.path.insert(0,str(g.parent/'tools'))
from presentation_logo_repairs import restore_opening_fonts
atlas,b.cells,b.maps=restore_opening_fonts(atlas,b.cells,b.maps,b.mapids)
blocks=[('tiles',bytes(atlas)),('cells',b''.join(struct.pack('<hhH',*r) for r in b.cells)),('maps',b''.join(struct.pack('<HH',*r) for r in b.maps)),('palettes',b''.join(struct.pack('<16H',*r) for r in b.pals)),('frames',b''.join(struct.pack('<4H',*r) for r in b.headers)),('palrefs',struct.pack('<'+'H'*len(b.palrefs),*b.palrefs)),('draws',b''.join(struct.pack('<HhhBBBB',*r) for r in b.draws))]
assert sum(len(data) for name,data in blocks if name!='draws')+1032<=185228, 'Opening video arena overflow'
# First four blocks share video arena; draw table occupies world after framebuffer.
for name,data in blocks:
 out=zlib.compress(data,9,wbits=-15);(g/f'opening-{name}.deflate').write_bytes(out);print(name,len(data),len(out))
# build_title_from_opening.py generates and compresses the title after this
# opening is packed. No title from an earlier build is an input here.
# Replace bulky tables with compressed binary sections; retain small index/event tables.
c=(g/'opening-replay-data.c').read_text()
import re
for name in ['cells','maps','palettes','draws','tiles','frames','palrefs']:
 c=re.sub(r'const [^\n]+ opening_'+name+r'\[.*?\};\n','',c,flags=re.S)
(g/'opening-replay-data.c').write_text(c)
asm=''
for name,_ in blocks:
 asm+=f'.section .rodata.opening_{name},"a",%progbits\n.balign 4\n.global opening_{name}_packed\nopening_{name}_packed:\n.incbin "generated/opening-{name}.deflate"\n'
Path('src/metal-slug-native/src/opening_replay_assets.S').write_text(asm)
h=(g/'opening-replay-data.h').read_text()
h=re.sub(r'OPENING_TILE_COUNT=\d+',f'OPENING_TILE_COUNT={len(atlas)//72}',h)
for name,data in blocks:
 h=h.replace('#endif',f'extern const uint8_t opening_{name}_packed[];\nenum {{OPENING_{name.upper()}_BYTES={len(data)}}};\n#endif')
(g/'opening-replay-data.h').write_text(h)

# Preserve separately regenerated boot and scene10 background assets.
p=Path("src/metal-slug-native/src/opening_replay_assets.S")
s=p.read_text()
for prefix,name in [("neogeo","tiles"),("neogeo","changes"),("neogeo","palettes"),("opening","background")]:
 s+=f'.section .rodata.{prefix}_{name},"a",%progbits\n.balign 4\n.global {prefix}_{name}_packed\n{prefix}_{name}_packed:\n.incbin "generated/{prefix}-{name}.deflate"\n'
p.write_text(s)

# Visibility must be regenerated whenever the atlas or timeline changes.
import subprocess
subprocess.run([sys.executable,str(g.parent/"tools/build_opening_visibility.py")],check=True)
