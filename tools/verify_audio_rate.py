#!/usr/bin/env python3
"""Verify resampled catalog duration/loop/reply timing against the prior catalog."""
import argparse, json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def main():
 p=argparse.ArgumentParser();p.add_argument('--baseline',required=True,type=Path);p.add_argument('--output',required=True,type=Path);a=p.parse_args()
 old=json.loads(a.baseline.read_text());new=json.loads((ROOT/'generated/native-audio-source.json').read_text())
 before={r['command']:r for r in old['assets']};assert len(before)==len(new['assets'])
 rate=new['sample_rate'];previous=old['sample_rate'];delta={'duration':0,'loop':0,'reply':0};loops=replies=0
 for r in new['assets']:
  b=before[r['command']]
  for field in ['header','music','priority','tracks','stop','source_events_sha256']:assert r[field]==b[field],(r['command'],field)
  delta['duration']=max(delta['duration'],abs(r['samples']/rate-b['samples']/previous))
  assert (r['loop']==0xffffffff)==(b['loop']==0xffffffff)
  if r['loop']!=0xffffffff:loops+=1;delta['loop']=max(delta['loop'],abs(r['loop']/rate-b['loop']/previous))
  assert len(r['replies'])==len(b['replies'])
  for (t,v),(ot,ov) in zip(r['replies'],b['replies']):
   assert v==ov;replies+=1;delta['reply']=max(delta['reply'],abs(t/rate-ot/previous))
 assert max(delta.values())<=1/rate+1/previous
 period=(16777216+rate//2)//rate
 result=dict(assets=len(new['assets']),loops=loops,replies=replies,source_events_unchanged=True,
  max_time_delta_ms={k:v*1000 for k,v in delta.items()},nominal_rate=rate,timer_cycles=period,
  timer_actual_rate=16777216/period,timer_frequency_error_percent=(16777216/period/rate-1)*100,
  limitation='Catalog timestamp quantization check, not full-campaign listening or hardware timing proof')
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))

if __name__=='__main__':main()
