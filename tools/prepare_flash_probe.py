"""Read-only ARM telemetry for title character selection regression tests."""
import argparse
import struct
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--cc', required=True)
p.add_argument('--elf', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=True)
fields = {
    'world': ('NativePlayWorld', ['ticks', 'fault', 'slug.present', 'weapons.players[0]', 'weapons.players[1]', 'arms[0].index', 'arms[1].index']),
    'session': ('NativeSession', ['config.first_character', 'paused']),
    'link': ('NativeLink', ['state', 'slot', 'agreed', 'control', 'peer_control']),
}
offsets = {
    'shotgun_flash': 'offsetof(struct NativePlayWorld,weapons.shotgun_flash)',
    'boss_flash': 'offsetof(struct NativePlayWorld,enemies.scene_flags[1])',
    'player.slot': 'offsetof(struct PlayerBodyState,character)',
    'player.palette': 'offsetof(struct PlayerBodyState,actor.palette_base)',
    'player.owner': 'offsetof(struct PlayerBodyState,actor.owner)',
    'player.damage_owner': 'offsetof(struct PlayerBodyState,actor.damage_owner)',
    'player.x': 'offsetof(struct PlayerBodyState,actor.transform.x)',
    'player.y': 'offsetof(struct PlayerBodyState,actor.transform.y)',
    'player.drawing': 'offsetof(struct PlayerBodyState,actor.animation.drawing)',
    'entity.base': 'offsetof(struct NativePlayWorld,entities.nodes)',
    'entity.stride': 'sizeof(struct NativeEntity)',
    'arm.palette': 'offsetof(struct NativeEntity,payload.arm.palette_base)',
    'arm.drawing': 'offsetof(struct NativeEntity,payload.arm.animation.drawing)',
}
expr = []
for symbol, (type_name, members) in fields.items():
    for member in members:
        expr += [f'offsetof(struct {type_name},{member})', f'sizeof(((struct {type_name}*)0)->{member})']
expr += list(offsets.values())
src = '#include "native_runtime.h"\n#include "native_link.h"\n#include <stddef.h>\nconst unsigned offsets[]={' + ','.join(expr) + '};\n'
(out/'offsets.c').write_text(src)
subprocess.run([a.cc, '-mcpu=arm7tdmi', '-marm', '-Isrc', '-c', str(out/'offsets.c'), '-o', str(out/'offsets.o')], cwd=root, check=True)
tool = Path(a.cc)
subprocess.run([str(tool.with_name('arm-none-eabi-objcopy')), '-O', 'binary', '-j', '.rodata', str(out/'offsets.o'), str(out/'offsets.bin')], check=True)
values = iter(struct.unpack('<' + str(len(expr)) + 'I', (out/'offsets.bin').read_bytes()))
nm = subprocess.check_output([str(tool.with_name('arm-none-eabi-nm')), str(a.elf.resolve())], text=True)
symbols = {row.split()[-1]: int(row.split()[0], 16) for row in nm.splitlines() if len(row.split()) >= 3}
rows = []
for symbol, (_, members) in fields.items():
    for member in members:
        rows.append(f'{symbols[symbol]+next(values):x} {next(values)} {symbol}.{member}')
for name in offsets:
    rows.append(f'{next(values):x} 0 offset.{name}')
rows.append(f'{symbols["world"]:x} 0 base.world')
for symbol in ('playing', 'presentation', 'demo', 'screen', 'frozen', 'linked', 'cinematic', 'cinematic_started', 'vblank_count', 'shown'):
    rows.append(f'{symbols[symbol]:x} 4 {symbol}')
rows.append(f'{symbols["native_app_status"]+8:x} 4 app.error')
(out/'probe.txt').write_text('\n'.join(rows)+'\n')
print('Character telemetry fields:', len(rows))
