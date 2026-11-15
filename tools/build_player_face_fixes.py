#!/usr/bin/env python3
"""Marco eye / Tarma sunglasses -> art/player-face-fixes/patch.json

Stage 4 of the player repairs (after animation, solid and twin fixes).
It never touches the source sprites.

The 75% frames were reduced and reviewed one by one, so the same face
feature came out 1, 2 or 3 pixels tall and 1 or 2 wide from frame to frame.

Marco   an OPEN eye seen from the side is always 1 pixel wide and 2 tall,
        dark brown (index 11) like the approved idle frames.
Tarma   sunglasses seen from the side always take the shape of the user's
        reference frame: a black (index 7) top row 3 pixels wide and the
        2 back pixels under it.

Where the feature is: it is located in the untouched SOURCE map and carried
to the 75% frame through the importer's own sampling grid.
  Marco  a black pixel with skin in front of it, right under a LEVEL run of
         bandana pixels (black, usually over one brown pixel).
  Tarma  a 3x2 black block under the hair with nothing black above or in
         front of it.
Frames where the source shows no such feature are left exactly as they
are: closed or squeezed eyes, faces turned to the camera, heads tilted up,
back views, deaths. Those keep their natural drawing.
"""
import json,sys,collections
from pathlib import Path
import numpy as np
from scipy import ndimage as ndi
sys.path.insert(0,str(Path(__file__).resolve().parent))
import build_animation_consistency as bac
import build_player_animation_fixes as stage1
import build_player_solid_fixes as stage2
R=Path(__file__).resolve().parents[1];G=R/'generated';OUT=R/'art/player-face-fixes'
SKIN={8,9,10};BLACK=7;BROWN=11;DARK={BLACK,BROWN};EYE=BROWN;LENS=BLACK
hexrows=stage1.hexrows;s75=stage1.s75;unhex=stage2.unhex
def near_hair(S,y,x,up=7,side=8):
    w=S[max(0,y-up):y+1,max(0,x-side):min(S.shape[1],x+side+1)];return int(((w==14)|(w==15)).sum())>=4
def marco_eye(S):
    """[(y,x)] of the top pixel of an open side-view eye in a source map."""
    H,W=S.shape;out=[];band=(S==1)|(S==4)
    for y,x in zip(*np.nonzero(S==BLACK)):
        if not(1<=x<W-1 and 2<=y<H-1):continue
        if S[y,x+1] not in SKIN or S[y,x-1] not in SKIN|{BROWN} or S[y-1,x] in (BLACK,0):continue
        # a level bandana right above: the head is upright and seen from the side
        if max(int(band[yy,max(0,x-6):x+2].sum()) for yy in (y-1,y-2))<4 or not near_hair(S,y,x):continue
        if not(S[y+1,x]==BROWN and S[y+1,x+1] in SKIN) and S[y+1,x] not in SKIN:continue
        out.append((int(y),int(x)))
    return out
def tarma_glasses(S):
    """[(y,x)] top-left of a side-view sunglasses lens in a source map."""
    B=S==BLACK;out=[];H,W=S.shape
    for y in range(1,H-2):
        for x in range(1,W-3):
            if not B[y:y+2,x:x+3].all() or B[y-1,x:x+3].any() or B[y:y+2,x+3].any() or B[y:y+2,x-1].all() or B[y+2,x:x+3].all():continue
            if near_hair(S,y,x):out.append((y,x))
    return out
def first_kept(k,values):
    for v in values:
        i=np.nonzero(k==v)[0]
        if len(i):return int(i[0])
    return None
def skin_for(out,src,ky,kx,r,c):
    """A skin colour for scaled pixel (r,c): its own source pixel when that is skin, else the nearest skin beside it."""
    if 0<=ky[r]<src.shape[0] and 0<=kx[c]<src.shape[1] and src[ky[r],kx[c]] in SKIN:return int(src[ky[r],kx[c]])
    for dc in (1,-1,2,-2):
        if 0<=c+dc<out.shape[1] and out[r,c+dc] in SKIN:return int(out[r,c+dc])
    return 9
def predicted(ky,kx,y,x):
    """Scaled cell the importer grid gives for source pixel (y,x); the reviewed art may sit 1px off it."""
    r=first_kept(ky,(y,y+1,y-1));c=first_kept(kx,(x,x-1,x+1));return r,c
def fix_marco(cur,src,ky,kx):
    eyes=marco_eye(src)
    if len(eyes)!=1:return cur,dict(status='no open side-view eye' if not eyes else 'ambiguous')
    r0,c0=predicted(ky,kx,*eyes[0]);out=cur.copy();H,W=out.shape
    if r0 is None or c0 is None:return cur,dict(status='eye outside the frame')
    ok=lambda r,c:1<=r<H-1 and 1<=c<W-1
    # The eye pixel itself came out white (index 1, from the headband right above it in
    # the source): the eye looks empty in that frame only. Paint it where it stands.
    if ok(r0,c0) and out[r0,c0]==BAND_WHITE and out[r0,c0-1]==10 and out[r0,c0+1] in {10,11}:
        before=''.join('%x'%out[rr,cc] for rr in (r0,r0+1) for cc in (c0-1,c0,c0+1))
        out[r0,c0]=EYE
        if out[r0+1,c0] in SKIN|{10,11}:out[r0+1,c0]=EYE
        return out,dict(status='fixed',at=[int(r0),int(c0)],drawn=0,before=before)
    # the eye already drawn there: a dark pixel with skin in front of it, searched around the predicted cell
    found=None
    for dr,dc in sorted(((dr,dc) for dr in (-1,0,1,2) for dc in (-1,0,1)),key=lambda t:(abs(t[0])+abs(t[1]),abs(t[1]),-t[0])):
        r,c=r0+dr,c0+dc
        if ok(r,c) and out[r,c] in DARK and out[r,c+1] in SKIN and not(out[r,c-1] in DARK and out[r,c-2 if c>=2 else 0] in DARK):found=(r,c);break
    before=''.join('%x'%out[rr,cc] for rr in range(max(0,r0-1),min(H,r0+4)) for cc in range(max(0,c0-2),min(W,c0+3)))
    if found:
        r,c=found
        while ok(r-1,c) and out[r-1,c] in DARK and out[r-1,c+1] in SKIN and out[r-1,c-1] not in DARK:r-=1      # top of the run
        n=1
        while ok(r+n,c) and out[r+n,c] in DARK and out[r+n,c+1] in SKIN:n+=1
    else:
        # The source shows an open eye but the 75% frame lost it: it became skin, the brown of
        # the socket (10) or a stray white pixel from the headband (1). Without it the eye
        # blinks on and off across an animation (user review: pistol reload, m7788-m7791).
        r,c,n=r0,c0,0
        # Often the 75% grid dropped the source eye row itself, so the cell below it is used.
        SOCKET=SKIN|{1,10,11}
        if not(ok(r,c) and ok(r+1,c) and out[r,c] in SOCKET and out[r+1,c] in SOCKET and out[r,c+1]!=0):return cur,dict(status='eye not drawn and no room for it')
        out[r,c]=EYE;n=1
    if n<2:
        if ok(r+1,c) and out[r+1,c] in SKIN and out[r+1,c+1] in SKIN|{1}:pass
        elif n==1 and ok(r-1,c) and out[r-1,c] in SKIN:r-=1
        else:return cur,dict(status='no room for a 2px eye')
    for rr in range(r+2,r+max(n,2)):out[rr,c]=skin_for(out,src,ky,kx,rr,c)            # trim a 3px eye
    for rr in (r,r+1):
        out[rr,c]=EYE
        if out[rr,c+1] in DARK:out[rr,c+1]=skin_for(out,src,ky,kx,rr,c+1)
        if out[rr,c-1] in DARK and c>=2 and out[rr,c-2] in SKIN:out[rr,c-1]=skin_for(out,src,ky,kx,rr,c-1)   # a 2px wide eye
    return out,dict(status='fixed' if not np.array_equal(out,cur) else 'already 1x2',at=[int(r),int(c)],drawn=int(n),before=before)
