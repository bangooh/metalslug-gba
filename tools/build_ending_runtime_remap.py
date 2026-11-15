"""Ending town: remap original fragment indices into existing ending palette banks."""
from pathlib import Path
import sys,re,json
import numpy as np
from compile_m1_native_maps import body,numbers
R=Path(__file__).resolve().parents[1];G=R/'generated';P=G/'native-background'
pal=np.array(numbers(body((G/'ending-background-palette.inc').read_text(),'ending_background_palette')),np.uint16).reshape(14,12,16)[4]
orig=np.array(numbers(body((G/'drawing-data.c').read_text(),'native_source_palettes')),np.uint16).reshape(-1,16)
profiles=np.fromfile(P/'profiles.u16','<u2').reshape(14,256)
remap=np.zeros((77,12,16),np.uint8);cost=np.zeros((77,12),np.uint16)
def rgb(c):return np.stack([c&31,(c>>5)&31,(c>>10)&31],-1).astype('i4')
for slot in range(77):
 n=int(profiles[4,slot])
 if n==65535:continue
 a=rgb(orig[n]);b=rgb(pal[:,1:]);d=((a[None,:,None,:]-b[:,None,:,:])**2).sum(-1)
 remap[slot,:,1:]=np.argmin(d[:,1:,:],axis=2)+1
 cost[slot]=np.minimum(d[:,1:,:].min(-1).sum(-1),65535)
packed=remap[:,:,::2]|(remap[:,:,1::2]<<4)
def arr(name,a,typ):
 dims=''.join('['+str(n)+']'for n in a.shape)
 def fmt(v):return str(int(v)) if np.ndim(v)==0 else '{'+','.join(fmt(x)for x in v)+'}'
 return 'static const '+typ+' '+name+dims+'='+fmt(a)+';\n'
(G/'ending-runtime-remap.inc').write_text(arr('ending_town_remap',packed,'uint8_t')+arr('ending_town_cost',cost,'uint16_t'))
print('table bytes',packed.nbytes+cost.nbytes)
