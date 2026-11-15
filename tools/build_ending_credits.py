"""Reviewed ending font masters, ink-based spacing and shared glyph OBJ maps.

Custom map IDs 0x80000001..3 are authored identifiers, not Neo Geo addresses.
Existing native drawing/map IDs remain stable; only ending maps are replaced.
"""
from pathlib import Path
import copy,hashlib,json,re,sys
import numpy as np
from PIL import Image,ImageDraw
from import_scaled_drawings import rows as crows,unpack_tiles,scale,pack,tiny_axis,chunks as partition
from apply_hud_font_edit import decode

ROOT=Path(__file__).resolve().parents[1];G=ROOT/'generated';ART=ROOT/'art/ending-credits';V=ROOT/'validation-ending-credits'
def load(p):return json.loads(p.read_text())
def save(p,d):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n')
def values(a):return [''.join(f'{int(c):x}' for c in r) for r in a]
def pixels(a):return np.array([[int(c,16) for c in r] for r in a],np.uint8)
def bounds(a):
    ys,xs=np.nonzero(a);return int(xs.min()),int(ys.min()),int(xs.max()+1),int(ys.max()+1)
def master_id(family,char):return f'ending-{family}:{ord(char):04x}'
def replace_array(s,name,rr):
    m=re.search(r'(const struct \w+ '+name+r'\[\]=\{)(.*?)(\};)',s,re.S)
    assert m,name
    return s[:m.start(2)]+',\n'.join('{'+','.join(map(str,r))+'}' for r in rr)+s[m.end(2):]

def prepare():
    base=load(ART/'base-project.json');upload=load(ART/'uploaded-edits.json');targets=load(ART/'base-targets.json')
    assert upload['base']==base['base'] and not upload['palette_overrides']
    masters={k:copy.deepcopy(a) for k,a in base['assets'].items() if a['target']['kind']=='ending-glyph'}
    edited=[]
    for a in upload['assets']:
        assert a['target']['kind'] in ('map','ending-glyph'),a['id']
        old=base['assets'][a['id']]
        assert a['target']==old['target']
        if a['target']['kind']=='ending-glyph':
            assert a['before_current']==old['rows']
            masters[a['id']]['rows']=masters[a['id']]['after_padded']=copy.deepcopy(a['rows']);edited.append(a['id'])
    # Verify every supplied composed map agrees with the edited masters.
    for a in upload['assets']:
        if a['target']['kind']!='map':continue
        out=np.zeros((len(a['rows']),len(a['rows'][0])),np.uint8)
        for p in targets['endingCards'][a['id']]:
            tile=pixels(masters[p['asset']]['rows']);h,w=tile.shape
            np.copyto(out[p['y']:p['y']+h,p['x']:p['x']+w],tile,where=tile!=0)
        assert values(out)==a['rows'],('Independent map edit requires reconciliation',a['id'])
    # B follows the user's edited P. K/L retain the same stem/gradient style.
    p=masters[master_id('thin','P')]['rows'];h=masters[master_id('thin','H')]['rows']
    thin_b=[p[i] for i in [0,1,2,3,5,6,1,2,3,5,0]]+['011111111000']
    raw=unpack_tiles((G/'native-sprites.4bpp').read_bytes()).reshape(-1,2,2,8,8).transpose(0,1,3,2,4).reshape(-1,16,16)
    k=values(scale(raw[0xccf]));thin_k=[h[y][:4]+k[y][4:] for y in range(12)]
    p=masters[master_id('bold','P')]['rows'];e=masters[master_id('bold','E')]['rows']
    bold_b=p[:6]+['777111177711','676100067611','666666666611','565565565111','111111111110','111111111100']
    bold_l=['fff110000000','fff110000000','fff100000000','fff100000000',p[4][:3]+'100000000',p[5][:3]+'100000000',p[6][:3]+'100000000',p[7][:3]+'100000000']+e[8:]
    additions=[('thin','B',thin_b,0xcc6,'P'),('thin','K',thin_k,0xccf,'H + source K'),('bold','B',bold_b,0xced,'P'),('bold','L',bold_l,0xcf7,'P + E')]
    for family,ch,rr,source,derived in additions:
        aid=master_id(family,ch);assert aid not in masters
        template=copy.deepcopy(masters[master_id(family,'P')]);template.update(id=aid,label=ch,
            target=dict(kind='ending-glyph',family=family,character=ch,source_sprite=source),rows=rr,after_padded=rr,before_current=rr,original=rr,derived_from=derived)
        assert len(rr)==12 and all(len(r)==12 for r in rr)
        masters[aid]=template
    save(ART/'fonts.json',dict(format='metalslug-ending-masters',version=1,masters=masters,uploaded_sha256=hashlib.sha256((ART/'uploaded-edits.json').read_bytes()).hexdigest(),edited=edited))
    return masters

