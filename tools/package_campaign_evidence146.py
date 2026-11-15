#!/usr/bin/env python3
"""Package frozen v146 campaign evidence, without ROM/ELF or bulk screenshots."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', choices=['B', 'C', 'D', 'E'], required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    label = args.candidate
    base = Path('build/work146-profile')
    doc = Path('docs/work146')
    run = base / ('run-candidate' + label)
    candidate = base / ('candidate' + label)
    report = json.loads((doc / ('candidate' + label + '146.json')).read_text())
    files = []
    for name in ['inputs.csv', 'commits.csv', 'execution.json', 'iwram.bin']:
        files.append((run / name, 'run/' + name))
    for name in ['probe.txt', 'symbols.txt', 'build.json', 'provenance.json']:
        files.append((candidate / name, 'candidate/' + name))
    files += [(base / ('candidate' + label + '.log'), 'trace.log'),
              (base / ('candidate' + label + '.stderr.log'), 'stderr.log')]
    for name in ['baseline145.json', 'candidateA146.json', 'candidateB146.json',
                 'candidateC146.json', 'candidateD146.json', 'candidateE146.json', 'active-span-comparison.json',
                 'pressure-summary.json', 'source-event-comparison.json',
                 'selection.json', 'candidateAB-analysis.md', 'profiling-method.md']:
        if (doc / name).exists():
            files.append((doc / name, 'reports/' + name))
    for name in ['native_campaign_followthrough.c', 'native_cycle_profile145.c',
                 'native_cycle_profile146.c']:
        files.append((Path('tests') / name, 'tests/' + name))
    for name in ['measure_gameplay145.py', 'summarize_profile146.py', 'package_campaign_evidence146.py']:
        files.append((Path('tools') / name, 'tools/' + name))
    for folder in ['m5-candidate' + label, 'm6-pressure' + label]:
        for path in sorted((doc / folder).iterdir()):
            if path.suffix in ['.csv', '.json', '.bin']:
                files.append((path, 'profiles/' + folder + '/' + path.name))
    with (run / 'commits.csv').open() as stream:
        commits = list(csv.DictReader(stream))
    states = set()
    for scene, commit in [(4, 57249), (5, 66408)]:
        row = next(r for r in commits if int(r['scene']) == scene and int(r['commits']) == commit and r['demo'] == '0')
        candidates = [p for p in run.glob('scene-' + str(scene) + '-*.state')
                      if int(p.stem.split('-')[-1]) < int(row['lcd']) - 30]
        states.add(max(candidates, key=lambda p: int(p.stem.split('-')[-1])))
    states.add(min((p for p in run.glob('scene-1-*.state')
                    if int(p.stem.split('-')[-1]) > report['return_to_title_lcd']),
                   key=lambda p: int(p.stem.split('-')[-1])))
    files.extend((path, 'states/' + path.name) for path in sorted(states))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    manifest = {}
    with zipfile.ZipFile(args.output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path, name in files:
            data = path.read_bytes()
            archive.writestr(name, data)
            manifest[name] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
        archive.writestr('manifest.json', json.dumps(manifest, indent=2) + '\n')
        archive.writestr('README.txt',
            'Candidate ' + label + ' evidence. The matching ROM is distributed separately; '
            'its SHA-256 is in candidate/provenance.json. Never load these states into another ROM.\n'
            'Primary FPS/tails are reports/candidate' + label + '146.json active_span. '
            'Top-level FPS retains the historical filtered metric only.\n'
            'run/inputs.csv contains physical buttons indexed by the original LCD frame. '
            'The state filename is its original LCD index; replays must retain that input offset.\n'
            'The profiling C sources require mGBA headers/library. The original executed runner '
            'hash, environment, command and source-build fingerprint are preserved in provenance.\n'
            'profiles/ contains read-only same-ROM timing evidence and pre-refresh RAM fixtures. '
            'See reports/profiling-method.md for invocation and measurement limitations.\n')
    print(json.dumps({'output': str(args.output), 'bytes': args.output.stat().st_size,
                      'files': len(files), 'sha256': hashlib.sha256(args.output.read_bytes()).hexdigest()}))


if __name__ == '__main__':
    main()