def bridge(out,src,lens,r,front):
    """Nose bridge: in the source the lens's top row is followed by the dark frame
    line (index 11) and the pale highlight sits one row lower. When the 75% frame put
    the pale highlight right beside the top of the lens, the bridge reads as a hole
    (user review of the parachute frame m8221). Give that pixel the frame colour."""
    y,x=lens;B=src==BLACK;xe=x
    while xe+1<src.shape[1] and B[y,xe+1]:xe+=1
    if xe+1>=src.shape[1] or src[y,xe+1]!=BROWN:return out
    c=front+1
    if c>=out.shape[1] or out[r,c] not in {1,8,9}:return out
    if out[r,front]!=BLACK:return out
    out=out.copy();out[r,c]=BROWN;return out
def fix_tarma(cur,src,ky,kx):
    g=tarma_glasses(src)
    if len(g)!=1:return cur,dict(status='no side-view sunglasses' if not g else 'ambiguous')
    r0,c0=predicted(ky,kx,*g[0]);out=cur.copy();H,W=out.shape
    if r0 is None or c0 is None:return cur,dict(status='lens outside the frame')
    # black pixels of the drawn lens around the predicted cell
    y0,y1,x0,x1=max(0,r0-2),min(H,r0+5),max(0,c0-2),min(W,c0+5)
    lab,n=ndi.label(out[y0:y1,x0:x1]==BLACK);best=None
    for i,sl in enumerate(ndi.find_objects(lab),1):
        h,w=sl[0].stop-sl[0].start,sl[1].stop-sl[1].start;size=int((lab[sl]==i).sum())
        touches=sl[0].start==0 and y0>0 or sl[1].start==0 and x0>0 or sl[0].stop==y1-y0 and y1<H or sl[1].stop==x1-x0 and x1<W
        if h>3 or w>4 or size<2 or touches:continue
        d=abs(sl[0].start+y0-r0)+abs(sl[1].start+x0-c0)
        if best is None or (d,-size)<best[0]:best=((d,-size),sl[0].start+y0,sl[1].start+x0,h,w)
    before=''.join('%x'%v for v in out[y0:y1,x0:x1].ravel())
    if best is None:return cur,dict(status='lens not drawn as a compact block, left as is')
    _,r,c,h,w=best
    # A clean 2x2 lens flush with the face front (usually with the brow line behind it)
    # already reads as sunglasses: growing it into the brow and pulling it off the
    # nose made it worse (user review of m8155 / m8241). Keep it as drawn.
    if h==2 and w==2 and (out[r:r+2,c:c+2]==BLACK).all():
        out=bridge(out,src,g[0],r,c+1)
        return out,dict(status='clean 2x2 lens, bridge fixed' if not np.array_equal(out,cur) else 'already a clean 2x2 lens, kept',at=[int(r),int(c)],shape='bridge',drawn=[2,2],before=before)
    if r+2>H or c+w>W:return cur,dict(status='lens outside the frame')
    front=c+w-1                                              # the lens keeps its front edge
    if front-1<0:return cur,dict(status='lens outside the frame')
    block=out[r:r+h,c:c+w].copy()
    for rr in range(r,r+h):
        for cc in range(c,c+w):
            if out[rr,cc]==BLACK and not(rr<r+2 and cc>=front-1):
                left=out[rr,cc-1] if cc>=1 else 0
                out[rr,cc]=BROWN if (cc<front-1 and left==BROWN) else skin_for(out,src,ky,kx,rr,cc)
    if w>=3:
        # reference (art supplied by the user): a 3-pixel top row, the 2 back pixels below it
        cells=[(r,front-2),(r,front-1),(r,front),(r+1,front-2),(r+1,front-1)]
    elif h<2:
        return cur,dict(status='lens drawn 1 pixel tall, left as is')    # 2x2 would paint below the lens
    else:
        # a lens drawn only 1-2 pixels wide: widening it to 3 pushed it back into the brow
        # and off the nose (user review of m8200 against m8254). Clean 2x2 at the front instead.
        cells=[(r,front-1),(r,front),(r+1,front-1),(r+1,front)]
    if r+1>=H or min(cc for _,cc in cells)<0 or any(out[rr,cc] not in SKIN|DARK for rr,cc in cells):return cur,dict(status='no room for the lens shape')
    for rr,cc in cells:out[rr,cc]=LENS
    if w>=3 and out[r+1,front]==BLACK:out[r+1,front]=skin_for(out,src,ky,kx,r+1,front)
    out=bridge(out,src,g[0],r,front)
    return out,dict(status='fixed' if not np.array_equal(out,cur) else 'already reference shape',at=[int(r),int(front-2)],shape='3+2' if w>=3 else '2x2',drawn=[int(h),int(w)],before=before)
