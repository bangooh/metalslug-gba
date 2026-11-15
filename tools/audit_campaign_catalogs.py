#!/usr/bin/env python3
"""Inventory authored campaign map commands; not a gameplay validation."""
import json,re
from compile_m1_native_maps import ROOT,G,body,numbers
s=(G/'scene-programs.c').read_text()
commands=[(numbers(m[1]),int(m[2]),int(m[3]),int(m[4])) for m in re.finditer(r'\{\{([^{}]*)\},(\d+),(\d+),(\d+)\}',body(s,'scene_commands'))]
descriptors=[numbers(x) for x in re.findall(r'\{([^{}]*)\}',body(s,'scene_descriptors'))]
rows=[]
for mission,program in enumerate([0,1,3,2,4,5],1):
 at=descriptors[program][0];seen=set();events=[]
 while at not in seen:
  assert 0<=at<len(commands)
  seen.add(at);args,nxt,op,plane=commands[at]
  if op in (0,3,10,11,13,18):events.append(dict(command=at,op=op,plane=plane,args=args))
  at=nxt
 rows.append(dict(mission=mission,program=program,commands=len(seen),images=sorted({e['args'][0] for e in events if e['op']==0}),events=events))
report=dict(scope='Static command-chain inventory only. Dynamic callbacks, destruction patches, relink images and palette variants require separate closure before integration.',campaign=rows,rom_limit=33554432,current_rom_bytes=32711316,remaining_bytes=843116,full_campaign_native=False)
out=G/'campaign-native-inventory.json';out.write_text(json.dumps(report,indent=2)+'\n')
for r in rows:print('M%d program%d commands%d images%s'%(r['mission'],r['program'],r['commands'],r['images']))
