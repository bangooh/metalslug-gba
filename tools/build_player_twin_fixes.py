#!/usr/bin/env python3
"""Marco/Tarma frame-to-frame consistency -> art/player-twin-fixes/patch.json

Stage 3 of the player repairs (after art/player-animation-fixes and
art/player-solid-fixes). It never touches the source sprites.

Problem: two consecutive animation frames whose SOURCE pictures are almost
the same (a recoil, a blink, a 1px bob) were reduced and reviewed one by
one, so their 75% pictures disagree all over and the sprite shimmers.

Fix: inside every group of such frames one frame is the reference. Each
follower takes the reference's pixels wherever the two source pictures are
equal (1px safety margin, after the rigid shift between them) and keeps
its own pixels where the source really differs. Followers are reached
breadth-first, so a chain a>b>c ends up consistent with one picture.

A follower is only rewritten when the texture mismatch over ALL of its
transitions drops; otherwise it is left as it was.

boil = mismatch(scaled pair, best shift) - mismatch(source pair, best shift)
"""
import json,sys,collections
from pathlib import Path
import numpy as np
from scipy import ndimage as ndi
sys.path.insert(0,str(Path(__file__).resolve().parent))
import build_animation_consistency as bac
import build_player_animation_fixes as stage1
import build_player_solid_fixes as stage2
R=Path(__file__).resolve().parents[1];G=R/'generated';OUT=R/'art/player-twin-fixes'
RELATED=0.45      # source mismatch up to which two frames count as the same picture
MIN_BOIL=0.05     # transitions calmer than this are left alone
MIN_GAIN=0.03     # required drop of a follower's summed boil
E8=np.ones((3,3),bool);hexrows=stage1.hexrows;s75=stage1.s75;unhex=stage2.unhex
def mismatch(a,b,r=3):
    """a,b=(canvas,x,y). Best (fraction,dx,dy): b moved by (dx,dy) compared with a."""
    (A,ax,ay),(B,bx,by)=a,b
    x0=min(ax,bx)-r;y0=min(ay,by)-r;x1=max(ax+A.shape[1],bx+B.shape[1])+r;y1=max(ay+A.shape[0],by+B.shape[0])+r
    ca=np.zeros((y1-y0,x1-x0),np.uint8);ca[ay-y0:ay-y0+A.shape[0],ax-x0:ax-x0+A.shape[1]]=A;best=None
    for dy in range(-r,r+1):
        for dx in range(-r,r+1):
            cb=np.zeros_like(ca);cb[by-y0+dy:by-y0+dy+B.shape[0],bx-x0+dx:bx-x0+dx+B.shape[1]]=B
            k=(np.count_nonzero(ca!=cb)/max(1,np.count_nonzero((ca!=0)|(cb!=0))),abs(dx)+abs(dy))
            if best is None or k<best[0]:best=(k,dx,dy)
    return best[0][0],best[1],best[2]
def onto(src,x,y,shape,X,Y):
    out=np.zeros(shape,np.uint8);ok=np.zeros(shape,bool)
    x0=max(X,x);y0=max(Y,y);x1=min(X+shape[1],x+src.shape[1]);y1=min(Y+shape[0],y+src.shape[0])
    if x1>x0 and y1>y0:out[y0-Y:y1-Y,x0-X:x1-X]=src[y0-y:y1-y,x0-x:x1-x];ok[y0-Y:y1-Y,x0-X:x1-X]=True
    return out,ok