HAIR={14,15};BAND_WHITE=1;BAND_SHADE=4
def marco_band(cur):
    """Headband like the standing frame: no brown line on top of it or behind it.
    Only for an upright head: one white band at most 3 rows tall, at least twice as wide, hair right above."""
    out=cur.copy();H,W=out.shape;lab,n=ndi.label(out==BAND_WHITE);best=None
    for i in range(1,n+1):
        ys,xs=np.nonzero(lab==i)
        if len(ys)<4 or ys.max()-ys.min()>2 or (xs.max()-xs.min()+1)<2*(ys.max()-ys.min()+1):continue
        above=out[max(0,ys.min()-3):ys.min(),max(0,xs.min()-1):xs.max()+2]
        if np.isin(above,list(HAIR)).sum()<2:continue
        if best is None or len(ys)>len(best[0]):best=(ys,xs)
    if best is None:return cur,0
    interior=lambda y,x:0<y<H-1 and 0<x<W-1 and out[y-1,x] and out[y+1,x] and out[y,x-1] and out[y,x+1]
    changed=0
    for y,x in zip(*best):
        t=(y-1,x)                                                   # on top of the band: hair
        if t[0]>=1 and out[t]==BROWN and interior(*t):
            up=out[t[0]-1,x];out[t]=up if up in HAIR else 15;changed+=1
        b=(y,x-1)                                                   # behind the band: its grey shade
        if b[1]>=1 and out[b]==BROWN and interior(*b) and out[y,x-2] not in (0,):out[b]=BAND_SHADE;changed+=1
    return out,changed
def band_component(out):
    """The upright headband: one white run at most 3 rows tall, at least twice as wide, hair right above."""
    lab,n=ndi.label(out==BAND_WHITE);best=None
    for i in range(1,n+1):
        ys,xs=np.nonzero(lab==i)
        if len(ys)<4 or ys.max()-ys.min()>2 or (xs.max()-xs.min()+1)<2*(ys.max()-ys.min()+1):continue
        above=out[max(0,ys.min()-3):ys.min(),max(0,xs.min()-1):xs.max()+2]
        if np.isin(above,list(HAIR)).sum()<2:continue
        if best is None or len(ys)>len(best[0]):best=(ys,xs)
    return best
def face_window(out,act,lens=None):
    """Rows and columns of eye and nose: under an upright headband (Marco) or around the lens (Tarma)."""
    if act==0:
        b=band_component(out)
        if b is None:return None
        ys,xs=b;return int(ys.max())+1,int(xs.min())-2,int(xs.max())+5
    if lens is None:return None
    r,c=lens;return r,c-3,c+6
def unmerge(out,src,ky,kx,act,lens=None):
    """Eye, brow and nose melted into one dark run: in the source those dark pixels are
    kept apart by skin, but the 75% grid dropped exactly the skin columns between them
    (user review: Marco with the heavy machine gun, m7567). Put one skin pixel back
    inside the run, taken from the dropped source columns, so the features read again."""
    fw=face_window(out,act,lens)
    if fw is None:return out,0
    out,n=_unmerge_once(out,src,ky,kx,act,fw)
    if n:
        out,n2=_unmerge_once(out,src,ky,kx,act,fw);n+=n2               # a 5-6 pixel smear needs two breaks
    return out,n
def _unmerge_once(out,src,ky,kx,act,fw):
    r0,c0,c1=fw;H,W=out.shape;out=out.copy();n=0
    for r in range(max(1,r0),min(H-1,r0+(4 if act==0 else 3))):
        c=max(1,c0)
        while c<min(W-1,c1):
            if out[r,c] not in DARK:c+=1;continue
            e=c
            while e+1<W-1 and out[r,e+1] in DARK:e+=1
            if e-c+1>=3 and out[r,c-1]!=0 and out[r,e+1]!=0 and sum(1 for cc in range(c,e+1) for rr in (r-1,r+1) if out[rr,cc] in SKIN|{10})>=2:
                sy=ky[r]
                for cc in range(c+1,e+1):                          # never the first pixel of the run
                    if act==1 and out[r,cc]==BLACK:continue          # never cut into Tarma's lens
                    lo=kx[cc-1]+1 if cc>=1 else kx[cc]
                    foot=[int(src[sy,x]) for x in range(max(0,lo),min(src.shape[1],kx[cc]+1))]
                    skin=[v for v in foot if v in SKIN|{10}]
                    if skin and 0<=sy<src.shape[0]:
                        out[r,cc]=skin[0];n+=1;break
            c=e+1
    return out,n
# Marco's nose, the shape most side-view frames already have (user review, m7553):
#   eye row      E+1..E+3 = skin, pale skin, outline
#   next row     E+1..E+4 = skin, skin, white highlight, outline
#   third row    E+1..E+3 = skin, outline, outline
NOSE=((9,8,11,0),(9,9,1,11),(9,11,11,None))
def front(out,r,E):
    nz=np.nonzero(out[r,E:]!=0)[0];return int(nz.max()) if len(nz) else -1
def marco_nose(out,R,E):
    """Draw the nose template right in front of the eye at (R,E) when the face profile is
    within one pixel of the usual one; other head angles are left alone."""
    H,W=out.shape
    if R+3>H or E+5>W:return out,0
    b=band_component(out)                                     # upright side view only: the standing nose
    if b is None or abs(int(b[0].max())+1-R)>1 or not(0<=E-int(b[1].min())<=4):return out,0
    fr=[front(out,R+k,E) for k in range(3)]
    if abs(fr[0]-3)>1 or abs(fr[1]-4)>1 or fr[2]<2 or fr[2]>6:return out,0
    new=out.copy()
    for k,row in enumerate(NOSE):
        if k==2 and fr[2]!=3:continue                         # an open mouth or a chin further out: keep it
        for i,v in enumerate(row):
            if v is None:continue
            c=E+1+i
            if v==0:
                if new[R+k,c] in DARK|SKIN and (c+1>=W or new[R+k,c+1]==0):new[R+k,c]=0
            else:new[R+k,c]=v
    return new,int((new!=out).sum())
def marco_squint(out,src,ky,kx):
    """Squinting frames (no open eye in the source, e.g. walking with the heavy machine gun,
    m7567): no dark shadow dots under the headband, a single 1x1 eye, the usual nose."""
    b=band_component(out)
    if b is None:return out,0,None
    ys,xs=b;R=int(ys.max())+1;E=int(xs.min())+2;H,W=out.shape
    if R+2>=H or E<3 or E+4>=W:return out,0,None
    if abs(front(out,R,E)-3)>1:return out,0,None
    row=out[R,int(xs.min())-1:E+1]
    if np.count_nonzero(np.isin(row,list(DARK)))<3:return out,0,None   # not the shadowed squint
    # an eye already drawn 1x2 (dark over dark, skin in front) is kept as it is (user review)
    tall=[c for c in (E,E-1,E+1) if out[R,c] in DARK and out[R+1,c] in DARK and out[R,c+1] not in DARK and out[R+1,c+1] not in DARK]
    if tall:                                                  # a normal face like m7553: keep it, nose only
        new,n=marco_nose(out,R,tall[0]);return new,n,((R,tall[0]) if n else None)
    new=out.copy()
    for c in range(int(xs.min())-1,E):
        if new[R,c] in DARK and new[R-1,c]!=0 and new[R+1,c]!=0:new[R,c]=9
    if not tall:
        new[R,E]=EYE
        if new[R+1,E] in DARK:new[R+1,E]=9
    new,_=marco_nose(new,R,E)
    return new,int((new!=out).sum()),(R,E)
