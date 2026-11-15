"""Pixel-authored symmetry, brackets, source dithering and bevel revision."""
import json,base64
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
from compile_m1_native_maps import body,numbers
from approved_font_overrides import ApprovedFonts
R=Path(__file__).resolve().parents[1];A=R/'art/ui131';D=R/'docs/ui131';G=R/'generated'
F=ApprovedFonts();new=json.loads((R/'art/ui130/approved.json').read_text());cells=new['fixed_cells'];objs={g['source']:g for g in new['objects']};audit=[]
def px(s):return np.array([[int(v,16) for v in r] for r in s],np.uint8)
def rows(a):return [''.join(format(int(v),'x') for v in r) for r in a]
def emit(a,b,w,h):
 for y in range(h):
  for x in range(w):cells[str(b+x+y*16)]=rows(a[y*6:y*6+6,x*6:x*6+6])
def frame(b,w,h):return np.block([[px(cells[str(b+x+y*16)]) if str(b+x+y*16) in cells else F.fixed[b+x+y*16] for x in range(w)] for y in range(h)])
patterns={
'0':['01110','11011','10001','10001','10001','10001','10001','11011','01110'],
'8':['01110','11011','10001','11011','01110','11011','10001','11011','01110'],
'3':['11110','00011','00001','00011','01110','00011','00001','00011','11110'],
'4':['00010','00110','01010','10010','11111','00010','00010','00010','00010'],
'1':['00100','01100','00100','00100','00100','00100','00100','00100','11111']}
def digit(ch,w,h,shades,border=True):
 # Symmetric coordinate map preserves both axes for0/8 at odd/even sizes.
 a=np.array([[v=='1' for v in r] for r in patterns[ch]],np.uint8)
 mw=w-2 if border else w;mh=h-2 if border else h
 ix=np.floor((np.arange(mw)+.5)*5/mw).astype(int);iy=np.floor((np.arange(mh)+.5)*9/mh).astype(int);m=a[iy[:,None],ix[None,:]].astype(bool)
 if ch in ('0','8'):m=m|m[::-1]|m[:,::-1]|m[::-1,::-1]
 if ch=='3':m=m|m[::-1]
 if ch=='1':
  # Centered foot, with no sampling-induced left/right bias.
  m[-1]=True
 if ch=='4':
  m[:]=False;stem=mw-2
  for y in range(max(2,mh-3)):
   x=max(0,stem-y);m[y,x]=True;m[y,stem]=True
  cross=min(mh-2,stem+1);m[cross,:]=True;m[:,stem]=True
 if w==12 and h==12:
  large={
   '0':['01111110','11111111','11000011','11000011','11000011','11000011','11000011','11000011','11111111','01111110'],
   '8':['01111110','11000011','11000011','11000011','01111110','01111110','11000011','11000011','11000011','01111110'],
   '3':['11111100','11111110','00000111','00000111','00111110','00111110','00000111','00000111','11111110','11111100'],
   '1':['00111000','01111000','00011000','00011000','00011000','00011000','00011000','00011000','11111111','11111111'],
   '4':['00000110','00001110','00011110','00110110','01100110','11000110','11111111','11111111','00000110','00000110']}
  m=np.array([[v=='1' for v in row] for row in large[ch]]);mw,mh=8,10
 out=np.zeros((h,w),np.uint8);mask=np.zeros_like(out,dtype=bool);off=1 if border else 0;ox=(w-mw)//2;mask[off:off+mh,ox:ox+mw]=m
 if border:
  for y,x in np.argwhere(mask):
   for dy,dx in ((-1,0),(1,0),(0,-1),(0,1)):
    yy,xx=y+dy,x+dx
    if 0<=yy<h and 0<=xx<w:out[yy,xx]=1
 for y,x in np.argwhere(mask):out[y,x]=shades[min(len(shades)-1,max(0,y-off)*len(shades)//mh)]
 if ch in ('0','8'):
  assert np.array_equal(out!=0,(out!=0)[::-1]) and np.array_equal(out!=0,(out!=0)[:,::-1])
 if ch=='3':assert np.array_equal(out!=0,(out!=0)[::-1])
 audit.append({'digit':ch,'size':[w,h],'symmetry_checked':ch in ('0','8','3')});return out
# Both large numeric families, condensed and small HUD digits.
time=numbers(body((G/'session-data.c').read_text(),'session_time_digits'));result=numbers(body((G/'mission-result-data.c').read_text(),'mission_result_digits'))
for ch in patterns:
 n=int(ch)
 emit(digit(ch,12,12,[8,7,6,5,4,3,2]),time[n]&4095,2,2)
 emit(digit(ch,12,12,[15,15,8,7,6,5]),result[n]&4095,2,2)
 b=0x400+(ord(ch)>>4)*32+(ord(ch)&15);emit(digit(ch,6,12,[15,15,8,7,6,5]),b,1,2)
 #5x5 glyph centered in6x6; symmetric glyph excludes unused cell padding.
 a=np.zeros((6,6),np.uint8);a[:5,:5]=digit(ch,5,5,[15,15,15,4,4],False);emit(a,0x330+n,1,1)
# Popup score digits use a symmetric7x9 footprint inside the12x12 object.
score=json.loads((R/'art/gameplay-text/glyphs.json').read_text())
score_updates=[]
for g in score['objects']:
 if g['character'] in patterns:
  a=np.zeros((12,12),np.uint8);a[2:11,2:9]=digit(g['character'],7,9,[2,3,4,5,6,7,8]);score_updates.append({'source':g['source'],'character':g['character'],'rows':rows(a)})
# Bracket terminals measure2px INCLUDING the vertical stem. Same mask mirrored.
a=np.zeros((6,6),np.uint8);a[:5,1]=15;a[0,2]=15;a[4,2]=4;a[4,1]=4
# Clear gaps (no asymmetric shadow); fit the common5px cap height.
emit(a,0x35b,1,1);emit(a[:,::-1],0x35d,1,1)
# Punctuation rebuilt at native5x5 with distinct stems and counters.
symbols={'+':['00100','00100','11111','00100','00100'],'"':['11011','11011','01010','00000','00000'],'#':['01010','11111','01010','11111','01010'],'$':['01111','10100','01110','00101','11110'],'%':['11001','11010','00100','01011','10011'],'&':['01100','10010','01100','10011','01101'],'*':['00100','10101','01110','10101','00100']}
for ch,pattern in symbols.items():
 a=np.zeros((6,6),np.uint8)
 for y,row in enumerate(pattern):
  for x,v in enumerate(row):
   if v=='1':a[y,x]=15 if y<3 else 4
 emit(a,0x300+ord(ch),1,1)
assert np.array_equal((a[:5,:5]!=0),(a[:5,:5]!=0)[:,::-1])
# Ordered two-colour transitions in original palette, rather than smooth RGB.
bayer=np.array([[0,2],[3,1]],float)/4
def ramp(v,seq,x,y):
 v=max(0,min(len(seq)-1,float(v)));lo=int(v);return seq[min(len(seq)-1,lo+int((v-lo)>bayer[y%2,x%2]))]
def colour(a):
 a=a.copy()
 for l,r in ((0,11),(11,24)):
  yy,xx=np.where(np.isin(a[:,l:r],[10,11,12]))
  if len(xx):
   d=xx-.38*yy;lo,hi=float(d.min()),float(d.max())
   for y,x,v in zip(yy,xx,d):a[y,l+x]=ramp(2*(v-lo)/max(.01,hi-lo),[12,11,10],x+l,y)
 return a
def triangle(phase):
 out=np.zeros((24,24),np.uint8);half=[0,2,4,7,10,7,4,2][phase]
 if not half:return out
 pts=[(12-half,16),(12+half,16),(12,23)];im=Image.new('L',(24,24));dr=ImageDraw.Draw(im);dr.polygon(pts,fill=1);mask=np.array(im,dtype=bool)
 for y,x in np.argwhere(mask):out[y,x]=ramp((y-16)*2/7,[6,7,8],x,y)
 e=Image.new('L',(24,24));ImageDraw.Draw(e).polygon(pts,outline=1);out[np.array(e,dtype=bool)]=15
 return out
for i,g in enumerate(new['objects']):
 a=colour(px(g['rows']));face=np.isin(a,[10,11,12]);before=a.copy()
 # A separate gray rim encloses the colored letter face; existing white
 # outer highlight and the offset black backing remain outside this rim.
 for y,x in np.argwhere(face):
  for dy,dx in ((0,-1),(0,1),(-1,0),(1,0)):
   yy,xx=y+dy,x+dx
   if 0<=yy<15 and 0<=xx<24 and not face[yy,xx] and before[yy,xx] in (1,9,13,14):a[yy,xx]=14 if dy<=0 else 13
 ys,xs=np.where(np.isin(a,[3,4,5,6,7,8]))
 for y,x in zip(ys,xs):a[y,x]=ramp((y-5)*5/18,[3,4,5,6,7,8],x,y)
 a[16:]=triangle(i%8)[16:];g['rows']=rows(a)
# GO face/arrow dithering, preserving direction-specific source gradient.
for g in score['go_frames']:
 b=g['base'];a=colour(frame(b,4,4));old=a.copy()
 for y,x in np.argwhere(np.isin(old,[3,4,5,6,7,8])):
  q=old[max(0,y-1):y+2,max(0,x-1):x+2];v=q[np.isin(q,[3,4,5,6,7,8])];a[y,x]=ramp(float(v.mean())-3,[3,4,5,6,7,8],x,y)
 if b==0x980:
  # Continuous straight tip and lower edge; foreground letters stay in front.
  pts=[(0,19),(12,16),(12,11),(22,17),(0,22)];im=Image.new('L',(24,24));ImageDraw.Draw(im).polygon(pts,fill=1);m=np.array(im,dtype=bool);z=np.zeros_like(a)
  for y,x in np.argwhere(m):z[y,x]=ramp(x*2/22,[6,7,8],x,y)
  e=Image.new('L',(24,24));ImageDraw.Draw(e).polygon(pts,outline=1);z[np.array(e,dtype=bool)]=15
  yy,xx=np.indices(a.shape);keep=((xx<=10)&(yy<=16))|((xx<=16)&(yy<=12))|((xx>=17)&(xx<=20)&(yy<=14))
  keep &= (a!=0)&~np.isin(a,[3,4,5,6,7,8]);z[keep]=a[keep];a=z
 emit(a,b,4,4)
# Mission face uses source warm ramp and inset upper-left shadow/lower-right
# glint: recessed lettering lit from the upper-left, within the existing rim.
mission={}
for ch,g in F.mission.items():
 a=np.array(F.provenance['mission_index_remap'],np.uint8)[px(g['rows'])];mask=np.isin(a,list(range(9,15)));yy,xx=np.where(mask)
 if len(yy):
  lo,hi=int(yy.min()),int(yy.max())
  for y,x in zip(yy,xx):
   v=(y-lo)*5/max(1,hi-lo)+.25*x/max(1,a.shape[1]-1)
   top=y==0 or not mask[y-1,x];left=x==0 or not mask[y,x-1];bottom=y==a.shape[0]-1 or not mask[y+1,x];right=x==a.shape[1]-1 or not mask[y,x+1]
   if top or left:v+=1.4
   elif bottom or right:v-=1.0
   a[y,x]=ramp(v,[14,13,12,11,10,9],x,y)
 mission[ch]=rows(a)
new.update(version=131,score_objects=score_updates,mission_indexed=mission,policy='Symmetric digit silhouettes; 2px bracket terminals; source ordered dithering; gray popup rims; straight arrow edges; inset mission shading.')
(A/'overrides.json').write_text(json.dumps(new,indent=2)+'\n');(D/'checks.json').write_text(json.dumps({'digit_checks':audit,'bracket_terminal_pixels':2,'brackets_mirrored':True,'rom_built':False},indent=2)+'\n')
print('written UI131',len(audit),'digit variants;17 mission glyphs; brackets;16 popups;40 GO frames')
