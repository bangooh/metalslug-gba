"""Source-contour UI revision. Comparison only; does not replace released ROM."""
import sys,json,base64
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
from import_scaled_drawings import unpack_tiles,scale
from compile_m1_native_maps import body,numbers
R=Path(__file__).resolve().parents[1];G=R/'generated';A=R/'art/ui124';D=R/'docs/ui124'
raw=unpack_tiles((G/'native-fixed.4bpp').read_bytes());pal=np.array(numbers(body((G/'drawing-data.c').read_text(),'native_source_palettes'))).reshape(-1,16)
def px(rows):return np.array([[int(c,16) for c in row] for row in rows],np.uint8)
def rows(a):return [''.join(format(int(v),'x') for v in row) for row in a]
def render(a,bank):
 c=pal[bank,a];rgb=np.stack([c&31,c>>5&31,c>>10&31],-1).astype('uint8')*8;rgb[a==0]=[40,48,56];return Image.fromarray(rgb)
original=json.loads((A/'source-popups-75.json').read_text());old=json.loads((R/'art/gameplay-text/popups.json').read_text())['objects'];new=[]
for g in original:
 a=px(g['rows']);out=a.copy()
 # Keep silhouette, counters, asymmetric bevel and white highlights exactly.
 # Only unify the alternating coloured face texels; no dilation/rounding.
 for y,x in np.argwhere((a>=10)&(a<=12)):out[y,x]=12 if y<5 else 11 if y<10 else 10
 new.append({'source':g['source'],'rows':rows(out)})
(A/'popups.json').write_text(json.dumps({'objects':new,'policy':'Source75% silhouette/counters/bevel preserved; face colour dithering flattened only'},indent=2)+'\n')
oldcells=json.loads((R/'art/gameplay-text/hud-display.json').read_text())['fixed_cells'];cells={};timer=[]
for n,b in enumerate(numbers(body((G/'session-data.c').read_text(),'session_time_digits'))):
 b&=4095;full=np.block([[raw[b+x+y*16] for x in range(2)] for y in range(2)]);a=scale(full);out=a.copy()
 # Palette bands follow original top highlight and dark orange bottom.
 # Keep pixel geometry including open counters and cut diagonal corners.
 for y,x in np.argwhere((a>=2)&(a<=8)):out[y,x]=[8,8,7,6,6,5,4,4,3,3,2,2][y]
 prev=np.block([[px(oldcells[str(b+x+y*16)]) for x in range(2)] for y in range(2)])
 timer.append((a,prev,out))
 for y in range(2):
  for x in range(2):cells[str(b+x+y*16)]=rows(out[y*6:y*6+6,x*6:x*6+6])
(A/'timer.json').write_text(json.dumps({'fixed_cells':cells,'policy':'Source-derived geometry and diagonal terminals; coherent horizontal colour bands'},indent=2)+'\n')
# All three columns use the same integer zoom; no smooth interpolation.
sheet=Image.new('RGB',(1000,480),'#283038');draw=ImageDraw.Draw(sheet)
for x,title in [(140,'Neo Geo at 75%'),(420,'Previous v123'),(700,'New source-contour revision')]:draw.text((x,12),title,fill='white')
for row,(k,title) in enumerate([(4,'P1'),(12,'P2')]):
 draw.text((12,70+row*140),title,fill='white')
 for col,record in enumerate([original[k],old[k],new[k]]):sheet.paste(render(px(record['rows']),0xa2 if k==4 else 0xa3).resize((120,120),Image.Resampling.NEAREST),(150+col*280,40+row*140))
for col in range(3):
 for i,n in enumerate([0,2,5,9]):sheet.paste(render(timer[n][col],1429).resize((60,60),Image.Resampling.NEAREST),(140+col*280+i*62,350))
draw.text((12,430),'Same pixel zoom: P1/P2 5x, timer5x. Source column is explicitly reduced to75%.',fill='white');sheet.save(D/'comparison-5x.png')
actual=Image.new('RGB',(420,110),'#283038');dr=ImageDraw.Draw(actual)
for col,label in enumerate(['Source75%','Previous','New']):
 dr.text((col*140,0),label,fill='white')
 for row,k in enumerate([4,12]):actual.paste(render(px([original,old,new][col][k]['rows']),0xa2 if k==4 else 0xa3),(col*140+8,20+row*30))
 for n in range(10):actual.paste(render(timer[n][col],1429),(col*140+n*13,85))
actual.save(D/'comparison-1x.png')
native=Image.new('RGB',(420,130),'#283038');nd=ImageDraw.Draw(native)
nd.text((0,0),'Neo Geo native pixels',fill='white');nd.text((210,0),'New GBA native pixels',fill='white')
for row,k in enumerate([4,12]):
 native.paste(render(px(original[k]['native_rows']),0xa2 if k==4 else 0xa3),(8,20+row*36))
 native.paste(render(px(new[k]['rows']),0xa2 if k==4 else 0xa3),(218,20+row*36))
for n,b in enumerate(numbers(body((G/'session-data.c').read_text(),'session_time_digits'))):
 b&=4095;full=np.block([[raw[b+x+y*16] for x in range(2)] for y in range(2)])
 native.paste(render(full,1429),(n*18,105));native.paste(render(timer[n][2],1429),(210+n*14,105))
native.save(D/'comparison-native-1x.png')
validation={'scope':'Visual design candidate only; ROM not rebuilt or play-tested.','popup_frames':16,'timer_digits':10,'popup_silhouettes_match_source75':all(np.array_equal(px(a['rows'])!=0,px(b['rows'])!=0) for a,b in zip(original,new)),'timer_silhouettes_match_source75':all(np.array_equal(a!=0,b!=0) for a,old,b in timer),'reference':'Actual source sprite/FIX indexed pixels, original palette; comparison source is75% sampling, not native Neo Geo resolution.'};(D/'validation.json').write_text(json.dumps(validation,indent=2)+'\n')
html='<meta charset="utf-8"><style>body{background:#283038;color:white;font:16px sans-serif;max-width:1050px;margin:auto}img{image-rendering:pixelated;max-width:100%}</style><h1>원본 윤곽 기반 UI 재제작 시안</h1><p>원본 75% 기준 / 이전 v123 / 새 시안. 둥근 외곽선 추가를 없애고 원본의 계단형 모서리·비대칭 입체 면·글자 속 공간을 유지했습니다. 내부 색 띠만 정리했습니다. ROM 반영 전 에셋 비교입니다.</p>'
for f in ['comparison-native-1x.png','comparison-1x.png','comparison-5x.png']:html+='<h2>'+f+'</h2><img src="data:image/png;base64,'+base64.b64encode((D/f).read_bytes()).decode()+'">'
(D/'UI-source-comparison-v124.html').write_text(html)
print(validation)
