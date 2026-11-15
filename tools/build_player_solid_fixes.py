#!/usr/bin/env python3
"""Marco/Tarma "punched-through" repairs -> art/player-solid-fixes/patch.json

Stage 2 of the player repairs. It starts from the pixels left by
review-v18, character-local-fixes and art/player-animation-fixes (stage 1)
and never touches the source sprites. Every decision is taken against the
untouched source map through the importer's own sampling grid, so each
scaled pixel is compared with exactly the source pixel it stands for.

1. pinhole: a transparent pixel whose source pixel and that pixel's four
   source neighbours are all opaque, and which has at least three opaque
   scaled neighbours, is filled with the source pixel. Silhouette trims
   (two or fewer opaque neighbours) are left alone.
2. dark dot: a near-black (palette index 7) pixel completely surrounded by
   sprite, whose 3x3 source window is fully opaque and holds at most
   MAX_DARK near-black pixels, takes the most common other colour of that
   window (the sampled source pixel itself when it is not near-black).
   A real 1px outline crossing the window has three near-black pixels and
   is therefore kept.
3. approved Marco composites (player.tiles): frames that equal their
   body+arm sprite composite are recomposed from the repaired sprites; the
   others get rules 1 and 2 against the source composite.

Consumed by import_scaled_drawings.py after art/player-animation-fixes.
Re-running is safe: pixels already changed by this patch or by the later
art/player-twin-fixes stage are unwound through the `original` rows those
patches record. After a re-run that changes this patch, rebuild stage 3.
"""
import json,sys
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parent))
import build_animation_consistency as bac
import build_player_animation_fixes as stage1
R=Path(__file__).resolve().parents[1];G=R/'generated';OUT=R/'art/player-solid-fixes'
DARK=7;MAX_DARK=2
hexrows=stage1.hexrows;s75=stage1.s75
def unhex(rows):return np.array([[int(c,16) for c in r] for r in rows],np.uint8)
def neighbours(op):
    """Number of opaque 4-neighbours per pixel."""
    p=np.pad(op,1).astype(np.int8);return p[:-2,1:-1]+p[2:,1:-1]+p[1:-1,:-2]+p[1:-1,2:]
def repair(cur,src,ky,kx):
    """cur: scaled canvas; src: source canvas; ky/kx: source row/col of every scaled row/col."""
    out=cur.copy();log=dict(pinhole=0,dark=0);H,W=src.shape
    sop=src!=0;sp=np.pad(sop,1)
    solid5=sop&sp[:-2,1:-1]&sp[2:,1:-1]&sp[1:-1,:-2]&sp[1:-1,2:]        # source pixel and 4 neighbours opaque
    n=neighbours(out!=0)
    for j,i in zip(*np.nonzero((out==0)&solid5[np.ix_(ky,kx)]&(n>=3))):
        out[j,i]=src[ky[j],kx[i]];log['pinhole']+=1
    n=neighbours(out!=0)
    for j,i in zip(*np.nonzero((out==DARK)&(n==4))):
        y,x=ky[j],kx[i]
        if y<1 or x<1 or y>=H-1 or x>=W-1:continue
        w=src[y-1:y+2,x-1:x+2]
        if (w==0).any() or np.count_nonzero(w==DARK)>MAX_DARK:continue
        out[j,i]=src[y,x] if src[y,x]!=DARK else int(np.bincount(w[w!=DARK],minlength=16).argmax());log['dark']+=1
    return out,log
