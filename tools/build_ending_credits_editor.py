"""Refresh the offline font editor from the applied shared ending masters."""
from pathlib import Path
from collections import Counter
import argparse, copy, hashlib, json, math, re, shutil
import numpy as np
from PIL import Image, ImageDraw

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/ending-credits'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,default=ROOT/'dist/font-editor')
OUT=parser.parse_args().output.resolve();OUT.mkdir(parents=True,exist_ok=True)
html=(ART/'base-editor.html').read_text()
match=re.search(r'(<script id="dataset" type="application/json">)(.*?)(</script>)',html,re.S)
data=json.loads(match[2])
fonts=json.loads((ART/'fonts.json').read_text())['masters']
cards=json.loads((ART/'cards.json').read_text())['cards']
data['base']='MetalSlug-v171-ending-credits-spacing'
data['source']='Applied ending masters, custom credits and additional mission glyphs'
data['object_atlas_sha256']=hashlib.sha256((ROOT/'generated/native-scaled-objects.4bpp').read_bytes()).hexdigest()
data['editorNotes']={'rom_applied':'All displayed glyphs match the current ROM.',
 'spacing':'One transparent pixel between ink bounds, five between words; columns and centers retained.'}
for a in json.loads((ART/'additional-mission-fonts.json').read_text())['assets']:
    data['assets'][a['id']]=copy.deepcopy(a)
    for key in ('original','before_current','after_padded'):data['assets'][a['id']][key]=copy.deepcopy(a['rows'])
old_ids=set(data['endingCards'])
data['glyphs']=[g for g in data['glyphs'] if g.get('asset') not in old_ids and not g['id'].startswith(('ending-thin:','ending-bold:'))]
for k in old_ids:del data['assets'][k]
used=Counter(p['asset'] for c in cards for p in c['placements'])
descriptors=[]
for aid,a in sorted(fonts.items()):
    a=copy.deepcopy(a)
    for key in ('original','before_current','after_padded'):a[key]=copy.deepcopy(a['rows'])
    data['assets'][aid]=a
    xs=[x for r in a['rows'] for x,v in enumerate(r) if v!='0']
    descriptors.append(dict(id=aid,asset=aid,label=a['label'],character=a['target']['character'],
        group=a['group'],width=len(a['rows'][0]),height=len(a['rows']),palette=a['palette'],
        advance=max(xs)-min(xs)+2,previewOffsetX=-min(xs),uses=used[aid]))
pal=next(iter(fonts.values()))['palette']
data['endingCards']={};data['endingLayout']={}
card_desc=[]
for c in cards:
    aid=c['id'];l,t,r,b=c['bounds']
    data['assets'][aid]=dict(id=aid,label=c['label'],group='엔딩 · 전체 문구',
        target=dict(kind='map',source=c['source'],map_id=c['map_id'],origin=[l,t]),
        palette=pal,rows=c['rows'],after_padded=c['rows'],before_current=c['rows'],original=c['rows'])
    data['endingCards'][aid]=[dict(asset=p['asset'],x=p['x']-l,y=p['y']-t)for p in c['placements']]
    data['endingLayout'][aid]=dict(bounds=c['bounds'],lines=c['lines'])
    card_desc.append(dict(id=aid,asset=aid,label=c['label'],group='엔딩 · 전체 문구 미리보기',
                         width=r-l,height=b-t,palette=pal,readOnly=True))
data['glyphs']+=descriptors+card_desc
html=html[:match.start(2)]+json.dumps(data,ensure_ascii=False,separators=(',',':')).replace('</',r'<\/')+html[match.end(2):]
start=html.index('// Preserve the cartridge map baseline')
end=html.index('\nconst $=',start)
js=r'''
function glyphInk(rows) {
  let left=rows[0].length,right=-1;
  for(const row of rows)for(let x=0;x<row.length;x++)if(row[x]!=='0'){left=Math.min(left,x);right=Math.max(right,x)}
  return right<0?{left:0,width:1}:{left,width:right-left+1};
}
function endingPlacement(id,as=assets) {
  const card=DATA.endingLayout[id],out=[];
  for(const line of card.lines) {
    const words=line.text.split(' ').map(word=>[...word].map(ch=>{
      const asset=`ending-${line.family}:${ch.charCodeAt(0).toString(16).padStart(4,'0')}`;
      return {asset,...glyphInk(assetRows(as[asset]))};
    }));
    const width=words.reduce((n,w)=>n+w.reduce((v,g)=>v+g.width,0)+w.length-1,0)+5*(words.length-1);
    let x=line.x-(line.align==='center'?Math.floor(width/2):0);
    for(let wi=0;wi<words.length;wi++){
      const word=words[wi];
      for(let i=0;i<word.length;i++){
        const g=word[i];out.push({asset:g.asset,x:x-g.left-card.bounds[0],y:line.y-card.bounds[1]});
        x+=g.width+(i<word.length-1?1:0);
      }
      if(wi<words.length-1)x+=5;
    }
  }
  return out;
}
function rebuildEnding(as=assets) {
  for(const id of Object.keys(DATA.endingLayout)){
    const original=DATA.assets[id].rows,dst=Array.from({length:original.length},()=>Array(original[0].length).fill('0'));
    for(const p of endingPlacement(id,as)){
      const src=assetRows(as[p.asset]);
      for(let y=0;y<src.length;y++)for(let x=0;x<src[0].length;x++)if(src[y][x]!=='0'){
        if(!dst[p.y+y]||p.x+x<0||p.x+x>=dst[0].length)throw Error('문구가 화면 폭을 넘었습니다.');
        dst[p.y+y][p.x+x]=src[y][x];
      }
    }
    as[id].rows=dst.map(r=>r.join(''));as[id].after_padded=clone(as[id].rows);
  }
}
'''
html=html[:start]+js+html[end:]
html=html.replace("function renderPhrase(){const gs=groupGlyphs()", "function renderPhrase(){for(const g of descriptors.filter(g=>g.asset&&assets[g.asset].target.kind==='ending-glyph')){const b=glyphInk(getRows(g));g.advance=b.width+1;g.previewOffsetX=-b.left}const gs=groupGlyphs()")
html=html.replace('F 적용 완료 · 엔딩 글자는 한 글자당 하나로 통합했습니다. 엔딩 정리본은 편집용이며 아직 ROM에는 적용하지 않았습니다.',
 'ROM 적용 완료 · 엔딩 공통 글자와 추가 크레딧 3장. 글자 폭이 바뀌면 자간과 가운데 정렬이 자동 조정됩니다.')
