"""Import user-authored 24px popup masters without resampling."""
import json,hashlib
from pathlib import Path
from collections import deque
import numpy as np
from PIL import Image
from compile_m1_native_maps import body,numbers
R=Path(__file__).resolve().parents[1]; A=R/'art/ui133'; A.mkdir(exist_ok=True)
b=json.loads((R/'art/ui132/overrides.json').read_text())
pal=np.array(numbers(body((R/'generated/drawing-data.c').read_text(),'native_source_palettes'))).reshape(-1,16)
def rows(a):return [''.join(format(int(v),'x') for v in row) for row in a]
def px(r):return np.array([[int(v,16) for v in row] for row in r],dtype=np.uint8)
checks=[]
for player in [1,2]:
 f=A/f'P{player}-user.png'; rgb=np.array(Image.open(f).convert('RGB')); assert rgb.shape==(24,24,3)
 # Only black connected to the canvas border is background; enclosed black stays opaque.
 outside=np.zeros((24,24),bool); q=deque((y,x) for y in range(24) for x in range(24) if y in (0,23) or x in (0,23))
 while q:
  y,x=q.popleft()
  if not(0<=y<24 and 0<=x<24) or outside[y,x] or rgb[y,x].any():continue
  outside[y,x]=True;q.extend([(y-1,x),(y+1,x),(y,x-1),(y,x+1)])
 bank=0xa1+player;c=pal[bank];colors=np.stack([c&31,c>>5&31,c>>10&31],-1)*8
 lut={tuple(v):i for i,v in enumerate(colors)}
 # Three P1 swatches pasted into P2 are the same semantic indices.
 if player==2:lut.update({(160,168,176):13,(200,208,216):14,(0,0,56):9})
 a=np.array([[lut[tuple(v)] for v in row] for row in rgb],dtype=np.uint8);a[outside]=0
 corrected=colors[a].astype('uint8');alpha=np.where(a==0,0,255).astype('uint8')
 Image.fromarray(np.dstack([corrected,alpha])).save(A/f'P{player}-24x24.png')
 count=int(np.any(corrected[~outside]!=rgb[~outside],axis=1).sum())
 for phase in range(8):
  g=b['objects'][(player-1)*8+phase];old=px(g['rows']);v=a.copy()
  if phase!=4:
   # Keep authored lettering; preserve source animation's arrow silhouette.
   arrow=np.isin(v,list(range(3,9)));oldarrow=np.isin(old,list(range(3,9)))
   v[arrow]=0;v[oldarrow]=old[oldarrow];v[16:]=old[16:]
  if phase==3:v[np.isin(v,[10,11,12])]=15
  g['rows']=rows(v)
 checks.append({'player':player,'input_sha256':hashlib.sha256(f.read_bytes()).hexdigest(),'palette_adjusted_pixels':count,'transparent_pixels':int(outside.sum()),'size':[24,24]})
b['version']=133;b['policy']='User P1/P2 native24px masters; semantic P2 palette correction10pixels; border-connected black transparency; existing arrow phases and flash retained. Inherits all UI132 font/HUD changes.'
(A/'overrides.json').write_text(json.dumps(b,indent=2)+'\n');(A/'checks.json').write_text(json.dumps(checks,indent=2)+'\n');print(checks)
