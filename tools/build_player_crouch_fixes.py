#!/usr/bin/env python3
"""Marco crouch review (v186) -> art/player-crouch-fixes/patch.json, applied to generated/.

Works directly on the packed 75% OBJ data (generated/scaled-drawing-data.c and
native-scaled-objects.4bpp), like tools/apply_character_assets.py, because the
full reimport (import_scaled_drawings.py) no longer matches drawing-data.c in v185.
Changed 8x8 blocks are appended to the pixel blob and only the chunks of the edited
maps are re-pointed, so tiles shared with other maps never change.

Rules (coordinates: row, column in the map picture whose top-left is ORIGIN[map]):
 HEAD_FROM  crouched heavy-machine-gun fire 7884-7888. The head slides back 1 and 2
            pixels with the recoil; at 75% the three recoil frames each came out with a
            different, narrower face (eye and nose lost). Like Tarma's 8473-8477, which
            keep one head through the recoil, they take the head of the first frame
            (7884) moved back with the recoil. Donor box: rows 1-7 cols 9-17, plus the
            drawn chin pixels of row 8 cols 10-16.
 BOX_FROM   8024 takes the head of 8023 (see the table below).
 PIXEL_EDIT crouched pistol fire 7920: the dark block under the headband was 3 wide ->
            skin behind a 1x2 eye; stray black pixel above the wrist removed; the
            pistol's underside (mixed pixels, bottom outline lost) redrawn like Tarma's
            8509: one fill row, then the outline row.
Re-running is a no-op once generated/ holds the patched pictures.
Usage: python3 tools/build_player_crouch_fixes.py [--check]
"""
import json,re,sys,hashlib
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1];G=R/'generated';OUT=R/'art/player-crouch-fixes'
ORIGIN={7832:(-15,-24),7833:(-15,-24),7834:(-15,-24),7835:(-15,-27),**{m:(-12,-24) for m in (7829,7830,7831,7836,7837,7838,8029,8030,8031,7814,7815,7820,7824,7825,7826,7827,7828)},7888:(-12,-30),7884:(-12,-30),7885:(-12,-30),7886:(-12,-30),7887:(-12,-30),7920:(-12,-24),**{m:(-12,-24) for m in range(8021,8029)},
        7603:(-12, -24), 7604:(-12, -24), 7606:(-12, -24), 7607:(-12, -24), 7608:(-12, -24), 7612:(-9, -36), 7613:(-9, -36), 7806:(-12, -24), 7807:(-12, -24), 7811:(-12, -24), 7812:(-12, -24), 7813:(-12, -24)}
SIZE=(48,64)                       # rows, columns of the working picture
# Hair above the headband (user review): in the 7884 head the yellow hair row right over the
# band was cut in two by three dark pixels. They become hair yellow, like the idle head
# (orange, yellow x3, orange). Done on 7884 and its twin 7888 first, so the recoil frames
# that take the 7884 head get the joined hair too.
HEADFULL=[(6,8,12,21),(8,11,11,21),(11,14,12,21)]                  # head with its chin row
# Second review round:
#  7604, 7606-7608  crouched pistol idle: the same ducked dark heads as 8024/8026-8028 -> head of 7603
BOX_FROM2=[(7604,7603,HEADFULL),(7606,7603,HEADFULL),(7607,7603,HEADFULL),(7608,7603,HEADFULL)]
#  crouched pistol fire 7806-7813: every open eye 1 wide, 2 tall, 3 columns behind the nose outline
#    7806/7812/7813  extra dark pixel behind the eye -> skin     7807  no eye -> 1x2     7811  1x1 -> 1x2
#  standing up 7612/7613: 7612 had no eye and a dark cheek; 7613 a 1x1 eye beside a dark block
PIXEL_EDIT2={7806:[(8,13,9)],7812:[(8,13,9)],7813:[(8,13,9)],7807:[(9,14,11),(10,14,11)],7811:[(10,15,11)],
             7612:[(11,8,11),(11,9,9),(11,11,9),(11,12,8),(12,7,9),(12,8,11),(12,9,8),(12,10,11),(12,11,9),(12,13,1),(12,14,11),
                   (13,7,9),(13,8,10),(13,9,8),(13,11,9),(13,12,11)],   # the whole idle eye+nose block (user review)
             7613:[(10,10,11),(10,7,9)]}
