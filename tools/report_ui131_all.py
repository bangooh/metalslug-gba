"""Native original vs approved UI130 indexed-asset comparison, every modified family."""
import sys,json,base64,html,hashlib
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
R=Path(__file__).resolve().parents[1]
# Reuse source extraction routines without regenerating earlier deliverables.
source=(R/'tools/build_ui126.py').read_text().split('# Serialize separate replacement layer')[0]
ns={'__file__':str(R/'tools/build_ui126.py'),'__name__':'source_extract'};exec(compile(source,'ui126-source-extraction','exec'),ns)
from approved_font_overrides import ApprovedFonts
from compile_m1_native_maps import body,numbers
F=ApprovedFonts();D=R/'docs/ui131-all';D.mkdir(exist_ok=True)
raw=ns['raw'];pal=ns['pal'];native=ns['frame'];obj=ns['source_obj'];px=ns['px'];old=ns['old'];groups={}
def fixed(b,w,h):return np.block([[F.fixed.get(b+x+y*16,ns['scale'](raw[b+x+y*16])) for x in range(w)] for y in range(h)])
def add(group,label,a,b,p):groups.setdefault(group,[]).append((label,a,b,p))
for name,b,w in [('ARMS',0xab1,3),('BOMB',0xae4,4),('CANNON',0xae0,4)]:add('labels',name,native(b,w,1),fixed(b,w,1),1447)
for n,b in enumerate(numbers(body((R/'generated/session-data.c').read_text(),'session_time_digits'))):
 b&=4095;add('timer',str(n),native(b,2,2),fixed(b,2,2),1429)
for i,g in enumerate(ns['orig']):add('popups',('P1' if i<8 else 'P2')+' phase '+str(i%8),px(g['native_rows']),px(F.popup_maps[g['source']]['rows']),0xa2 if i<8 else 0xa3)
for g in ns['game']['go_frames']:
 b=g['base'];add('go',f"GO {g['kind']} / {b:03X}",native(b,4,4),fixed(b,4,4),0xa2)
for name in ['condensed_6x12','result_12x12']:
 for ch,g in ns['fonts'][name]['glyphs'].items():
  if name=='condensed_6x12':b=0x400+(ord(ch)>>4)*32+(ord(ch)&15);w,h=1,2;p=1424
  else:b=0xba0 if ch=='P' else numbers(body((R/'generated/mission-result-data.c').read_text(),'mission_result_digits'))[int(ch)]&4095;w,h=2,2;p=1426
  add('condensed' if w==1 else 'result',ch,native(b,w,h),fixed(b,w,h),p)
clean=json.loads((R/'art/gameplay-text/hud-clean.json').read_text())['fixed_cells']
for code in sorted(map(int,clean)):
 ch=chr(code-0x300);add('hud',f'{code:03X} '+(ch if ch.isprintable() else ''),raw[code],fixed(code,1,1),1424)
for g in ns['game']['objects']:add('pickup',g['character'],obj(g['source']),px(F.score_maps[g['source']]['rows']),0x1b8)
for code,ch in F.maps.items():
 a=obj(code);add('mission',ch,a,F.drawing(code,ns['scale'](a)),0x1aa)
def render(a,p):
 c=pal[p,a];rgb=np.stack([c&31,c>>5&31,c>>10&31],-1).astype('uint8')*8;rgb[a==0]=[40,48,56];return Image.fromarray(rgb)
def sheet(items,path,z=5):
 hs=[max(a.shape[0],b.shape[0])*z+32 for _,a,b,_ in items];im=Image.new('RGB',(620,45+sum(hs)),'#283038');d=ImageDraw.Draw(im)
 d.text((110,12),'Original Neo Geo / native pixels',fill='white');d.text((375,12),'Approved GBA / native pixels',fill='white');y=42
 for (label,a,b,p),h in zip(items,hs):
  d.text((10,y+4),label,fill='white')
  for x,v in [(120,a),(390,b)]:
   q=render(v,p);im.paste(q.resize((q.width*z,q.height*z),Image.Resampling.NEAREST),(x,y))
  y+=h
 im.save(path)