IDLE_MAP=7553        # Marco's idle upper body (drawing 189): the face every pose returns to
def src_band_anchor(src):
    """(bottom row, first column) of the upright headband in a SOURCE map, or None."""
    lab,n=ndi.label(src==BAND_WHITE);best=None
    for i in range(1,n+1):
        ys,xs=np.nonzero(lab==i)
        if len(ys)<6 or ys.max()-ys.min()>4:continue
        above=src[max(0,ys.min()-4):ys.min(),max(0,xs.min()-1):xs.max()+2]
        if np.isin(above,list(HAIR)).sum()<3:continue
        if best is None or len(ys)>len(best[0]):best=(ys,xs)
    return None if best is None else (int(best[0].max()),int(best[1].min()))
# Pistol fire -> idle: the frames of the return take the idle face (user choice), so the
# face does not change as Marco settles back into the idle pose. 7567 replaces its squint.
# 7563 joined later (user review: 7563-7567 all with the 7553 nose).
RETURN_TO_IDLE=(7563,7564,7565,7566,7567)
NOSE_ZONE=(range(1,4),range(4,8))   # rows below the band, columns from the band start: the nose
def scaled_anchor(src,ky,kx):
    a=src_band_anchor(src)
    if a is None:return None
    r=np.nonzero(ky>=a[0])[0];c=np.nonzero(kx>=a[1])[0]
    return (int(r[0]),int(c[0])) if len(r) and len(c) else None
def copy_idle_face(out,src,ky,kx,idle):
    """The idle face block (headband bottom row to the chin, back of the head to the nose)
    put over this frame at the same place relative to the headband; the silhouette stays."""
    isrc,icur,iky,ikx=idle
    A=scaled_anchor(src,ky,kx);IA=scaled_anchor(isrc,iky,ikx)
    if A is None or IA is None:return out,0
    new=out.copy();H,W=out.shape;n=0
    for dr in range(0,5):
        for dc in range(-3,10):
            r,c=A[0]+dr,A[1]+dc;ir,ic=IA[0]+dr,IA[1]+dc
            if not(0<=r<H and 0<=c<W and 0<=ir<icur.shape[0] and 0<=ic<icur.shape[1]):continue
            v=icur[ir,ic]
            if dr in NOSE_ZONE[0] and dc in NOSE_ZONE[1]:
                # the nose sticks out past this frame's profile: take it whole, including
                # the transparent pixels in front of it (only over face pixels)
                if v==0 and out[r,c] not in SKIN|DARK|{BAND_WHITE}:continue
                if out[r,c]!=v:new[r,c]=v;n+=1
                continue
            if out[r,c]==0 or v==0 or v in (BAND_WHITE,4,5) and dr>0 and out[r,c] not in (BAND_WHITE,4,5):continue
            if out[r,c]!=v:new[r,c]=v;n+=1
    return new,n
# Pistol fire frames that take another frame's nose (user review: 7562 like 7561).
NOSE_FROM={7562:7561}
def copy_nose(out,src,ky,kx,donor):
    """The donor's nose (rows 1-3 under the band, columns 2-6 from the band start) put on this
    frame, lined up on the headband: the scaled anchors can differ by a column, so the band
    row decides the column shift."""
    dsrc,dcur,dky,dkx=donor
    A=scaled_anchor(src,ky,kx);DA=scaled_anchor(dsrc,dky,dkx)
    if A is None or DA is None:return out,0
    def score(sh):
        a=out[A[0],A[1]-3:A[1]+10];b=dcur[DA[0],DA[1]-3+sh:DA[1]+10+sh]
        k=min(len(a),len(b));return int((a[:k]==b[:k]).sum())
    sh=max(range(-2,3),key=lambda v:(score(v),-abs(v)))
    new=out.copy();n=0;H,W=out.shape
    for dr in range(1,4):
        for dc in range(2,7):
            r,c=A[0]+dr,A[1]+dc;ir,ic=DA[0]+dr,DA[1]+dc+sh
            if not(0<=r<H and 0<=c<W and 0<=ir<dcur.shape[0] and 0<=ic<dcur.shape[1]):continue
            v=dcur[ir,ic]
            if v==0 and out[r,c] not in SKIN|DARK|{BAND_WHITE}:continue
            if out[r,c]!=v:new[r,c]=v;n+=1
    return new,n
# Small-nose faces (user review: the idle-like heads whose nose was drawn short, many holding
# the heavy machine gun) take the idle 7553 nose. Only the nose columns move (the frame's own
# eye stays); anything in front of the face - hand, gun, sleeve - stays in front of it.
GUN={2,3,6,12,13}
def gun_black(out):
    """Black pixels that outline a gun or hand (touch gun colours): in front of the face."""
    blk=ndi.label(out==BLACK,structure=np.ones((3,3)))[0];res=np.zeros(out.shape,bool)
    for i in range(1,blk.max()+1):
        m=blk==i
        if np.isin(out[ndi.binary_dilation(m,structure=np.ones((3,3)))],list(GUN)).any():res|=m
    return res
def head_align(out,A,icur,IA):
    """(score, has eye, row shift, column shift) lining this head up on the idle head, or None."""
    def px(img,r,c):return int(img[r,c]) if 0<=r<img.shape[0] and 0<=c<img.shape[1] else -1
    # line the head up on the idle one: the band can be drawn a column or two short, so the
    # eye and the band row together decide the shift
    best=None
    for dv in (-1,0,1):
        for sh in range(-3,2):
            eye=px(out,A[0]+1+dv,A[1]+2+sh) in DARK and px(out,A[0]+2+dv,A[1]+2+sh) in DARK
            sc=sum(px(out,A[0]+dr+dv,A[1]+dc+sh)==px(icur,IA[0]+dr,IA[1]+dc) for dr in (0,1,2) for dc in range(-3,3))
            if sc<(10 if eye else 11):continue                    # same head: band + eye, or band + face
            if best is None or (sc,eye)>best[0:2]:best=(sc,eye,dv,sh)
    return best
# Eyeless faces (user review): a head lined up on the idle one with no dark pixel where the
# eye goes gets a 1x1 eye at the top of the idle eye, unless a hand or gun covers that spot.
def min_eye(out,src,ky,kx,idle):
    isrc,icur,iky,ikx=idle
    A=scaled_anchor(src,ky,kx);IA=scaled_anchor(isrc,iky,ikx)
    if A is None or IA is None:return out,0,None
    best=head_align(out,A,icur,IA)
    if best is None:return out,0,None
    _,_,dv,sh=best;r0,c0=A[0]+dv,A[1]+sh;H,W=out.shape
    gb=gun_black(out)
    near=[(r0+dr,c0+dc) for dr in (1,2) for dc in (1,2,3)]
    if any(0<=r<H and 0<=c<W and out[r,c] in DARK and not gb[r,c] for r,c in near):return out,0,None
    r,c=r0+1,c0+2
    if not(0<=r<H and 0<=c<W) or out[r,c] not in SKIN:return out,0,None
    new=out.copy();new[r,c]=EYE
    return new,1,(r,c)