# Third review round.
#  7814/7815 (crouched, pistol lowered after firing): the idle eye + nose block lined up on the
#    nose outline; 7815's headband row was broken by dark pixels -> one white run.
#  7820, 7824-7828 (kneeling with the heavy machine gun at the shoulder, before/after the burst):
#    the nose outline cut into the gun and the stock had no outline over the chin, so the gun read
#    as BEHIND the face. Redrawn after Tarma's frames (see gun_front).
def gun_front(sh):
    # Tarma's 8409/8413-8417 as the reference (same columns as Marco): the black rear outline of
    # the gun closes over the mouth on the nose row, and the stock's outline runs back across
    # the chin row to the shoulder, with the supporting hand under it. Marco keeps his 1x2 eye
    # and one skin pixel in front of it.
    e=[(10,15,7),(10,16,7),(10,17,7),(11,9,7),(11,10,7),(11,11,7),(11,12,7),(11,13,7),(11,14,10),(11,15,10),(11,16,7)]
    return [(r,c+sh,v) for r,c,v in e]
# Crouch walk with the heavy machine gun 8029-8031: neck and hand were drawn over the gun's rear, so
# the face read as in FRONT of the gun. After Tarma's 8618-8620 (same columns): the gun's top strip
# starts right under the chin and its body closes behind the neck.
WALK_GUN={8029:[(14,13,7),(14,14,7),(14,15,2),(14,16,2),(14,17,2),(14,18,2),(15,14,7),(15,15,3),(15,16,3),(16,15,3),(16,16,3)],
          8030:[(14,14,7),(14,15,2),(14,16,2),(14,17,2),(14,18,2),(15,15,3),(15,16,3),(16,15,3),(16,16,3),(16,17,3)],
          8031:[(14,13,7),(14,14,7),(14,15,2),(14,16,2),(14,17,2),(14,18,7),(15,14,7),(15,15,3),(15,16,3),(16,15,3),(16,16,3)]}
# Kneeling heavy machine gun aimed up / level, 7829-7838 (user review: the gun looks behind the face).
# Tarma's frame with the same index + 589 has the same origin and pose. Taken from it: every gun
# pixel (outline 7, body 2/3) that lies over Marco's skin in the 5 rows from his eye row down,
# 5 columns behind to 7 columns in front of the eye. Marco's eye pixel pair and the skin in front
# of it on the eye row stay, so his 1x2 eye keeps showing.
TARMA_GUN=list(range(7829,7839));TARMA_OFFSET=589
def eye_anchor(a):
    for y in range(1,a.shape[0]-1):
        for x in range(1,a.shape[1]-4):
            if a[y,x]==11 and a[y,x+1]==9 and a[y,x+2]==8 and a[y,x+3]==11 and (a[y-1,max(0,x-4):x+2]==1).sum()>=2:return y,x
    return None
PIXEL_EDIT3={7814:[(8,10,11),(8,11,11),(8,12,9),(8,14,9),(8,15,8),(9,10,9),(9,11,11),(9,13,11)],
             7815:[(7,13,1),(7,15,1),(7,16,11),(8,13,9),(9,12,11),(9,14,11),(10,11,9),(10,12,10),(10,13,8)],
             **{m:gun_front(0) for m in (7820,7824,7825,7826,7827)},7828:gun_front(-1)}
