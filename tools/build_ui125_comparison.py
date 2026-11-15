"""Angular popup cleanup. Original full-resolution reference, never normalized."""
import json,base64,sys
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
from compile_m1_native_maps import body,numbers
R=Path(__file__).resolve().parents[1];A=R/'art/ui125';D=R/'docs/ui125'
original=json.loads((R/'art/ui124/source-popups-75.json').read_text());old=json.loads((R/'art/ui124/popups.json').read_text())['objects'];pal=np.array(numbers(body((R/'generated/drawing-data.c').read_text(),'native_source_palettes'))).reshape(-1,16)
def px(rows):return np.array([[int(v,16) for v in row] for row in rows],np.uint8)
def rows(a):return [''.join(format(int(v),'x') for v in row) for row in a]
def render(a,bank):
 c=pal[bank,a];rgb=np.stack([c&31,c>>5&31,c>>10&31],-1).astype('uint8')*8;rgb[a==0]=[40,48,56];return Image.fromarray(rgb)
new=[];audit=[]
for i,g in enumerate(old):
 a=px(g['rows']);phase=i%8
 # Interior red/yellow or blue/cyan shaft: remove checkerboard colour noise.
 for y,x in np.argwhere((a>=3)&(a<=8)):a[y,x]=min(8,3+max(0,y-5)//3)
 # Keep lettering and its asymmetric original bevel above the triangle.
 # All lower rows are authored anew: contiguous, symmetric, one apex.
 a[15:]=0;half=[0,1,3,6,8,6,3,1][phase]
 if half:
  spans=[]
  for y in range(15,24):
   h=half*(23-y)//8;left=12-h;right=12+h
   a[y,left:right+1]=6 if y<18 else 7 if y<21 else 8
   a[y,left]=a[y,right]=15;spans.append((left,right))
  assert all(l+r==24 for l,r in spans)
  assert spans[-1]==(12,12)
  assert all(spans[k+1][0]>=spans[k][0] and spans[k+1][1]<=spans[k][1] for k in range(8))
  audit.append({'source':g['source'],'spans':spans,'single_apex':True})
 new.append({'source':g['source'],'rows':rows(a)})
(A/'popups.json').write_text(json.dumps({'objects':new,'scope':'16 popup frames; angular contiguous arrow silhouettes; original letter bevel retained'},indent=2)+'\n')
# Same integer scale per source pixel. Native32px source stays32px, GBA24px stays24px.
im=Image.new('RGB',(880,600),'#283038');d=ImageDraw.Draw(im)
for x,label in [(40,'Original Neo Geo: native32 x32'),(360,'Previous proposal:24 x24'),(630,'New proposal:24 x24')]:d.text((x,15),label,fill='white')
for row,k in enumerate([4,12]):
 y=50+row*240
 for col,a in enumerate([px(original[k]['native_rows']),px(old[k]['rows']),px(new[k]['rows'])]):
  z=render(a,0xa2 if k==4 else 0xa3);im.paste(z.resize((z.width*6,z.height*6),Image.Resampling.NEAREST),([50,380,650][col],y))
 d.text((12,y+205),'P1' if k==4 else 'P2',fill='white')
d.text((30,560),'All source pixels shown at6x. Original reference is NOT reduced to75%.',fill='white');im.save(D/'original-vs-new-6x.png')
im=Image.new('RGB',(420,118),'#283038');d=ImageDraw.Draw(im)
for x,label in [(0,'Original native'),(145,'Previous'),(280,'New')]:d.text((x,0),label,fill='white')
for row,k in enumerate([4,12]):
 for x,a in zip([8,155,290],[px(original[k]['native_rows']),px(old[k]['rows']),px(new[k]['rows'])]):im.paste(render(a,0xa2 if k==4 else 0xa3),(x,25+row*44))
im.save(D/'original-vs-new-1x.png')
im=Image.new('RGB',(768,192),'#283038')
for i,g in enumerate(new):im.paste(render(px(g['rows']),0xa2 if i<8 else 0xa3).resize((96,96),Image.Resampling.NEAREST),((i%8)*96,(i//8)*96))
im.save(D/'all-phases-4x.png')
(D/'validation.json').write_text(json.dumps({'scope':'Design assets only, not ROM integrated or gameplay tested','native_reference_pixels':32,'gba_pixels':24,'original_reference_resampled':False,'frames':16,'triangle_audit':audit,'other_assets':'Timer/HUD/GO unchanged from previous proposals/releases'},indent=2)+'\n')
html='<meta charset="utf-8"><style>body{background:#283038;color:white;font:16px sans-serif;max-width:1000px;margin:auto}img{max-width:100%;image-rendering:pixelated}</style><h1>P1/P2 화살표 윤곽 재정리</h1><p>왼쪽은 원본32×32, 오른쪽은 GBA24×24입니다. 원본을75%로 줄이지 않았으며 모두 동일한 픽셀 확대율입니다. 삼각형은 중심축 대칭, 연속된 사선, 단일 꼭짓점으로 다시 그렸습니다. 기존 글자의 비대칭 입체 테두리는 유지하고, 화살표의 색 점무늬와 아래쪽 돌출 그림자를 정리했습니다. 아직 ROM에 반영하지 않은 비교 에셋입니다.</p>'
for name in ['original-vs-new-1x.png','original-vs-new-6x.png','all-phases-4x.png']:html+='<h2>'+name+'</h2><img src="data:image/png;base64,'+base64.b64encode((D/name).read_bytes()).decode()+'">'
(D/'popup-angular-comparison.html').write_text(html)
print('16 frames;14 visible arrow phases symmetric, contiguous, single apex. Full face slopes1px per row.')
