"""Compile final indexed PNG masters. No dependence on earlier font pixels."""
from pathlib import Path
import json,struct,numpy as np
from PIL import Image
R=Path(__file__).resolve().parents[1]
def apply():
 from import_scaled_drawings import pack
 d=R/'art/final-fonts';g=R/'generated';m=json.loads((d/'manifest.json').read_text())
 def read(name,size):
  im=Image.open(d/name)
  if im.mode!='P' or im.size!=tuple(size):raise ValueError('Keep indexed PNG mode and dimensions: '+name)
  a=np.array(im)
  if a.max()>15:raise ValueError('Palette indices must be 0..15: '+name)
  return a
 def write(p,b):
  if not p.exists() or p.read_bytes()!=b:p.write_bytes(b)
 a=read(m['fixed'],[512,512]).reshape(64,8,64,8).transpose(0,2,1,3).reshape(4096,8,8)
 fixed=b''.join(pack(x) for x in a);frag=bytearray()
 for tile in a:
  for y in range(3):
   for x in range(3):frag.extend(struct.pack('<H',sum(int(c)<<(4*i) for i,c in enumerate(tile[y*2:y*2+2,x*2:x*2+2].flatten()))))
 words=struct.unpack('<36864H',frag);mask=[sum(bool(any(words[i*9:(i+1)*9]))<<(i%32) for i in range(b*32,b*32+32)) for b in range(128)]
 fonts=json.loads((R/'art/ending-credits/fonts.json').read_text());changed=False
 for e in m['ending']:
  im=read(e['file'],e['size']);rows=[''.join(format(int(v),'x') for v in row) for row in im];master=fonts['masters'][e['id']]
  changed|=rows!=master['rows'];master['rows']=master['after_padded']=rows
 old=json.loads((R/'art/presentation-refined/bios-font.json').read_text());new=dict(old)
 for e in m['bios']:
  im=read(e['file'],e['size'])
  if not np.isin(im,[0,15]).all():raise ValueError('BIOS glyph indices must be 0 or15')
  new[e['character']]=[''.join('#' if v else '.' for v in row) for row in im]
 # All PNG inputs validated before changing outputs.
 if changed:
  import build_ending_credits as ec
  cards=ec.load(R/'art/ending-credits/cards.json');ec.apply(fonts['masters'],cards['cards']);ec.save(R/'art/ending-credits/fonts.json',fonts)
  # Chunk overrides must be regenerated after ending compaction.
  import subprocess,sys
  for name in ['bridge-horizontal.inc','blast-packed.inc']:(g/name).unlink(missing_ok=True)
  for fn in ['build_bridge_horizontal_chunks.py','build_blast_packed.py']:subprocess.run([sys.executable,str(R/'tools'/fn)],check=True)
 if old!=new:
  from apply_font_infinity_project import update_bios
  update_bios(old,new);(R/'art/presentation-refined/bios-font.json').write_text(json.dumps(new,indent=2))
 write(g/'native-scaled-fixed-6x6.4bpp',fixed);write(g/'native-fix-fragments.u16',bytes(frag));write(g/'native-fix-visible.inc',(','.join(hex(x)+'u' for x in mask)+'\n').encode())
 print('Final fonts: FIX4096, ending48, BIOS13; ending changed='+str(changed)+', BIOS changed='+str(old!=new))
if __name__=='__main__':apply()