def small_nose(out,src,ky,kx,idle):
    isrc,icur,iky,ikx=idle
    A=scaled_anchor(src,ky,kx);IA=scaled_anchor(isrc,iky,ikx)
    if A is None or IA is None:return out,0
    H,W=out.shape
    def px(img,r,c):return int(img[r,c]) if 0<=r<img.shape[0] and 0<=c<img.shape[1] else -1
    best=head_align(out,A,icur,IA)
    if best is None:return out,0                                 # another head angle
    _,_,dv,sh=best;r0,c0=A[0]+dv,A[1]+sh
    row=[px(out,r0+2,c0+dc) for dc in range(3,10)]
    tips=[k+3 for k in range(len(row)-1) if row[k]==BAND_WHITE and row[k+1]==BROWN]
    itip=[dc for dc in range(3,9) if px(icur,IA[0]+2,IA[1]+dc)==BAND_WHITE]
    short=False
    if tips:
        if not itip or tips[0]>=itip[0]:return out,0              # has the full-length tip
        # tip drawn short (user review, m7613/m8033/m7583): the idle length, and the front of the
        # row under the band one pixel further out too, so the nose ridge runs diagonally
        short=True
    front_black=gun_black(out)
    new=out.copy();n=0
    for dr in range(1,4):
        for dc in range(3,7):
            if short and dr==1 and dc==3:continue                    # the frame's own ridge start
            r,c=r0+dr,c0+dc;v=px(icur,IA[0]+dr,IA[1]+dc)
            if not(0<=r<H and 0<=c<W) or v<0:continue
            t=int(out[r,c])
            if t in GUN or front_black[r,c]:continue                     # hand / gun in front: keep it
            if t not in SKIN|DARK|{BAND_WHITE,0}:continue
            if v==0 and t!=0 and dc<6:continue                        # keep the cheek, only trim in front
            if t!=v:new[r,c]=v;n+=1
    return new,n
def nose_tip(out,src,ky,kx):
    """The 75% grid dropped the source row with the nose tip (white highlight then outline at
    the front of the face), so the face has no nose (user review: pistol fire m7561-m7563).
    Put that row's front back into the scaled row just below it, the rest of the row kept."""
    an=src_band_anchor(src)
    if an is None:return out,0
    H,W=src.shape
    # a head bowed over the gun (idle reload, m7880) has the nose tip level with the band
    tips=[(y,x) for y in range(max(0,an[0]-2),min(H,an[0]+6)) for x in range(max(1,an[1]),min(W-1,an[1]+14))
          if src[y,x]==BAND_WHITE and src[y,x-1] in (8,9) and x+2<W and
             (src[y,x+1]==BROWN and src[y,x+2] not in SKIN|{1} or src[y,x+1] in (8,9) and src[y,x+2]==BROWN)]
    if len(tips)!=1:return out,0
    y,x=tips[0]
    if y in set(int(v) for v in ky) and y>an[0]:return out,0      # the tip row is there
    if y in set(int(v) for v in ky):
        # bowed head only (tip at band level): the row is there but the tip pixel was lost (the sampled front reads as outline):
        # put the source's last two skin pixels and the highlight back in that row
        r=[rr for rr in range(out.shape[0]) if int(ky[rr])==y][0]
        cols=[c for c in range(out.shape[1]) if x-2<=int(kx[c])<=x]
        if len(cols)<2 or any(out[r,c]==BAND_WHITE for c in cols):return out,0
        if not any(int(kx[c])==x for c in cols):return out,0
        new=out.copy()
        for c in cols:
            if out[r,c] in SKIN|DARK:new[r,c]=int(src[y,kx[c]])
        return new,int((new!=out).sum())
    rows=[r for r in range(out.shape[0]) if int(ky[r])==y+1]
    if not rows:return out,0
    r=rows[0];new=out.copy()
    cols=[c for c in range(out.shape[1]) if x-3<=int(kx[c])<=x+1]
    if len(cols)<3:return out,0
    near=[c for c in range(max(0,cols[0]-1),min(out.shape[1],cols[-1]+2))]
    if any(out[rr,c]==BAND_WHITE for rr in (r-1,r) if 0<=rr<out.shape[0] for c in near):return out,0   # a tip is already drawn
    for c in cols:
        v=int(src[y,kx[c]])
        if v:new[r,c]=v
    if not any(new[r,c]==BAND_WHITE for c in cols):              # the tip column itself was dropped too
        c=max(cc for cc in cols if kx[cc]<=x)
        if c+1<out.shape[1]:new[r,c]=BAND_WHITE;new[r,c+1]=BROWN
    # the tip must end up at the front edge of the face: highlight, outline, then nothing skin-coloured
    tip=[c for c in range(out.shape[1]-1) if new[r,c]==BAND_WHITE and c in cols]
    if not tip:return out,0
    c=tip[-1];e=c+1
    while e<out.shape[1] and new[r,e] in SKIN:e+=1
    if e>=out.shape[1] or new[r,e]!=BROWN or e-c>2 or (e+1<out.shape[1] and new[r,e+1] in SKIN|{1}):return out,0
    return new,int((new!=out).sum())

# ---- Tarma side view: glasses and nose always in the idle style (user review) ----
TARMA_IDLE=8142      # Tarma's idle upper body (drawing 190)
def src_hair_anchor(src):
    """(bottom row, first column) of the largest hair patch in the top half of a SOURCE map."""
    lab,n=ndi.label(np.isin(src,list(HAIR)),structure=np.ones((3,3)));best=None
    for i in range(1,n+1):
        ys,xs=np.nonzero(lab==i)
        if len(ys)<5 or ys.min()>src.shape[0]*0.5:continue
        if best is None or len(ys)>len(best[0]):best=(ys,xs)
    return None if best is None else (int(best[0].max()),int(best[1].min()))
def hair_anchor(src,ky,kx):
    a=src_hair_anchor(src)
    if a is None:return None
    r=np.nonzero(ky>=a[0])[0];c=np.nonzero(kx>=a[1])[0]
    return (int(r[0]),int(c[0])) if len(r) and len(c) else None
