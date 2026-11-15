"""Pack FT_BLAST_LOW's unit-scale pixels into fewer legal OBJ rectangles.

Offline only: preserve indexed pixels, anchors and animation; no new RAM.
Run after scaled maps/masks have been regenerated, once per asset build.
"""
from pathlib import Path
import re,json
from functools import lru_cache
from compile_m1_native_maps import body,numbers
R=Path(__file__).resolve().parents[1]; G=R/'generated'
assert not (G/'blast-packed.inc').exists(), 'Already generated'
def rows(s,n):return [numbers(x) for x in re.findall(r'\{([^{}]+)\}',body(s,n))]
f=G/'scaled-drawing-data.c';s=f.read_text();chunks=rows(s,'native_scaled_chunks');maps=rows(s,'native_scaled_maps')
mf=G/'object-masks.c';ms=mf.read_text();masks=rows(ms,'native_chunk_masks')
ds=(G/'drawing-data.c').read_text();draws=rows(ds,'drawings');parts=rows(ds,'native_draw_parts')
base=int(re.search(r'\{&fort_program,&drawings\[(\d+)\]',ds)[1])
j=json.loads((G/'fort-source.json').read_text());todo=[24];seen=set();ids=set()
while todo:
 i=todo.pop()
 if i in seen:continue
 seen.add(i);c=j['commands'][i]
 if c['op']=='DRAW':
  start,count=draws[base+c['target']][:2]
  ids.update(parts[k][2] for k in range(start,start+count))
 if c['op'] not in ('END','REMOVE'):todo.append(c['next'])
 if c['op'] in ('JUMP','LOOP'):todo.append(c['target'])
raw=bytearray((G/'native-scaled-objects.4bpp').read_bytes());old=len(raw);add=[];am=[];records=[]
dims={(8,8):(0,0),(16,16):(0,1),(32,32):(0,2),(64,64):(0,3),(16,8):(1,0),(32,8):(1,1),(32,16):(1,2),(64,32):(1,3),(8,16):(2,0),(8,32):(2,1),(16,32):(2,2),(32,64):(2,3)}
def decode(cc):
 pixels={}
 for off,x,y,w,h,_,_ in cc:
  for v in range(h):
   for u in range(w):
    b=raw[off+(v//8*(w//8)+u//8)*32+(v%8)*4+(u%8)//2];p=(b>>((u%2)*4))&15
    if p:
     assert (x+u,y+v) not in pixels or pixels[x+u,y+v]==p,'Conflicting overlap'
     pixels[x+u,y+v]=p
 return pixels
for mid in sorted(ids):
 start,count=maps[mid*9];cc=chunks[start:start+count];pix=decode(cc)
 if not pix:continue
 x0=min(x for x,y in pix);y0=min(y for x,y in pix);W=(max(x for x,y in pix)-x0+8)//8;H=(max(y for x,y in pix)-y0+8)//8
 occupied={((x-x0)//8,(y-y0)//8) for x,y in pix}
 @lru_cache(None)
 def pack(x,y,w,h):
  if not any(x<=u<x+w and y<=v<y+h for u,v in occupied):return (0,0,())
  best=(999,999999,())
  # Padding is permitted only within this partition.
  if (w*8,h*8) in dims:best=(1,w*h,((x,y,w,h),))
  for k in range(1,w):
   a=pack(x,y,k,h);b=pack(x+k,y,w-k,h);v=(a[0]+b[0],a[1]+b[1],a[2]+b[2]);best=min(best,v)
  for k in range(1,h):
   a=pack(x,y,w,k);b=pack(x,y+k,w,h-k);v=(a[0]+b[0],a[1]+b[1],a[2]+b[2]);best=min(best,v)
  return best
 n,tiles,rects=pack(0,0,W,H)
 if n>=count:continue
 first=len(chunks)+len(add);new=[]
 for tx,ty,tw,th in rects:
  x=x0+tx*8;y=y0+ty*8;w=tw*8;h=th*8;data=bytearray();mask=[0,0]
  for by in range(th):
   for bx in range(tw):
    for v in range(8):
     for u in range(0,8,2):
      a=pix.get((x+bx*8+u,y+by*8+v),0);b=pix.get((x+bx*8+u+1,y+by*8+v),0);data.append(a|(b<<4));mask[int(by*8+v>=32)]|=(1<<a)|(1<<b)
  shape,size=dims[w,h];row=[len(raw),x,y,w,h,shape,size];add.append(row);new.append(row);am.append([m&~1 for m in mask]);raw+=data
 assert decode(new)==pix
 # Mirroring is an exact coordinate transform of this equality.
 for flip in range(4):
  transform=lambda p:((-p[0]-1 if flip&1 else p[0]),(-p[1]-1 if flip&2 else p[1]))
  assert {transform(p):v for p,v in decode(new).items()}=={transform(p):v for p,v in pix.items()}
 records.append({'map':mid,'first':first,'count':n,'old_count':count,'tiles':tiles,'pixels':len(pix)})
assert any(r['map']==376 for r in records)
assert len(raw)-old<65000, 'ROM budget'
def append(text,name,rs):
 m=re.search(r'(\b'+name+r'\[[^;=]*\]\s*=\s*\{)(.*?)(\};)',text,re.S)
 return text[:m.start(2)]+m[2].rstrip().rstrip(',')+','+','.join('{'+','.join(map(str,r))+'}' for r in rs)+text[m.end(2):]
f.write_text(append(s,'native_scaled_chunks',add));mf.write_text(append(ms,'native_chunk_masks',am));(G/'native-scaled-objects.4bpp').write_bytes(raw)
for name in ('scaled-drawing-data.h','object-masks.h'):
 p=G/name;t=p.read_text().replace('NATIVE_SCALED_CHUNK_COUNT='+str(len(chunks)),'NATIVE_SCALED_CHUNK_COUNT='+str(len(chunks)+len(add))).replace('NATIVE_SCALED_PIXEL_BYTES='+str(old),'NATIVE_SCALED_PIXEL_BYTES='+str(len(raw))).replace('native_chunk_masks['+str(len(chunks))+']','native_chunk_masks['+str(len(chunks)+len(add))+']');p.write_text(t)
(G/'blast-packed.inc').write_text('/* Generated unit-scale FT_BLAST_LOW maps. */\nstatic const struct NativeScaledMap blast_packed_maps[]={'+','.join('{%d,%d}'%(r['first'],r['count']) for r in records)+'};\nstatic const struct NativeScaledMap*blast_packed(unsigned n){switch(n){'+''.join('case %d:return &blast_packed_maps[%d];'%(r['map'],i) for i,r in enumerate(records))+'default:return 0;}}\n')
report={'added_bytes':len(raw)-old,'maps':records,'pixel_equality_all_flips':True}
(G/'blast-packed.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
