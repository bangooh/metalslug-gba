"""Compile captured AES BIOS pixels to 30 Hz indexed tile updates.
Neutral logo edge palettes receive an offline contrast adjustment.
Requires numpy. Source capture is in art/neogeo-boot/bios-replay.npz.
"""
from pathlib import Path
import numpy as np,zlib,struct,json
from presentation_logo_repairs import neogeo_palettes
r=Path(__file__).resolve().parents[1];g=r/'generated';z=np.load(r/'art/neogeo-boot/bios-replay.npz');fs=z['indices'];ps=neogeo_palettes(z['palettes']);colors=np.unique(fs);assert len(colors)<=16
atlas=[bytes(32)];lookup={atlas[0]:0};prev=np.zeros(600,dtype='<u2');events=[]
for fi,f in enumerate(fs):
 ids=np.searchsorted(colors,f).astype('u1');tm=[]
 for y in range(20):
  for x in range(30):
   a=ids[y*8:y*8+8,x*8:x*8+8];raw=(a[:,::2]|a[:,1::2]<<4).tobytes()
   if raw not in lookup:lookup[raw]=len(atlas);atlas.append(raw)
   tm.append(lookup[raw])
 tm=np.array(tm,dtype='<u2');changed=np.where(tm!=prev)[0]
 if len(changed):events.append((fi*2,len(changed),[(int(i),int(tm[i])) for i in changed]))
 prev=tm
raw=b''.join(struct.pack('<HH',frame,count)+b''.join(struct.pack('<HH',*pair) for pair in pairs) for frame,count,pairs in events)
pal=np.zeros((len(fs),16),dtype='<u2');pal[:,:len(colors)]=ps[:,colors]
blocks=[('tiles',b''.join(atlas)),('changes',raw),('palettes',pal.tobytes())]
h='#ifndef NEOGEO_BOOT_H\n#define NEOGEO_BOOT_H\n#include <stdint.h>\n'
for name,data in blocks:
 packed=zlib.compress(data,9,wbits=-15);(g/f'neogeo-{name}.deflate').write_bytes(packed)
 h+=f'extern const uint8_t neogeo_{name}_packed[];\nenum {{NEOGEO_{name.upper()}_BYTES={len(data)}}};\n'
h+=f'enum {{NEOGEO_EVENTS={len(events)},NEOGEO_FRAMES={len(fs)},NEOGEO_DURATION=424}};\n#endif\n';(g/'neogeo-boot.h').write_text(h)
print('BIOS animation generated:',len(fs),'frames,',len(events),'tile updates')