TARMA_FACE=SKIN|DARK|{BAND_WHITE,0}|HAIR
IDLE_LENS=(6,12)     # top-left lens pixel in the scaled idle map (2x2 lens)
def tarma_idle_glasses(out,base_cur,src,ky,kx,idle):
    """Lens, the frame over the nose and the nose tip copied from the idle head: a block of
    3 rows from the lens top, 2 columns behind to 4 in front of the lens, placed on this
    frame's own lens (or where the idle lens sits when the lens was lost). The head is lined
    up on the back of the head and cheek, away from the glasses. Hands, guns and anything
    else in front of the face stay in front."""
    isrc,icur,iky,ikx=idle
    A=hair_anchor(src,ky,kx);IA=hair_anchor(isrc,iky,ikx)
    if A is None or IA is None:return out,0
    def px(img,r,c):return int(img[r,c]) if 0<=r<img.shape[0] and 0<=c<img.shape[1] else -1
    best=None
    for dv in (-2,-1,0,1,2):
        for sh in range(-3,3):
            sc=sum(px(base_cur,A[0]+dr+dv,A[1]+dc+sh)==px(icur,IA[0]+dr,IA[1]+dc) for dr in range(-4,4) for dc in range(-3,3))
            if best is None or sc>best[0]:best=(sc,dv,sh)
    if best[0]<30:return out,0                                   # not the idle side view
    _,dv,sh=best;r0,c0=A[0]+dv,A[1]+sh;H,W=out.shape
    fb=gun_black(base_cur)
    lens=[(r,c) for r in range(r0-2,r0+3) for c in range(c0+3,c0+9)
          if 0<=r<H and 0<=c<W and base_cur[r,c]==LENS and not fb[r,c]]
    if lens:
        lr=min(r for r,_ in lens);lc=min(c for r,c in lens if r==lr)
    else:
        lr,lc=r0+IDLE_LENS[0]-IA[0],c0+IDLE_LENS[1]-IA[1]
    fb=gun_black(out);new=out.copy();n=0
    for dr in range(0,3):
        for dc in range(-2,5):
            r,c=lr+dr,lc+dc;v=px(icur,IDLE_LENS[0]+dr,IDLE_LENS[1]+dc)
            if not(0<=r<H and 0<=c<W) or v<0:continue
            t=int(out[r,c])
            if t not in TARMA_FACE or fb[r,c]:continue                # in front of the face: keep
            if v==0 and t!=0 and dc<3:continue                        # keep the cheek, trim only in front
            if t!=v:new[r,c]=v;n+=1
    # a lens pixel left one row above the block (the scaled lens sat a row high) goes to hair/frame
    for c in range(lc-1,lc+3):
        r=lr-1
        if 0<=r<H and 0<=c<W and new[r,c]==LENS and not fb[r,c]:new[r,c]=BROWN;n+=1
    return new,n
TARMA_FIXED=[]

# ---- pistol raise / aim sequence 7582-7600 (user review: eye, nose and headband by the rules) ----
SEQ_IDLE_FACE=(7582,)                       # first frame out of idle: the idle face (head unchanged)
SEQ_BAND=tuple(range(7583,7595))            # headband rows like the standing band (no brown top)
SEQ_EYE=tuple(range(7585,7591))             # head raised: white eye pixel over a 1px eye -> 1x2 eye
SEQ_NOSE={}                                 # (superseded by FORCE_NOSE)
# The idle nose put on, lined up on the band row and the eye (user review 7586-7589, 7599):
# map -> (row shift, column shift) from the idle face; raised heads sit one column back.
# Eye touch-ups by review (offsets from the band row and band start):
#  7585: the eye ran three pixels down (row 3 outline left under it) -> 1x2 like 7586
#  7599: eye one column behind the idle nose -> moved forward, 1x2 (user review)
EYE_EDIT={7585:[(3,1,9)],7599:[(1,1,9),(1,2,EYE),(2,2,EYE)]}
def eye_edit(out,src,ky,kx,edits):
    A=scaled_anchor(src,ky,kx)
    if A is None:return out,0
    r0=band_rows(out,A)
    if r0 is None:return out,0
    new=out.copy()
    for dr,dc,v in edits:
        r,c=r0+dr,A[1]+dc
        if 0<=r<out.shape[0] and 0<=c<out.shape[1] and out[r,c] in SKIN|DARK:new[r,c]=v
    return new,int((new!=out).sum())
FORCE_NOSE={7585:(0,-1),7586:(0,-1),7587:(0,-1),7588:(0,-1),7589:(0,-1),7590:(0,-1),7599:(0,0)}
def force_idle_nose(out,src,ky,kx,idle,dv,sh):
    isrc,icur,iky,ikx=idle
    A=scaled_anchor(src,ky,kx);IA=scaled_anchor(isrc,iky,ikx)
    if A is None or IA is None:return out,0
    r0=band_rows(out,A);ir0=band_rows(icur,IA)
    if r0 is None or ir0 is None:return out,0
    fb=gun_black(out);new=out.copy();n=0;H,W=out.shape
    for dr in (1,2,3):
        for dc in range(3,8):
            r,c=r0+dr+dv,A[1]+dc+sh;ir,ic=ir0+dr,IA[1]+dc
            if not(0<=r<H and 0<=c<W):continue
            v=int(icur[ir,ic]) if 0<=ir<icur.shape[0] and 0<=ic<icur.shape[1] else 0
            t=int(out[r,c])
            if t in GUN or fb[r,c] or t not in SKIN|DARK|{BAND_WHITE,0}:continue   # gun / hand in front
            if t!=v:new[r,c]=v;n+=1
    return new,n
def band_rows(out,A):
    """Row of this frame's lower band line (the one matching the idle '441111' row), or None."""
    best=None
    for r in (A[0]-1,A[0],A[0]+1):
        if not 0<=r<out.shape[0]:continue
        n=sum(1 for c in range(A[1],min(out.shape[1],A[1]+4)) if out[r,c]==BAND_WHITE)
        if best is None or n>best[0]:best=(n,r)
    return None if best is None or best[0]<2 else best[1]
def band_to_idle(out,src,ky,kx,idle):
    isrc,icur,iky,ikx=idle
    A=scaled_anchor(src,ky,kx);IA=scaled_anchor(isrc,iky,ikx)
    if A is None or IA is None:return out,0
    r0=band_rows(out,A);ir0=band_rows(icur,IA)
    if r0 is None or ir0 is None:return out,0
    new=out.copy();n=0;H,W=out.shape
    BANDLIKE={BAND_WHITE,4,5,BROWN}|HAIR
    for dr in (-1,0):
        for dc in range(-3,6):
            r,c=r0+dr,A[1]+dc;v=int(icur[ir0+dr,IA[1]+dc]) if 0<=IA[1]+dc<icur.shape[1] else 0
            if not(0<=r<H and 0<=c<W) or v==0:continue
            if v not in {BAND_WHITE,4,5}:continue                  # only the band itself
            if out[r,c] not in BANDLIKE:continue                    # skin / gun / hand stay
            if out[r,c]!=v:new[r,c]=v;n+=1
    return new,n
