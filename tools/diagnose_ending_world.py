#!/usr/bin/env python3
"""Real native ending logic with accepting transport sinks; not gameplay QA."""
import concurrent.futures
import hashlib
import json
from pathlib import Path
import subprocess
from compile_port import port_units

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/ending-host'


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    units = port_units()
    units = units[:units.index('src/native_frontend.c')]
    excluded = ('native_', 'object_', 'background_', 'fixed_', 'video_palette',
                'palette_video', 'tile_video', 'source_drawing', 'scaled_drawing',
                'drawing.c', 'source_input', 'session_system', 'gba_')
    units = [u for u in units if u.endswith('.c') and
             not Path(u).name.startswith(excluded)]
    units += ['src/mission_tables.S', 'tests/ending_world_diagnostic.c']
    # Host pointer widths differ; cartridge struct budgets are checked by
    # build_native.py's ARM compiler. This diagnostic does not map GBA RAM.
    flags = ['-O1', '-g', '-fno-strict-aliasing', '-ffunction-sections',
             '-fdata-sections', '-D_Static_assert(a,b)=',
             '-Wall', '-Wextra', '-Werror', '-Wno-misleading-indentation',
             '-Wno-attributes']

    def compile_unit(unit):
        obj = OUT / (unit.replace('/', '_') + '.o')
        command = ['cc', *flags, '-c', unit, '-o', str(obj)]
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(f'{unit}:\n{result.stderr}')
        return str(obj)

    with concurrent.futures.ThreadPoolExecutor(6) as executor:
        objects = list(executor.map(compile_unit, units))
    executable = OUT / 'ending-world'
    subprocess.run(['cc', *objects, '-Wl,--gc-sections,-z,noexecstack',
                    '-o', str(executable)], cwd=ROOT, check=True)
    traces = []
    origins = [(0, 0), (32000, -384), (-32000, -384), (0, -32700)]
    for x, y in origins:
        run = subprocess.run([str(executable), str(x), str(y)], cwd=ROOT,
                             check=True, capture_output=True)
        (OUT / f'fixed-origin-{x}-{y}.csv').write_bytes(run.stdout)
        traces.append(run.stdout)
    assert all(trace == traces[0] for trace in traces)
    # Rebuild only the bridge with the previous child-owned event root.
    # The same functional assertion must reject the missing later pages,
    # rather than merely checking how the new structure is represented.
    old_bridge = (ROOT / 'src/play_world.c').read_text()
    new_call = 'return mission_event_begin(&w->weapons,w->hits.enemy_group,source);'
    old_call = 'return mission_event_spawn(&w->weapons,w->hits.enemy_group,source,0)!=0;'
    assert old_bridge.count(new_call) == 1
    old_source = OUT / 'old-child-root.c'
    old_source.write_text(old_bridge.replace(new_call, old_call))
    old_obj = OUT / 'old-child-root.o'
    subprocess.run(['cc', *flags, '-I', str(ROOT / 'src'), '-c', str(old_source),
                    '-o', str(old_obj)], cwd=ROOT, check=True)
    old_objects = [str(old_obj) if Path(o).name == 'src_play_world.c.o' else o
                   for o in objects]
    old_executable = OUT / 'old-child-root'
    subprocess.run(['cc', *old_objects, '-Wl,--gc-sections,-z,noexecstack',
                    '-o', str(old_executable)], cwd=ROOT, check=True)
    old_run = subprocess.run([str(old_executable), '0', '0'], cwd=ROOT,
                             capture_output=True)
    (OUT / 'old-child-root.csv').write_bytes(old_run.stdout)
    assert old_run.returncode != 0
    assert b'credit_mask=1025,late_credits=0,late_corpses=0' in old_run.stdout
    report = dict(passed=True, origins=origins,
                  identical_screen_draw_traces=True,
                  trace_sha256=hashlib.sha256(traces[0]).hexdigest(),
                  summary=traces[0].decode().splitlines()[-1],
                  previous_child_root_rejected=True,
                  previous_child_root_summary=old_run.stdout.decode().splitlines()[-1],
                  scope='Native scene11 driver, events and actor handlers; all11 credit/FIN pages, post0x1000 corpses/credits, repeated child clears, explicit root stop/restart. No hardware rendering, input-only playthrough or Neo Geo visual-equivalence claim.')
    (OUT / 'ending-logic.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
