"""Native24px P1/P2 lettering and eight original arrow animation phases."""
import json
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1]
P=['1111110','1111111','1100011','1100011','1100011','1111111','1111110','1100000','1100000','1100000','1100000','1100000','1100000']
ONE=['001100','011100','111100','001100','001100','001100','001100','001100','001100','001100','001100','111111','111111']
TWO=['011110','111111','110011','000011','000011','000110','001100','011000','110000','110000','110000','111111','111111']
def letters(a,rows,x0):
 mask=np.zeros((24,24),bool)
 for y,row in enumerate(rows):
  for x,v in enumerate(row):
   if v=='1':mask[y+1,x+x0]=True
 for y,x in np.argwhere(mask):
  if y+2<24 and x+2<24:a[y+2,x+2]=1
 for y,x in np.argwhere(mask):
  for dy,dx in [(-1,0),(1,0),(0,-1),(0,1)]:
   if 0<=y+dy<24 and 0<=x+dx<24:a[y+dy,x+dx]=15
 for y,x in np.argwhere(mask):a[y,x]=12 if x==x0 or y<4 else 11 if x<x0+3 else 10
objects=[]
for player in range(2):
 for phase,width in enumerate([0,2,5,9,13,9,5,2]):
  a=np.zeros((24,24),np.uint8)
  # Arrow rotates around its centre; lettering remains screen-facing.
  if width:
   mask=np.zeros((24,24),bool)
   for y in range(12,23):
    half=2 if y<18 else 6-(y-18)
    half=max(0,half)*width//13
    mask[y,12-half:13+half]=True
   for y,x in np.argwhere(mask):
    for dy,dx in [(-1,0),(1,0),(0,-1),(0,1)]:
     if 0<=y+dy<24 and 0<=x+dx<24:a[y+dy,x+dx]=15
   for y,x in np.argwhere(mask):a[y,x]=min(8,3+(y-12)//2)
  letters(a,P,2);letters(a,ONE if player==0 else TWO,14)
  if phase==3:a[(a>=10)&(a<=12)]=15
  objects.append({'source':2427272+player*128+phase*16,'rows':[''.join(format(int(v),'x') for v in row) for row in a]})
(R/'art/gameplay-text/popups.json').write_text(json.dumps({'objects':objects,'scope':'P1/P2 native24px; original8 animation frames, source palettes and timing retained'},indent=2)+'\n')
