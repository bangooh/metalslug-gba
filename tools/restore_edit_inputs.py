#!/usr/bin/env python3
"""Reconstruct local editor inputs from ROM-derived bases and pixel edits.

The repository distributes the recipe, never the reconstructed art directory.
Every image checks its source and result hash; JSON formatting and hashes are
also checked before writing. These are conversion checks, not rights clearance.
"""
import argparse,hashlib,json,re
from pathlib import Path
import numpy as np
from PIL import Image
from rom_edit_sources import Sources,apply_patch
ROOT=Path(__file__).resolve().parents[1]
def restore(source,bios):
 index=json.loads((ROOT/'assets/index.json').read_text())
 recipe={'format':index['format'],'images':[],'files':[]}
 for name in index['image_chunks']:recipe['images'].extend(json.loads((ROOT/'assets'/name).read_text()))
 for name in index['input_recipes']:recipe['files'].append(json.loads((ROOT/'assets'/name).read_text()))
 if recipe.get('format')!='rom-relative-pixel-edits-v1':raise ValueError('Unsupported edit recipe')
 sources=Sources(ROOT,source,bios);images=[]
 for i,record in enumerate(recipe['images']):
  if 'image' in record:
   if not 0<=record['image']<i:raise ValueError('Forward/cyclic image reference')
   base=images[record['image']]
  else:base=sources.get(record['source'])
  images.append(apply_patch(base,record))
 pending=[]
 for entry in recipe['files']:
  rel=Path(entry['path'])
  if rel.is_absolute() or '..' in rel.parts or rel.parts[0]!='art':raise ValueError('Invalid local art path')
  target=ROOT/rel
  if entry['type']=='json':
   def image_text(m):
    v=entry['images'][int(m[1])];a=images[v['image']];parts=v['parts'];alphabet=v['alphabet']
    if len(parts)!=len(a)+1:raise ValueError('Invalid image formatting')
    return ''.join(parts[y]+''.join(alphabet[int(c)] for c in row) for y,row in enumerate(a))+parts[-1]
   data=re.sub(r'@@ROM_IMAGE_(\d+)@@',image_text,entry['template']).encode()
   if hashlib.sha256(data).hexdigest()!=entry['sha256']:raise ValueError('Reconstructed JSON mismatch: '+str(rel))
   json.loads(data);pending.append((target,'bytes',data))
  elif entry['type']=='text':pending.append((target,'bytes',entry['text'].encode()))
  elif entry['type'] in ('png','npy'):pending.append((target,entry['type'],entry))
  else:raise ValueError('Unknown edit input kind')
 for path,kind,value in pending:
  path.parent.mkdir(parents=True,exist_ok=True)
  if kind=='bytes':path.write_bytes(value)
  elif kind=='npy':np.save(path,images[value['image']])
  else:
   a=images[value['image']];im=Image.frombytes('P',(a.shape[1],a.shape[0]),a.tobytes());im.putpalette(value['palette']);im.save(path)
 print(f'Restored {len(pending)} local inputs from {len(images)} verified ROM-relative images.')
 (ROOT/'generated/edit-reconstruction.json').write_text(json.dumps({'format':recipe['format'],'images_verified':len(images),'files_restored':len(pending),'recipe_sha256':hashlib.sha256(json.dumps(recipe,sort_keys=True,separators=(',',':')).encode()).hexdigest(),'gameplay_tested':False},indent=2)+'\n')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--bios',type=Path,required=True);a=p.parse_args();restore(a.source.resolve(),a.bios.resolve())