HAIR_JOIN={7884:[(3,12,14),(3,13,14),(3,14,14)],7888:[(3,12,14),(3,13,14),(3,14,14)]}
HEAD_FROM={7885:(7884,-1),7886:(7884,-2),7887:(7884,-1)}
# Crouched heavy-machine-gun sequence 8021-8025: frame 8024 alone came out with a ducked,
# dark head (eyes merged into one dark row) between two upright ones, so the face jumps
# for one frame. Tarma's 8610-8614 keep one head. 8024 takes the head of 8023 in place;
# its own gun (one row higher in this frame) stays in front of the chin.
# map -> (donor, [(row0,row1,col0,col1) half-open boxes copied 1:1])
HEAD23=[(6,8,12,21),(8,11,11,21),(11,13,12,21),(13,14,12,15)]     # head of 8023, gun row kept
GUN24=[(13,18,12,21)]                                               # 8024: receiver in front of chin and neck
# Applied in this order (user review of 8021-8028):
#  8024        head of 8023 (the frame's ducked dark head made the face jump)
#  8021-8023   the gun in front of the face like 8024: its receiver block over chin and neck
#  8025        head of 8023, then the gun block of 8024 like 8021-8023
#  8026-8028   head and headband of 8023 instead of the ducked squint head; gun row kept
BOX_FROM=[(8024,8023,HEAD23),(8021,8024,GUN24),(8022,8024,GUN24),(8023,8024,GUN24),
          (8025,8023,[(6,8,12,21),(8,11,11,21),(11,13,12,21),(13,14,12,21)]),
          (8025,8024,GUN24),   # and the gun in front of the face like the other seven
          (8026,8023,HEAD23),(8027,8023,HEAD23),(8028,8023,HEAD23)]
PIXEL_EDIT={7920:[(8,13,9),(10,19,0),(12,22,6),(12,23,6),(12,24,6),(12,25,6),(12,26,6),(12,27,7),
                  (13,23,7),(13,24,7),(13,25,7),(13,26,7),(13,27,7)]}
