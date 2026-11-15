"""Four-pixel M proposals based on UI127, not the rejected lowered-bar UI128."""
import json
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
from compile_m1_native_maps import body,numbers
R=Path(__file__).resolve().parents[1];A=R/'art/ui129';D=R/'docs/ui129';A.mkdir(exist_ok=True);D.mkdir(exist_ok=True)
base=json.loads((R/'art/ui127/overrides.json').read_text());pal=np.array(numbers(body((R/'generated/drawing-data.c').read_text(),'native_source_palettes'))).reshape(-1,16)[1447]
def frame(j,b,w):return np.array([[int(c,16) for c in ''.join(j['fixed_cells'][str(b+x)][y] for x in range(w))] for y in range(6)],np.uint8)
def make(pattern):
 j=json.loads(json.dumps(base));j['version']=129
 for b,w,x0,y0 in [(0xab1,3,10,0),(0xae4,4,13,1)]:
  a=frame(j,b,w)
  if b==0xae4:
   # Expand M from3 to4px; move the final B one pixel to preserve a gap.
   tail=a[1:,17:21].copy();a[1:,13:22]=0;a[1:,18:22]=tail
  else:a[:,10:15]=0
  mask=np.array([[v=='1' for v in row] for row in pattern])
  for y,x in np.argwhere(mask):
   if y0+y+1<6 and x0+x+1<w*6:a[y0+y+1,x0+x+1]=1
  for y,x in np.argwhere(mask):a[y0+y,x0+x]= (6 if y0+y<3 else 14) if b==0xab1 else (15 if y0+y<4 else 14)
  for x in range(w):j['fixed_cells'][str(b+x)]=[''.join(format(int(v),'x') for v in row) for row in a[:,x*6:x*6+6]]
 j['M_mask']=pattern;j['policy']='Four pixel M; UI127 base; BOMB last B moved1px for spacing. Asset proposals only.';return j
patterns=[['1001','1111','1001','1001','1001'],['1001','1111','1111','1001','1001']]
variants=[base]+[make(p) for p in patterns]
for n,j in enumerate(variants[1:],1):(A/f'option{n}.json').write_text(json.dumps(j,indent=2)+'\n')
def render(a):
 c=pal[a];rgb=np.stack([c&31,c>>5&31,c>>10&31],-1).astype('uint8')*8;rgb[a==0]=[40,48,56];return Image.fromarray(rgb)
im=Image.new('RGB',(810,310),'#283038');dr=ImageDraw.Draw(im)
for i,t in enumerate(['Original first option','4px M / one inner row','4px M / two inner rows']):dr.text((25+i*270,16),t,fill='white')
for row,(label,b,w) in enumerate([('ARMS',0xab1,3),('BOMB',0xae4,4)]):
 for i,j in enumerate(variants):
  z=render(frame(j,b,w));im.paste(z.resize((z.width*9,z.height*9),Image.Resampling.NEAREST),(25+i*270,65+row*115));im.paste(z,(25+i*270,130+row*115))
im.save(D/'M-four-pixel-options.png')
(D/'README.txt').write_text('4px-wide M alternatives based on first/UI127 option. Option1 one inner row, option2 two inner rows. BOMB final B moved1px to leave spacing after the widened M. No ROM integration.\n')
print('2 four-pixel M proposals saved')
