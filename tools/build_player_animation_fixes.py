#!/usr/bin/env python3
"""Marco/Tarma animation asset repairs -> art/player-animation-fixes/patch.json

Works on the pixels the importer produces today (75% sample, then review-v18,
then character-local-fixes) and never touches the source sprites.

1. tears: a pixel the reviewed sprite made transparent although the plain
   importer sample of the source is opaque is restored (importer's palette
   index) when its connected piece is a see-through tear and not an outline
   trim: lying in a gap of the sprite and >=60% enclosed by sprite pixels, or
   the only bridge between parts that would otherwise float apart.
2. twins: for the listed transitions whose SOURCE frames are near-identical
   but whose scaled frames disagree, the follower frame reuses the reference
   frame's pixels wherever the two source pictures are equal.
3. approved Marco composites (player.tiles): frames that equal their
   body+arm sprite composite are recomposed from the repaired sprites; the
   others get rule 1 against that composite.

Consumed by import_scaled_drawings.py after character-local-fixes.
"""
import hashlib,json,sys
from pathlib import Path
import numpy as np
from PIL import Image
from scipy import ndimage as ndi
sys.path.insert(0,str(Path(__file__).resolve().parent))
import build_animation_consistency as bac
R=Path(__file__).resolve().parents[1];G=R/'generated';OUT=R/'art/player-animation-fixes'
ENCLOSED=0.6
# (reference drawing, follower drawing) in player_animation_program
TWINS=[(298,78),(298,84),(266,442),(432,596),(813,913)]
E8=np.ones((3,3),bool)
def hexrows(a):return [''.join('%x'%v for v in r) for r in a]
def kept(n):
    ramp=np.arange(n,dtype=np.int32)[None,:].repeat(4,0)
    return np.array(Image.fromarray(ramp).resize((n*3//2,6),Image.Resampling.NEAREST).resize((n*3//4,3),Image.Resampling.NEAREST))[0]
def s75(v):
    n=v*3;return n//4 if n>=0 else -((-n+3)//4)
class Scaled:
    """Current scaled maps decoded from the generated C table and pixel blob."""
    def __init__(s):
        import re
        t=(G/'scaled-drawing-data.c').read_text()
        def rows(name):
            m=re.search(r'const struct \w+ '+name+r'\[\]=\{(.*?)\};',t,re.S);return [tuple(map(int,r.split(','))) for r in re.findall(r'\{([^{}]+)\}',m[1])]
        s.maps=rows('native_scaled_maps');s.chunks=rows('native_scaled_chunks');s.raw=(G/'native-scaled-objects.4bpp').read_bytes()
    def canvas(s,mid,ox,oy,w,h):
        first,count=s.maps[mid*9];cv=np.zeros((h,w),np.uint8)
        for off,x,y,cw,ch,_,_ in s.chunks[first:first+count]:
            d=bac.unpack(s.raw[off:off+cw*ch//2]).reshape(ch//8,cw//8,8,8).transpose(0,2,1,3).reshape(ch,cw)
            yy,xx=np.nonzero(d);cv[y-oy+yy,x-ox+xx]=d[yy,xx]
        return cv
def grid(D,S,mid):
    """Importer grid of one map: source canvas, current scaled canvas, kept rows/cols, scaled origin, source origin."""
    first,count,attr=D.maps[mid];pp=D.pieces[first:first+count];img,cl,ct=D.content(mid)
    left=min(p[1] for p in pp)//4*4;top=min(-p[2] for p in pp)//4*4;right=(max(p[1]+16*p[3] for p in pp)+3)//4*4;bottom=(max(-p[2]+16*p[4] for p in pp)+3)//4*4
    src=np.zeros((bottom-top,right-left),np.uint8);src[ct-top:ct-top+img.shape[0],cl-left:cl-left+img.shape[1]]=img
    ox,oy=left*3//4,top*3//4;cur=S.canvas(mid,ox,oy,src.shape[1]*3//4,src.shape[0]*3//4)
    return src,cur,kept(src.shape[0]),kept(src.shape[1]),(ox,oy),(left,top)
def tears(cur,imp):
    """Restore see-through tears of `cur` from `imp`; returns (pixels, log)."""
    op=cur!=0;removed=(imp!=0)&~op
    if not removed.any():return cur,[]
    pad=np.pad(op,3);gap=(ndi.binary_closing(pad,structure=np.ones((5,5),bool))|ndi.binary_fill_holes(pad))[3:-3,3:-3]
    lab,n=ndi.label(removed&gap,structure=E8);parts,_=ndi.label(op,structure=E8);out=cur.copy();log=[]
    for i in range(1,n+1):
        c=lab==i;ring=ndi.binary_dilation(c,structure=E8)&~c
        ratio=np.count_nonzero(ring&op)/max(1,np.count_nonzero(ring));touching=set(np.unique(parts[ring&op]))-{0}
        why='enclosed' if ratio>=ENCLOSED else 'bridge' if len(touching)>=2 else None
        if why:out[c]=imp[c];log.append(dict(rule=why,pixels=int(c.sum()),enclosed=round(float(ratio),2)))
    return out,log
def twin(D,ref,fol,curs):
    """Follower canvas with the reference's pixels wherever both SOURCE canvases agree (1px safety margin)."""
    (ma,pxa,pya),(mb,pxb,pyb)=ref,fol
    sa,ca,_,_,(oxa,oya),(la,ta)=curs[ma];sb,cb,ky,kx,(oxb,oyb),(lb,tb)=curs[mb]
    # reference source/scaled canvases resampled onto the follower's canvases (actor space, zero shift)
    def onto(src,x,y,shape,X,Y):
        out=np.zeros(shape,np.uint8);ok=np.zeros(shape,bool)
        x0=max(X,x);y0=max(Y,y);x1=min(X+shape[1],x+src.shape[1]);y1=min(Y+shape[0],y+src.shape[0])
        if x1>x0 and y1>y0:out[y0-Y:y1-Y,x0-X:x1-X]=src[y0-y:y1-y,x0-x:x1-x];ok[y0-Y:y1-Y,x0-X:x1-X]=True
        return out,ok
    As,_=onto(sa,la+pxa,ta+pya,sb.shape,lb+pxb,tb+pyb)
    same=ndi.binary_erosion(As==sb,structure=E8,border_value=1)[np.ix_(ky,kx)]
    Ac,ok=onto(ca,oxa+s75(pxa),oya+s75(pya),cb.shape,oxb+s75(pxb),oyb+s75(pyb))
    use=same&ok&((Ac!=0)|(cb!=0));return np.where(use,Ac,cb)
def main():
    D=bac.Data();S=Scaled();first,count=next((f,c) for n,f,c in D.sets if n=='player_animation_program')
    prog=json.loads((G/'player-program-source.json').read_text());actor={}
    for c in prog['commands']:
        if c['op']=='DRAW':actor.setdefault(c['target'],set()).add(c['actor'])
    draw={}       # drawing -> (map, part x, part y down)
    for d in range(count):
        fp,n=D.drawings[first+d];pp=[p for p in D.parts[fp:fp+n] if p[2]!=65535 and p[2]<len(D.maps)]
        if len(pp)==1 and D.content(pp[0][2]) is not None:draw[d]=(pp[0][2],pp[0][0],-pp[0][1])
    owners={};
    for d,(m,_,_) in draw.items():owners.setdefault(m,dict(drawings=[],actors=set()));owners[m]['drawings'].append(d);owners[m]['actors']|=actor.get(d,set())
    curs={m:grid(D,S,m) for m in owners};new={};logs={}
    for m,(src,cur,ky,kx,o,_) in curs.items():
        fixed,log=tears(cur,src[np.ix_(ky,kx)])
        if log:new[m]=fixed;logs[m]=dict(tears=log)
    work={m:(c[0],new.get(m,c[1]))+c[2:] for m,c in curs.items()}
    for r,f in TWINS:
        mb=draw[f][0];before=work[mb][1];after=twin(D,draw[r],draw[f],work)
        after,_=tears(after,np.where(after!=0,after,before))      # a seam may not open a new tear
        if not np.array_equal(after,before):
            new[mb]=after;work[mb]=(work[mb][0],after)+work[mb][2:];logs.setdefault(mb,{})['twin']=dict(reference_drawing=r,follower_drawing=f,pixels=int((after!=before).sum()))
    assets=[]
    for m in sorted(new):
        src,cur,ky,kx,o,_=curs[m]
        assets.append(dict(id='map:%d'%D.ledger[m]['source'],target=dict(kind='scaled-map',source=D.ledger[m]['source'],map_id=m,origin=list(o)),
            actors=sorted(owners[m]['actors']),drawings=sorted(owners[m]['drawings']),changed_pixels=int((new[m]!=cur).sum()),**logs[m],original=hexrows(cur),rows=hexrows(new[m])))
    # ---- approved Marco composites ----
    bind=json.loads((G/'approved-player-source.json').read_text())['bindings'];raw=(G/'player.tiles').read_bytes();frames=[]
    def comp(b,pick):
        cv=np.zeros((64,64),np.uint8)
        for d,dy in ((b['lower'],0),(b['upper'],-b['offset_y'])):
            m,px,py=draw[d];src,cur,ky,kx,(ox,oy),_=curs[m];im=pick(m);yy,xx=np.nonzero(im)
            Y=yy+oy+s75(py)+dy+56;X=xx+ox+s75(px)+32;ok=(Y>=0)&(Y<64)&(X>=0)&(X<64);cv[Y[ok],X[ok]]=im[yy[ok],xx[ok]]
        return cv
    for i,b in enumerate(bind):
        old=bac.unpack(raw[i*2048:(i+1)*2048]).reshape(8,8,8,8).transpose(0,2,1,3).reshape(64,64)
        before=comp(b,lambda m:curs[m][1]);after=comp(b,lambda m:new.get(m,curs[m][1]))
        if np.count_nonzero(before!=old)<=2:
            fixed=np.where(before==old,after,old);rule='recomposed from repaired sprites'
        else:
            fixed,log=tears(old,np.where(old!=0,old,after));rule='tears restored from sprite composite' if log else None
        if not np.array_equal(fixed,old):
            frames.append(dict(id='approved-player:%d'%i,target=dict(kind='approved-player',frame=i,binding=b),rule=rule,changed_pixels=int((fixed!=old).sum()),original=hexrows(old),rows=hexrows(fixed)))
    OUT.mkdir(parents=True,exist_ok=True)
    doc=dict(schema='mslug-player-animation-fixes',version=1,enclosed_threshold=ENCLOSED,twins=TWINS,assets=assets+frames)
    (OUT/'patch.json').write_text(json.dumps(doc,separators=(',',':'))+'\n')
    summary=dict(maps=len(assets),map_pixels=sum(a['changed_pixels'] for a in assets),
        marco_maps=sum(1 for a in assets if 0 in a['actors']),tarma_maps=sum(1 for a in assets if 1 in a['actors']),
        marco_pixels=sum(a['changed_pixels'] for a in assets if 0 in a['actors']),tarma_pixels=sum(a['changed_pixels'] for a in assets if 1 in a['actors']),
        twin_maps=sum(1 for a in assets if 'twin' in a),approved_frames=len(frames),approved_pixels=sum(f['changed_pixels'] for f in frames),
        approved=[(f['target']['frame'],f['rule'],f['changed_pixels']) for f in frames])
    (OUT/'summary.json').write_text(json.dumps(summary,indent=1)+'\n');print(json.dumps(summary))
if __name__=='__main__':main()
