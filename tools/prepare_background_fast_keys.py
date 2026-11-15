#!/usr/bin/env python3
"""Compile exact fragment keys for authored tiles, prioritizing all M1 maps.
Selection is source-catalog frequency, not captured gameplay assets.
"""
import json,re,hashlib
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1];G=R/'generated'
def main():
 lookup=np.fromfile(G/'native-bg-lookup.u16',dtype='<u2');fragments=np.fromfile(G/'native-bg-fragments.u32',dtype='<u4').reshape(-1,9);opacity=np.fromfile(G/'native-bg-opacity.u16',dtype='<u2').reshape(-1,4)
 ledger=json.loads((G/'tile-video-source.json').read_text());scenes=json.loads((G/'scene-program-source.json').read_text());m1={c['values'][0]for c in scenes['commands'][:113]if c['op']==0}
 arrays={};score=np.zeros(len(fragments),np.int64);m1_ids=set();records=[]
 for rec in ledger['records']:
  name=rec['file']
  if name not in arrays:arrays[name]={m[1]:np.array([int(x.strip(),0)for x in m[2].split(',')if x.strip()],np.uint32)for m in re.finditer(r'(?:static\s+)?const uint16_t (\w+)\[[^]]*\]\s*=\s*\{([^{}]*)\};',(G/name).read_text())}
  cells=arrays[name][rec['cells']].reshape(-1,2);a=cells[:,1];ids=lookup[(((a>>4)&15)<<16)|cells[:,0]];assert np.all(ids<len(fragments))
  primary=name=='opening-programs.c' or (name=='scene-programs.c' and int(rec['cells'].removeprefix('map_cells'))in m1)
  if primary:m1_ids.update(map(int,ids))
  np.add.at(score,ids,64 if primary else 1);records.append((rec,ids,primary))
 # Give every M1 tile priority, then favor frequent whole-catalog tiles.
 order=sorted(range(len(fragments)),key=lambda n:(n not in m1_ids,-int(score[n]),n));selected=order[:1500000//144]
 direct=lookup.astype('<u4');payload=np.empty((len(selected),4,9),dtype='<u4');byid=np.full(len(fragments),-1,np.int32)
 for n,idx in enumerate(selected):
  byid[idx]=n
  for flip in range(4):
   for y in range(3):
    for x in range(3):
     block=int(fragments[idx,(2-y if flip&2 else y)*3+(2-x if flip&1 else x)]);payload[n,flip,y*3+x]=(block<<10)|flip if opacity[block,0]else 0x0fffffff
  direct[lookup==idx]=0x80000000+n*36
 # Verify all raw source ids, flip modes and fragment positions against
 # the original lookup/fragment/opacity pipeline, not a gameplay capture.
 verified=0
 for idx in selected:
  n=int(byid[idx])
  for flip in range(4):
   for y in range(3):
    for x in range(3):
     block=int(fragments[idx,(2-y if flip&2 else y)*3+(2-x if flip&1 else x)]);expected=block<<10|flip if opacity[block,0]else 0x0fffffff
     assert int(payload[n,flip,y*3+x])==expected;verified+=1
 valid=lookup<len(fragments);expect=lookup.astype('<u4');sel=valid.copy();sel[valid]=byid[lookup[valid]]>=0;expect[sel]=0x80000000+byid[lookup[sel]].astype(np.uint32)*36;assert np.array_equal(expect,direct)
 (G/'native-bg-fast-lookup.u32').write_bytes(direct.tobytes());(G/'native-bg-fast-keys.u32').write_bytes(payload.tobytes())
 (G/'native-bg-fast-keys.h').write_text('#ifndef NATIVE_BG_FAST_KEYS_H\n#define NATIVE_BG_FAST_KEYS_H\n#include <stdint.h>\nextern const uint32_t native_bg_fast_lookup[131072];\nextern const uint32_t native_bg_fast_keys[];\n#endif\n')
 report=dict(selected_tiles=len(selected),m1_unique_tiles=len(m1_ids),m1_selected=len(m1_ids&set(selected)),m1_images=sorted(m1),lookup_bytes=direct.nbytes,keys_bytes=payload.nbytes,keys_verified=verified,selection='All authored M1 scene0 bind images and opening patches first; source-catalog frequency for remaining budget. No play capture supplies assets.',exact=True,rom_linked=False,palette_precompiled=False,full_8x8_composition=False,files={n:hashlib.sha256((G/n).read_bytes()).hexdigest()for n in ['native-bg-fast-lookup.u32','native-bg-fast-keys.u32']})
 (G/'native-bg-fast-keys.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
if __name__=='__main__':main()
