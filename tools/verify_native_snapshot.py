#!/usr/bin/env python3
"""Read-only committed-frame capture and comparison with the099 renderer."""
import argparse, os, struct, subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--cc', required=True, help='ARM compiler')
    p.add_argument('--mgba-prefix', required=True, type=Path)
    p.add_argument('--rom', required=True, type=Path)
    p.add_argument('--elf', default=ROOT/'build/native/native.elf', type=Path)
    p.add_argument('--inputs', required=True, type=Path)
    p.add_argument('--probe', default=ROOT/'build/native/probe.txt', type=Path)
    p.add_argument('--frame', type=int, required=True)
    p.add_argument('--output', required=True, type=Path)
    a = p.parse_args()
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    lib = a.mgba_prefix/'lib/x86_64-linux-gnu'
    subprocess.run(['cc', '-O2', '-I'+str(a.mgba_prefix/'include'), 'tests/native_snapshot.c',
                    '-L'+str(lib), '-Wl,-rpath-link,'+str(lib), '-lmgba', '-o', str(out/'capture')], cwd=ROOT, check=True)
    env = dict(os.environ, LD_LIBRARY_PATH=str(lib))
    with (out/'capture.log').open('w') as log:
        subprocess.run([str(out/'capture'), str(a.rom.resolve()), str(a.probe.resolve()),
                        str(a.inputs.resolve()), str(a.frame), str(out)], env=env, stdout=log, stderr=log, check=True)
    fields = [('world','video.views','NativePlayWorld'), ('world','video.pools','NativePlayWorld'),
              ('video','tiles.planes','NativeVideo'), ('video','tiles.columns','NativeVideo'),
              ('video','tiles.fixed','NativeVideo'), ('video','palettes.slots','NativeVideo'),
              ('video','backgrounds','NativeVideo'), ('video','colors','NativeVideo'),
              ('video','auto_frame','NativeVideo'), ('video','auto_disabled','NativeVideo'),
              ('video','fixed','NativeVideo'),('video','hud_active','NativeVideo')]
    source = '#include "play_world.h"\n#include "native_video.h"\n#include <stddef.h>\nconst unsigned a[]={'
    source += ','.join(f'offsetof(struct {t},{f}),sizeof(((struct {t}*)0)->{f})' for s,f,t in fields)+'};'
    (out/'offsets.c').write_text(source)
    subprocess.run([a.cc, '-mcpu=arm7tdmi', '-marm', '-Isrc', '-c', str(out/'offsets.c'), '-o', str(out/'offsets.o')], cwd=ROOT, check=True)
    subprocess.run([a.cc.replace('gcc','objcopy'), '-O', 'binary', '-j', '.rodata', str(out/'offsets.o'), str(out/'offsets.bin')], check=True)
    symbols = subprocess.check_output([a.cc.replace('gcc','nm'), '-n', str(a.elf)], text=True)
    addresses = {line.split()[-1]:int(line.split()[0],16) for line in symbols.splitlines() if len(line.split())>=3}
    offsets = struct.unpack('<24I',(out/'offsets.bin').read_bytes())
    ram = (out/'ewram.bin').read_bytes()
    pieces = []
    for i,(symbol,field,type_name) in enumerate(fields):
        at = addresses[symbol]-0x02000000+offsets[2*i]
        pieces.append(ram[at:at+offsets[2*i+1]])
    (out/'fixture.bin').write_bytes(b''.join(pieces[:10]))
    (out/'fixed.bin').write_bytes(pieces[10])
    (out/'hud-active.bin').write_bytes(pieces[11])
    units = ['tests/native_snapshot_equivalence.c', 'src/background_video.c', 'tests/reference/background_video_099.c',
             'src/background_tile.c', 'src/video_palette.c', 'src/fixed_object.c', 'src/object_video.c',
             'generated/object-masks.c', 'src/tile_video_assets.S']
    subprocess.run(['cc', '-O2', '-Wno-attributes', '-ffunction-sections', '-fdata-sections', '-Isrc', *units,
                    '-Wl,--gc-sections,-z,noexecstack', '-o', str(out/'compare')], cwd=ROOT, check=True)
    result = subprocess.check_output([str(out/'compare'), str(out)], text=True)
    (out/'comparison.txt').write_text(result)
    print((out/'capture.log').read_text()+result)

if __name__ == '__main__':
    main()