def definitions():
    def line(f,t,x,y,align='left'):return dict(family=f,text=t,x=x,y=y,align=align)
    cards=[]
    originals=[
        (0x2536c0,'SUPER VEHICLE 001 / METAL SLUG',[line('thin','SUPER VEHICLE 001',0,-30,'center'),line('thin','METAL SLUG',0,-12,'center')]),
        (0x253718,'PLANNING / MEEHER / KIRE-NAG',[line('thin','PLANNING',-102,-30),line('bold','MEEHER',0,-30),line('bold','KIRE-NAG',0,-12)]),
        (0x25375e,'DESIGN / AKIO / SUSUMU',[line('thin','DESIGN',-102,-30),line('bold','AKIO',0,-30),line('bold','SUSUMU',0,-12)]),
        (0x253798,'MAX.D / TOMOHIRO',[line('bold','MAX.D',0,-30),line('bold','TOMOHIRO',0,-12)]),
        (0x2537c6,'T.OKUI / KOZO',[line('bold','T.OKUI',0,-30),line('bold','KOZO',0,-12)]),
        (0x2537ee,'PROGRAM / HAMACHAN / ANDY',[line('thin','PROGRAM',-102,-30),line('bold','HAMACHAN',0,-30),line('bold','ANDY',0,-12)]),
        (0x253828,'A.KUROOKA / T.YOKOTA',[line('bold','A.KUROOKA',0,-30),line('bold','T.YOKOTA',0,-12)]),
        (0x253870,'H.YAMADA / PIERRE',[line('bold','H.YAMADA',0,-30),line('bold','PIERRE',0,-12)]),
        (0x2538ac,'SOUND / HIYA! / JIM',[line('thin','SOUND',-102,-30),line('bold','HIYA!',0,-30),line('bold','JIM',0,-12)]),
        (0x2538e0,'NAZCA CORPORATION',[line('thin','NAZCA CORPORATION',0,-24,'center')]),
        (0x25390e,'FIN',[line('thin','FIN',0,-24,'center')]),
    ]
    for src,label,lines in originals:cards.append(dict(source=src,id='fin' if src==0x25390e else f'ending-map:{src:x}',label=label,lines=lines))
    for i,(label,ss) in enumerate([
        ('THANKS / TEAM MUPUNG',[('thin','THANKS'),('bold','TEAM MUPUNG')]),
        ('SPECIAL THANKS / CAMILLA / MARIE STELLA',[('thin','SPECIAL THANKS'),('bold','CAMILLA'),('bold','MARIE STELLA')]),
        ('GBA PORTING / BANGOO',[('thin','GBA PORTING'),('bold','BANGOO')])]):
        height=len(ss)*18-6;top=-18-height//2
        cards.append(dict(source=0x80000001+i,id=f'ending-custom:{i}',label=label,
                          lines=[line(f,t,0,top+j*18,'center') for j,(f,t) in enumerate(ss)],authored=True))
    for c in cards:
        top=min(l['y'] for l in c['lines']);bottom=max(l['y']+12 for l in c['lines'])
        c['bounds']=[-120,top,120,bottom]
    return cards

