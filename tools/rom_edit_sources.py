"""Deterministic pixel bases extracted from the user's original ROMs.

Only addresses, placement rules and conversion algorithms live in this module.
No game pixels are embedded. Source arrays exist only during the local build.
"""
import hashlib,json,struct,zipfile
from pathlib import Path
import numpy as np
from PIL import Image

def sha(a):return hashlib.sha256(np.asarray(a,dtype=np.uint8).tobytes()).hexdigest()
def scale(a):
 h,w=a.shape
 return np.array(Image.fromarray(a).resize((w*3//2,h*3//2),Image.Resampling.NEAREST).resize((w*3//4,h*3//4),Image.Resampling.NEAREST))
def unpack(raw):
 a=np.frombuffer(raw,np.uint8).reshape(-1,8,4);out=np.empty((len(a),8,8),np.uint8);out[:,:,::2]=a&15;out[:,:,1::2]=a>>4;return out
class Sources:
 def __init__(self,root,source,bios):
  self.root=Path(root);self.source=Path(source);self.raw=(self.source/'build/mslug_prom.bin').read_bytes()
  if hashlib.sha256(self.raw).hexdigest()!='1b8eee0891b6f8c837348e60ea2923ae229e0fb603d34ae62ae06131332f0d3d':raise ValueError('Wrong normalized game ROM')
  g=self.root/'generated';self.sprites=unpack((g/'native-sprites.4bpp').read_bytes()).reshape(-1,2,2,8,8).transpose(0,1,3,2,4).reshape(-1,16,16)
  self.fixed=unpack((g/'native-fixed.4bpp').read_bytes());self.map_cache={};self.cache={}
  with zipfile.ZipFile(bios) as z:
   names=[n for n in z.namelist() if Path(n).name=='sfix.sfix']
   if len(names)!=1:raise ValueError('BIOS must contain exactly one sfix.sfix')
   d=np.frombuffer(z.read(names[0]),np.uint8).reshape(-1,32)
  self.bios=np.zeros((len(d),8,8),np.uint8)
  for x,o in ((4,0),(6,8),(0,16),(2,24)):
   self.bios[:,:,x]=d[:,o:o+8]&15;self.bios[:,:,x+1]=d[:,o:o+8]>>4
 def map(self,at):
  if at in self.map_cache:return self.map_cache[at]
  pos=at if at<0x100000 else at-0x100000
  count,attr=self.raw[pos:pos+2];pos+=2;pieces=[]
  for _ in range(count):
   x,y,w,h=struct.unpack_from('>hhBB',self.raw,pos);pos+=6
   ts=struct.unpack_from('>'+str(w*h)+'H',self.raw,pos);pos+=w*h*2;pieces.append((x,y,w,h,ts))
  l=min(p[0] for p in pieces)//4*4;t=min(-p[1] for p in pieces)//4*4
  r=(max(x+16*w for x,y,w,h,ts in pieces)+3)//4*4;b=(max(-y+16*h for x,y,w,h,ts in pieces)+3)//4*4
  out=np.zeros((b-t,r-l),np.uint8)
  for x,y,w,h,ts in pieces:
   for c in range(w):
    for row in range(h):
     a=self.sprites[((attr>>4)<<16)|ts[c*h+row]];dst=out[-y-t+row*16:-y-t+row*16+16,x-l+c*16:x-l+c*16+16];np.copyto(dst,a,where=a!=0)
  self.map_cache[at]=(out,(l,t,r,b));return self.map_cache[at]
 def get(self,spec):
  key=json.dumps(spec,sort_keys=True)
  if key in self.cache:return self.cache[key].copy()
  kind=spec['kind']
  if kind=='map':a=self.map(spec['address'])[0]
  elif kind=='sprite':a=self.sprites[spec['index']]
  elif kind=='fixed':a=self.fixed[spec['index']]
  elif kind=='bios':
   c=spec['code'];a=np.concatenate([self.bios[0x100+c],self.bios[0x200+c]])[1:,:7];a=(a==1).astype(np.uint8)
   if 'keep' in spec:a=a[spec['keep']]
  elif kind=='pose':
   ledger=json.loads((self.root/'generated/player-program-source.json').read_text());a=np.zeros((64,64),np.uint8)
   for drawing,dy in [(spec['lower'],0),(spec['upper'],-spec['offset_y'])]:
    for part in ledger['drawings'][drawing]:
     if part['map']==0xffffffff:continue
     raw,bb=self.map(part['map']);p=scale(raw);x=32+(bb[0]+part['dx'])*3//4;y=56+(bb[1]-part['dy_up']+dy)*3//4
     h,w=p.shape;x0=max(0,x);y0=max(0,y);x1=min(64,x+w);y1=min(64,y+h)
     if x0<x1 and y0<y1:
      src=p[y0-y:y1-y,x0-x:x1-x];np.copyto(a[y0:y1,x0:x1],src,where=src!=0)
  elif kind=='fixed-atlas':
   ts=np.zeros((4096,8,8),np.uint8)
   for i,tile in enumerate(self.fixed):ts[i,:6,:6]=scale(tile)
   a=ts.reshape(64,64,8,8).transpose(0,2,1,3).reshape(512,512)
  else:raise ValueError('Unknown source kind '+kind)
  if spec.get('scale'):a=scale(a)
  if spec.get('mask'):a=(a!=0).astype(np.uint8)
  if 'fit' in spec:
   h,w=spec['fit'];b=np.zeros((h,w),np.uint8);hh=min(h,a.shape[0]);ww=min(w,a.shape[1]);b[:hh,:ww]=a[:hh,:ww];a=b
  a=np.ascontiguousarray(a);self.cache[key]=a;return a.copy()

def apply_patch(base,patch):
 if list(base.shape)!=patch['shape'] or sha(base)!=patch['base_sha256']:raise ValueError('Pixel patch source mismatch')
 out=base.copy();flat=out.reshape(-1);last=-1
 for index,value in patch['changes']:
  if not isinstance(index,int) or index<=last or index>=flat.size or not 0<=value<=15:raise ValueError('Invalid pixel change')
  flat[index]=value;last=index
 if sha(out)!=patch['result_sha256']:raise ValueError('Pixel patch output mismatch')
 return out
