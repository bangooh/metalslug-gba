#!/usr/bin/env python3
"""Offline asset packaging integrity only; never run a game or native mixer.

Check offsets, hashes, reply bounds and the stored IMA state at each loop.
This is not an audio listening, hardware, regression or gameplay test.
"""
import hashlib,json,re
from pathlib import Path
R=Path(__file__).resolve().parents[1]
def sha(data):return hashlib.sha256(data).hexdigest()
def main():
 g=R/'generated';m=json.loads((g/'native-audio-source.json').read_text());data=(g/'native-audio.ima').read_bytes()
 table=list(map(int,re.search(r'=\{(.*?)\};',(g/'audio_ima_fast.h').read_text(),re.S).group(1).split(',')))
 assert len(table)==89*16 and len(data)==m['bytes']
 offset=events=loops=0;previous=-1
 for a in m['assets']:
  assert a['command']>previous;previous=a['command']
  assert a['offset']==offset and a['samples']>0 and a['first_event']==events
  n=(a['samples']+1)//2;encoded=data[offset:offset+n]
  assert sha(encoded)==a['adpcm_sha256']
  assert all(0<=t<a['samples'] and (v==255 or 0<=v<=63)for t,v in a['replies'])
  assert a['replies']==sorted(a['replies'],key=lambda r:r[0])
  if a['loop']!=0xffffffff:
   loops+=1;assert 0<a['loop']<a['samples'];predictor=index=0
   for i in range(a['loop']):
    byte=encoded[i//2];code=byte&15 if i&1 else byte>>4;packed=table[index*16+code]
    delta=packed&0x3ffff
    if delta&0x20000:delta-=0x40000
    predictor=max(-32768,min(32767,predictor+delta));index=packed>>18
   assert (predictor,index)==(a['loop_predictor'],a['loop_index']),hex(a['command'])
  offset+=n;events+=len(a['replies'])
 assert offset==len(data)
 result=dict(scope='Offline audio asset integrity only; native game/mixer not executed',assets=len(m['assets']),
  bytes=len(data),reply_events=events,loops_with_verified_ima_state=loops,sha256=sha(data),
  source_manifest_sha256=sha((g/'native-audio-source.json').read_bytes()),gameplay_verified=False,hardware_verified=False)
 (g/'native-audio-integrity.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
