"""Update only NAZCA map5505 packed chunks and masks; retain all other assets."""
from pathlib import Path
import json,re,hashlib
import numpy as np
from import_scaled_drawings import rows,tiny_axis,pack,chunks as partition
R=Path(__file__).resolve().parents[1];G=R/'generated'
if json.loads((G/'scaled-drawing-source.json').read_text()).get('presentation_refined',{}).get('map')==5505:
 print('Refined NAZCA already applied');raise SystemExit(0)
s=(G/'scaled-drawing-data.c').read_text();maps=[list(x)for x in rows(s,'native_scaled_maps')];dense=[list(x)for x in rows(s,'native_dense_maps')];chunks=[list(x)for x in rows(s,'native_scaled_chunks')]
a=np.load(R/'art/presentation-refined/nazca-5505.npy');assert a.shape==(72,248) and not a[:,240:].any();a=a[:,:240]
blob=bytearray((G/'native-scaled-objects.4bpp').read_bytes());oldlen=len(blob);changes=0
for vy in range(3):
 for vx in range(3):
  im,lx=tiny_axis(a,-120,vx,1);im,ty=tiny_axis(im,-36,vy,0)
  start=len(chunks)
  for x,y,w,h,shape,size,tile in partition(im):
   raw=pack(tile);off=len(blob);blob.extend(raw)
   chunks.append([off,lx+x,ty+y,w,h,shape,size]);changes+=1
  maps[5505*9+vy*3+vx]=[start,len(chunks)-start]
dense[5505]=[0,0]
for name,vals in [('native_scaled_maps',maps),('native_dense_maps',dense)]:
 m=re.search(r'(const struct NativeScaledMap '+name+r'\[\]=\{)(.*?)(\};)',s,re.S)
 s=s[:m.start(2)]+',\n'.join('{'+','.join(map(str,c))+'}'for c in vals)+s[m.end(2):]
body=re.search(r'(const struct NativeScaledChunk native_scaled_chunks\[\]=\{)(.*?)(\};)',s,re.S)
s=s[:body.start(2)]+',\n'.join('{'+','.join(map(str,c))+'}'for c in chunks)+s[body.end(2):]
(G/'scaled-drawing-data.c').write_text(s);(G/'native-scaled-objects.4bpp').write_bytes(blob)
p=G/'scaled-drawing-data.h';p.write_text(re.sub(r'NATIVE_SCALED_PIXEL_BYTES=\d+',f'NATIVE_SCALED_PIXEL_BYTES={len(blob)}',re.sub(r'NATIVE_SCALED_CHUNK_COUNT=\d+',f'NATIVE_SCALED_CHUNK_COUNT={len(chunks)}',p.read_text())))
p=G/'scaled-drawing-source.json';j=json.loads(p.read_text());j['object_bytes']=len(blob);j['map_audit'][5505]['pixel_sha256']=hashlib.sha256(a.tobytes()).hexdigest();j['presentation_refined']={'map':5505,'private_append_bytes':len(blob)-oldlen}
for n in j['files']:
 if (G/n).is_file():j['files'][n]=hashlib.sha256((G/n).read_bytes()).hexdigest()
p.write_text(json.dumps(j,indent=2)+'\n')
from import_object_masks import generate
generate();print('NAZCA chunks changed',changes,'appended bytes',len(blob)-oldlen)
