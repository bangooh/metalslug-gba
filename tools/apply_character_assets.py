#!/usr/bin/env python3
"""Apply reviewed indexed pixels to exact v159 ROM; update generated source assets.
Usage: python apply_character_assets.py SOURCE_ROOT BASE_ROM PATCH_JSON OUTPUT_ROM
Requires numpy. No game code is recompiled or changed.
"""
import sys,json,re,struct,hashlib,copy
from pathlib import Path
import numpy as np
root,base,patch,out=map(Path,sys.argv[1:]);G=root/'generated';rom=bytearray(base.read_bytes());original_rom=bytes(rom)
assert hashlib.sha256(rom).hexdigest()=='e247b47c9b177fd24790f351756d347074f39ad0a10fe203c3e55faa854c9440','Wrong baseline ROM'
P=json.loads(patch.read_text());BG=root/'art/character-local-fixes/baseline';s=(BG/'scaled-drawing-data.c').read_text()
def table(name):return [list(map(int,v.split(',')))for v in re.findall(r'\{([^{}]+)\}',re.search(name+r'\[\]=\{(.*?)\};',s,re.S)[1])]
M=table('native_scaled_maps');D=table('native_dense_maps');C=table('native_scaled_chunks');oldC=copy.deepcopy(C)
def packtable(rows,fmt):return b''.join(struct.pack(fmt,*r)for r in rows)
def locate(b):
 p=rom.find(b);assert p>=0 and rom.find(b,p+1)<0,'Missing or ambiguous baseline block';return p
