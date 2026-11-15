#!/usr/bin/env python3
"""Extract the source palette interpolation LUT, without executing gameplay."""
import argparse,hashlib,json,struct
from pathlib import Path
R=Path(__file__).resolve().parents[1]
SHA='1b8eee0891b6f8c837348e60ea2923ae229e0fb603d34ae62ae06131332f0d3d'
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args()
 data=(a.source/'build/mslug_prom.bin').read_bytes()
 if hashlib.sha256(data).hexdigest()!=SHA:raise ValueError('PROM fingerprint')
 rows=[]
 for i in range(32):
  at=struct.unpack_from('>I',data,0x12f30+i*4)[0];row=data[at:at+32]
  if len(row)!=32 or any(v>31 for v in row):raise ValueError('RGB ramp')
  rows.append(list(row))
 g=R/'generated'
 (g/'render-data.h').write_text('#ifndef NATIVE_RENDER_DATA_H\n#define NATIVE_RENDER_DATA_H\n#include <stdint.h>\nextern const uint8_t native_color_ramp[32][32];\n#endif\n')
 (g/'render-data.c').write_text('#include "render-data.h"\nconst uint8_t native_color_ramp[32][32]={'+','.join('{'+','.join(map(str,row))+'}' for row in rows)+'};\n')
 report=dict(prom_sha256=SHA,palette_ramp=dict(pointer_table=0x12f30,rows=32,columns=32,sha256=hashlib.sha256(bytes(sum(rows,[]))).hexdigest()),
  source_entries=['2B58 highbank palette load','2C30 target interpolation','13408 period/progress/immediateFFFF','13480/13504 perRGB delta lookup','13624/13694 darken/lighten','2C86 bit3 skipfade and0..C3 plusFF'],
  gba_affine_reference='https://gbadev.net/tonc/affobj.html',gameplay_executed=False)
 (g/'render-source.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
if __name__=='__main__':main()
