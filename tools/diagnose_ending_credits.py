#!/usr/bin/env python3
"""Real native ending logic with accepting transport sinks; not gameplay QA."""
import concurrent.futures
import hashlib
import json
from pathlib import Path
import subprocess
from compile_port import port_units

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/ending-credits-host'


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    units = port_units()
    units = units[:units.index('src/native_frontend.c')]
    excluded = ('native_', 'object_', 'background_', 'fixed_', 'video_palette',
                'palette_video', 'tile_video', 'source_drawing', 'scaled_drawing',
                'drawing.c', 'source_input', 'session_system', 'gba_')
    units = [u for u in units if u.endswith('.c') and
             not Path(u).name.startswith(excluded)]
    units += ['src/mission_tables.S', 'tests/ending_credits_diagnostic.c']
    # Host pointer widths differ; cartridge struct budgets are checked by
    # build_native.py's ARM compiler. This diagnostic does not map GBA RAM.
    flags = ['-O1', '-g', '-fno-strict-aliasing', '-ffunction-sections',
             '-fdata-sections', '-D_Static_assert(a,b)=',
             '-Wall', '-Wextra', '-Werror', '-Wno-misleading-indentation',
             '-Wno-attributes']

    def compile_unit(unit):
        obj = OUT / (unit.replace('/', '_') + '.o')
        if obj.exists() and obj.stat().st_mtime>(ROOT/unit).stat().st_mtime:return str(obj)
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
    reports=[]
    for branch in [0]:
        run=subprocess.run([str(executable), '0', '0', str(branch)],cwd=ROOT,capture_output=True)
        (OUT/f'branch-{branch}.log').write_bytes(run.stdout+run.stderr)
        if run.returncode:raise RuntimeError(run.stdout.decode()[-2500:]+run.stderr.decode())
        reports.append(dict(branch=branch,trace_sha256=hashlib.sha256(run.stdout).hexdigest(),
                            summary=[s for s in run.stdout.decode().splitlines() if s.startswith(('summary,','credit,'))]))
    report=dict(passed=True,branches=reports,unverified='C/D standalone host fixtures stop at tick771 before credits (world11/weapons30/entities32); not a full session path. These branches are not claimed verified.',scope='Actual native ending B logic with accepting video/audio transports; 14 ordered credit cards, page duration, original FIN fade/unlock. No emulator rendering or input-only gameplay claim.')
    (OUT/'ending-credits-logic.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
