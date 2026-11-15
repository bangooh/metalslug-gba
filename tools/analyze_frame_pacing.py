#!/usr/bin/env python3
"""Measure new-frame intervals, excluding duplicate displays and transitions."""
import argparse, collections, csv, json, math, statistics
from pathlib import Path

LCD_MS = 280896 * 1000 / 16777216

def samples(directory, first_tick=10, last_tick=5200):
    rows = list(csv.DictReader((directory/'commits.csv').open()))
    result = []
    for a, b in zip(rows, rows[1:]):
        if any(r['playing'] != '1' or r['presentation'] != '0' or
               r['demo'] != '0' or r['paused'] != '0' or r['scene'] != '0'
               for r in (a, b)):
            continue
        if not first_tick <= int(a['ticks']) < int(b['ticks']) <= last_tick:
            continue
        if int(b['commits'])-int(a['commits']) != 1 or int(b['ticks'])-int(a['ticks']) != 1:
            continue
        gap = int(b['lcd'])-int(a['lcd'])
        assert gap > 0
        result.append(dict(tick=int(b['ticks']), lcd=int(b['lcd']),
                           camera=int(b['camera_x']), lcd_frames=gap, ms=gap*LCD_MS))
    return result

def metrics(rows):
    durations = sorted(r['ms'] for r in rows)
    return dict(samples=len(rows), histogram_lcd=dict(sorted(collections.Counter(r['lcd_frames'] for r in rows).items())),
                p50_ms=durations[math.floor((len(rows)-1)*.50)],
                p95_ms=durations[math.floor((len(rows)-1)*.95)],
                p99_ms=durations[math.floor((len(rows)-1)*.99)],
                max_ms=max(durations), mean_ms=statistics.mean(durations),
                stddev_ms=statistics.pstdev(durations),
                at_least_100ms=sum(r['ms'] >= 100 for r in rows),
                at_least_8_lcd_frames=sum(r['lcd_frames'] >= 8 for r in rows))

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--baseline', required=True, type=Path)
    p.add_argument('--candidate', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    a = p.parse_args(); a.output.mkdir(parents=True, exist_ok=True)
    report = dict(lcd_frame_ms=LCD_MS, measurement='Commit counter observed once per LCD frame; not a direct timestamp of first visible pixels. Late transfer/reveal and white LCD frames are reported separately.', scope='M1 consecutive simulation/commit pairs; ticks10..5200; non-demo, unpaused gameplay. Startup and results transitions excluded.',
                  comparison='Same state-aware controller, separate input-only runs; combat dwell and sampled frames differ. Not identical world states or a human timing experiment.')
    for name, directory in [('baseline', a.baseline), ('candidate', a.candidate)]:
        rows = samples(directory); report[name] = metrics(rows)
        with (a.output/(name+'.csv')).open('w') as f:
            writer = csv.DictWriter(f, fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
    (a.output/'measurement.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report))

if __name__ == '__main__':
    main()
