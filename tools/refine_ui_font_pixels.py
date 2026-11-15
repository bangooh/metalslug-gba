"""Generate coherent small and name-entry fixed fonts; no runtime font work.

Run after importing the base fixed atlas. Geometric symmetry is validated on
ink masks before applying the established downward gradient and drop shadow.
"""
from pathlib import Path
import json,struct
import numpy as np
from import_scaled_drawings import unpack_tiles,pack
from build_hud_glyphs import letter_codes,hexrows
R=Path(__file__).resolve().parents[1];G=R/'generated'

def generate():
 small=json.loads((R/'art/ui-font-cleanup/small-masks.json').read_text())
 large=json.loads((R/'art/ui-font-cleanup/large-masks.json').read_text())
 # Match punctuation in both sets; name-entry's END icon is kept unchanged.
 for c,rows in small.items():
  if c not in large:large[c]=[''.join(row[x] for x in [0,0,1,1,2,3,3,4,4]) for row in rows]
 assert set(small)==set(large)==set(map(chr,range(32,96)))
 def mask(rows):return np.array([[v=='#'for v in row]for row in rows],bool)
 checks={}
 for label,font,width in [('small',small,5),('large',large,9)]:
  for ch,rows in font.items():assert len(rows)==10 and all(len(row)==width for row in rows),(label,ch)
  for ch in 'AMVWXYTHIOU':
   a=mask(font[ch]);assert np.array_equal(a,a[:,::-1]),(label,ch,'mirror')
  for ch in 'NZ':
   a=mask(font[ch]);assert np.array_equal(a,a[::-1,::-1]),(label,ch,'point')
  for ch in '0123456789':
   yy=np.where(mask(font[ch]))[0];assert (yy.min(),yy.max())==(0,9),(label,ch,'height')
  checks[label]={'glyphs':len(font),'ink_height_digits':10,'vertical_mirror':'AMVWXYTHIOU','point_symmetry':'NZ'}
 fixed=bytearray((G/'native-scaled-fixed-6x6.4bpp').read_bytes());frags=bytearray((G/'native-fix-fragments.u16').read_bytes());before=unpack_tiles(fixed).copy();changed={}
 def cell(rows,width):
  m=mask(rows);out=np.zeros((12,width),np.uint8);ink=np.zeros_like(out,dtype=bool)
  x=0 if width==6 else 1
  ink[:10,x:x+m.shape[1]]=m
  ramp=[15,15,15,8,8,7,7,6,6,5]
  for y in range(10):out[y,ink[y]]=ramp[y]
  shadow=np.zeros_like(ink);shadow[:,1:]|=ink[:,:-1];shadow[1:]|=ink[:-1];shadow[1:,1:]|=ink[:-1,:-1]
  out[shadow&~ink]=1;return out
 for ch,rows in small.items():
  a=cell(rows,6)
  for i,code in enumerate(letter_codes(ch)):changed[code]=a[i*6:i*6+6]
 for ch,rows in large.items():
  c=ord(ch);code=0xb00+((c&8)<<5)+((c&0x77)<<1);a=cell(rows,12)
  for dy in range(2):
   for dx in range(2):changed[code+dy*16+dx]=a[dy*6:dy*6+6,dx*6:dx*6+6]
 p=R/'art/hud-font-edit/patch.json';data=json.loads(p.read_text());lookup={a['id']:a for a in data['assets']};template=next(a for a in data['assets'] if a['target']['kind']=='fixed')
 for code,a in sorted(changed.items()):
  aid=f'fixed:{code}';rows=hexrows(a)
  if aid not in lookup:
   old=hexrows(before[code,:6,:6]);e={'id':aid,'label':hex(code),'original':old,'before_current':old,'target':{'kind':'fixed','code':code},'palette':template['palette']};data['assets'].append(e)
  else:e=lookup[aid]
  e['rows']=e['after_padded']=rows;e['group']='UI font geometry v2'
  pad=np.zeros((8,8),np.uint8);pad[:6,:6]=a;fixed[code*32:code*32+32]=pack(pad)
  words=[sum(int(v)<<(4*i)for i,v in enumerate(a[y*2:y*2+2,x*2:x*2+2].flatten())) for y in range(3)for x in range(3)]
  frags[code*18:code*18+18]=struct.pack('<9H',*words)
 p.write_text(json.dumps(data,ensure_ascii=False,indent=1)+'\n');(G/'native-scaled-fixed-6x6.4bpp').write_bytes(fixed);(G/'native-fix-fragments.u16').write_bytes(frags)
 words=struct.unpack('<36864H',frags);bits=[sum(bool(any(words[i*9:(i+1)*9]))<<(i%32)for i in range(b*32,b*32+32))for b in range(128)]
 (G/'native-fix-visible.inc').write_text(','.join(hex(x)+'u'for x in bits)+'\n')
 checks['fixed_tiles']=len(changed);(R/'art/ui-font-cleanup/validation.json').write_text(json.dumps(checks,indent=2)+'\n')
 print(json.dumps(checks))
if __name__=='__main__':generate()
