"""Fit reviewed mission glyphs to an optically even gap (user review 2026-11-12).

Ink bounds include the dark border. Two flat sides (stems) keep one transparent
pixel between borders; when either facing side is round or slanted (O, S, C,
A, digits) the borders touch, so the round letters do not look set apart."""
from pathlib import Path
import sys,json,numpy as np
R=Path(__file__).resolve().parents[1];G=R/'generated'
from import_scaled_drawings import rows
from apply_hud_font_edit import decode
ledger=json.loads((G/'scaled-drawing-source.json').read_text());audit={m['source']:m for m in ledger['map_audit']};s=(G/'scaled-drawing-data.c').read_text();maps=rows(s,'native_scaled_maps');cs=rows(s,'native_scaled_chunks');blob=(G/'native-scaled-objects.4bpp').read_bytes()
def generate():
 def get(src):
  m=audit[src];l,t,r,b=m['scaled_bounds'];return decode(blob,cs,maps[m['id']*9],l,t,(b-t,r-l))
 outdir=R/'validation-hud-spacing';outdir.mkdir(exist_ok=True)
 # Original event layout is retained; only final letter x targets are adjusted.
 source=(G/'mission-result-data.c').read_text();layouts=[(0,14),(14,18),(32,17),(49,21)];letters=rows(source,'mission_banner_layout');j=json.load(open(G/'mission-result-source.json'))
 glyphsrc=[j['drawings'][i][0]['map'] for i in range(3,17)]
 nums=[j['drawings'][i][0]['map'] for i in range(17,23)]
 targets=[l[0]for l in letters];report=[]
 for lid,(first,count)in enumerate(layouts):
  for y in sorted(set(l[1]for l in letters[first:first+count])):
   line=[]
   for idx in range(first,first+count):
    x,yy,delay,glyph,blink,number=letters[idx]
    if yy!=y or number:continue
    src=nums[0]if number else glyphsrc[glyph];p=get(src);xx=np.nonzero(p)[1];origin=audit[src]['scaled_bounds'][0];left=origin+int(xx.min());right=origin+int(xx.max())
    line.append(dict(idx=idx,src=src,x=x,left=left,right=right,p=p))
   for a in line:
    ys,xs=np.nonzero(a['p']);lo=[int(xs[ys==y].min()) for y in np.unique(ys)];hi=[int(xs[ys==y].max()) for y in np.unique(ys)]
    a['flat_l']=np.mean(np.array(lo)<=min(lo)+0)>=0.6;a['flat_r']=np.mean(np.array(hi)>=max(hi)-0)>=0.6
   line.sort(key=lambda a:a['x']);start=min(a['x']*3//4+a['left']for a in line);end=max(a['x']*3//4+a['right']for a in line)
   gaps=[1 if (line[k-1]['flat_r'] and line[k]['flat_l']) else 0 for k in range(1,len(line))]
   width=sum(a['right']-a['left']+1 for a in line)+sum(gaps)
   cursor=(start+end+1-width)//2;oldgaps=[]
   for k,a in enumerate(line):
    if k:oldgaps.append(a['x']*3//4+a['left']-(line[k-1]['x']*3//4+line[k-1]['right'])-1)
    desired=cursor-a['left'];targets[a['idx']]=(desired*4+2)//3
    assert targets[a['idx']]*3//4+a['left']==cursor
    cursor+=a['right']-a['left']+1+(gaps[k] if k<len(gaps) else 0)
   report.append(dict(layout=lid,y=y,old_gaps=oldgaps,new_gaps=gaps,width=width,source_x=[a['x']for a in line],new_x=[targets[a['idx']]for a in line]))
 (G/'mission-banner-spacing.inc').write_text('/* Native glyph ink bounds: one transparent pixel between letters.\n * Original source layouts, timing and vertical positions remain in mission-result-data.c. */\n'+','.join(map(str,targets))+'\n')
 (outdir/'spacing.json').write_text(json.dumps(report,indent=2)+'\n')

if __name__=="__main__":generate()
