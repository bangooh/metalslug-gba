"""Native-size HUD faces, using original tile addresses and palette indices."""
import json,sys
from pathlib import Path
import numpy as np
from import_scaled_drawings import unpack_tiles,scale
from compile_m1_native_maps import body,numbers
R=Path(__file__).resolve().parents[1];G=R/'generated'
raw=unpack_tiles((G/'native-fixed.4bpp').read_bytes());cells={}
def emit(a,base,w,h):
 for y in range(h):
  for x in range(w):cells[str(base+x+y*16)]=[''.join(format(int(v),'x') for v in row) for row in a[y*6:y*6+6,x*6:x*6+6]]
def frame(base,w,h):return scale(np.block([[raw[base+x+y*16] for x in range(w)] for y in range(h)]))
digits=['01110/10001/10001/10001/10001/10001/01110','00100/01100/00100/00100/00100/00100/01110','01110/10001/00001/00110/01100/11000/11111','11110/00001/00001/01110/00001/00001/11110','00010/00110/01010/10010/11111/00010/00010','11111/10000/10000/11110/00001/00001/11110','01110/10000/10000/11110/10001/10001/01110','11111/00001/00010/00100/01000/01000/01000','01110/10001/10001/01110/10001/10001/01110','01110/10001/10001/01111/00001/00001/01110']
# Nine-pixel face inside a12px cell; open counters and intact black contour.
for n,base in enumerate(numbers(body((G/'session-data.c').read_text(),'session_time_digits'))):
 m=np.array([[c=='1' for c in row] for row in digits[n].split('/')]);m=np.repeat(np.repeat(m,[1,2,1,2,1,2,1],axis=0),[2,2,1,2,2],axis=1)
 a=np.zeros((12,12),np.uint8)
 for y,x in np.argwhere(m):
  for dy,dx in [(0,0),(-1,0),(1,0),(0,-1),(0,1)]:a[y+1+dy,x+1+dx]=1
 for y,x in np.argwhere(m):a[y+1,x+1]=[8,7,7,6,5,4,4,3,2,2][y]
 emit(a,base&4095,2,2)
letters={'A':['0110','1001','1111','1001','1001'],'R':['1110','1001','1110','1010','1001'],'M':['1001','1111','1111','1001','1001'],'S':['0111','1000','0110','0001','1110'],'B':['1110','1001','1110','1001','1110'],'O':['0110','1001','1001','1001','0110'],'C':['011','100','100','100','011'],'N':['101','111','111','111','101']}
def label(word,base,wide=False):
 cols=3 if word=="ARMS" else 4; extent=cols*6
 if word=="ARMS":
  letters.update({"A":["010","101","111","101","101"],"R":["110","101","110","101","101"],"M":["101","111","111","101","101"],"S":["011","100","010","001","110"]})
 a=frame(base,cols,1);a[:]=0;a[0,:]=6
 widths=[len(letters[ch][0]) for ch in word];x=(extent-sum(widths)-(len(word)-1))//2
 for ch,w in zip(word,widths):
  for y,row in enumerate(letters[ch]):
   for xx,v in enumerate(row):
    if v=='1':
     for dy,dx in [(0,-1),(0,1),(-1,0),(1,0)]:
      yy=y+1+dy;xxx=x+xx+dx
      if 0<=yy<6 and 0<=xxx<extent:a[yy,xxx]=1
  x+=w+1
 x=(extent-sum(widths)-(len(word)-1))//2
 for ch,w in zip(word,widths):
  for y,row in enumerate(letters[ch]):
   for xx,v in enumerate(row):
    if v=='1':a[y+1,x+xx]=(10 if wide else 15) if y<3 else (9 if wide else 14)
  x+=w+1
 emit(a,base,cols,1)
label('ARMS',0xab1);label('BOMB',0xae4)
# CANNON needs the original compact6-letter silhouette; preserve that source.
out=R/'art/gameplay-text/hud-display.json';out.write_text(json.dumps({'fixed_cells':cells,'scope':'Timer0..9, ARMS and BOMB; source palette/placement retained'},indent=2)+'\n')
print(len(cells),'FIX cells')