def layout(card,masters):
    placements=[];audit=[]
    for line in card['lines']:
        words=[]
        for word in line['text'].split(' '):
            chars=[]
            for ch in word:
                aid=master_id(line['family'],ch);a=pixels(masters[aid]['rows']);l,t,r,b=bounds(a)
                chars.append((aid,l,r,r-l))
            words.append(chars)
        width=sum(sum(c[3] for c in word)+len(word)-1 for word in words)+5*(len(words)-1)
        x=line['x']-(width//2 if line['align']=='center' else 0)
        audit.append(dict(**line,width=width,ink_left=x,ink_right=x+width-1))
        for wi,word in enumerate(words):
            for ci,(aid,l,r,w) in enumerate(word):
                placements.append(dict(asset=aid,x=x-l,y=line['y']))
                x+=w+(1 if ci+1<len(word) else 0)
            if wi+1<len(words):x+=5
    l,t,r,b=card['bounds'];canvas=np.zeros((b-t,r-l),np.uint8)
    for p in placements:
        im=pixels(masters[p['asset']]['rows']);x=p['x']-l;y=p['y']-t;h,w=im.shape
        assert x>=0 and y>=0 and x+w<=r-l and y+h<=b-t,(card['label'],p)
        np.copyto(canvas[y:y+h,x:x+w],im,where=im!=0)
    return placements,canvas,audit

def apply(masters,cards):
    s=(G/'scaled-drawing-data.c').read_text();maps=[list(a) for a in crows(s,'native_scaled_maps')];dense=[list(a) for a in crows(s,'native_dense_maps')];chunks=[list(a) for a in crows(s,'native_scaled_chunks')]
    blob=bytearray((G/'native-scaled-objects.4bpp').read_bytes());old_bytes=len(blob);old_chunks=len(chunks)
    report=load(G/'scaled-drawing-source.json');audit={m['source']:m for m in report['map_audit']}
    before={}
    for name,table in [('map',maps),('dense',dense)]:
        for i,(first,count) in enumerate(table):
            h=hashlib.sha256()
            for off,x,y,w,hh,sh,sz in chunks[first:first+count]:h.update(str((x,y,w,hh,sh,sz)).encode());h.update(blob[off:off+w*hh//2])
            before[name,i]=h.hexdigest()
    lookup={bytes(blob[o:o+w*h//2]):o for o,x,y,w,h,*_ in chunks}
    def add(tile,x,y,shape,size):
        raw=pack(tile)
        if raw not in lookup:lookup[raw]=len(blob);blob.extend(raw)
        chunks.append([lookup[raw],x,y,tile.shape[1],tile.shape[0],shape,size])
    glyphs={aid:partition(pixels(a['rows'])) for aid,a in masters.items()}
    for card in cards:
        src=card['source']
        if src not in audit:
            assert card.get('authored')
            mid=len(dense);dense.append([0,0]);maps.extend([[0,0] for _ in range(9)])
            audit[src]=dict(id=mid,source=src,source_kind='authored_credit',source_bounds=[],scaled_bounds=card['bounds'])
            report['map_audit'].append(audit[src])
        m=audit[src];mid=m['id'];card['map_id']=mid;m['scaled_bounds']=card['bounds']
        placements,im,line_audit=layout(card,masters);card['placements']=placements;card['rows']=values(im);card['spacing']=line_audit
        l,t,r,b=card['bounds']
        for vy in range(3):
            for vx in range(3):
                first=len(chunks)
                if not vx and not vy:
                    for p in placements:
                        for x,y,w,h,sh,sz,tile in glyphs[p['asset']]:add(tile,p['x']+x,p['y']+y,sh,sz)
                    want,xx,yy=im,l,t
                else:
                    want,xx=tiny_axis(im,l,vx,1);want,yy=tiny_axis(want,t,vy,0)
                    for x,y,w,h,sh,sz,tile in partition(want):add(tile,xx+x,yy+y,sh,sz)
                maps[mid*9+vy*3+vx]=[first,len(chunks)-first]
                assert np.array_equal(decode(blob,chunks,maps[mid*9+vy*3+vx],xx,yy,want.shape),want)
        dense[mid]=[0,0];m['chunks']=maps[mid*9][1];m['pixel_sha256']=hashlib.sha256(im.tobytes()).hexdigest()
    # Compact referenced blocks without touching their bytes, including cloud padding.
    compact=bytearray();live=[];blocks={}
    for table in (maps,dense):
        for i,(first,count) in enumerate(table):
            start=len(live)
            for off,x,y,w,h,sh,sz in chunks[first:first+count]:
                raw=bytes(blob[off:off+w*h//2])
                if raw not in blocks:blocks[raw]=len(compact);compact.extend(raw)
                live.append([blocks[raw],x,y,w,h,sh,sz])
            table[i]=[start,count]
    assert len(live)<65536
    changed_ids={c['map_id'] for c in cards};preserved=0
    for name,table in [('map',maps),('dense',dense)]:
        for i,(first,count) in enumerate(table):
            if (i//9 if name=='map' else i) in changed_ids:continue
            h=hashlib.sha256()
            for off,x,y,w,hh,sh,sz in live[first:first+count]:h.update(str((x,y,w,hh,sh,sz)).encode());h.update(compact[off:off+w*hh//2])
            assert h.hexdigest()==before[name,i],(name,i);preserved+=1
    for name,rr in [('native_scaled_maps',maps),('native_dense_maps',dense),('native_scaled_chunks',live)]:s=replace_array(s,name,rr)
    (G/'scaled-drawing-data.c').write_text(s);(G/'native-scaled-objects.4bpp').write_bytes(compact)
    h=(G/'scaled-drawing-data.h').read_text()
    for key,n in dict(NATIVE_SCALED_SOURCE_MAP_COUNT=len(dense),NATIVE_SCALED_MAP_COUNT=len(maps),NATIVE_SCALED_CHUNK_COUNT=len(live),NATIVE_SCALED_PIXEL_BYTES=len(compact)).items():h=re.sub(key+r'=\d+',f'{key}={n}',h)
    (G/'scaled-drawing-data.h').write_text(h)
    report.update(maps=len(dense),map_variants=len(maps),chunks=len(live),object_bytes=len(compact))
    for name in report['files']:
        p=G/name;report['files'][name]=dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest())
    save(G/'scaled-drawing-source.json',report)
    from import_object_masks import generate
    generate()
    return dict(sprite_bytes_delta=len(compact)-old_bytes,chunks_delta=len(live)-old_chunks,
                estimated_rom_delta=len(compact)-old_bytes+(len(live)-old_chunks)*16+3*40,
                unrelated_map_references_preserved=preserved)

def register(cards, originals=None):
    # Stable old indices; add three drawing entries to the capsule's contiguous set.
    originals=Path(originals) if originals is not None else ART/'registration-base'
    baseline=(originals/'drawing-data.c').read_text()
    s=baseline;maps=[list(a) for a in crows(s,'native_draw_maps')];parts=[list(a) for a in crows(s,'native_draw_parts')];drawings=[list(a) for a in crows(s,'drawings')]
    new_drawings=[]
    for card in cards:
        if not card.get('authored'):continue
        assert card['map_id']==len(maps)
        maps.append([0,0,0]);new_drawings.append([len(parts),1]);parts.append([0,0,card['map_id'],65535,8])
    cut=1910+77;drawings[cut:cut]=new_drawings
    for name,rr in [('native_draw_maps',maps),('native_draw_parts',parts),('drawings',drawings)]:s=replace_array(s,name,rr)
    def shift(m):
        symbol,first,count=m[1],int(m[2]),int(m[3]);return '{&'+symbol+',&drawings['+str(first+(3 if first>=cut else 0))+'],'+str(count+(3 if symbol=='capsule_program' else 0))+'}'
    s=re.sub(r'\{&(\w+),&drawings\[(\d+)\],(\d+)\}',shift,s)
    s=s.replace('extern const struct AnimationProgram scene_text_program;\n','')
    s=re.sub(r'\{&scene_text_program,&drawings\[\d+\],\d+\},','',s)
    (G/'drawing-data.c').write_text(s)
    h=(originals/'drawing-data.h').read_text()
    h=re.sub(r'NATIVE_DRAW_PART_COUNT=\d+',f'NATIVE_DRAW_PART_COUNT={len(parts)}',h);h=re.sub(r'NATIVE_DRAW_MAP_COUNT=\d+',f'NATIVE_DRAW_MAP_COUNT={len(maps)}',h)
    h=h.replace('NATIVE_DRAWING_SET_COUNT=55','NATIVE_DRAWING_SET_COUNT=54')
    (G/'drawing-data.h').write_text(h)
    s=(originals/'capsule-programs.c').read_text()
    m=re.search(r'(static const struct AnimationCommand commands\[\]=\{)(.*?)(\};)',s,re.S)
    extra=[]
    for i in range(3):
        n=249+i*3;extra.extend([f'{{0u,{n+1},0,AN_WAIT,0,5,0}},',f'{{0u,{n+2},{77+i},AN_DRAW,0,0,0}},','{0u,65535,0,AN_END,0,0,0},'])
    s=s[:m.end(2)]+'\n'+'\n'.join(extra)+s[m.end(2):]
    s=s.replace('commands,0,249,0','commands,0,258,0').replace('capsule_droppers[11]={13,14,15,16,17,18,19,20,21,22,23}', 'capsule_droppers[14]={13,14,15,16,17,18,19,20,21,22,23,249,252,255}')
    (G/'capsule-programs.c').write_text(s)
    (G/'capsule-programs.h').write_text((originals/'capsule-programs.h').read_text().replace('capsule_droppers[11]','capsule_droppers[14]'))

def previews(masters,cards):
    pal=np.array([[int(c[i:i+2],16) for i in (1,3,5)]+[255] for c in next(iter(masters.values()))['palette']],np.uint8);pal[0]=0
    out=V/'preview';out.mkdir(exist_ok=True)
    for c in cards:Image.fromarray(pal[pixels(c['rows'])]).save(out/(c['id'].replace(':','-')+'.png'))
    preview=Image.new('RGB',(720,810),(40,49,59));d=ImageDraw.Draw(preview)
    for i,c in enumerate(cards[-3:]):
        d.text((24,i*270+10),f'ADDED CREDIT {i+1}',fill=(255,190,90))
        im=Image.fromarray(pal[pixels(c['rows'])]).resize((720,len(c['rows'])*3),Image.Resampling.NEAREST)
        preview.paste(im,(0,i*270+65),im)
    preview.save(V/'new-credits-preview.png')
    sheet=Image.new('RGBA',(6*16,4*16))
    for i,(family,ch) in enumerate([('thin','P'),('thin','B'),('thin','K'),('bold','P'),('bold','B'),('bold','L')]):
        im=Image.fromarray(pal[pixels(masters[master_id(family,ch)]['rows'])]);sheet.paste(im,(i*16,0))
    sheet.crop((0,0,96,14)).resize((1152,168),Image.Resampling.NEAREST).save(V/'added-glyphs.png')

def main():
    masters=prepare();cards=definitions();report=apply(masters,cards);register(cards);previews(masters,cards)
    save(ART/'cards.json',dict(version=1,letter_gap=1,word_gap=5,cards=cards))
    report.update(glyphs=len(masters),cards=len(cards),max_base_chunks=max(len(c['placements']) for c in cards),extra_card_order=[c['label'] for c in cards[-3:]])
    save(V/'asset-validation.json',report);print(json.dumps(report))
if __name__=='__main__':main()
