"""Fill existing overlap padding in M6 clouds; full-size composite is unchanged.

No texture allocation or silhouette growth. At fractional OBJ zoom, neighbouring
chunks sample on slightly different phases. Replicate the already-present
neighbour pixels inside their overlapping rectangles to cover a rounding seam.
"""
import hashlib,json,re
from pathlib import Path
R=Path(__file__).resolve().parents[1];G=R/'generated'
def arrays(text,name):
 return [tuple(map(int,x.split(','))) for x in re.findall(r'\{([^{}]+)\}',re.search(name+r'\[[^=]*=\{(.*?)\};',text,re.S)[1])]
def pixels(data,c):
 off,cx,cy,w,h,*_=c
 for y in range(h):
  for x in range(w):
   at=off+((y//8)*(w//8)+x//8)*32+(y&7)*4+(x&7)//2;shift=(x&1)*4
   yield (cx+x,cy+y),at,shift,(data[at]>>shift)&15
def composite(data,chunks):
 out={}
 for c in chunks:
  for xy,at,shift,value in pixels(data,c):
   if value:out.setdefault(xy,value)
 return out
def main():
 source=(G/'scaled-drawing-data.c').read_text();maps=arrays(source,'native_scaled_maps');dense=arrays(source,'native_dense_maps');chunks=arrays(source,'native_scaled_chunks')
 path=G/'native-scaled-objects.4bpp';before=path.read_bytes();after=bytearray(before);changed={};records=[]
 for mapid in (9097,9099):
  first,count=maps[mapid*9];selected=chunks[first:first+count];canvas=composite(before,selected);total=0;addresses=set()
  for c in selected:
   for xy,at,shift,value in pixels(before,c):
    replacement=canvas.get(xy,0)
    if not value and replacement:
     after[at]=(after[at]&~(15<<shift))|(replacement<<shift);addresses.add(at);total+=1
  affected={i for i,c in enumerate(chunks) if any(c[0]<=at<c[0]+c[3]*c[4]//2 for at in addresses)}
  users=[('regular',i//9,i%9) for i,(f,n) in enumerate(maps) if any(f<=j<f+n for j in affected)]
  users += [('dense',i,0) for i,(f,n) in enumerate(dense) if any(f<=j<f+n for j in affected)]
  assert not users or users==[('regular',mapid,0)],(mapid,users)
  assert composite(after,selected)==canvas
  records.append(dict(map=mapid,padding_pixels=total,bytes=len(addresses),full_size_pixel_mismatches=0,other_maps_affected=0))
 report=dict(before_sha256=hashlib.sha256(before).hexdigest(),after_sha256=hashlib.sha256(after).hexdigest(),bytes=len(after),maps=records)
 if before!=after:
  path.write_bytes(after)
  ledger_path=G/'scaled-drawing-source.json';ledger=json.loads(ledger_path.read_text());ledger['files'][path.name]['sha256']=report['after_sha256'];ledger['affine_cloud_padding']=report;ledger_path.write_text(json.dumps(ledger,indent=2)+'\n')
 report_path=G/'affine-cloud-padding.json'
 if before==after and report_path.exists():
  previous=json.loads(report_path.read_text())
  if previous.get('after_sha256')==report['after_sha256']:
   print(json.dumps(dict(already_applied=True,**previous)));return
 report_path.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
if __name__=='__main__':main()