def unpack(raw,w,h):
    a=np.frombuffer(raw,dtype=np.uint8).reshape(-1,8,4);b=np.empty((len(a),8,8),np.uint8);b[:,:,::2]=a&15;b[:,:,1::2]=a>>4
    return b.reshape(h//8,w//8,8,8).transpose(0,2,1,3).reshape(h,w).copy()
def pack(a):
    h,w=a.shape;b=a.reshape(h//8,8,w//8,8).transpose(0,2,1,3).reshape(-1,8,8);return (b[:,:,::2]|b[:,:,1::2]<<4).astype(np.uint8).tobytes()
hexrows=lambda a:[''.join('%x'%v for v in r) for r in a]
unhex=lambda rows:np.array([[int(c,16) for c in r] for r in rows],np.uint8)
def main():
    check='--check' in sys.argv
    s=(G/'scaled-drawing-data.c').read_text();pixels=bytearray((G/'native-scaled-objects.4bpp').read_bytes())
    def table(name):return [list(map(int,v.split(','))) for v in re.findall(r'\{([^{}]+)\}',re.search(name+r'\[\]=\{(.*?)\};',s,re.S)[1])]
    M=table('native_scaled_maps');D=table('native_dense_maps');C=table('native_scaled_chunks')
    def picture(first,count,origin):
        H,W=SIZE;ox,oy=origin;img=np.zeros(SIZE,np.uint8);cover={};dec={}
        for ci in range(first,first+count):
            off,x,y,w,h,_,_=C[ci];p=unpack(bytes(pixels[off:off+w*h//2]),w,h);dec[ci]=p
            for yy in range(h):
                for xx in range(w):
                    tx,ty=x-ox+xx,y-oy+yy
                    if p[yy,xx]:
                        assert 0<=tx<W and 0<=ty<H,('picture box too small',ci)
                        assert img[ty,tx] in (0,p[yy,xx]);img[ty,tx]=p[yy,xx]
                    if 0<=tx<W and 0<=ty<H:cover.setdefault((tx,ty),[]).append((ci,xx,yy))
        return img,cover,dec
    def variants(m):
        out=[('full',)+tuple(M[m*9])]
        # variants 1..8 are the 1-2 pixel wide extreme-shrink slivers (never used for the
        # players at normal size); they are left as they are.
        if D[m][1]:out.append(('dense',)+tuple(D[m]))
        return out
    now={m:picture(*variants(m)[0][1:],ORIGIN[m])[0] for m in ORIGIN}
    prev={a['target']['map_id']:a for a in json.loads((OUT/'patch.json').read_text())['assets']} if (OUT/'patch.json').exists() else {}
    def fit(rows):
        x=unhex(rows);y=np.zeros(SIZE,np.uint8);y[:x.shape[0],:x.shape[1]]=x;return y
    for a in prev.values():a['rows']=hexrows(fit(a['rows']));a['original']=hexrows(fit(a['original']))
    base={}
    for m,img in now.items():
        a=prev.get(m)
        if a is not None and np.array_equal(img,unhex(a['rows'])):base[m]=unhex(a['original'])
        else:base[m]=img
    new={m:b.copy() for m,b in base.items()};log={}
    for m,edits in HAIR_JOIN.items():
        for r,c,v in edits:
            assert new[m][r,c]==11,('hair join expects the dark pixel',m,r,c)
            new[m][r,c]=v
        log[m]='hair joined over the headband'
    for m,(d,sh) in HEAD_FROM.items():
        donor=new[d];t=new[m];t[1:8,9+sh:18+sh]=donor[1:8,9:18]
        chin=donor[8,10:17];view=t[8,10+sh:17+sh];np.copyto(view,chin,where=chin!=0)
        log[m]='head of %d moved %d'%(d,sh)
    head23=base[8023].copy()
    for m,d,boxes in BOX_FROM:
        src=head23 if d==8023 else new[d]
        for r0,r1,c0,c1 in boxes:new[m][r0:r1,c0:c1]=src[r0:r1,c0:c1]
        if boxes is GUN24 and new[m][13,21]==7:new[m][13,20]=7      # join the frame's own top outline
        log[m]=('gun block of %d' if boxes is GUN24 else 'head of %d')%d
    for m,d,boxes in BOX_FROM2:
        for r0,r1,c0,c1 in boxes:new[m][r0:r1,c0:c1]=base[d][r0:r1,c0:c1]
        log[m]='head of %d'%d
    for m,edits in PIXEL_EDIT2.items():
        for r,c,v in edits:new[m][r,c]=v
        log[m]='eye 1x2 (review)'
    for m,edits in PIXEL_EDIT3.items():
        for r,c,v in edits:new[m][r,c]=v
        log[m]='gun in front of the face' if m>=7820 else 'idle eye and nose'
    for m in TARMA_GUN:
        t=picture(*variants(m+TARMA_OFFSET)[0][1:],ORIGIN[m])[0];a=new[m];an=eye_anchor(a)
        assert an,('no eye anchor',m)
        R1,E=an
        for y in range(R1,min(R1+5,a.shape[0])):
            for x in range(max(0,E-5),min(a.shape[1],E+8)):
                if (y==R1 and x<E+2) or (y==R1+1 and x in (E,E+1)):continue
                if t[y,x] in (2,3,7) and a[y,x] in (1,8,9,10,11):a[y,x]=t[y,x]
        log[m]='gun over the face from Tarma %d'%(m+TARMA_OFFSET)
    for m,edits in WALK_GUN.items():
        for r,c,v in edits:new[m][r,c]=v
        log[m]='gun in front of the neck (after Tarma)'
    for m,edits in PIXEL_EDIT.items():
        for r,c,v in edits:new[m][r,c]=v
        log[m]='review pixels'
    assets=[dict(id='map:%d'%m,target=dict(kind='scaled-map',map_id=m,origin=list(ORIGIN[m])),rule=log[m],
                 changed_pixels=int((new[m]!=base[m]).sum()),original=hexrows(base[m]),rows=hexrows(new[m])) for m in sorted(new) if not np.array_equal(new[m],base[m])]
    todo=[a for a in assets if not np.array_equal(now[a['target']['map_id']],unhex(a['rows']))]
    print(json.dumps({'maps':[(a['target']['map_id'],a['changed_pixels'],a['rule']) for a in assets],'to_apply':[a['target']['map_id'] for a in todo]}))
    if check:return 0 if not todo else 1
    modified={}
    for a in todo:
        m=a['target']['map_id'];want=unhex(a['rows'])
        for label,first,count in variants(m):
            img,cover,dec=picture(first,count,ORIGIN[m])
            assert np.array_equal(img,now[m]),('variant differs from the full picture',m,label)
            for ty,tx in zip(*np.nonzero(img!=want)):
                opts=cover.get((int(tx),int(ty)));assert opts,('new pixel outside the existing OBJ allocation',m,label,int(ty),int(tx))
                owners=[v for v in opts if dec[v[0]][v[2],v[1]]] or opts[:1]
                for ci,x,y in owners:dec[ci][y,x]=want[ty,tx]
                # a pixel covered by several chunks must not keep a stale colour in the others
                if want[ty,tx]==0:
                    for ci,x,y in opts:dec[ci][y,x]=0
            for ci,p in dec.items():
                data=pack(p);off=C[ci][0]
                if data!=bytes(pixels[off:off+len(data)]):assert modified.get(ci,data)==data;modified[ci]=data
    cache={}
    for ci,data in sorted(modified.items()):
        if data not in cache:cache[data]=len(pixels);pixels.extend(data)
        C[ci][0]=cache[data]
    for a in todo:   # round trip
        m=a['target']['map_id']
        for label,first,count in variants(m):assert np.array_equal(picture(first,count,ORIGIN[m])[0],unhex(a['rows'])),('round trip',m,label)
    if todo:
        (G/'native-scaled-objects.4bpp').write_bytes(pixels)
        newtable='native_scaled_chunks[]={'+',\n'.join('{'+','.join(map(str,r))+'}' for r in C)+'};'
        s2=re.sub(r'native_scaled_chunks\[\]=\{.*?\};',lambda _:newtable,s,flags=re.S);(G/'scaled-drawing-data.c').write_text(s2)
        h=(G/'scaled-drawing-data.h').read_text();(G/'scaled-drawing-data.h').write_text(re.sub(r'NATIVE_SCALED_PIXEL_BYTES=\d+','NATIVE_SCALED_PIXEL_BYTES=%d'%len(pixels),h))
        lp=G/'scaled-drawing-source.json'
        if lp.exists():
            ledger=json.loads(lp.read_text())
            for a in todo:
                try:ledger['map_audit'][a['target']['map_id']]['pixel_sha256']=hashlib.sha256(unhex(a['rows']).tobytes()).hexdigest()
                except (KeyError,IndexError,TypeError):pass
            ledger['object_bytes']=len(pixels);ledger['player_crouch_fix_patch']='art/player-crouch-fixes/patch.json'
            for n in ['native-scaled-objects.4bpp','scaled-drawing-data.c','scaled-drawing-data.h']:
                if n in ledger.get('files',{}):ledger['files'][n]=hashlib.sha256((G/n).read_bytes()).hexdigest()
            lp.write_text(json.dumps(ledger,indent=2))
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'patch.json').write_text(json.dumps(dict(schema='mslug-player-crouch-fixes',version=1,assets=assets),separators=(',',':'))+'\n')
    print(json.dumps({'changed_chunks':len(modified),'appended_bytes':sum(len(d) for d in cache),'pixel_bytes':len(pixels)}))
    return 0
if __name__=='__main__':sys.exit(main())
