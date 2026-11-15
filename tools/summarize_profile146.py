#!/usr/bin/env python3
"""Summarize a read-only v146 function profile and validate same-ROM replay.

The private FixedCache layout is 7292 bytes, as in v145/v146. RAM must have
been captured with FIX_CAPTURE_TICK equal to --tick. No machine state is edited.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import struct


def rows(path):
    with path.open() as stream:
        return list(csv.DictReader(stream))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', type=Path, required=True)
    parser.add_argument('--run', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--tick', type=int, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    frames = {int(r['lcd']): r for r in rows(args.profile / 'frames.csv')}
    checks, mismatches = 0, []
    commits = rows(args.run / 'commits.csv')
    for record in commits:
        observed = frames.get(int(record['lcd']))
        if observed is None:
            continue
        checks += 1
        for expected, actual in [('ticks', 'tick'), ('commits', 'commits')]:
            if record[expected] != observed[actual]:
                mismatches.append([record['lcd'], expected, record[expected], observed[actual]])
    functions = {}
    for record in rows(args.profile / 'calls.csv'):
        if int(record['start_tick']) != args.tick:
            continue
        name = record.pop('function')
        functions.setdefault(name, []).append({key: int(value) for key, value in record.items()})
    events = [r for r in rows(args.profile / 'fixed-events.csv')
              if int(r['tick']) == args.tick]
    probe = {}
    for line in (args.candidate / 'probe.txt').read_text().splitlines():
        address, width, name = line.split()
        probe[name] = int(address, 16)
    data = (args.profile / 'ewram.bin').read_bytes()
    base = probe['video.fixed.bytes'] - 0x02000000 - 32772 + 32768 - 7292
    count = struct.unpack_from('<H', data, base + 5108)[0]
    entries = []
    for i in range(count):
        attr0, attr1, tile, tiles, used = struct.unpack_from('<HHHHH', data, base + 3824 + i * 10)
        entries.append(dict(attr0=attr0, attr1=attr1, tile=tile, tiles=tiles, used=used))
    live = [e for e in entries if e['used']]
    intervals = []
    for before, after in zip(commits, commits[1:]):
        if int(before['ticks']) == args.tick - 1 and int(after['ticks']) == args.tick:
            intervals.append({'before': before, 'after': after,
                              'lcd_gap': int(after['lcd']) - int(before['lcd'])})
    result = {
        'profile': str(args.profile), 'run': str(args.run), 'candidate': str(args.candidate),
        'rom_sha256': hashlib.sha256((args.candidate / 'rom.gba').read_bytes()).hexdigest(),
        'tick': args.tick, 'same_rom_replay': {'commit_checks': checks,
                                            'mismatch_count': len(mismatches), 'mismatches': mismatches},
        'intervals': intervals,
        'cache_entry_snapshot': {'layout_bytes': 7292, 'count': count,
                                'tiles': struct.unpack_from('<H', data, base + 5106)[0],
                                'palette_count': struct.unpack_from('<H', data, base + 5110)[0],
                                'live_rectangles': len(live), 'live_rectangle_tiles': sum(e['tiles'] for e in live),
                                'entries': entries},
        'fixed_events': events, 'functions': functions,
        'limits': 'Function calls are nested and must not be added as disjoint costs. Live rectangle tile count is the sum before output compaction. RAM capture tick is supplied by the invoking profile command.'
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'output': str(args.output), 'checks': checks, 'mismatches': len(mismatches),
                      'live_rectangles': len(live), 'live_tiles': sum(e['tiles'] for e in live)}))


if __name__ == '__main__':
    main()
