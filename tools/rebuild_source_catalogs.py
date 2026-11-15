#!/usr/bin/env python3
"""Stage user ROMs and regenerate the pinned 68000 source catalogs."""
import argparse
import hashlib
import json
import subprocess
import sys
import zipfile
from pathlib import Path
from import_drawing_data import MD5, PROM_SHA

ROOT = Path(__file__).resolve().parents[1]
COMMIT = '574a517547f05bc8c1539fab1b7d566035543496'

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,required=True)
    p.add_argument('--rom',type=Path,required=True)
    a=p.parse_args();source=a.source.resolve()
    commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()
    if commit!=COMMIT:
        raise ValueError('Use the pinned upstream commit '+COMMIT)
    rom=source/'rom';rom.mkdir(exist_ok=True)
    build=source/'build';build.mkdir(exist_ok=True)
    with zipfile.ZipFile(a.rom) as z:
        names={Path(n).name:n for n in z.namelist() if not n.endswith('/')}
        for name in ['201-p1.bin','201-m1.bin','201-v1.bin','201-v2.bin',*MD5]:
            data=z.read(names[name])
            if name in MD5 and hashlib.md5(data).hexdigest()!=MD5[name]:
                raise ValueError('Unexpected original ROM member '+name)
            (rom/name).write_bytes(data)
    raw=(rom/'201-p1.bin').read_bytes()
    if len(raw)!=0x200000:
        raise ValueError('Expected 2 MiB P-ROM')
    swapped=bytearray(raw);swapped[::2],swapped[1::2]=raw[1::2],raw[::2]
    normalized=swapped[0x100000:]+swapped[:0x100000]
    if hashlib.sha256(normalized).hexdigest()!=PROM_SHA:
        raise ValueError('Unexpected normalized P-ROM fingerprint')
    (build/'mslug_prom.bin').write_bytes(normalized)
    subprocess.run([sys.executable,str(source/'assets/tools/levels.py')],cwd=source,check=True)
    (ROOT/'build').mkdir(exist_ok=True);(ROOT/'generated').mkdir(exist_ok=True)
    names=json.loads((ROOT/'audit/source-importers.json').read_text())
    # Explicit prerequisites: alphabetical execution only works with stale
    # generated catalogs present. Runtime bindings consume the other ledgers.
    first=['index_source.py','import_player_arm.py','import_player_programs.py']
    # These two converters belong to the retired flattened-map prototype.
    # Neither output is linked by compile_port.port_units().
    obsolete={'import_actors.py','import_m1_world.py'}
    names=first+[n for n in names if n not in first and n not in obsolete and n!='import_runtime_data.py']+['import_runtime_data.py']
    for name in names:
        subprocess.run([sys.executable,str(ROOT/'tools'/name),'--source',str(source)],cwd=ROOT,check=True)
    print(f'Regenerated {len(names)} source importers from pinned ROM/code.')

if __name__=='__main__':main()
