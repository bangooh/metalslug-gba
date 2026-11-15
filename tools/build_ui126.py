"""Source-style UI cleanup. Indexed native pixels, offline only.
Original references always use full Neo Geo pixels, never resized to GBA size.
"""
import json,base64,hashlib
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
from import_scaled_drawings import unpack_tiles,scale,rows as crows
from compile_m1_native_maps import body,numbers
from approved_font_overrides import ApprovedFonts
R=Path(__file__).resolve().parents[1]; A=R/'art/ui126';D=R/'docs/ui126';G=R/'generated'
A.mkdir(exist_ok=True);D.mkdir(exist_ok=True)
def load(p):return json.loads(p.read_text())
def px(s):return np.array([[int(c,16) for c in r] for r in s],np.uint8)
def rows(a):return [''.join(format(int(v),'x') for v in r) for r in a]
s=(G/'drawing-data.c').read_text();pal=np.array(numbers(body(s,'native_source_palettes'))).reshape(-1,16)
raw=unpack_tiles((G/'native-fixed.4bpp').read_bytes());old=ApprovedFonts(revision=False);fixed={};objects=[];comparisons=[]
def frame(base,w,h):return np.block([[raw[base+x+y*16] for x in range(w)] for y in range(h)])
def oldframe(base,w,h):return np.block([[old.fixed.get(base+x+y*16,scale(raw[base+x+y*16])) for x in range(w)] for y in range(h)])
def emit(a,b,w,h):
 for y in range(h):
  for x in range(w):fixed[str(b+x+y*16)]=rows(a[y*6:y*6+6,x*6:x*6+6])
