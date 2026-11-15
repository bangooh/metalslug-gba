#!/usr/bin/env python3
"""Summarize an input-only campaign using its ROM-specific probe and log.

No emulator or compilation is started. Compare fresh runs, not foreign savestates.
FPS means consecutive live game-tick throughput measured against real LCD time.
"""
import argparse
import csv
import json
import math
import struct
from collections import Counter
from pathlib import Path

HZ = 16777216 / 280896
NAMES = {0: 'M1', 1: 'M2', 3: 'M3', 2: 'M4', 4: 'M5', 5: 'M6'}

def measure(run, trace):
    rows = [{k: int(v) for k, v in r.items()} for r in csv.DictReader((run / 'commits.csv').open())]
    samples = []
    for line in trace.open():
        try:
            samples.append(json.loads(line))
        except ValueError:
            pass
    finish = next((r['lcd_frame'] for r in samples if r.get('session.state') == 10 and r.get('world.results.progress') == 6), None)
    full_samples = samples
    input_check = None
    if (run/'inputs.csv').exists():
        events = []
        malformed = 0
        for row in csv.reader((run/'inputs.csv').open()):
            try:
                events.append((int(row[0]),int(row[1])))
            except (ValueError,IndexError):
                malformed += 1
        index = held = 0
        mismatches = []
        for row in full_samples:
            while index < len(events) and events[index][0] <= row['lcd_frame']:
                held = events[index][1]
                index += 1
            if 'keys' in row and held != row['keys']:
                mismatches.append({'lcd':row['lcd_frame'],'input_file_keys':held,'trace_keys':row['keys']})
        input_check = {'trace_samples':len(full_samples),'input_events':len(events),'last_event_lcd':events[-1][0] if events else None,'malformed_rows':malformed,'mismatch_count':len(mismatches),'first_mismatches':mismatches[:10]}
    if finish is not None:
        rows = [r for r in rows if r['lcd'] <= finish]
        samples = [r for r in samples if r['lcd_frame'] <= finish]
    missions, all_gaps = {}, []
    active_missions, active_all = {}, []
    for scene, name in NAMES.items():
        eligible = [(a, b) for a, b in zip(rows, rows[1:]) if a['scene'] == b['scene'] == scene and all(r['playing'] and not r['presentation'] and not r['paused'] and not r['demo'] and r['live0'] == 1 for r in (a, b))]
        active = [(a, b) for a, b in eligible if b['commits'] == a['commits'] + 1 and b['ticks'] >= a['ticks'] and b['lcd'] > a['lcd']]
        if active:
            active_all.extend(active)
            gaps = sorted(b['lcd'] - a['lcd'] for a, b in active)
            ticks = sum(b['ticks'] - a['ticks'] for a, b in active)
            elapsed = sum(gaps)
            slow = sorted(active, key=lambda p: p[1]['lcd']-p[0]['lcd'], reverse=True)[:10]
            active_missions[name] = {
                'intervals': len(active), 'ticks': ticks, 'lcd_frames': elapsed,
                'tick_fps': HZ*ticks/elapsed, 'display_fps': HZ*len(active)/elapsed,
                'tick_delta_histogram': dict(sorted(Counter(b['ticks']-a['ticks'] for a,b in active).items())),
                'eligible_commit_delta_histogram': dict(sorted(Counter(b['commits']-a['commits'] for a,b in eligible).items())),
                'excluded_intervals': len(eligible)-len(active),
                'p50_ms': gaps[math.ceil(len(gaps)*.50)-1]*1000/HZ,
                'p95_ms': gaps[math.ceil(len(gaps)*.95)-1]*1000/HZ,
                'p99_ms': gaps[math.ceil(len(gaps)*.99)-1]*1000/HZ,
                'worst_ms': max(gaps)*1000/HZ,
                'over_100ms': sum(d*1000/HZ >= 100 for d in gaps),
                'worst': [{'start_lcd':a['lcd'],'end_lcd':b['lcd'],'start_commit':a['commits'],'end_commit':b['commits'],'start_tick':a['ticks'],'end_tick':b['ticks'],'lcd_gap':b['lcd']-a['lcd'],'camera':a['camera_x'],'entities':a['entities']} for a,b in slow],
            }
        pairs = [(a,b) for a,b in eligible if b['ticks'] == a['ticks'] + 1]
        if not pairs:
            continue
        gaps = [b['lcd'] - a['lcd'] for a, b in pairs]
        all_gaps += gaps
        ordered = sorted(gaps)
        slow = sorted(pairs, key=lambda p: p[1]['lcd']-p[0]['lcd'], reverse=True)[:10]
        missions[name] = {
            'intervals': len(gaps), 'lcd_frames': sum(gaps), 'fps': HZ*len(gaps)/sum(gaps),
            'p50_ms': ordered[math.ceil(len(gaps)*.50)-1]*1000/HZ,
            'p95_ms': ordered[math.ceil(len(gaps)*.95)-1]*1000/HZ,
            'p99_ms': ordered[math.ceil(len(gaps)*.99)-1]*1000/HZ,
            'worst_ms': max(gaps)*1000/HZ,
            'over_100ms': sum(d*1000/HZ >= 100 for d in gaps),
            'worst': [{'start_lcd': a['lcd'], 'end_lcd': b['lcd'], 'start_tick': a['ticks'], 'lcd_gap': b['lcd']-a['lcd'], 'camera': a['camera_x'], 'entities': a['entities']} for a,b in slow],
        }
    fault_keys = set().union(*(r.keys() for r in samples))
    faults = {k: max(r.get(k, 0) for r in samples) for k in sorted(fault_keys) if (k.endswith('.fault') or k == 'app.error') and not k.startswith('address.')}
    stack = None
    if (run/'iwram.bin').exists():
        data = (run/'iwram.bin').read_bytes()
        stack = {name: end-next((x for x in range(start,end,4) if struct.unpack_from('<I',data,x)[0] != 0xA55A3CC3),end) for name,start,end in [('main',0x6700,0x7700),('irq',0x7700,0x7fa0)]}
    return {
        'method': 'Fresh input-only state-aware controller. active_span is primary: same-mission consecutive alive active display commits, keeping 0/2 sampled tick deltas. Top-level missions/aggregate_fps retain the legacy exactly-one-tick filter for historical comparison only. LCD clock; emulator, not physical hardware proof.',
        'run': str(run), 'trace': str(trace), 'return_to_title_lcd': finish,
        'missions': missions,
        'aggregate_fps': HZ*len(all_gaps)/sum(all_gaps) if all_gaps else None,
        'active_span': {
            'method': 'Consecutive display commits in the same mission; both samples playing, alive, unpaused, outside presentation/demo. Monotonic ticks and LCD. Tick deltas 0 and 2 reflect sampling across IRQ/CPU publication and are retained; summed tick deltas telescope across contiguous active spans. Display tails include every such commit interval.',
            'missions': active_missions,
            'aggregate_tick_fps': HZ*sum(b['ticks']-a['ticks'] for a,b in active_all)/sum(b['lcd']-a['lcd'] for a,b in active_all) if active_all else None,
            'aggregate_display_fps': HZ*len(active_all)/sum(b['lcd']-a['lcd'] for a,b in active_all) if active_all else None,
        },
        'faults': faults,
        'late_max': max((r.get('video.late_transfers', 0) for r in samples), default=0),
        'observed_stack_bytes': stack,
        'input_trace_validation': input_check,
        'post_title': {
            'observed_through_lcd': full_samples[-1]['lcd_frame'] if full_samples else None,
            'late_max': max((r.get('video.late_transfers',0) for r in full_samples if finish is not None and r['lcd_frame']>finish),default=0),
            'fault_max': max((v for r in full_samples if finish is not None and r['lcd_frame']>finish for k,v in r.items() if (k.endswith('.fault') or k=='app.error') and not k.startswith('address.')), default=0),
        } if finish is not None else None,
        'execution': json.loads((run/'execution.json').read_text()) if (run/'execution.json').exists() else None,
    }

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--run',type=Path,required=True)
    p.add_argument('--trace',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--baseline',type=Path)
    a=p.parse_args(); result=measure(a.run,a.trace)
    if a.baseline:
        base=json.loads(a.baseline.read_text())
        result['comparison']={name:{'fps_percent':(m['fps']/base['missions'][name]['fps']-1)*100,'worst_ms_delta':m['worst_ms']-base['missions'][name]['worst_ms'],'over_100ms_delta':m['over_100ms']-base['missions'][name]['over_100ms']} for name,m in result['missions'].items() if name in base['missions']}
        result['aggregate_fps_percent']=(result['aggregate_fps']/base['aggregate_fps']-1)*100
        if 'active_span' in base:
            current, previous = result['active_span'], base['active_span']
            current['comparison'] = {name: {'tick_fps_percent': (m['tick_fps']/previous['missions'][name]['tick_fps']-1)*100,
                                           'display_fps_percent': (m['display_fps']/previous['missions'][name]['display_fps']-1)*100,
                                           'worst_ms_delta': m['worst_ms']-previous['missions'][name]['worst_ms'],
                                           'over_100ms_delta': m['over_100ms']-previous['missions'][name]['over_100ms']}
                                     for name,m in current['missions'].items() if name in previous['missions']}
            current['aggregate_tick_fps_percent'] = (current['aggregate_tick_fps']/previous['aggregate_tick_fps']-1)*100
            current['aggregate_display_fps_percent'] = (current['aggregate_display_fps']/previous['aggregate_display_fps']-1)*100
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'output':str(a.output),'primary_tick_fps':result['active_span']['aggregate_tick_fps'],'primary_display_fps':result['active_span']['aggregate_display_fps'],'missions_tick_fps':{n:round(m['tick_fps'],4) for n,m in result['active_span']['missions'].items()},'legacy_aggregate_fps':result['aggregate_fps'],'return_to_title_lcd':result['return_to_title_lcd'],'fault_max':max(result['faults'].values(),default=0),'late_max':result['late_max']}))

if __name__=='__main__':main()