overview=groups['labels']+[groups['timer'][n] for n in (0,1,3,4,8)]+[groups['popups'][i] for i in (4,12)]+[g for g in groups['go'] if g[0].endswith(('840','980','E40'))]
overview += [q for q in groups['hud'] if q[0][:3] in ('322','323','324','325','326','32A','32B','35B','35D')]
overview += [q for q in groups['mission'] if q[0] in ('M','S','C','E')]
sheet(overview,D/'overview.png')
# Manageable sheets: every glyph/frame included, at most12 rows per image.
sections=[]
for key,items in groups.items():
 files=[]
 for off in range(0,len(items),12):
  name=f'{key}-{off//12+1}.png';sheet(items[off:off+12],D/name);files.append(name)
 sections.append((key,len(items),files))
names={'labels':'ARMS · BOMB · CANNON','timer':'타이머 숫자','popups':'P1/P2 전체 프레임','go':'GO 전체 프레임','condensed':'세로형 영문·숫자','result':'결과 숫자·P','hud':'작은 HUD 문자','pickup':'획득 점수 숫자','mission':'MISSION / COMPLETE 문자'}
page='''<!doctype html><html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>확정 폰트·HUD 전체 원본 비교</title><style>body{background:#283038;color:#eee;font:16px/1.7 system-ui;max-width:900px;margin:24px auto;padding:0 14px}img{max-width:100%;image-rendering:pixelated}summary{padding:14px;background:#394650;cursor:pointer}details{margin:14px 0}table{border-collapse:collapse;width:100%}td,th{padding:8px;border-bottom:1px solid #63717b;text-align:left}a{color:#8cd7ff}</style><h1>확정 폰트·HUD 전체 원본 비교</h1><p>숫자 대칭·직선 사선·괄호·특수문자·도트 그라데이션·MISSION 내부 음영을 반영한 에셋입니다. 5픽셀 M은 유지했습니다. 왼쪽은 축소하지 않은 네오지오 원본, 오른쪽은 GBA 에셋입니다. 양쪽 모두 픽셀당 동일한 5배 확대이며, 원본의 크기를 75%로 맞추지 않았습니다.</p><p>색상은 해당 폰트의 원본 팔레트를 사용했습니다. 점수·MISSION처럼 색이 바뀌는 항목은 대표 팔레트 한 상태를 비교합니다. 실행 화면이 아닌 에셋 자체의 비교입니다.</p><p>ARMS는 R을3픽셀로 조정해18픽셀 폭을 유지했고, BOMB는 마지막 B를 옮겨24픽셀 안에5픽셀 M을 넣었습니다. CANNON 하단 진한 색 제거, P1/P2 외형 수정, GO 세 방향 정지 자세의 사선 정리를 포함합니다. GO 40프레임에 도트 그라데이션을 적용했습니다. 사선 윤곽 재작성은 세 방향의 정지 자세에 적용했습니다. 숫자0·8은 외형의 양축 대칭,3은 상하 대칭을 검사했습니다. 대괄호 삐침은 세로획을 포함해2픽셀이고, 별표는5픽셀 폭 안에서 좌우 대칭입니다. +는5×5로 네 끝을 각각1픽셀 연장했습니다.</p>'''
page+='<table><tr><th>항목</th><th>비교 개수</th></tr>'+''.join('<tr><td>'+names[k]+'</td><td>'+str(n)+'</td></tr>' for k,n,_ in sections)+'</table>'
def img(name):return '<img loading="lazy" src="data:image/png;base64,'+base64.b64encode((D/name).read_bytes()).decode()+'">'
page+='<h2>주요 항목</h2>'+img('overview.png')
for k,n,files in sections:page+='<details><summary>'+names[k]+' — '+str(n)+'</summary>'+''.join(img(f) for f in files)+'</details>'
page+='<p>확정 에셋 선택을 소스에 연결했습니다. 이 변경으로 ROM을 새로 빌드하거나 실행 검증하지는 않았습니다. 기존 배포 ROM은 v0.5.123-hud입니다. 타이틀 로고·장식 아이콘·배경 등 이번 폰트 수정 대상 밖의 그래픽은 포함하지 않았습니다.</p></html>'
(D/'All-fonts-HUD-original-comparison.html').write_text(page)
manifest={'approved_M_width':5,'asset_sha256':F.revision,'counts':{k:len(v) for k,v in groups.items()},'reference_resized':False,'zoom':5,'rom_built':False,'pipeline_selection':'art/ui131/overrides.json'}
(D/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(manifest)
