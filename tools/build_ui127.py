"""User-directed UI corrections, layered on UI126. Offline indexed pixels."""
import json,base64
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
from compile_m1_native_maps import body,numbers
R=Path(__file__).resolve().parents[1];A=R/'art/ui127';D=R/'docs/ui127'
def load(p):return json.loads(p.read_text())
def px(s):return np.array([[int(c,16) for c in row] for row in s],np.uint8)
def rows(a):return [''.join(format(int(v),'x') for v in row) for row in a]
old=load(R/'art/ui126/overrides.json');new=json.loads(json.dumps(old));new['version']=127
cells=new['fixed_cells'];pals=np.array(numbers(body((R/'generated/drawing-data.c').read_text(),'native_source_palettes'))).reshape(-1,16)
def frame(c,b,w,h):return np.block([[px(c[str(b+x+y*16)]) for x in range(w)] for y in range(h)])
def emit(a,b,w,h):
 for y in range(h):
  for x in range(w):cells[str(b+x+y*16)]=rows(a[y*6:y*6+6,x*6:x*6+6])
# User chose v123 BO form. Restore the whole BOMB label consistently.
released=load(R/'art/gameplay-text/hud-display.json')['fixed_cells']
for x in range(4):cells[str(0xae4+x)]=released[str(0xae4+x)]
# CANNON: keep the latest shape, use the bright gold index on every face row.
a=frame(cells,0xae0,4,1);a[a==9]=10;emit(a,0xae0,4,1)
# Restore source-based popup silhouette from the supplied original reference.
# Keep the UI126 face/arrow colour rules, but do not retain its enlarged tip
# or the added leading-edge pixels that changed P/number proportions.
source=load(R/'art/ui124/source-popups-75.json')
for i,g in enumerate(new['objects']):
 a=px(source[i]['rows'])
 for l,r in ((0,11),(11,24)):
  yy,xx=np.where(np.isin(a[:,l:r],[10,11,12]))
  if len(xx):
   d=xx-.38*yy;lo,hi=float(d.min()),float(d.max())
   for y,x,v in zip(yy,xx,d):a[y,l+x]=12-min(2,int(3*(v-lo)/max(.01,hi-lo+.01)))
 mask=np.isin(a,list(range(3,9)));ys=np.where(mask)[0]
 if len(ys):
  lo,hi=int(ys.min()),int(ys.max())
  for y,x in np.argwhere(mask):a[y,x]=[3,4,5,6,7,8][min(5,(y-lo)*6//(hi-lo+1))]
 g['rows']=rows(a)
# GO arrow masks: straight rasterized segments, source direction and shading.
# Only arrow surfaces change; protect the foreground letter silhouettes.
polygons={0x840:[(0,18),(12,18),(12,12),(22,22),(0,22)],
          0x980:[(0,19),(12,16),(12,11),(22,17),(0,22)],
          0xe40:[(12,10),(22,0),(22,22),(18,22),(18,10)]}
changed_go=[]
for b,points in polygons.items():
 original=frame(cells,b,4,4);maskim=Image.new('L',(24,24));dr=ImageDraw.Draw(maskim);dr.polygon(points,fill=1);mask=np.array(maskim,dtype=bool)
 edgeim=Image.new('L',(24,24));ImageDraw.Draw(edgeim).polygon(points,outline=1);edge=np.array(edgeim,dtype=bool)
 arrow=np.zeros((24,24),np.uint8)
 for y,x in np.argwhere(mask):
  if x+1<24 and y+1<24:arrow[y+1,x+1]=9
 coords=np.argwhere(np.isin(original,list(range(3,9))))
 for y,x in np.argwhere(mask):
  yy,xx=coords[np.argmin(((coords-[y,x])**2).sum(axis=1))];arrow[y,x]=original[yy,xx]
 arrow[edge]=15
 yy,xx=np.indices((24,24))
 if b!=0xe40:protected=((xx<=10)&(yy<=17))|((xx<=16)&(yy<=12))|((xx>=17)&(yy<=15))
 else:protected=((xx<=10)&(yy>=5))|((xx>=10)&(xx<=17)&(yy>=12))
 # Transparent gaps stay transparent; no rectangular pasted background.
 protected &= (original!=0)&~np.isin(original,list(range(3,9)))
 arrow[protected]=original[protected];emit(arrow,b,4,4);changed_go.append(b)
new['policy']='UI126 plus requested BOMB restoration, uniform gold CANNON, original popup contour, straight GO arrows in three held poses. Source timing and intermediate frames retained.'
(A/'overrides.json').write_text(json.dumps(new,indent=2)+'\n')
def render(a,bank):
 c=pals[bank,a];rgb=np.stack([c&31,c>>5&31,c>>10&31],-1).astype('uint8')*8;rgb[a==0]=[40,48,56];return Image.fromarray(rgb)
items=[]
for name,b,w,h,bank in [('BOMB',0xae4,4,1,1447),('CANNON',0xae0,4,1,1447),('GO right',0x840,4,4,0xa2),('GO slant',0x980,4,4,0xa2),('GO up',0xe40,4,4,0xa2)]:items.append((name,frame(old['fixed_cells'],b,w,h),frame(cells,b,w,h),bank))
for i in (4,12):items.insert(2 if i==4 else 3,('P1' if i==4 else 'P2',px(old['objects'][i]['rows']),px(new['objects'][i]['rows']),0xa2 if i==4 else 0xa3))
im=Image.new('RGB',(620,1160),'#283038');d=ImageDraw.Draw(im);d.text((150,12),'Before UI126',fill='white');d.text((410,12),'After UI127',fill='white');y=45
for name,a,b,bank in items:
 d.text((12,y+5),name,fill='white')
 for x,v in [(160,a),(420,b)]:
  z=render(v,bank);im.paste(z.resize((z.width*6,z.height*6),Image.Resampling.NEAREST),(x,y))
 y+=a.shape[0]*6+30
im=im.crop((0,0,620,y));im.save(D/'requested-corrections.png')
html='<meta charset="utf-8"><style>body{background:#283038;color:white;font:16px/1.7 system-ui;max-width:850px;margin:auto}img{max-width:100%;image-rendering:pixelated}</style><h1>요청하신 글꼴 수정</h1><p>BOMB는 이전 형태 복원. CANNON 하단 진한 색 제거. P1/P2는 원본 기반 외형 복원, 기존 대각선 색 처리 유지. GO 세 방향의 정지 자세 화살표를 직선으로 정리했습니다. 중간 회전 프레임은 그대로이며, ROM 실행 검증 전입니다.</p><img src="data:image/png;base64,'+base64.b64encode((D/'requested-corrections.png').read_bytes()).decode()+'">'
(D/'UI127-corrections.html').write_text(html)
print('written',len(cells),'FIX cells;16 popup frames;3 GO held poses')
