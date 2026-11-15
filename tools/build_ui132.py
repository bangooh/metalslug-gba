"""Raised white mission rim and reference-matched popup construction."""
import json,base64
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
from compile_m1_native_maps import body,numbers
R=Path(__file__).resolve().parents[1];A=R/'art/ui132';D=R/'docs/ui132';A.mkdir(exist_ok=True);D.mkdir(exist_ok=True)
base=json.loads((R/'art/ui131/overrides.json').read_text());new=json.loads(json.dumps(base));pal=np.array(numbers(body((R/'generated/drawing-data.c').read_text(),'native_source_palettes'))).reshape(-1,16)
def px(r):return np.array([[int(v,16) for v in row] for row in r],np.uint8)
def rows(a):return [''.join(format(int(v),'x') for v in row) for row in a]
# Attached sample has6px integer cells, origin40,30. Quantize cell centers
# to semantic indexed surfaces, rejecting compression noise in the screenshot.
im=Image.open(A/'reference-popup.png').convert('RGB')
def index(c):
 r,g,b=map(int,c)
 if max(c)<12:return 1
 if b>150 and r<30:return 12 if g>130 else 11 if g>35 else 10
 if r<60 and g<60:
  if b>r+18:return 9
  return 0
 if max(c)-min(c)<35:
  return 15 if min(c)>220 else 14 if min(c)>170 else 13
 if b>150 and r<30:return 12 if g>130 else 11 if g>35 else 10
 if r>50:
  if g<25:return 3 if r<130 else 4 if r<230 else 5
  return 8 if b>70 else 7 if g>130 else 6
 return 0
ref=np.array([[index(im.getpixel((43+6*x,33+6*y))) if 43+6*x<im.width and 33+6*y<im.height else 0 for x in range(24)] for y in range(24)],np.uint8)
(A/'reference-p1.json').write_text(json.dumps({'rows':rows(ref),'grid_origin':[40,30],'grid_step':6},indent=2)+'\n')
original=json.loads((R/'art/ui124/source-popups-75.json').read_text())
for i,g in enumerate(new['objects']):
 phase=i%8;old=px(base['objects'][i]['rows']);a=px(original[i]['rows'])
 # All letter phases share the same restored stem, black channel and gray
 # side plane. Keep source arrow phase geometry and the approved straight tip.
 p=ref[:16,:11];region=np.isin(p,[1,9,10,11,12,13,14,15]);a[:16,:11][region]=p[region]
 if i<8:
  p=ref[:16,12:];region=np.isin(p,[1,9,10,11,12,13,14,15]);a[:16,12:][region]=p[region]
 else:
  # P2 uses the original2 outline, with its enclosing gray bevel intact.
  for y in range(15):
   for x in range(12,24):
    if a[y,x] in (10,11,12):
     for dx in (-1,1):
      xx=x+dx
      if 12<=xx<24 and a[y,xx] in (1,13,14):a[y,xx]=14
 # Retain the existing ordered colour ramp; do not spreadgray into the face.
 for y,x in np.argwhere(np.isin(a,[10,11,12])):
  local=x if x<11 else x-12;v=np.clip((local-.38*y+3)/6,0,2);lo=int(v);threshold=[[0,.5],[.75,.25]][y%2][x%2];a[y,x]=[12,11,10][min(2,lo+int(v-lo>threshold))]
 if phase==3:a[np.isin(a,[10,11,12])]=15
 # Arrow and animation coverage remain the current approved phase masks.
 arrow=np.isin(old,[3,4,5,6,7,8]);a[arrow]=old[arrow];a[16:]=old[16:]
 g['rows']=rows(a)
for ch,rs in new['mission_indexed'].items():
 a=px(rs);mask=np.isin(a,list(range(9,15)));yy,xx=np.where(mask);lo,hi=int(yy.min()),int(yy.max())
 for y,x in zip(yy,xx):
  # The white rim casts a continuous dark inset line down/right.
  shadow=(y>0 and a[y-1,x]==15) or (x>0 and a[y,x-1]==15)
  if shadow:a[y,x]=9;continue
  v=np.clip((y-lo)*5/max(1,hi-lo),0,5);n=int(v);t=[[0,.5],[.75,.25]][y%2][x%2];a[y,x]=[14,13,12,11,10,9][min(5,n+int(v-n>t))]
 new['mission_indexed'][ch]=rows(a)
new['version']=132;new['policy']='Raised white mission rim with continuous inner cast shadow, P1 attachment-derived gray side plane and black channel; P2 adapted using original2 shape.'
(A/'overrides.json').write_text(json.dumps(new,indent=2)+'\n')
def render(a,p):
 c=pal[p,a];rgb=np.stack([c&31,c>>5&31,c>>10&31],-1).astype('uint8')*8;rgb[a==0]=[40,48,56];return Image.fromarray(rgb)
items=[('P1',px(base['objects'][4]['rows']),px(new['objects'][4]['rows']),0xa2),('P2',px(base['objects'][12]['rows']),px(new['objects'][12]['rows']),0xa3)]
for ch in 'EMSC':items.append((ch,px(base['mission_indexed'][ch]),px(new['mission_indexed'][ch]),0x1aa))
im=Image.new('RGB',(620,1100),'#283038');d=ImageDraw.Draw(im);d.text((120,12),'Previous UI131',fill='white');d.text((390,12),'Revised UI132',fill='white');y=45
for label,a,b,p in items:
 d.text((12,y+5),label,fill='white')
 for x,v in [(120,a),(390,b)]:
  z=render(v,p);im.paste(z.resize((z.width*6,z.height*6),Image.Resampling.NEAREST),(x,y))
 y+=max(a.shape[0],b.shape[0])*6+25
im.crop((0,0,620,y)).save(D/'comparison.png')
html='<meta charset="utf-8"><style>body{background:#283038;color:white;font:16px/1.7 system-ui;max-width:800px;margin:auto}img{max-width:100%;image-rendering:pixelated}</style><h1>흰 테두리의 내부 그림자 · P1/P2 재제작</h1><p>흰 테두리는 유지하고 그 안쪽의 위·왼쪽에 연속된 진한 붉은 그림자를 넣었습니다. P1은 첨부의6픽셀 격자를 읽어 회색 옆면·검은 틈을 복원했고, P2는 같은 P 구조와 원본2 윤곽을 조합했습니다. 색상은 기존 팔레트와 도트 그라데이션입니다. 에셋 비교이며 ROM 미반영입니다.</p><img src="data:image/png;base64,'+base64.b64encode((D/'comparison.png').read_bytes()).decode()+'">'
(D/'UI132-comparison.html').write_text(html)
print('17 mission letters and16 popup maps saved')
