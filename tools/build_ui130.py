"""Five-pixel M layout preview; exact indexed game assets, no interpolation."""
import json
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
from compile_m1_native_maps import body,numbers
R=Path(__file__).resolve().parents[1];A=R/'art/ui130';D=R/'docs/ui130';A.mkdir(exist_ok=True);D.mkdir(exist_ok=True)
base=json.loads((R/'art/ui127/overrides.json').read_text());old=json.loads((R/'art/ui129/option2.json').read_text());new=json.loads(json.dumps(base));pal=np.array(numbers(body((R/'generated/drawing-data.c').read_text(),'native_source_palettes'))).reshape(-1,16)[1447]
def frame(j,b,w):return np.array([[int(c,16) for c in ''.join(j['fixed_cells'][str(b+x)][y] for x in range(w))] for y in range(6)],np.uint8)
M=['10001','11011','10101','10001','10001']
for b,w in [(0xab1,3),(0xae4,4)]:
 a=frame(base,b,w)
 if b==0xab1:
  # ARMS stays18px. R becomes3px, yielding one column for5px M.
  a[:,5:15]=0
  placements=[(5,0,['110','101','110','101','101']),(9,0,M)]
 else:
  # BOMB stays24px. M5px and final B remain separated by one column.
  tail=a[1:,17:21].copy();a[1:,13:23]=0;a[1:,19:23]=tail
  placements=[(13,1,M)]
 for x0,y0,p in placements:
  mask=np.array([[v=='1' for v in row] for row in p])
  for y,x in np.argwhere(mask):
   if y0+y+1<6 and x0+x+1<w*6:a[y0+y+1,x0+x+1]=1
  for y,x in np.argwhere(mask):a[y0+y,x0+x]=(6 if y0+y<3 else 14) if b==0xab1 else (15 if y0+y<4 else 14)
 for x in range(w):new['fixed_cells'][str(b+x)]=[''.join(format(int(v),'x') for v in row) for row in a[:,x*6:x*6+6]]
new['version']=130;new['M_mask']=M;new['policy']='5px M preview. ARMS18px with3px R; BOMB24px with final B shifted2px. Not ROM integrated.'
(A/'proposal.json').write_text(json.dumps(new,indent=2)+'\n')
def render(a):
 c=pal[a];rgb=np.stack([c&31,c>>5&31,c>>10&31],-1).astype('uint8')*8;rgb[a==0]=[40,48,56];return Image.fromarray(rgb)
im=Image.new('RGB',(690,340),'#283038');d=ImageDraw.Draw(im)
d.text((40,18),'Previous: M width 4px',fill='white');d.text((370,18),'New: M width 5px',fill='white')
for row,(name,b,w) in enumerate([('ARMS',0xab1,3),('BOMB',0xae4,4)]):
 y=65+row*130
 for x,j in [(40,old),(370,new)]:
  z=render(frame(j,b,w));im.paste(z.resize((z.width*10,z.height*10),Image.Resampling.NEAREST),(x,y));im.paste(z,(x,y+78))
d.text((40,310),'10x integer zoom above; actual game pixel size below each label.',fill='#bac3cc')
im.save(D/'M-five-pixel-preview.png')
(D/'README.txt').write_text('Five-pixel M preview. M mask:10001/11011/10101/10001/10001. ARMS remains18px by narrowing R to3px. BOMB remains24px by shifting final B2px. Source palette retained. No ROM integration or runtime test.\n')