pixels=bytearray((BG/'native-scaled-objects.4bpp').read_bytes());oldpixels=bytes(pixels);pixbase=locate(pixels);cp=locate(packtable(C,'<IhhBBBB'));assert locate(packtable(M,'<HH'))>=0;assert locate(packtable(D,'<HH'))>=0
players=bytearray((BG/'player.tiles').read_bytes());pb=locate(players)
def arr(rows):return np.array([[int(c,16)for c in r]for r in rows],np.uint8)
def unpack(raw,w,h):
 a=np.frombuffer(raw,dtype=np.uint8).reshape(-1,8,4);b=np.empty((len(a),8,8),np.uint8);b[:,:,::2]=a&15;b[:,:,1::2]=a>>4;return b.reshape(h//8,w//8,8,8).transpose(0,2,1,3).reshape(h,w).copy()
def pack(a):
 h,w=a.shape;b=a.reshape(h//8,8,w//8,8).transpose(0,2,1,3).reshape(-1,8,8);return (b[:,:,::2]|b[:,:,1::2]<<4).tobytes()
def tiny(a,origin,factor,axis):
 if not factor:return a,origin
 extent=a.shape[axis];left=origin*factor//256;right=-(-(origin+extent)*factor//256);coords=np.arange(left,right)*256//factor-origin;valid=(coords>=0)&(coords<extent);b=np.take(a,np.clip(coords,0,extent-1),axis=axis).copy()
 if axis==0:b[~valid,:]=0
 else:b[:,~valid]=0
 return b,left
modified={};verified=[]
def apply_map(first,count,old,new,origin,label):
 if not count:assert not np.any(new);return
 H,W=old.shape;ox,oy=origin;rebuilt=np.zeros_like(old);decoded={};coverage={}
 for ci in range(first,first+count):
  off,x,y,w,h,shape,size=oldC[ci];p=unpack(oldpixels[off:off+w*h//2],w,h);decoded[ci]=p;dx,dy=x-ox,y-oy
  for yy in range(h):
   for xx in range(w):
    tx,ty=dx+xx,dy+yy
    if 0<=tx<W and 0<=ty<H:
     if p[yy,xx]:assert rebuilt[ty,tx]in(0,p[yy,xx]);rebuilt[ty,tx]=p[yy,xx]
     coverage.setdefault((tx,ty),[]).append((ci,xx,yy))
 assert np.array_equal(old,rebuilt),('Baseline mismatch',label)
 for ty,tx in zip(*np.where(old!=new)):
  opts=coverage.get((tx,ty));assert opts,('New pixel outside existing OBJ allocation',label,int(tx),int(ty))
  owners=[v for v in opts if decoded[v[0]][v[2],v[1]]]
  if not owners:
   owners=sorted(opts,key=lambda v:np.count_nonzero(decoded[v[0]][v[2]//8*8:v[2]//8*8+8,v[1]//8*8:v[1]//8*8+8]),reverse=True)[:1]
  for ci,x,y in owners:decoded[ci][y,x]=new[ty,tx]
 rebuilt=np.zeros_like(new)
 for ci,p in decoded.items():
  off,x,y,w,h,_,_=oldC[ci];data=pack(p)
  if data!=oldpixels[off:off+len(data)]:
   if ci in modified:assert modified[ci]==data
   modified[ci]=data
  dx,dy=x-ox,y-oy
  for yy in range(h):
   for xx in range(w):
    tx,ty=dx+xx,dy+yy
    if 0<=tx<W and 0<=ty<H and p[yy,xx]:rebuilt[ty,tx]=p[yy,xx]
 assert np.array_equal(new,rebuilt),('Roundtrip mismatch',label)
 verified.append(label)
for a in P['assets']:
 t=a['target'];old=arr(a['original']);new=arr(a['rows'])
 if t['kind']=='approved-player':
  start=t['frame']*2048;assert bytes(players[start:start+2048])==pack(old);players[start:start+2048]=pack(new)
 elif t['kind']=='scaled-map':
  mid=t['map_id'];ox,oy=t['origin']
  for vy in range(3):
   for vx in range(3):
    u,lx=tiny(old,ox,vx,1);u,ty=tiny(u,oy,vy,0);v,_=tiny(new,ox,vx,1);v,_=tiny(v,oy,vy,0)
    first,count=M[mid*9+vy*3+vx];apply_map(first,count,u,v,(lx,ty),f'{a["id"]}:{vx},{vy}')
  first,count=D[mid]
  if count:apply_map(first,count,old,new,(ox,oy),a['id']+':dense')
 else:raise ValueError(t['kind'])
# Deduplicate new blocks separately in generated source and exact-ROM append space.
cache={};append_start=len(rom)
while len(rom)%4:rom.append(0)
for ci,data in sorted(modified.items()):
 if data not in cache:
  sourceoff=len(pixels);pixels.extend(data);romoff=len(rom);rom.extend(data);cache[data]=(sourceoff,romoff)
 sourceoff,romoff=cache[data];C[ci][0]=sourceoff
 struct.pack_into('<I',rom,cp+ci*12,romoff-pixbase)
rom[pb:pb+len(players)]=players
assert len(rom)<=32*1024*1024,('GBA ROM limit',len(rom))
# The original ARM code bounds-checks byte offsets relative to pixbase.
# Appended ROM blocks require a ROM-address span, not the compact source blob length.
bound_at=locate(struct.pack('<I',8570304))
assert bound_at==0x31cf8, 'Unexpected native_scaled_pixels bounds literal'
rom_span=len(rom)-pixbase
struct.pack_into('<I',rom,bound_at,rom_span)
allowed=set(range(pb,pb+len(players)))|{cp+i*12+j for i in modified for j in range(4)}|set(range(bound_at,bound_at+4))
assert all(i in allowed for i,(u,v)in enumerate(zip(original_rom,rom))if u!=v)
# Full original pixel blob remains unchanged, so shared blocks in other maps cannot change.
assert rom[pixbase:pixbase+len(oldpixels)]==oldpixels
for ci,data in modified.items():
 offset=struct.unpack_from('<I',rom,cp+ci*12)[0];assert rom[pixbase+offset:pixbase+offset+len(data)]==data
(G/'native-scaled-objects.4bpp').write_bytes(pixels);(G/'player.tiles').write_bytes(players)
newtable='native_scaled_chunks[]={'+',\n'.join('{'+','.join(map(str,r))+'}'for r in C)+'};';s=re.sub(r'native_scaled_chunks\[\]=\{.*?\};',lambda _:newtable,s,flags=re.S);(G/'scaled-drawing-data.c').write_text(s)
h=(G/'scaled-drawing-data.h').read_text();h=re.sub(r'NATIVE_SCALED_PIXEL_BYTES=\d+',f'NATIVE_SCALED_PIXEL_BYTES={len(pixels)}',h);(G/'scaled-drawing-data.h').write_text(h)
ledger=json.loads((G/'scaled-drawing-source.json').read_text())
for a in P['assets']:
 if a['target']['kind']=='scaled-map':ledger['map_audit'][a['target']['map_id']]['pixel_sha256']=hashlib.sha256(arr(a['rows']).tobytes()).hexdigest()
ledger['object_bytes']=len(pixels);ledger['character_local_fix_patch']=str(patch.name);ledger['approved_player_sha256']=hashlib.sha256(players).hexdigest()
for n in ['native-scaled-objects.4bpp','scaled-drawing-data.c','scaled-drawing-data.h']:
 if n in ledger.get('files',{}):ledger['files'][n]=hashlib.sha256((G/n).read_bytes()).hexdigest()
(G/'scaled-drawing-source.json').write_text(json.dumps(ledger,indent=2))
a=json.loads((G/'approved-player-source.json').read_text());a['approved_sha256']=hashlib.sha256(players).hexdigest();(G/'approved-player-source.json').write_text(json.dumps(a,indent=2))
out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(rom)
report={'base_sha256':hashlib.sha256(original_rom).hexdigest(),'rom_sha256':hashlib.sha256(rom).hexdigest(),'rom_bytes':len(rom),'remaining_32MiB':32*1024*1024-len(rom),'assets':len(P['assets']),'map_variants_roundtrip':len(verified),'changed_chunks':len(modified),'appended_bytes':len(rom)-append_start,'function_instructions_unchanged':True,'runtime_bounds_literal_updated':True,'runtime_pixel_span':rom_span,'original_shared_tiles_unchanged':True,'emulator_tested':False,'full_source_rebuild_equivalence':False}
out.with_suffix('.verification.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