def main():
    D=bac.Data();S=stage1.Scaled();first,count=next((f,c) for n,f,c in D.sets if n=='player_animation_program')
    prog=json.loads((G/'player-program-source.json').read_text());actor={}
    for c in prog['commands']:
        if c['op']=='DRAW':actor.setdefault(c['target'],set()).add(c['actor'])
    # unwind the later face stage and this stage, newest first
    unwind=[{a['id']:a for a in json.loads(q.read_text())['assets']} if q.exists() else {} for q in (R/'art/player-face-fixes/patch.json',OUT/'patch.json')]
    def stage2_pixels(key,pixels):
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
    base={}
    for m in owners:
        g=stage1.grid(D,S,m);base[m]=(g[0],stage2_pixels('map:%d'%D.ledger[m]['source'],g[1]))+g[2:]
    work={m:g[1].copy() for m,g in base.items()}
    def src_frame(d):
        m,px,py=draw[d];g=base[m];return g[0],g[5][0]+px,g[5][1]+py
    def scl_frame(d,pix=None):
        m,px,py=draw[d];g=base[m];return (work[m] if pix is None else pix),g[4][0]+s75(px),g[4][1]+s75(py)
    # transitions between different maps of the player program
    prog_cmds=D.progs['player_animation_program'];seen=set();trans=[]
    for i,j in bac.successors(prog_cmds).items():
        a,b=int(prog_cmds[i][2]),int(prog_cmds[j][2])
        if a==b or (a,b) in seen or (b,a) in seen or a not in draw or b not in draw or draw[a][0]==draw[b][0]:continue
        if np.count_nonzero(src_frame(a)[0])<40 or np.count_nonzero(src_frame(b)[0])<40:continue
        seen.add((a,b));vs,sx,sy=mismatch(src_frame(a),src_frame(b));trans.append(dict(a=a,b=b,src=vs,s=(sx,sy)))
    def boil(t,pa=None,pb=None):
        return mismatch(scl_frame(t['a'],pa),scl_frame(t['b'],pb))[0]-t['src']
    for t in trans:t['before']=boil(t)
    by_draw=collections.defaultdict(list)
    for t in trans:
        if t['src']<=0.45:by_draw[t['a']].append(t);by_draw[t['b']].append(t)
    def twin(ref,fol,s,t):
        """Follower pixels: reference picture where the sources agree. B(p)==A(p+s); scaled copy from A(q+t)."""
        ma,pxa,pya=draw[ref];mb,pxb,pyb=draw[fol]
        sa,_,_,_,(oxa,oya),(la,ta)=base[ma];sb,_,ky,kx,(oxb,oyb),(lb,tb)=base[mb];cb=work[mb]
        As,_=onto(sa,la+pxa-s[0],ta+pya-s[1],sb.shape,lb+pxb,tb+pyb)
        same=ndi.binary_erosion(As==sb,structure=E8,border_value=1)[np.ix_(ky,kx)]
        Ac,ok=onto(work[ma],oxa+s75(pxa)-t[0],oya+s75(pya)-t[1],cb.shape,oxb+s75(pxb),oyb+s75(pyb))
        use=same&ok&((Ac!=0)|(cb!=0));out=np.where(use,Ac,cb)
        out=np.where((out==0)&(cb!=0)&(sb[np.ix_(ky,kx)]!=0),cb,out)      # a seam may not open a hole
        return out
    new={};logs={};groups=[]
    # Later passes start from frames rewritten earlier, so a frame that now
    # disagrees with a rewritten neighbour follows it too.
    for sweep in range(4):
        cand=[t for t in trans if t['src']<=RELATED and boil(t)>=MIN_BOIL]
        adj=collections.defaultdict(list)
        for t in cand:
            adj[t['a']].append((t['b'],t['s']));adj[t['b']].append((t['a'],(-t['s'][0],-t['s'][1])))
        done=set();count_before=len(new)
        for start in sorted(adj,key=lambda d:(draw[d][0] not in new,-len(adj[d]),d)):
            if start in done:continue
            done.add(start);queue=[start];members=[start];fixed_here=[]
            while queue:
                ref=queue.pop(0)
                for fol,s in adj[ref]:
                    if fol in done:continue
                    done.add(fol);queue.append(fol);members.append(fol);mb=draw[fol][0]
                    if mb in new or mb==draw[ref][0]:continue
                    options=[]
                    fl=lambda v:sorted({(3*v)//4,-((-3*v)//4)})
                    for tx in fl(s[0]):
                        for ty in fl(s[1]):
                            pix=twin(ref,fol,s,(tx,ty))
                            if np.array_equal(pix,work[mb]):continue
                            users=[d for d in owners[mb]['drawings']]
                            ts=[t for d in users for t in by_draw[d]]
                            def total(p):
                                return sum(boil(t,p if draw[t['a']][0]==mb else None,p if draw[t['b']][0]==mb else None) for t in ts)
                            options.append((total(pix),pix,(tx,ty),total(work[mb])))
                    if not options:continue
                    after,pix,t,before=min(options,key=lambda o:o[0])
                    if before-after<MIN_GAIN:continue
                    logs[mb]=dict(reference_drawing=ref,follower_drawing=fol,source_shift=list(s),scaled_shift=list(t),pixels=int((pix!=work[mb]).sum()),boil_sum_before=round(before,4),boil_sum_after=round(after,4))
                    work[mb]=pix;new[mb]=pix;fixed_here.append(fol)
            if fixed_here:groups.append(dict(reference_drawing=start,members=sorted(members),rewritten=sorted(fixed_here)))
        if len(new)==count_before:break
    for t in trans:t['after']=boil(t)
    assets=[]
    for m in sorted(new):
        cur=base[m][1]
        if np.array_equal(new[m],cur):continue
        assets.append(dict(id='map:%d'%D.ledger[m]['source'],target=dict(kind='scaled-map',source=D.ledger[m]['source'],map_id=m,origin=list(base[m][4])),
            actors=sorted(owners[m]['actors']),drawings=sorted(owners[m]['drawings']),changed_pixels=int((new[m]!=cur).sum()),twin=logs[m],original=hexrows(cur),rows=hexrows(new[m])))
    # ---- approved Marco composites that equal their sprite composite follow the sprites ----
    bind=json.loads((G/'approved-player-source.json').read_text())['bindings'];raw=(G/'player.tiles').read_bytes();frames=[]
    def comp(b,pick):
        cv=np.zeros((64,64),np.uint8)
        for d,dy in ((b['lower'],0),(b['upper'],-b['offset_y'])):
            m,px,py=draw[d];(ox,oy)=base[m][4];im=pick(m);yy,xx=np.nonzero(im)
            Y=yy+oy+s75(py)+dy+56;X=xx+ox+s75(px)+32;ok=(Y>=0)&(Y<64)&(X>=0)&(X<64);cv[Y[ok],X[ok]]=im[yy[ok],xx[ok]]
        return cv
    skipped=[]
    for i,b in enumerate(bind):
        old=bac.unpack(raw[i*2048:(i+1)*2048]).reshape(8,8,8,8).transpose(0,2,1,3).reshape(64,64)
        old=stage2_pixels('approved-player:%d'%i,old)
        before=comp(b,lambda m:base[m][1]);after=comp(b,lambda m:work[m])
        if np.array_equal(before,after):continue
        if np.count_nonzero(before!=old)<=2:
            fixed=np.where(before==old,after,old)      # keeps the one or two hand-set pixels of the composite
            frames.append(dict(id='approved-player:%d'%i,target=dict(kind='approved-player',frame=i,binding=b),rule='recomposed from repaired sprites',changed_pixels=int((fixed!=old).sum()),original=hexrows(old),rows=hexrows(fixed)))
        else:skipped.append(i)
    OUT.mkdir(parents=True,exist_ok=True)
    doc=dict(schema='mslug-player-twin-fixes',version=1,related=RELATED,min_boil=MIN_BOIL,min_gain=MIN_GAIN,groups=groups,assets=assets+frames)
    plain=lambda o:int(o) if isinstance(o,np.integer) else float(o)
    (OUT/'patch.json').write_text(json.dumps(doc,separators=(',',':'),default=plain)+'\n')
    def side(a):
        tt=[t for t in trans if t['src']<=0.45 and a in actor.get(t['a'],())]
        return dict(related_transitions=len(tt),maps_rewritten=sum(1 for x in assets if a in x['actors']),
            boil_over_10pct=[sum(t['before']>.10 for t in tt),sum(t['after']>.10 for t in tt)],boil_over_15pct=[sum(t['before']>.15 for t in tt),sum(t['after']>.15 for t in tt)],
            boil_over_20pct=[sum(t['before']>.20 for t in tt),sum(t['after']>.20 for t in tt)],mean_boil=[round(float(np.mean([t['before'] for t in tt])),4),round(float(np.mean([t['after'] for t in tt])),4)],
            worse_by_2pct=sum(t['after']-t['before']>.02 for t in tt))
    summary=dict(note='pairs are [before,after]',groups=len(groups),maps_rewritten=len(assets),pixels=sum(a['changed_pixels'] for a in assets),marco=side(0),tarma=side(1),
        approved_frames=[(f['target']['frame'],f['changed_pixels']) for f in frames],approved_not_following_sprites=skipped,
        transitions=[dict(a=t['a'],b=t['b'],actors=sorted(actor.get(t['a'],())),source=round(t['src'],4),before=round(t['before'],4),after=round(t['after'],4)) for t in trans if t['src']<=0.45 and (t['before']>.10 or abs(t['after']-t['before'])>.02)])
    (OUT/'summary.json').write_text(json.dumps(summary,indent=1,default=plain)+'\n')
    print(json.dumps({k:v for k,v in summary.items() if k!='transitions'},default=plain))
if __name__=='__main__':main()
