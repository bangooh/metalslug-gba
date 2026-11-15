#!/usr/bin/env python3
"""Release acceptance requires matched captures of the actual Neo Geo ROM.
Host unit tests and GBA-only playthroughs never satisfy this gate.
"""
import argparse,hashlib,json
from pathlib import Path
from PIL import Image
PHASES=('opening','scroll','transition','destruction','boss','results')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def check(candidate,reference,manifest,missions):
 data=json.loads(manifest.read_text());errors=[];root=manifest.parent
 for key,path in [('gba_rom_sha256',candidate),('neogeo_rom_sha256',reference)]:
  if data.get(key)!=sha(path):errors.append(key+': ROM digest mismatch')
 covered=set()
 for case in data.get('cases',[]):
  tag=f"M{case.get('mission',-1)+1}/{case.get('phase','?')}"
  if case.get('reference_kind')!='neogeo_rom_capture':errors.append(tag+': original ROM execution capture required')
  for side in ('neogeo','gba'):
   artifact=case.get(side,{})
   try:
    path=root/artifact['image']
    if sha(path)!=artifact['sha256']:raise ValueError('image digest mismatch')
    with Image.open(path) as im:im.verify()
   except (OSError,KeyError,ValueError) as e:errors.append(tag+': '+side+' '+str(e))
  a,b=case.get('neogeo',{}).get('state',{}),case.get('gba',{}).get('state',{})
  # Coordinates must already be expressed in the same original world units.
  for key in ('scene','camera_x','camera_y','event_state'):
   if key not in a or key not in b or a[key]!=b[key]:errors.append(tag+': unmatched '+key)
  if case.get('review')!='passed' or case.get('unresolved_differences',[]):errors.append(tag+': unresolved/unreviewed comparison')
  if not case.get('approved_adaptations'):errors.append(tag+': scaling/crop adaptations must be explicit')
  covered.add((case.get('mission'),case.get('phase')))
 for mission in missions:
  for phase in PHASES:
   if (mission,phase) not in covered:errors.append(f'M{mission+1}/{phase}: missing reference comparison')
 return errors
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--candidate',required=True,type=Path);p.add_argument('--reference',required=True,type=Path);p.add_argument('--manifest',required=True,type=Path);p.add_argument('--missions',nargs='+',type=int,default=list(range(6)));a=p.parse_args()
 errors=check(a.candidate,a.reference,a.manifest,a.missions)
 print(json.dumps({'reference_verified':not errors,'errors':errors},ensure_ascii=False,indent=2));raise SystemExit(bool(errors))
