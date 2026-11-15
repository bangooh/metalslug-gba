#!/usr/bin/env python3
from pathlib import Path
import difflib,hashlib,json,zipfile
R=Path(__file__).resolve().parents[1];B=R/'build/native110';out=R.parent/'release';out.mkdir(exist_ok=True)
diffs=[]
for folder in ['src','tools','tests']:
 for p in sorted((R/folder).rglob('*')):
  if p.suffix not in ['.c','.h','.S','.py','.ld'] or '__pycache__' in p.parts:continue
  rel=p.relative_to(R);old=B/'baseline'/rel
  before=old.read_text() if old.exists() else '';after=p.read_text()
  if before!=after:diffs.extend(difflib.unified_diff(before.splitlines(True),after.splitlines(True),fromfile='a/'+str(rel),tofile='b/'+str(rel)))
(R/'docs/native-assets-0.5.110/changes.patch').write_text(''.join(diffs))
target=out/'metal-slug-gba-native-v0.5.110-evidence.zip'
with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 for name in ['build.json','summary.json','probe.txt','symbols.txt','trace.jsonl','commits.csv','inputs.csv','entities.csv','execution.json','ewram.bin','iwram.bin','audio.s16le','native.elf']:
  p=B/'hot-play'/name
  if p.exists():z.write(p,'final-play/'+name)
 for frame in [6428,9000,25000]:
  for p in (B/f'final-snap-{frame}').iterdir():
   if p.suffix in ['.bin','.json','.png','.log','.c']:z.write(p,f'snapshot-{frame}/'+p.name)
 for p in (B/'final-frontend').rglob('*'):
  if p.is_file() and p.suffix in ['.json','.jsonl','.csv']:z.write(p,'frontend/'+str(p.relative_to(B/'final-frontend')))
 z.write(B/'final-audio/verification.json','audio-ring.json')
 for p in (R/'docs/native-assets-0.5.110').iterdir():
  if p.is_file():z.write(p,'report-data/'+p.name)
 for name in ['build.json','summary.json','commits.csv']:
  z.write(R/'build/rom109/audio-only'/name,'baseline109/'+name)
 z.write(R/'docs/native-assets-0.5.110.ko.html','native-assets-0.5.110.ko.html')
print(json.dumps({'file':str(target),'bytes':target.stat().st_size,'sha256':hashlib.sha256(target.read_bytes()).hexdigest()}))