def seq_eye(out,src,ky,kx):
    A=scaled_anchor(src,ky,kx)
    if A is None:return out,0
    r0=band_rows(out,A)
    if r0 is None:return out,0
    new=out.copy()
    for c in range(A[1],A[1]+4):
        r=r0+1
        if r+1<out.shape[0] and out[r,c]==BAND_WHITE and out[r+1,c] in DARK and out[r,c-1] in SKIN|DARK:
            new[r,c]=EYE;return new,1
    return out,0
# Running jump with the pistol (user review: the pistol "appears then vanishes"): the pistol
# hangs below the hand in these upper-body frames and is drawn over the legs in the same
# olive colours, so at 75% it melts into the trousers. Its fill (4/5/6) becomes the darkest
# olive (3), keeping the black outline, so it reads as the gun. Boxes: rows up from the
# last drawn row, columns, in the scaled map.
JUMP_PISTOL={7644:(5,9,13),7645:(5,6,10),7646:(5,0,6),7647:(6,6,11),7652:(5,9,13),7653:(3,11,16)}
def jump_pistol(out,box):
    up,c0,c1=box;H=int(np.nonzero(out.any(1))[0].max())+1;new=out.copy()
    for r in range(H-up,H):
        for c in range(c0,min(c1,out.shape[1])):
            if out[r,c] in (4,5,6):new[r,c]=3
    return new,int((new!=out).sum())
