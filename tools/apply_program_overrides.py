#!/usr/bin/env python3
"""Restore reviewed v191 program deltas after import; rejects unknown inputs."""
import argparse,hashlib,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);a=p.parse_args();root=Path(__file__).resolve().parents[1]
rows=json.loads((root/'audit/v191-program-overrides.json').read_text());pending=[]
for row in rows:
 target=a.source/row['path'];raw=target.read_bytes();h=hashlib.sha256(raw).hexdigest()
 if h==row['after']:continue
 if h!=row['before']:raise SystemExit('Unexpected content: '+row['path'])
 lines=raw.decode().splitlines(keepends=True)
 for e in reversed(row['edits']):lines[e['start']:e['end']]=e['lines']
 out=''.join(lines).encode()
 if hashlib.sha256(out).hexdigest()!=row['after']:raise SystemExit('Output mismatch: '+row['path'])
 pending.append((target,out))
for path,data in pending:path.write_bytes(data)
print(f'Applied {len(pending)} overrides; all {len(rows)} target hashes match v191.')
