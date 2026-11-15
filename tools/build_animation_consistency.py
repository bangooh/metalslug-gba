#!/usr/bin/env python3
"""Find animation frames that are the same picture moved by a few source
pixels, and rescale them on ONE shared 75% sampling lattice.

The stock importer drops every fourth row/column on a lattice anchored at
each map's own origin. When consecutive frames are a rigid 1..3 px move of
the same picture (idling engines, recoil, bobbing), each frame loses different
rows/columns, so the 75% frames differ in 20..50% of their pixels although the
source frames differ in under 6%. On screen the texture boils.

Output: art/animation-consistency/patch.json, consumed by
import_scaled_drawings.py. Only maps whose source frames are near-identical
after a shift are touched; hand-reviewed maps are never overridden.
"""
import collections,hashlib,json,re,sys
from pathlib import Path
import numpy as np
from PIL import Image
R=Path(__file__).resolve().parents[1];G=R/'generated';OUT=R/'art/animation-consistency'
RIGID=0.06      # max fraction of differing source pixels after the best shift
SEARCH=3        # source pixels
def rows(s,name,pat=r'const struct \w+ '):
    m=re.search(pat+name+r'\[\]=\{(.*?)\};',s,re.S);return [tuple(map(int,r.split(','))) for r in re.findall(r'\{([^{}]+)\}',m[1])]
def unpack(raw):
    a=np.frombuffer(raw,dtype=np.uint8).reshape(-1,8,4);o=np.empty((len(a),8,8),np.uint8);o[:,:,::2]=a&15;o[:,:,1::2]=a>>4;return o