SMALL=[];EYELESS=[]
def main():
    D=bac.Data();S=stage1.Scaled();first,count=next((f,c) for n,f,c in D.sets if n=='player_animation_program')
    prog=json.loads((G/'player-program-source.json').read_text());actor={}
    for c in prog['commands']:
        if c['op']=='DRAW':actor.setdefault(c['target'],set()).add(c['actor'])
    previous={a['id']:a for a in json.loads((OUT/'patch.json').read_text())['assets']} if (OUT/'patch.json').exists() else {}
    def unwound(key,pixels):
        a=previous.get(key)
        if a is None:return pixels
        assert np.array_equal(pixels,unhex(a['rows'])),('generated data is neither stage-3 nor this patch',key)
        return unhex(a['original'])
    draw={}
    for d in range(count):
        fp,n=D.drawings[first+d];pp=[p for p in D.parts[fp:fp+n] if p[2]!=65535 and p[2]<len(D.maps)]
        if len(pp)==1 and D.content(pp[0][2]) is not None:draw[d]=(pp[0][2],pp[0][0],-pp[0][1])
    owners={}
    for d,(m,_,_) in draw.items():owners.setdefault(m,dict(drawings=[],actors=set()));owners[m]['drawings'].append(d);owners[m]['actors']|=actor.get(d,set())
    base={}
    for m in owners:
        g=stage1.grid(D,S,m);base[m]=(g[0],unwound('map:%d'%D.ledger[m]['source'],g[1]))+g[2:]
    new={};logs={};stats={0:collections.Counter(),1:collections.Counter()}
    for m,(src,cur,ky,kx,o,_) in base.items():
        acts=owners[m]['actors']
        if len(acts)!=1:continue
        a=next(iter(acts));fixed,log=(fix_marco if a==0 else fix_tarma)(cur,src,ky,kx);stats[a][log['status']]+=1
        if a==0 and 'at' in log and log.get('drawn')!=0 and front(cur,log['at'][0]+2,log['at'][1])<=2 and band_component(cur) is not None:
            # mouth set one pixel back from the standing face: the original eye and nose fit it
            # better than the standing ones (user review, m7739); leave the face as drawn
            fixed=cur.copy();log=dict(status='recessed face, kept as drawn',at=log['at']);stats[0]['recessed face kept']+=1
        elif a==0 and 'at' in log and log.get('drawn')!=0:
            fixed,nn=marco_nose(fixed,*log['at'])
            if nn:log=dict(log,nose_pixels=nn);stats[0]['nose shaped']+=1
        elif a==0 and log['status']=='no open side-view eye':
            fixed,nq,at=marco_squint(fixed,src,ky,kx)
            if nq and at is not None:log=dict(log,status='squint face',at=list(at),squint_pixels=nq);stats[0]['squint face']+=1
        if a==0:
            fixed,nb=marco_band(fixed)
            if nb:log=dict(log,band_pixels=nb);stats[0]['headband fixed']+=1
        if a==0 and (m in RETURN_TO_IDLE or m in SEQ_IDLE_FACE):
            fixed,ni=copy_idle_face(cur,src,ky,kx,(base[IDLE_MAP][0],base[IDLE_MAP][1],base[IDLE_MAP][2],base[IDLE_MAP][3]))
            fixed,_=marco_band(fixed)
            log=dict(status='idle face (pistol return)' if m in RETURN_TO_IDLE else 'idle face (aim sequence start)',idle_face_pixels=ni);stats[0]['idle face copied']+=1

        if a==0 and m not in RETURN_TO_IDLE:
            fixed,nt=nose_tip(fixed,src,ky,kx)
            if nt:log=dict(log,nose_tip_pixels=nt);stats[0]['nose tip restored']+=1
        if a==0 and m not in RETURN_TO_IDLE and m not in NOSE_FROM and m!=IDLE_MAP and log.get('status')!='recessed face, kept as drawn':
            fixed,ns=small_nose(fixed,src,ky,kx,(base[IDLE_MAP][0],base[IDLE_MAP][1],base[IDLE_MAP][2],base[IDLE_MAP][3]))
            if ns:log=dict(log,small_nose_to_idle=ns);stats[0]['small nose -> idle nose']+=1;SMALL.append(m)
        if a==0 and m not in RETURN_TO_IDLE and m!=IDLE_MAP:
            fixed,ne,at=min_eye(fixed,src,ky,kx,(base[IDLE_MAP][0],base[IDLE_MAP][1],base[IDLE_MAP][2],base[IDLE_MAP][3]))
            if ne:log=dict(log,min_eye=list(at));stats[0]['eyeless -> 1x1 eye']+=1;EYELESS.append(m)
        if a==0 and m in SEQ_BAND:
            fixed,nb2=band_to_idle(fixed,src,ky,kx,(base[IDLE_MAP][0],base[IDLE_MAP][1],base[IDLE_MAP][2],base[IDLE_MAP][3]))
            if nb2:log=dict(log,band_to_idle=nb2);stats[0]['aim sequence: standing band']+=1
        if a==0 and m in SEQ_EYE:
            fixed,ne2=seq_eye(fixed,src,ky,kx)
            if ne2:log=dict(log,seq_eye=ne2);stats[0]['aim sequence: 1x2 eye']+=1
        if a==0 and m in SEQ_NOSE:
            d=SEQ_NOSE[m];dfix,_=nose_tip(base[d][1],base[d][0],base[d][2],base[d][3])
            fixed,nf2=copy_nose(fixed,src,ky,kx,(base[d][0],dfix,base[d][2],base[d][3]))
            if nf2:log=dict(log,nose_from=d,nose_copy_pixels=nf2);stats[0]['aim sequence: nose from %d'%d]+=1
        if a==0 and m in FORCE_NOSE:
            fixed,nf3=force_idle_nose(fixed,src,ky,kx,(base[IDLE_MAP][0],base[IDLE_MAP][1],base[IDLE_MAP][2],base[IDLE_MAP][3]),*FORCE_NOSE[m])
            if nf3:log=dict(log,forced_idle_nose=nf3);stats[0]['idle nose (review list)']+=1
        if a==0 and m in JUMP_PISTOL:
            fixed,np_=jump_pistol(fixed,JUMP_PISTOL[m])
            if np_:log=dict(log,jump_pistol=np_);stats[0]['jump pistol darkened']+=1
        if a==0 and m in EYE_EDIT:
            fixed,ne3=eye_edit(fixed,src,ky,kx,EYE_EDIT[m])
            if ne3:log=dict(log,eye_edit=ne3);stats[0]['eye (review list)']+=1
        if a==0 and m in NOSE_FROM:
            d=NOSE_FROM[m]
            # the donor's own nose as this builder leaves it (tip restored first)
            dfix,_=nose_tip(base[d][1],base[d][0],base[d][2],base[d][3])
            fixed,nf=copy_nose(fixed,src,ky,kx,(base[d][0],dfix,base[d][2],base[d][3]))
            if nf:log=dict(log,nose_from=d,nose_copy_pixels=nf);stats[0]['nose copied from %d'%d]+=1
        fixed,nu=unmerge(fixed,src,ky,kx,a,tuple(log['at']) if a==1 and 'at' in log else None)
        if nu:log=dict(log,unmerged_pixels=nu);stats[a]['eye/nose separated']+=1
        if a==1 and m!=TARMA_IDLE:
            fixed,ng=tarma_idle_glasses(fixed,cur,src,ky,kx,(base[TARMA_IDLE][0],base[TARMA_IDLE][1],base[TARMA_IDLE][2],base[TARMA_IDLE][3]))
            if ng:log=dict(log,idle_glasses_pixels=ng);stats[1]['glasses+nose -> idle style']+=1;TARMA_FIXED.append(m)
        if not np.array_equal(fixed,cur):new[m]=fixed;logs[m]=log
    print('small nose -> idle nose:',sorted(SMALL))
    print('eyeless -> 1x1 eye:',sorted(EYELESS))
    print('tarma glasses+nose -> idle:',sorted(TARMA_FIXED))
    assets=[]
    for m in sorted(new):
        cur=base[m][1]
        assets.append(dict(id='map:%d'%D.ledger[m]['source'],target=dict(kind='scaled-map',source=D.ledger[m]['source'],map_id=m,origin=list(base[m][4])),
            actors=sorted(owners[m]['actors']),drawings=sorted(owners[m]['drawings']),changed_pixels=int((new[m]!=cur).sum()),face=logs[m],original=hexrows(cur),rows=hexrows(new[m])))
    # ---- approved Marco composites ----
    bind=json.loads((G/'approved-player-source.json').read_text())['bindings'];raw=(G/'player.tiles').read_bytes();frames=[];approved_log=[]
    def comp(b,pick):
        cv=np.zeros((64,64),np.uint8)
        for d,dy in ((b['lower'],0),(b['upper'],-b['offset_y'])):
            m,px,py=draw[d];(ox,oy)=base[m][4];im=pick(m);yy,xx=np.nonzero(im)
            Y=yy+oy+s75(py)+dy+56;X=xx+ox+s75(px)+32;ok=(Y>=0)&(Y<64)&(X>=0)&(X<64);cv[Y[ok],X[ok]]=im[yy[ok],xx[ok]]
        return cv
    def source_composite(b):
        items=[]
        for d,dy in ((b['lower'],0),(b['upper'],-b['offset_y'])):
            m,px,py=draw[d];img,cl,ct=D.content(m);items.append((img,cl+px,ct+py+dy))
        x0=min(i[1] for i in items);y0=min(i[2] for i in items);x1=max(i[1]+i[0].shape[1] for i in items);y1=max(i[2]+i[0].shape[0] for i in items)
        cv=np.zeros((y1-y0,x1-x0),np.uint8)
        for im,x,y in items:
            v=cv[y-y0:y-y0+im.shape[0],x-x0:x-x0+im.shape[1]];np.copyto(v,im,where=im!=0)
        return cv,x0,y0
    for i,b in enumerate(bind):
        old=unwound('approved-player:%d'%i,bac.unpack(raw[i*2048:(i+1)*2048]).reshape(8,8,8,8).transpose(0,2,1,3).reshape(64,64))
        before=comp(b,lambda m:base[m][1]);after=comp(b,lambda m:new.get(m,base[m][1]))
        if np.count_nonzero(before!=old)<=2:
            fixed=np.where(before==old,after,old);rule='recomposed from repaired sprites'
        else:
            src,sx,sy=source_composite(b);pad=np.pad(src,1)
            ky=np.clip(((np.arange(64)-56)*4+2)//3-sy,-1,src.shape[0])+1;kx=np.clip(((np.arange(64)-32)*4+2)//3-sx,-1,src.shape[1])+1
            fixed,log=fix_marco(old,pad,ky,kx);fixed,nb=marco_band(fixed);rule='set on the composite: eye '+log['status']+', headband %d px'%nb
        approved_log.append((i,rule,int((fixed!=old).sum())))
        if not np.array_equal(fixed,old):
            frames.append(dict(id='approved-player:%d'%i,target=dict(kind='approved-player',frame=i,binding=b),rule=rule,changed_pixels=int((fixed!=old).sum()),original=hexrows(old),rows=hexrows(fixed)))
    OUT.mkdir(parents=True,exist_ok=True)
    doc=dict(schema='mslug-player-face-fixes',version=1,marco_eye=dict(width=1,height=2,index=EYE),tarma_lens=dict(width=2,height=2,index=LENS),assets=assets+frames)
    (OUT/'patch.json').write_text(json.dumps(doc,separators=(',',':'))+'\n')
    summary=dict(marco=dict(stats[0]),tarma=dict(stats[1]),maps_changed=len(assets),pixels=sum(a['changed_pixels'] for a in assets),approved=approved_log)
    (OUT/'summary.json').write_text(json.dumps(summary,indent=1)+'\n');print(json.dumps({k:v for k,v in summary.items() if k!='approved'}));print('approved changed',[a for a in approved_log if a[2]])
if __name__=='__main__':main()
