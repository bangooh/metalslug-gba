"""M center depth comparison; one/two pixel alternatives at actual GBA size."""
import json,base64
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
from compile_m1_native_maps import body,numbers
R=Path(__file__).resolve().parents[1];D=R/'docs/ui128';A=R/'art/ui128';D.mkdir(exist_ok=True);A.mkdir(exist_ok=True)
base=json.loads((R/'art/ui127/overrides.json').read_text());pal=np.array(numbers(body((R/'generated/drawing-data.c').read_text(),'native_source_palettes'))).reshape(-1,16)[1447]
def frame(j,b,w):return np.array([[int(c,16) for c in ''.join(j['fixed_cells'][str(b+x)][y] for x in range(w))] for y in range(6)],np.uint8)
def change(j,depth):
 for b,w,left,center,top in [(0xab1,3,10,[11,12],1),(0xae4,4,13,[14],2)]:
  a=frame(j,b,w)
  # Remove center ink and its shadow inside the M only; sides unchanged.
  for x in center:
   for y in range(top,6):
    if a[y,x] in (6,14,15):a[y,x]=1
  dest=min(5,top+depth)
  for x in center:a[dest,x]=6 if b==0xab1 and dest<3 else 14 if b==0xab1 else 15 if dest<4 else 14
  for x in range(w):j['fixed_cells'][str(b+x)]=[''.join(format(int(v),'x') for v in row) for row in a[:,x*6:x*6+6]]
 return j
variants=[base]+[change(json.loads(json.dumps(base)),d) for d in (1,2)]
for depth,j in enumerate(variants[1:],1):
 j['version']=128;j['M_center_lowered_pixels']=depth;(A/f'overrides-depth{depth}.json').write_text(json.dumps(j,indent=2)+'\n')
def render(a):
 c=pal[a];rgb=np.stack([c&31,c>>5&31,c>>10&31],-1).astype('uint8')*8;rgb[a==0]=[40,48,56];return Image.fromarray(rgb)
im=Image.new('RGB',(790,260),'#283038');d=ImageDraw.Draw(im)
for i,t in enumerate(['Current','Center down 1px','Center down 2px']):d.text((40+i*260,15),t,fill='white')
for y,(name,b,w) in enumerate([('ARMS',0xab1,3),('BOMB',0xae4,4)]):
 for i,j in enumerate(variants):
  z=render(frame(j,b,w));im.paste(z.resize((z.width*9,z.height*9),Image.Resampling.NEAREST),(40+i*260,60+y*100))
 im.paste(render(frame(variants[2],b,w)),(740,125+y*100))
im.save(D/'M-center-comparison.png')
(D/'README.txt').write_text('ARMS/BOMB M-center alternatives relative to UI127. depth2 is the requested candidate. Asset-only: not integrated into ROM. UI127 includes previous requested BOMB/CANNON/popup/GO changes.\n')
print('saved two depth candidates')
