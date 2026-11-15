"""Split only bridge affine textures horizontally; retain Y sampling origins."""
from pathlib import Path
import sys,re,json
sys.path.insert(0,str(Path(__file__).parent))
from compile_m1_native_maps import body,numbers
R=Path(__file__).resolve().parents[1];G=R/'generated';f=G/'scaled-drawing-data.c';s=f.read_text()
if (G/'bridge-horizontal.inc').exists():raise SystemExit('Already generated; rebuild scaled assets before regenerating')
chunks=[numbers(x)for x in re.findall(r'\{([^{}]+)\}',body(s,'native_scaled_chunks'))]
maps=[numbers(x)for x in re.findall(r'\{([^{}]+)\}',body(s,'native_scaled_maps'))]
maskpath=G/'object-masks.c';ms=maskpath.read_text();masks=[numbers(x)for x in re.findall(r'\{([^{}]+)\}',body(ms,'native_chunk_masks'))]
raw=bytearray((G/'native-scaled-objects.4bpp').read_bytes());old=len(raw);add=[];am=[];indices=[]
for mid in [9153,9154]:
 start,count=maps[mid*9];first=len(chunks)+len(add)
 for i in range(start,start+count):
  off,x,y,w,h,shape,size=chunks[i]
  if w!=64:add.append(chunks[i]);am.append(masks[i]);continue
  for half in range(2):
   b=bytearray()
   for row in range(h//8):b+=raw[off+row*256+half*128:off+row*256+half*128+128]
   dims={(32,8):(1,1),(32,16):(1,2),(32,32):(0,2),(32,64):(2,3)};shape,size=dims[32,h]
   add.append([len(raw),x+half*32,y,32,h,shape,size]);am.append(masks[i]);raw+=b
 indices.append((first,len(chunks)+len(add)-first))
def append_array(text,name,rows):
 m=re.search(r'(\b'+name+r'\[[^;=]*\]\s*=\s*\{)(.*?)(\};)',text,re.S)
 return text[:m.start(2)]+m[2].rstrip().rstrip(',')+','+','.join('{'+','.join(map(str,r))+'}'for r in rows)+text[m.end(2):]
f.write_text(append_array(s,'native_scaled_chunks',add));maskpath.write_text(append_array(ms,'native_chunk_masks',am));(G/'native-scaled-objects.4bpp').write_bytes(raw)
for n in ['scaled-drawing-data.h','object-masks.h']:
 p=G/n;t=p.read_text();t=t.replace('NATIVE_SCALED_CHUNK_COUNT='+str(len(chunks)),'NATIVE_SCALED_CHUNK_COUNT='+str(len(chunks)+len(add))).replace('NATIVE_SCALED_PIXEL_BYTES='+str(old),'NATIVE_SCALED_PIXEL_BYTES='+str(len(raw)))
 # Macros, if emitted instead of enum constants.
 t=re.sub(r'(NATIVE_SCALED_CHUNK_COUNT\s+)'+str(len(chunks))+r'\b',lambda m:m[1]+str(len(chunks)+len(add)),t);t=re.sub(r'(NATIVE_SCALED_PIXEL_BYTES\s+)'+str(old)+r'\b',lambda m:m[1]+str(len(raw)),t);t=t.replace('native_chunk_masks['+str(len(chunks))+']','native_chunk_masks['+str(len(chunks)+len(add))+']');p.write_text(t)
(G/'bridge-horizontal.inc').write_text('static const struct NativeScaledMap bridge_horizontal_maps[2]={'+','.join('{%d,%d}'%r for r in indices)+'};\n')
print(json.dumps({'added_bytes':len(raw)-old,'chunks':len(add),'maps':indices}))
