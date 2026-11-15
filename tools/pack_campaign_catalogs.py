"""Lossless row and opacity deduplication; direct lookups, no frame decompression."""
from pathlib import Path
import re,json,argparse
import numpy as np
from compile_m1_native_maps import ROOT as R,G,body,numbers
reports=[]
parser=argparse.ArgumentParser();parser.add_argument('--mission',type=int,choices=(3,4,5,6));args=parser.parse_args()
for m in ([args.mission] if args.mission else range(3,7)):
 P=G/f'm{m}-cartridge';f=G/f'm{m}-assets.c';bak=P/'assets-unpacked.c'
 if not bak.exists() or "/maps.bin" in (R/f"m1_native/m{m}_assets.S").read_text():bak.write_text(f.read_text())
 s=bak.read_text();rs=[numbers(x) for x in re.findall(r'\{([^{}]+)\}',body(s,f'm{m}_images'))];a=np.fromfile(P/'maps.bin','<u2');data=[];rows=[];lookup={};out=[]
 for off,w,h,x,y in rs:
  out.append((len(rows),w,h,x,y))
  for row in a[off:off+w*h].reshape(h,w):
   key=row.tobytes()
   if key not in lookup:lookup[key]=len(data);data.extend(row)
   rows.append(lookup[key])
 np.array(data,'<u2').tofile(P/'maps-packed.bin');np.array(rows,'<u4').tofile(P/'map-rows.bin')
 s=s.replace(body(s,f'm{m}_images'),','.join('{'+','.join(map(str,r))+'}' for r in out));f.write_text(s)
 masks=np.fromfile(P/'opacity.bin',np.uint8).reshape(-1,8);unique,ids=np.unique(masks,axis=0,return_inverse=True);assert len(unique)<65536;assert np.array_equal(unique[ids],masks)
 unique.tofile(P/'opacity-packed.bin');ids.astype('<u2').tofile(P/'opacity-ids.bin')
 h=G/f'm{m}-assets.h';s=h.read_text();extra=f'extern const uint32_t m{m}_map_rows[];\nextern const uint16_t m{m}_opacity_ids[];\n'
 if extra not in s:s=s.replace('#endif',extra+'#endif')
 h.write_text(s)
 asm=R/f'm1_native/m{m}_assets.S';s=asm.read_text().replace('/maps.bin','/maps-packed.bin').replace('/opacity.bin','/opacity-packed.bin')
 for name,filename in [('map_rows','map-rows'),('opacity_ids','opacity-ids')]:
  if f'.global m{m}_{name}\n' not in s:s+=f'.section .rodata.m{m}_{name},"a",%progbits\n.balign 4\n.global m{m}_{name}\nm{m}_{name}:\n.incbin "generated/m{m}-cartridge/{filename}.bin"\n'
 asm.write_text(s)
 reports.append({'mission':m,'saved_bytes':a.nbytes-(len(data)*2+len(rows)*4)+masks.nbytes-unique.nbytes-len(ids)*2,'lossless':True})
report=R/'build/campaign134/catalog-packing.json';report.parent.mkdir(parents=True,exist_ok=True)
print(reports);report.write_text(json.dumps(reports,indent=2))
