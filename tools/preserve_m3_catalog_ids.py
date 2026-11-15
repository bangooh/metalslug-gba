"""Keep animation158 logical tile IDs stable when rebuilding M3 patches.
Run after build_campaign_cartridge + build_m1_4bpp, before packing.
"""
from pathlib import Path
import re,numpy as np,json,argparse
r=Path(__file__).resolve().parents[1];d=r/'generated/m3-cartridge';parser=argparse.ArgumentParser();parser.add_argument('--approved-dir',type=Path,required=True);a=parser.parse_args().approved_dir
old=np.fromfile(a/'tiles.bin','u1').reshape(-1,64);new=np.fromfile(d/'tiles.bin','u1').reshape(-1,64);tiles=list(old);lookup={t.tobytes():i for i,t in enumerate(old)};remap=[];added=[]
for i,t in enumerate(new):
 k=t.tobytes()
 if k not in lookup:lookup[k]=len(tiles);tiles.append(t);added.append(i)
 remap.append(lookup[k])
remap=np.array(remap,dtype='<u2');raw=list(np.fromfile(a/'tiles4.bin','u1').reshape(-1,32));rawindex={t.tobytes():i for i,t in enumerate(raw)};nraw=np.fromfile(d/'tiles4.bin','u1').reshape(-1,32);nids=np.fromfile(d/'ids4.bin','<u2');nattrs=np.fromfile(d/'attrs4.bin','<u2');ids=list(np.fromfile(a/'ids4.bin','<u2'));attrs=list(np.fromfile(a/'attrs4.bin','<u2'));opacity=list(np.fromfile(a/'opacity.bin','u1').reshape(-1,8));nopacity=np.fromfile(d/'opacity.bin','u1').reshape(-1,8)
for i in added:
 t=nraw[nids[i]];k=t.tobytes()
 if k not in rawindex:rawindex[k]=len(raw);raw.append(t)
 ids.append(rawindex[k]);attrs.append(nattrs[i]);opacity.append(nopacity[i])
np.array(tiles,'u1').tofile(d/'tiles.bin');np.array(raw,'u1').tofile(d/'tiles4.bin');np.array(ids,'<u2').tofile(d/'ids4.bin');np.array(attrs,'<u2').tofile(d/'attrs4.bin');np.array(opacity,'u1').tofile(d/'opacity.bin')
remap[np.fromfile(d/'maps.bin','<u2')].tofile(d/'maps.bin')
# Operations are indexed by patchmaps, so only their tile references change.
f=r/'generated/m3-assets.c';s=f.read_text();m=re.search(r'm3_ops\[\]=\{(.*?)\};',s);ops=[list(map(int,x.split(','))) for x in re.findall(r'\{([^{}]+)\}',m[1])];trans=np.fromfile(d/'transitions.bin','<u2').reshape(-1,2);trans=remap[trans]
for off,n,repl in ops:
 if n:trans[off:off+n]=trans[off:off+n][np.argsort(trans[off:off+n,0])]
for op in ops:
 if op[2]!=65535:op[2]=int(remap[op[2]])
s=s[:m.start(1)]+','.join('{'+','.join(map(str,x))+'}' for x in ops)+s[m.end(1):];f.write_text(s);trans.tofile(d/'transitions.bin')
h=r/'generated/m3-assets.h';h.write_text(re.sub(r'M3_TILE_COUNT=\d+',f'M3_TILE_COUNT={len(tiles)}',h.read_text()))
print(json.dumps(dict(original_logical_tiles=len(old),preserved=len(old),new_tiles=len(added),logical_tiles=len(tiles),physical_tiles=len(raw))))
# Keep original animation IDs but omit physical tiles unreachable from maps,
# patch operations and the M3 animation table. Logical holes remain valid.
used=set(map(int,np.fromfile(d/'maps.bin','<u2')))
used.update(int(op[2]) for op in ops if op[2]!=65535);used.update(map(int,trans.ravel()))
s=(r/'generated/background-animation158.h').read_text();m=re.search(r'auto_tiles_m3\[\]=\{(.*?)\};',s,re.S)
for base,p0,p1 in re.findall(r'\{\d+u,(\d+),\{(\d+),(\d+)\}\}',m[1]):used.update(map(int,(base,p0,p1)))
assert max(used)<len(ids)
needed=sorted({int(ids[i]) for i in used}|{0});index={p:i for i,p in enumerate(needed)}
np.array([raw[i] for i in needed],'u1').tofile(d/'tiles4.bin')
np.array([index[int(v)] if i in used else 0 for i,v in enumerate(ids)],'<u2').tofile(d/'ids4.bin')
print('Referenced physical tiles',len(needed),'removed',len(raw)-len(needed))
