#!/usr/bin/env python3
"""Package the frozen M2 candidate and its scoped execution evidence."""
import argparse
import hashlib
import json
import shutil
import zipfile
from pathlib import Path

R = Path(__file__).resolve().parents[1]
B = R / 'build/m2-120'
D = R / 'docs/m2-0.5.120'
EXPECTED = 'e2165d4823dd69b18fe46415672abdd6cc7b9baa1f802d78693203cef8d1eb02'


def sha(p):
    with p.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def clean(p):
    return (p.is_file() and not p.is_symlink()
            and not any(x.startswith('.') or x == '__pycache__'
                        for x in p.relative_to(R).parts)
            and p.suffix not in {'.pyc', '.pyo', '.o', '.gba', '.elf'})


def archive(path, files):
    with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_DEFLATED,
                         compresslevel=6) as z:
        for p in sorted(set(files)):
            assert p.is_file(), p
            z.write(p, 'metal-slug-native/' + p.relative_to(R).as_posix())
    with zipfile.ZipFile(path) as z:
        assert z.testzip() is None
        return len(z.namelist())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--output', type=Path, required=True)
    out = ap.parse_args().output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    rom = R / 'dist/metal-slug-gba-native-v0.5.120-m2.gba'
    assert sha(rom) == sha(B / 'm/rom.gba') == EXPECTED
    raw = rom.read_bytes()
    assert len(raw) == 32711316 and len(raw) <= 32 * 1024 * 1024
    assert raw[0xbd] == (-sum(raw[0xa0:0xbd]) - 0x19) & 255

    evidence = set()
    def add(p):
        assert p.is_file(), p
        evidence.add(p)
    for p in D.rglob('*'):
        if p.is_file() and p.name != 'evidence-index.json':
            add(p)
    for name in ['build.json', 'native.elf', 'probe.txt', 'symbols.txt',
                 'offsets.c', 'offsets.bin']:
        add(B / 'm' / name)
    for name in ['build.json', 'probe.txt']:
        add(B / 'legacy-k' / name)
    for route in ['m-route', 'legacy-k-route']:
        for p in (B / route).iterdir():
            if p.is_file() and p.suffix in {'.json', '.jsonl', '.csv', '.bin', '.log'}:
                add(p)
    for p in (B / 'm-route').glob('commit-*/*'):
        if p.is_file() and p.suffix != '.o':
            add(p)
    manifest = json.loads((D / 'reference-manifest.json').read_text())
    for case in manifest['cases']:
        add(B / case['neogeo']['source_capture'])
        add(B / 'm-route' / f"frame-{case['gba']['source_lcd']:06d}.ppm")
    add(B / 'm-route/frame-116197.ppm')
    for ng in ['neogeo3', 'neogeo5', 'neogeo6', 'neogeo7', 'neogeo8']:
        for p in (B / ng).iterdir():
            if p.is_file() and p.suffix in {'.csv', '.log'}:
                add(p)
        if ng != 'neogeo8':
            add(B / ng / 'm2-latest.sta')
    add(B / 'neogeo3/m2-start.sta')
    for name in ['boss2-position-oracle.json', 'm2-scroll-latch-oracle.json',
                 'runtime-tests-m.log', 'stream-tests-f.log',
                 'camera-lock-test.log', 'm5-retired-test.log', 'build-m.log',
                 'arena-transition-assets.log', 'arena-transition-4bpp.log']:
        add(B / name)
    for name in ['trap.log', 'ewram.bin', 'iwram.bin']:
        add(B / 'h-fault-trap' / name)
    # Include the old M1 commit hashes used in the regression comparison.
    add(B / 'base-route/frame-hashes.csv')
    index = {
        'version': '0.5.120-m2', 'rom_bytes': len(raw), 'rom_sha256': EXPECTED,
        'scope': 'Input-only M1 through M2 boss/results; stops at M3 initialization before its first video commit.',
        'full_reference_verified': False, 'target_30fps_met': False,
        'layout': 'Extract evidence and source archives into the same directory to reproduce relative paths.',
        'excluded': ['Original Neo Geo ROM/PROM', 'Unsuccessful experimental ROMs',
                     'Invalid neogeo4 branch and neogeo5 boot state',
                     'Later attract-mode neogeo8 save', 'Invalid mGBA save states'],
        'files': [{'path': p.relative_to(R).as_posix(), 'bytes': p.stat().st_size,
                   'sha256': sha(p)} for p in sorted(evidence)]
    }
    (D / 'evidence-index.json').write_text(json.dumps(index, indent=2) + '\n')
    add(D / 'evidence-index.json')

    sources = []
    for top in ['art', 'generated', 'm1_native', 'src', 'tests', 'tools', 'docs']:
        sources.extend(p for p in (R / top).rglob('*') if clean(p))
    sources.extend(R / name for name in ['AGENTS.md', 'README-M1.ko.txt',
                   'README-M2.ko.txt', 'README.ko.md', 'README.txt',
                   'VERSION', 'requirements.txt'])
    required = ['src/native_video.c', 'src/m5.c', 'm1_native/runtime.c',
                'generated/m2-assets.c', 'm1_native/m2_assets.S',
                'tools/build_native.py', 'tools/package_m2_120.py']
    assert all(R / name in sources for name in required)
    for p in sources:
        assert p.name not in {'source-prom.bin', 'mslug.zip', 'neogeo.zip'}

    outputs = []
    for p in [rom, D / 'M2-native-v0.5.120.html']:
        dest = out / p.name
        shutil.copyfile(p, dest)
        assert sha(dest) == sha(p)
        outputs.append(dest)
    for name, files in [('metal-slug-gba-v0.5.120-m2-source.zip', sources),
                        ('M2-native-v0.5.120-evidence.zip', evidence)]:
        dest = out / name
        count = archive(dest, files)
        print(json.dumps({'archive': name, 'files': count,
                          'bytes': dest.stat().st_size}), flush=True)
        outputs.append(dest)
    receipt = [{'file_name': p.name, 'path': str(p),
                'bytes': p.stat().st_size, 'sha256': sha(p)} for p in outputs]
    (out / 'm2-120-package-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