def add(label,native,before,after,bank):comparisons.append((label,native,before,after,bank))
def bands(a,indices,shades):
 a=a.copy();m=np.isin(a,indices);ys=np.where(m)[0]
 if len(ys):
  lo,hi=int(ys.min()),int(ys.max())
  for y,x in np.argwhere(m):a[y,x]=shades[min(len(shades)-1,(y-lo)*len(shades)//(hi-lo+1))]
 return a
# P1/P2: preserve source front/bevel/back indices; diagonal colour groups.
orig=load(R/'art/ui124/source-popups-75.json')
for i,g in enumerate(orig):
 a=px(g['rows']);phase=i%8
 if phase!=3:
  # Recover white leading edges lost by the legacy 150% -> 50% sampler.
  for y in range(2,15):
   xs=np.flatnonzero(np.isin(a[y,12:],[10,11,12]))+12
   if len(xs) and a[y,xs[0]-1] in (0,3,4,5,6,7,8,9):a[y,xs[0]]=15
  for y in (6,):
   if a[y,6] in (10,11,12):a[y,6]=14
 # Use local letter coordinates: bright lower-left, dark upper-right.
 for left,right in ((0,11),(11,24)):
  m=np.isin(a[:,left:right],[10,11,12]);yy,xx=np.where(m)
  if len(xx):
   depth=xx-.38*yy;lo,hi=float(depth.min()),float(depth.max())
   for y,x,q in zip(yy,xx,depth):a[y,left+x]=12-min(2,int(3*(q-lo)/max(.01,hi-lo+.01)))
 # Arrow is a separate surface with source vertical red->yellow / blue->cyan.
 a=bands(a,list(range(3,9)),[3,4,5,6,7,8])
 if phase==4:
  # Keep the original wide, shallow triangle and offset dark backing.
  a[17:]=0
  for y,l,r in [(17,3,20),(18,5,19),(19,6,17),(20,8,16),(21,9,14),(22,11,13),(23,12,12)]:
   if y<23:a[y,max(0,l-1)]=9
   a[y,l:r+1]=7 if y<20 else 8;a[y,l]=15;a[y,r]=14 if y<21 else 15
  a[23,12]=15
 objects.append({'source':g['source'],'rows':rows(a)})
 if phase==4:add('P1' if i<8 else 'P2',px(g['native_rows']),px(old.popup_maps[g['source']]['rows']),a,0xa2 if i<8 else 0xa3)
# Timer: source angular silhouette and counters, coherent ORIGINAL vertical palette.
for n,b in enumerate(numbers(body((G/'session-data.c').read_text(),'session_time_digits'))):
 b&=4095;native=frame(b,2,2);a=bands(scale(native),list(range(2,9)),[8,7,6,5,4,3,2]);emit(a,b,2,2)
 add('Timer '+str(n),native,oldframe(b,2,2),a,1429)
# Native label strokes: explicit masks avoid sampling away their thin horizontals.
label_faces={'A':['0110','1001','1111','1001','1001'],'R':['1110','1001','1110','1010','1001'],'M':['1001','1111','1001','1001','1001'],'S':['111','100','010','001','110'],'B':['110','101','110','101','110'],'O':['010','101','101','101','010'],'C':['11','10','10','10','11'],'N':['101','111','111','101','101']}
for label,b,w in [('ARMS',0xab1,3),('BOMB',0xae4,4),('CANNON',0xae0,4)]:
 native=frame(b,w,1);a=np.zeros((6,w*6),np.uint8);mask=np.zeros_like(a,dtype=bool)
 faces=dict(label_faces)
 if label=='CANNON':faces['A']=['010','101','111','101','101']
 extent=sum(len(faces[ch][0]) for ch in label)+len(label)-1
 x=max(0,(w*6-extent-1)//2)
 for ch in label:
  for y,row in enumerate(faces[ch]):
   for xx,v in enumerate(row):
    if v=='1':mask[y,x+xx]=True
  x+=len(faces[ch][0])+1
 for y,x in np.argwhere(mask):
  if y+1<6 and x+1<w*6:a[y+1,x+1]=1
 for y,x in np.argwhere(mask):a[y,x]=(10 if y<3 else 9) if label=='CANNON' else (6 if y<3 else 14)
 if label!='ARMS':a[:,-1]=7
 emit(a,b,w,1);add(label,native,oldframe(b,w,1),a,1447)
# GO: use all source poses rather than warping a generic G/O mask.
game=load(R/'art/gameplay-text/glyphs.json')
for g in game['go_frames']:
 b=g['base'];native=frame(b,4,4);a=scale(native)
 # Palette9 is backing;13/14 bevel and15 highlight stay separate.
 for y,x in np.argwhere(np.isin(a,[10,11,12])):
  local=x if x<10 else x-10
  a[y,x]=12 if local-.35*y<1.5 else 11 if local-.35*y<4 else 10
 # Average only the source arrow's own gradient indices. Its light
 # direction differs for right/slanted/up poses; never force vertical bands.
 before=a.copy()
 for y,x in np.argwhere(np.isin(before,[3,4,5,6,7,8])):
  region=before[max(0,y-1):y+2,max(0,x-1):x+2];v=region[np.isin(region,[3,4,5,6,7,8])]
  a[y,x]=int(np.floor(float(v.mean())+.5))
 emit(a,b,4,4)
 if b in (0x840,0x980,0xe40):add('GO '+str(g['kind']),native,px(g['rows']),a,0xa2)
# Preserve approved letter shapes; remove alternating colour speckles only.
fonts=load(R/'art/approved-fonts/fonts.json')['fonts']
for name in ['condensed_6x12','result_12x12']:
 for ch,g in fonts[name]['glyphs'].items():
  if name=='condensed_6x12':b=0x400+(ord(ch)>>4)*32+(ord(ch)&15);w,h=1,2
  else:b=0xba0 if ch=='P' else numbers(body((G/'mission-result-data.c').read_text(),'mission_result_digits'))[int(ch)]&4095;w,h=2,2
  prev=oldframe(b,w,h);a=bands(scale(frame(b,w,h)) if name=='result_12x12' and ch in '89' else px(g['rows']),list(range(2,16)),[15,15,15,8,7,6,5]);emit(a,b,w,h)
  if name=='result_12x12' or ch in '01259AMNR':add(name+' '+ch,frame(b,w,h),prev,a,1426 if name=='result_12x12' else 1424)
# Repair the former result8/9 address mistake without damaging unrelated labels.
atlas=load(R/'art/approved-fonts/fix-glyphs.json');it=iter(atlas['glyphs']);basecells={}
for base,count in atlas['ranges']:
 for code in range(base,base+count):basecells[code]=next(it)
for code in (0xb80,0xb81,0xb82,0xb83,0xb90,0xb91,0xb92,0xb93):fixed[str(code)]=basecells[code]
# Small HUD and pickup digits already have consistent single-pixel strokes.
# Compare all digits; retain these authored shapes rather than applying erosion.
for n in range(10):
 b=0x330+n;add('HUD '+str(n),raw[b],old.fixed[b],old.fixed[b],1424)
# Full OBJ source extraction for score and mission references.
maps=crows(s,'native_draw_maps');pieces=crows(s,'native_draw_pieces');tiles=numbers(body(s,'native_draw_tiles'))
ledger=load(G/'drawing-source.json');lookup={m['source']:i for i,m in enumerate(ledger['maps'])}
sprites=unpack_tiles((G/'native-sprites.4bpp').read_bytes()).reshape(-1,2,2,8,8).transpose(0,1,3,2,4).reshape(-1,16,16)
def source_obj(source):
 first,count,attr=maps[lookup[source]];pp=pieces[first:first+count];l=min(p[1] for p in pp)//4*4;t=min(-p[2] for p in pp)//4*4;r=(max(p[1]+16*p[3] for p in pp)+3)//4*4;b=(max(-p[2]+16*p[4] for p in pp)+3)//4*4;a=np.zeros((b-t,r-l),np.uint8)
 for start,x,y,w,h in pp:
  for xx in range(w):
   for yy in range(h):
    src=sprites[((attr>>4)<<16)|tiles[start+xx*h+yy]];dst=a[-y-t+16*yy:-y-t+16*yy+16,x-l+16*xx:x-l+16*xx+16];np.copyto(dst,src,where=src!=0)
 return a
for g in game['objects']:add('Pickup '+g['character'],source_obj(g['source']),px(g['rows']),px(g['rows']),0x1b8)
for source,ch in old.maps.items():
 native=source_obj(source);a=old.drawing(source,scale(native));add('Mission '+ch,native,a,a,0x1aa)
# Serialize separate replacement layer; unchanged original provenance remains intact.
out={'version':126,'policy':'Source-specific indexed surfaces; no runtime rasterization; no anti-aliasing','fixed_cells':fixed,'objects':objects}
(A/'overrides.json').write_text(json.dumps(out,indent=2)+'\n')
def render(a,bank):
 c=pal[bank,a];rgb=np.stack([c&31,c>>5&31,c>>10&31],-1).astype('uint8')*8;rgb[a==0]=[40,48,56];return Image.fromarray(rgb)
def sheet(items,name,z=5):
 heights=[max(a.shape[0],b.shape[0],c.shape[0])*z+42 for _,a,b,c,_ in items];im=Image.new('RGB',(960,sum(heights)+50),'#283038');d=ImageDraw.Draw(im)
 for x,t in [(130,'Original Neo Geo pixels'),(430,'Released v123'),(700,'Revised GBA pixels')]:d.text((x,12),t,fill='white')
 y=40
 for (label,native,prev,new,bank),h in zip(items,heights):
  d.text((10,y+5),label,fill='white')
  for x,a in zip([135,435,705],[native,prev,new]):
   zimg=render(a,bank);im.paste(zimg.resize((zimg.width*z,zimg.height*z),Image.Resampling.NEAREST),(x,y))
  y+=h
 im.save(D/name)
sheet([q for q in comparisons if q[0] in ['P1','P2','Timer 0','Timer 2','ARMS','BOMB','CANNON','GO 0']],'overview.png',5)
for family,prefix in [('popups','P'),('timer','Timer'),('labels','ARM'),('go','GO'),('result','result_'),('condensed','condensed_'),('hud','HUD'),('pickup','Pickup'),('mission','Mission')]:
 items=[q for q in comparisons if q[0].startswith(prefix)]
 if family=='popups':items=[q for q in comparisons if q[0] in ('P1','P2')]
 if family=='labels':items=[q for q in comparisons if q[0] in ('ARMS','BOMB','CANNON')]
 sheet(items,family+'.png',5)
# all revised popup phases for animation consistency review
im=Image.new('RGB',(768,192),'#283038')
for i,g in enumerate(objects):im.paste(render(px(g['rows']),0xa2 if i<8 else 0xa3).resize((96,96),Image.Resampling.NEAREST),((i%8)*96,i//8*96))
im.save(D/'popup-phases.png')
validation={'fixed_cells':len(fixed),'popup_maps':len(objects),'GO_frames':len(game['go_frames']),'condensed_glyphs':len(fonts['condensed_6x12']['glyphs']),'result_glyphs':11,'retained_clean_assets':'95 small HUD cells,10 pickup digits,17 approved mission maps','source_reference':'full-resolution indexed Neo Geo assets, same integer zoom; never source75%','scope':'Authored asset layer and static pixel comparisons. No new ROM/playthrough/performance claim.'}
for a in fixed.values():assert px(a).shape==(6,6) and px(a).max()<16
for g in objects:assert px(g['rows']).shape==(24,24) and px(g['rows']).max()<16
(D/'validation.json').write_text(json.dumps(validation,indent=2)+'\n')
html='''<!doctype html><meta charset="utf-8"><title>UI126 원본 스타일 대조</title><style>body{background:#283038;color:#eee;font:16px/1.7 system-ui;max-width:1000px;margin:30px auto}img{max-width:100%;image-rendering:pixelated}summary{cursor:pointer;padding:12px;background:#38434e}p{padding:0 12px}</style><h1>원본 스타일을 유지하는 글꼴 정리</h1><p>왼쪽: 축소하지 않은 네오지오 원본. 가운데: 배포 v123. 오른쪽: 새 GBA 에셋. 모두 원본 한 픽셀당 동일한 5배 확대입니다. 크기가 다른 것은 각 기기의 실제 픽셀 수 차이입니다. 에셋 대조이며 실행 화면이 아닙니다.</p><p>P1/P2의 흰색·회색 앞 테두리와 어긋난 검정·회색 뒷면을 구분하고, 컬러 면은 좌하단에서 우상단으로 어둡게 정리했습니다. 타이머·결과 글자는 원본의 위아래 색 변화를 유지합니다. GO는 원본 형태로 복원하고, CANNON까지 포함했습니다. 작은 HUD·획득 점수·MISSION 글자는 기존 정돈된 획을 유지해 함께 비교했습니다.</p>'''
for name,title in [('overview','주요 변경'),('popups','P1 / P2'),('popup-phases','P1 / P2 전체 프레임 — 새 에셋'),('timer','타이머 0–9'),('labels','ARMS / BOMB / CANNON'),('go','GO 세 방향'),('result','결과 숫자'),('condensed','세로형 영문'),('hud','작은 HUD — 유지'),('pickup','획득 점수 — 유지'),('mission','MISSION 글자 — 유지')]:
 html+='<details'+(' open' if name=='overview' else '')+'><summary>'+title+'</summary><img src="data:image/png;base64,'+base64.b64encode((D/(name+'.png')).read_bytes()).decode()+'"></details>'
html+='<p>현재 단계: 폰트 에셋과 원본 대조. 새 ROM 실행 검증 및 성능 측정은 포함하지 않습니다.</p>'
(D/'UI126-source-style.html').write_text(html)
print(validation)
if __name__=='__main__':pass
