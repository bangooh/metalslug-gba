"""Compare fresh input-only runs; exclude menus, pauses, deaths and demo."""
import csv,json,math
from pathlib import Path
H=16777216/280896
names={0:'M1',1:'M2',3:'M3',2:'M4',4:'M5',5:'M6'}
root=Path('build/extreme144');out={}
for name in ['base','candidate','combined','predicated']:
 rows=[{k:int(v) for k,v in row.items()} for row in csv.DictReader(open(root/('run-'+name)/'commits.csv'))]
 trace=[]
 for line in open(root/(name+'.log')):
  try:trace.append(json.loads(line))
  except ValueError:pass
 complete=next((r['lcd_frame'] for r in trace if r.get('session.state')==10 and r.get('world.results.progress')==6),None)
 if complete is not None:
  rows=[r for r in rows if r['lcd']<=complete];trace=[r for r in trace if r['lcd_frame']<=complete]
 missions={}
 for sc,title in names.items():
  ds=[b['lcd']-a['lcd'] for a,b in zip(rows,rows[1:]) if a['scene']==b['scene']==sc and b['ticks']==a['ticks']+1 and all(r['playing'] and not r['presentation'] and not r['paused'] and not r['demo'] and r['live0']==1 for r in (a,b))]
  if ds:missions[title]={'intervals':len(ds),'fps':H*len(ds)/sum(ds),'p95_ms':sorted(ds)[math.ceil(len(ds)*.95)-1]/H*1000,'worst_ms':max(ds)/H*1000}
 faults={k:max(r.get(k,0) for r in trace) for k in trace[0] if (k.endswith('.fault') or k=='app.error') and not k.startswith('address.')}
 ep=root/('run-'+name)/'execution.json'
 out[name]={'missions':missions,'return_to_title_lcd':complete,'faults':faults,'late_max':max(r.get('video.late_transfers',0) for r in trace),'execution':json.loads(ep.read_text()) if ep.exists() else 'running'}
out['method']='Fresh-start mGBA, identical mode8/M3_NATIVE=1 state-aware button controller. Consecutive alive active game ticks only; actual LCD clock. Workloads can diverge; not pixel-identical or physical hardware evidence.'
Path('docs/extreme144/measurement.json').write_text(json.dumps(out,indent=2)+'\n')
for n in ['base','candidate','combined','predicated']:
 print(n,out[n]['return_to_title_lcd'],{k:round(v['fps'],4) for k,v in out[n]['missions'].items()},out[n]['late_max'],max(out[n]['faults'].values()))