def repair_composite(old,src,sx,sy):
    """Rules 1 and 2 for a 64x64 approved frame (origin -32,-56) against a source composite at (sx,sy)."""
    ky=np.clip(((np.arange(64)-56)*4+2)//3-sy,-1,src.shape[0]);kx=np.clip(((np.arange(64)-32)*4+2)//3-sx,-1,src.shape[1])
    pad=np.pad(src,1);return repair(old,pad,ky+1,kx+1)
def main():
    D=bac.Data();S=stage1.Scaled();first,count=next((f,c) for n,f,c in D.sets if n=='player_animation_program')
    prog=json.loads((G/'player-program-source.json').read_text());actor={}
    for c in prog['commands']:
        if c['op']=='DRAW':actor.setdefault(c['target'],set()).add(c['actor'])
    # Unwind in reverse order whatever later stages and this one already put into the generated data.
    unwind=[]
    for q in (R/'art/player-face-fixes/patch.json',R/'art/player-twin-fixes/patch.json',OUT/'patch.json'):
        unwind.append({a['id']:a for a in json.loads(q.read_text())['assets']} if q.exists() else {})
    def stage1_pixels(key,pixels):
        for patch in unwind:
            a=patch.get(key)
            if a is None:continue
            assert np.array_equal(pixels,unhex(a['rows'])),('generated data does not match the recorded fix stages',key)
            pixels=unhex(a['original'])
        return pixels
    draw={}
    for d in range(count):
        fp,n=D.drawings[first+d];pp=[p for p in D.parts[fp:fp+n] if p[2]!=65535 and p[2]<len(D.maps)]
        if len(pp)==1 and D.content(pp[0][2]) is not None:draw[d]=(pp[0][2],pp[0][0],-pp[0][1])
    owners={}
    for d,(m,_,_) in draw.items():owners.setdefault(m,dict(drawings=[],actors=set()));owners[m]['drawings'].append(d);owners[m]['actors']|=actor.get(d,set())
    curs={}
    for m in owners:
        g=stage1.grid(D,S,m);curs[m]=(g[0],stage1_pixels('map:%d'%D.ledger[m]['source'],g[1]))+g[2:]
    new={};logs={}
    for m,(src,cur,ky,kx,o,_) in curs.items():
        fixed,log=repair(cur,src,ky,kx)
        if not np.array_equal(fixed,cur):new[m]=fixed;logs[m]=log
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
            m,px,py=draw[d];(ox,oy)=curs[m][4];im=pick(m);yy,xx=np.nonzero(im)
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
        old=bac.unpack(raw[i*2048:(i+1)*2048]).reshape(8,8,8,8).transpose(0,2,1,3).reshape(64,64)
        old=stage1_pixels('approved-player:%d'%i,old)
        before=comp(b,lambda m:curs[m][1]);after=comp(b,lambda m:new.get(m,curs[m][1]))
        if np.array_equal(before,old):fixed=after;rule='recomposed from repaired sprites';log={}
        else:
            fixed,log=repair_composite(old,*source_composite(b));rule='pinholes and dark dots against source composite'
        if not np.array_equal(fixed,old):
            frames.append(dict(id='approved-player:%d'%i,target=dict(kind='approved-player',frame=i,binding=b),rule=rule,changed_pixels=int((fixed!=old).sum()),**log,original=hexrows(old),rows=hexrows(fixed)))
    OUT.mkdir(parents=True,exist_ok=True)
    doc=dict(schema='mslug-player-solid-fixes',version=1,dark_index=DARK,max_dark_in_source_window=MAX_DARK,assets=assets+frames)
    (OUT/'patch.json').write_text(json.dumps(doc,separators=(',',':'))+'\n')
    def side(a):
        rows=[x for x in assets if a in x['actors']];return dict(maps=len(rows),pinhole=sum(x['pinhole'] for x in rows),dark=sum(x['dark'] for x in rows))
    summary=dict(player_maps=len(owners),maps_changed=len(assets),pinhole_pixels=sum(a['pinhole'] for a in assets),dark_pixels=sum(a['dark'] for a in assets),
        marco=side(0),tarma=side(1),approved_frames=len(frames),approved_pixels=sum(f['changed_pixels'] for f in frames),
        approved=[(f['target']['frame'],f['rule'],f['changed_pixels']) for f in frames])
    (OUT/'summary.json').write_text(json.dumps(summary,indent=1)+'\n');print(json.dumps(summary))
if __name__=='__main__':main()
