#!/usr/bin/env python3
"""Reject missing, duplicate or unsupported ROM/BIOS members before conversion."""
import argparse,hashlib,json,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def check(rom,bios):
 expected=json.loads((ROOT/'assets/input-hashes.json').read_text())
 for kind,path in [('mslug.zip',rom),('neogeo.zip',bios)]:
  with zipfile.ZipFile(path) as z:
   for row in expected[kind]:
    matches=[n for n in z.namelist() if not n.endswith('/') and Path(n).name==row['name']]
    if len(matches)!=1:raise ValueError(f'{kind}: expected one {row["name"]}, found {len(matches)}')
    if kind=='neogeo.zip' and matches[0]!=row['name']:raise ValueError('BIOS members must be at the ZIP root: '+row['name'])
    data=z.read(matches[0])
    if len(data)!=row['bytes'] or hashlib.sha256(data).hexdigest()!=row['sha256']:raise ValueError('Unsupported original member: '+row['name'])
 print('Original game and BIOS inputs verified; no ROM data is distributed.')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--rom',type=Path,required=True);p.add_argument('--bios',type=Path,required=True);a=p.parse_args();check(a.rom,a.bios)