html=html.replace('baseline,sheetLayout};','baseline,sheetLayout,endingPlacement,rebuildEnding};')
(OUT/'MetalSlug-Final-Ending-Font-Editor.html').write_text(html)
project=dict(format='metalslug-font-project',version=1,base=data['base'],tiles=data['tiles'],assets=data['assets'],palette_overrides={})
(OUT/'MetalSlug-unified-ending-font-project.json').write_text(json.dumps(project,ensure_ascii=False,indent=1)+'\n')
mission=[g for g in data['glyphs'] if g['group'].startswith('FINAL')]
(OUT/'font-targets.json').write_text(json.dumps(dict(base=data['base'],glyphs=mission+descriptors+card_desc,endingCards=data['endingCards'],endingLayout=data['endingLayout']),ensure_ascii=False,indent=2)+'\n')
shutil.rmtree(OUT/'png',ignore_errors=True)
groups=[('01-final-mission-complete',mission),('02-ending-thin',[g for g in descriptors if 'thin:' in g['id']]),
 ('03-ending-bold',[g for g in descriptors if 'bold:' in g['id']]),('04-ending-cards',card_desc)]
preview=Image.new('RGB',(960,1200),(23,31,42));draw=ImageDraw.Draw(preview);py=20
for folder,gs in groups:
    dest=OUT/'png'/folder;dest.mkdir(parents=True)
    cw=max(g['width'] for g in gs)+2;ch=max(g['height'] for g in gs)+2;cols=min(32,max(1,math.ceil(math.sqrt(len(gs)))))
    sheet=Image.new('RGBA',(cols*cw,math.ceil(len(gs)/cols)*ch));layout=[]
    for i,g in enumerate(gs):
        rows=data['assets'][g['asset']]['rows'];pixels=np.array([[int(c,16)for c in r]for r in rows],np.uint8)
        palette=np.array([[int(c[k:k+2],16)for k in (1,3,5)]+[255]for c in g['palette']],np.uint8);palette[0]=0
        im=Image.fromarray(palette[pixels]);filename=f'{i+1:02d}-'+re.sub(r'[^A-Za-z0-9_-]','_',g['id'])+'.png';im.save(dest/filename)
        x=i%cols*cw;y=i//cols*ch;sheet.paste(im,(x,y));layout.append(dict(id=g['id'],file=filename,x=x,y=y,width=g['width'],height=g['height']))
    sheet.save(dest/'font-sheet.png')
    (dest/'font-sheet-layout.json').write_text(json.dumps(dict(base=data['base'],group=gs[0]['group'],width=sheet.width,height=sheet.height,glyphs=layout),ensure_ascii=False,indent=2)+'\n')
    if folder.startswith('04'):continue
    draw.text((24,py),folder,fill=(255,190,90));py+=30
    im=sheet.resize((sheet.width*3,sheet.height*3),Image.Resampling.NEAREST);preview.paste(im,(24,py),im);py+=im.height+30
preview.crop((0,0,960,py)).save(OUT/'font-preview.png')
(OUT/'README-ko.txt').write_text('''MetalSlug 엔딩·미션 폰트 편집기
HTML을 Chrome 또는 Edge에서 여세요. 설치·인터넷이 필요 없습니다.
동봉 프로젝트와 PNG는 이번 ROM에 적용한 상태입니다.
얇은 글자/굵은 글자에서 같은 문자는 하나만 편집하며 모든 크레딧에 반영됩니다.
얇은 B·K, 굵은 B·L을 추가했습니다. B는 사용자가 수정한 P를 바탕으로 만들었습니다.
엔딩 자간은 그림자 포함 실제 폭 사이 1px, 단어 사이 5px입니다.
직책과 이름의 열을 유지하고 제목·추가 크레딧은 가운데 정렬합니다.
전체 문구 그룹은 자동 미리보기입니다. 글자를 고치면 폭에 맞춰 재정렬합니다.
수정 후 '수정 JSON'을 저장해 보내주세요. 원본 복원은 이번 적용 상태로 돌아갑니다.
png 폴더에는 개별 PNG·시트·시트 좌표 JSON이 있습니다.
추가 크레딧 순서: NAZCA CORPORATION → THANKS / TEAM MUPUNG →
SPECIAL THANKS / CAMILLA / MARIE STELLA → GBA PORTING / BANGOO → FIN.
추가 업로드의 미션 글자 M, I, 1, 3, 4, N, A도 적용했습니다.
''')
print(json.dumps(dict(masters=len(descriptors),cards=len(card_desc),pngs=sum(len(gs) for _,gs in groups))))
