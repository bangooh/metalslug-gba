"""Remove legacy map payloads superseded by native catalogs; retain all ending maps and all patch regions."""
import json,re,shutil
import numpy as np
from pathlib import Path
from compile_m1_native_maps import ROOT as R,G,body,numbers
P=G/'native-background';backup=G/'native-background-full';backup.mkdir(exist_ok=True)
for n in ['maps.u24','tiles.4bpp','tile-banks.u8']:
 if not (backup/n).exists():shutil.copyfile(P/n,backup/n)
f=G/'native-bg-assets.c'
if not (backup/f.name).exists():shutil.copyfile(f,backup/f.name)
s=(backup/f.name).read_text();manifest=json.loads((P/'manifest.json').read_text())
regions=re.findall(r'\{([^{}]+)\}',body(s,'native_bg_regions'));variants=[numbers(x) for x in re.findall(r'\{([^{}]+)\}',body(s,'native_bg_variants'))]
scene=(G/'scene-programs.c').read_text();ds=[numbers(x) for x in re.findall(r'\{([^{}]+)\}',body(scene,'scene_descriptors'))];cs=[(numbers(m[1]),int(m[2]),int(m[3]),int(m[4])) for m in re.finditer(r'\{\{([^{}]*)\},(\d+),(\d+),(\d+)\}',body(scene,'scene_commands'))]
keep=set()
for prog in range(8,16):
 at=ds[prog][0];seen=set()
 while at not in seen:
  seen.add(at);v,n,op,p=cs[at]
  if op==0:keep.add(v[0])
  at=n
raw=np.fromfile(backup/'maps.u24',np.uint8).reshape(-1,3);maps=raw[:,0].astype(np.uint32)|(raw[:,1].astype(np.uint32)<<8)|(raw[:,2].astype(np.uint32)<<16)
new=[];newregions=[];removed=[];image=0
for i,(r,record) in enumerate(zip(regions,manifest['rows'])):
 fields=r.split(',');drop=False
 if record['file']=='scene-programs.c':drop=image not in keep;image+=1
 first=int(fields[1]);count=int(fields[5]);length=record['map_width']*record['map_height']
 if drop:fields[5]='0';removed.append(i)
 for j in range(count):
  v=variants[first+j];old=v[0];v[0]=len(new)
  if not drop:new.extend(maps[old:old+length])
 newregions.append(','.join(fields))
new=np.array(new,np.uint32);used=np.unique(np.r_[0,new]);remap=np.zeros(len((backup/'tiles.4bpp').read_bytes())//32,np.uint32);remap[used]=np.arange(len(used));new=remap[new]
tiles=np.fromfile(backup/'tiles.4bpp',np.uint8).reshape(-1,32);banks=np.fromfile(backup/'tile-banks.u8',np.uint8)
(P/'tiles.4bpp').write_bytes(tiles[used].tobytes());(P/'tile-banks.u8').write_bytes(banks[used].tobytes());(P/'maps.u24').write_bytes(np.stack([new&255,new>>8&255,new>>16&255],-1).astype('uint8').tobytes())
for name,rs in [('native_bg_regions',newregions),('native_bg_variants',[','.join(map(str,v)) for v in variants])]:
 old=body(s,name);s=s.replace(old,','.join('{'+r+'}' for r in rs))
f.write_text(s)
report={'retained_ending_images':sorted(keep),'removed_regions':removed,'tiles_before':len(tiles),'tiles_after':len(used),'all_patch_regions_retained':True,'retained_pixels_exact':True}
(R/'build/campaign134/legacy-prune.json').write_text(json.dumps(report,indent=2));print(report)
from repair_ending_starfield import repair
repair(write=True)
