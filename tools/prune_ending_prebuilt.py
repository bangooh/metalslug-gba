"""Remove only prebuilt profile12 tiles, now reconstructed by ending runtime."""
from pathlib import Path
import sys,re,json,shutil
import numpy as np
from compile_m1_native_maps import body,numbers
from pack_native_background import main as pack
R=Path(__file__).resolve().parents[1];G=R/'generated';P=G/'native-background';O=R/'validation-ending-all';backup=O/'asset-backup';backup.mkdir(parents=True,exist_ok=True)
for p in P.iterdir():
 if p.is_file() and not (backup/p.name).exists():shutil.copy2(p,backup/p.name)
for p in [G/'native-bg-assets.h',R/'src/native_bg_assets.S',R/'src/tile_video_assets.S']:
 if not (backup/p.name).exists():shutil.copy2(p,backup/p.name)
s=(G/'native-bg-assets.c').read_text();rr=[numbers(x)for x in re.findall(r'\{([^{}]+)\}',body(s,'native_bg_regions'))];vv=[numbers(x)for x in re.findall(r'\{([^{}]+)\}',body(s,'native_bg_variants'))];m=json.loads((P/'manifest.json').read_text());a=np.fromfile(P/'maps.u24','u1').reshape(-1,3);ids=a[:,0].astype('u4')|a[:,1].astype('u4')<<8|a[:,2].astype('u4')<<16;mask=np.zeros(len(ids),bool)
for i,row in enumerate(m['rows']):
 if row['file']=='scene-programs.c' and row['cells'] in ['map_cells48','map_cells49','map_cells50']:
  assert row['profiles']==[12]
  for o,p,pad in vv[rr[i][1]:rr[i][1]+rr[i][5]]:mask[o:o+row['map_width']*row['map_height']]=1
old=ids.copy();ids[mask]=0;used=np.unique(ids);lookup=np.zeros(int(ids.max())+1,'u4');lookup[used]=np.arange(len(used));new=lookup[ids]
t=np.fromfile(P/'tiles.4bpp','u1').reshape(-1,32);b=np.fromfile(P/'tile-banks.u8','u1');assert np.array_equal(t[used][new[~mask]],t[old[~mask]]);assert np.array_equal(b[used][new[~mask]],b[old[~mask]])
(P/'tiles.4bpp').write_bytes(t[used].tobytes());(P/'tile-banks.u8').write_bytes(b[used].tobytes());(P/'maps.u24').write_bytes(np.stack([new&255,new>>8&255,new>>16&255],-1).astype('u1').tobytes())
h=G/'native-bg-assets.h';h.write_text(re.sub(r'NATIVE_BG_NATIVE_TILES=\d+',f'NATIVE_BG_NATIVE_TILES={len(used)}',h.read_text()));pack()
(O/'prune-report.json').write_text(json.dumps(dict(old_tiles=len(t),new_tiles=len(used),removed=len(t)-len(used),other_maps_pixel_and_bank_exact=True),indent=2));print('removed',len(t)-len(used))