def scale(im):
    h,w=im.shape
    return np.array(Image.fromarray(im).resize((w*3//2,h*3//2),Image.Resampling.NEAREST).resize((w*3//4,h*3//4),Image.Resampling.NEAREST))
def s75(v):
    n=v*3;return n//4 if n>=0 else -((-n+3)//4)
def rnd75(v):
    return (abs(v)*3+2)//4*(1 if v>=0 else -1)
class Data:
    def __init__(s):
        t=(G/'drawing-data.c').read_text()
        s.drawings=rows(t,'drawings',r'static const struct \w+ ');s.parts=rows(t,'native_draw_parts');s.maps=rows(t,'native_draw_maps');s.pieces=rows(t,'native_draw_pieces')
        s.tiles=list(map(int,re.search(r'const uint16_t native_draw_tiles\[\]=\{([^}]+)\};',t)[1].split(',')))
        s.sets=[(a,int(b),int(c)) for a,b,c in re.findall(r'\{&(\w+),&drawings\[(\d+)\],(\d+)\}',t)]
        s.ledger=json.loads((G/'drawing-source.json').read_text())['maps']
        s.progs={}
        for f in sorted(G.glob('*.c')):
            x=f.read_text();m=re.search(r'const struct AnimationProgram (\w+)=\{commands',x)
            if m:s.progs[m[1]]=[tuple(r.split(',')) for r in re.findall(r'\{([^{}]+)\}',re.search(r'static const struct AnimationCommand commands\[\]=\{(.*?)\};',x,re.S)[1])]
        raw=unpack((G/'native-sprites.4bpp').read_bytes());s.sprites=raw.reshape(-1,2,2,8,8).transpose(0,1,3,2,4).reshape(-1,16,16);s.cache={}
        reviewed=json.loads((R/'art/review-v18/overrides.json').read_text())['assets']
        s.locked={a['target']['source'] for a in reviewed if a['target']['kind'] in('scaled-map','score','popup')}
        local=R/'art/character-local-fixes/patch.json'
        if local.exists():s.locked|={a['target']['source'] for a in json.loads(local.read_text())['assets'] if a['target']['kind']=='scaled-map'}
    def content(s,mid):
        """Source pixels of one map, cropped: (pixels,left,top) in map-local Y-down."""
        if mid not in s.cache:
            first,count,attr=s.maps[mid];pp=s.pieces[first:first+count]
            left=min(p[1] for p in pp);top=min(-p[2] for p in pp);right=max(p[1]+16*p[3] for p in pp);bottom=max(-p[2]+16*p[4] for p in pp)
            cv=np.zeros((bottom-top,right-left),np.uint8)
            for start,x,y,w,h in pp:
                for col in range(w):
                    for row in range(h):
                        src=s.sprites[((attr>>4)<<16)|s.tiles[start+col*h+row]];d=cv[-y-top+16*row:-y-top+16*row+16,x-left+16*col:x-left+16*col+16];np.copyto(d,src,where=src!=0)
            ys,xs=np.nonzero(cv)
            s.cache[mid]=None if not len(ys) else (cv[ys.min():ys.max()+1,xs.min():xs.max()+1].copy(),left+int(xs.min()),top+int(ys.min()))
        return s.cache[mid]
def successors(prog):
    n=len(prog);out={}
    for i,c in enumerate(prog):
        if c[3]!='AN_DRAW':continue
        j=int(c[1]);hops=0
        while 0<=j<n and hops<64 and prog[j][3] not in('AN_DRAW','AN_END'):
            j=int(prog[j][2]) if prog[j][3]=='AN_JUMP' else int(prog[j][1]);hops+=1
        if 0<=j<n and prog[j][3]=='AN_DRAW':out[i]=j
    return out
def best_shift(a,b):
    """Smallest mismatch of b moved by (dx,dy) over a; both (pixels,x,y) in actor space."""
    (A,ax,ay),(B,bx,by)=a,b;r=SEARCH
    x0=min(ax,bx)-r;y0=min(ay,by)-r;x1=max(ax+A.shape[1],bx+B.shape[1])+r;y1=max(ay+A.shape[0],by+B.shape[0])+r
    ca=np.zeros((y1-y0,x1-x0),np.uint8);ca[ay-y0:ay-y0+A.shape[0],ax-x0:ax-x0+A.shape[1]]=A;res=None
    for dy in range(-r,r+1):
        for dx in range(-r,r+1):
            cb=np.zeros_like(ca);cb[by-y0+dy:by-y0+dy+B.shape[0],bx-x0+dx:bx-x0+dx+B.shape[1]]=B
            v=np.count_nonzero(ca!=cb)/max(1,np.count_nonzero((ca!=0)|(cb!=0)))
            key=(v,abs(dx)+abs(dy))
            if res is None or key<res[0]:res=(key,dx,dy)
    return res[0][0],res[1],res[2]
def main():
    D=Data();edges=collections.defaultdict(dict);frames={}
    for name,first,count in D.sets:
        prog=D.progs.get(name)
        if not prog:continue
        def frame(d):
            if d>=count:return None
            fp,n=D.drawings[first+d];pp=[p for p in D.parts[fp:fp+n] if p[2]!=65535 and p[2]<len(D.maps)]
            if len(pp)!=1:return None
            c=D.content(pp[0][2])
            if c is None or c[0].shape[0]>200 or c[0].shape[1]>320:return None
            return dict(map=pp[0][2],px=pp[0][0],py=-pp[0][1],img=(c[0],c[1]+pp[0][0],c[2]-pp[0][1]))
        done=set()
        for i,j in successors(prog).items():
            a,b=int(prog[i][2]),int(prog[j][2])
            if a==b or (a,b) in done or (b,a) in done:continue
            done.add((a,b));fa,fb=frame(a),frame(b)
            if not fa or not fb or fa['map']==fb['map']:continue
            A,B=fa['img'][0],fb['img'][0]
            if abs(A.shape[0]-B.shape[0])>2*SEARCH or abs(A.shape[1]-B.shape[1])>2*SEARCH or np.count_nonzero(A)<80:continue
            v,dx,dy=best_shift(fa['img'],fb['img'])
            if v>RIGID:continue
            # b moved by (dx,dy) matches a  =>  b's picture is a's picture moved by (-dx,-dy)
            frames[(name,a)]=fa;frames[(name,b)]=fb
            edges[(name,a)][(name,b)]=(-dx,-dy,v);edges[(name,b)][(name,a)]=(dx,dy,v)
    seen=set();wanted=collections.defaultdict(list);groups=[]
    for start in sorted(edges):
        if start in seen:continue
        comp=[start];seen.add(start);delta={start:(0,0)};q=[start]
        while q:
            u=q.pop(0)
            for v,(dx,dy,_) in sorted(edges[u].items()):
                if v in seen:continue
                seen.add(v);delta[v]=(delta[u][0]+dx,delta[u][1]+dy);comp.append(v);q.append(v)
        # Reference: the frame with most rigid neighbours keeps its pixels.
        ref=max(comp,key=lambda n:(len(edges[n]),-comp.index(n)));rx,ry=delta[ref];fr=frames[ref]
        groups.append(dict(reference=list(ref),frames=[list(n)+[delta[n][0]-rx,delta[n][1]-ry] for n in comp]))
        for n in comp:
            f=frames[n];dx,dy=delta[n][0]-rx,delta[n][1]-ry
            g0=(fr['px']+dx-f['px'],fr['py']+dy-f['py'])
            k=(s75(fr['px'])+rnd75(dx)-s75(f['px']),s75(fr['py'])+rnd75(dy)-s75(f['py']))
            wanted[f['map']].append((g0,k,n,ref))
    assets=[];skipped=[]
    for mid,uses in sorted(wanted.items()):
        src=D.ledger[mid]['source'];plans={(u[0][0]%4,u[0][1]%4,u[1][0]-(u[0][0]*3-(u[0][0]%4)*3)//4,u[1][1]-(u[0][1]*3-(u[0][1]%4)*3)//4) for u in uses}
        if len(plans)>1:skipped.append(dict(map=mid,reason='used at conflicting phases',uses=[list(u[2]) for u in uses]));continue
        if src in D.locked:skipped.append(dict(map=mid,reason='hand-reviewed map',uses=[list(u[2]) for u in uses]));continue
        (g0,k,_,_)=uses[0];img,cl,ct=D.content(mid)
        # Importer result today (lattice anchored at map-local0).
        first,count,attr=D.maps[mid];pp=D.pieces[first:first+count]
        left=min(p[1] for p in pp)//4*4;top=min(-p[2] for p in pp)//4*4;right=(max(p[1]+16*p[3] for p in pp)+3)//4*4;bottom=(max(-p[2]+16*p[4] for p in pp)+3)//4*4
        cur=np.zeros((bottom-top,right-left),np.uint8);cur[ct-top:ct-top+img.shape[0],cl-left:cl-left+img.shape[1]]=img;cur=scale(cur)
        # Same picture on the reference lattice: origin congruent to g0 mod4.
        L=cl-((cl-g0[0])%4);T=ct-((ct-g0[1])%4);W=-(-(cl+img.shape[1]-L)//4)*4;H=-(-(ct+img.shape[0]-T)//4)*4
        cv=np.zeros((H,W),np.uint8);cv[ct-T:ct-T+img.shape[0],cl-L:cl-L+img.shape[1]]=img;new=scale(cv)
        ox=(L-g0[0])*3//4+k[0];oy=(T-g0[1])*3//4+k[1]
        # Unchanged when both the pixels and their placement already agree.
        def norm(a,x,y):
            ys,xs=np.nonzero(a);return (a[ys.min():ys.max()+1,xs.min():xs.max()+1].tobytes(),a[ys.min():ys.max()+1,xs.min():xs.max()+1].shape,x+int(xs.min()),y+int(ys.min()))
        if norm(cur,left*3//4,top*3//4)==norm(new,ox,oy):continue
        assets.append(dict(map=mid,target=dict(kind='scaled-map',source=src),origin=[ox,oy],baseline_sha256=hashlib.sha256(cur.tobytes()).hexdigest(),
            rows=[''.join('%x'%v for v in r) for r in new],frames=[list(u[2]) for u in uses],reference=list(uses[0][3])))
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'patch.json').write_text(json.dumps(dict(schema='mslug-lattice-patch',version=1,rigid_threshold=RIGID,search=SEARCH,assets=assets),separators=(',',':'))+'\n')
    (OUT/'groups.json').write_text(json.dumps(dict(groups=groups,skipped=skipped),indent=1)+'\n')
    print(json.dumps(dict(groups=len(groups),frames=sum(len(g['frames']) for g in groups),maps_rewritten=len(assets),skipped=len(skipped))))
if __name__=='__main__':main()
