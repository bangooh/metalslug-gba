#!/usr/bin/env python3
"""Render the missing soldier knife sound offline from original Z80/V ROMs.
No extractor/emulator is linked into the GBA program. Existing bytes preserved.
"""
import argparse,hashlib,json,re,subprocess,zipfile
from pathlib import Path
import numpy as np
from scipy.signal import resample_poly
R=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--audio-tools',type=Path,required=True);ap.add_argument('--rom',type=Path,required=True);a=ap.parse_args()
b=R/'build/actor-audio';b.mkdir(exist_ok=True);g=R/'generated';v=a.audio_tools/'vendor/audio-extract'
with zipfile.ZipFile(a.rom) as z:
 names={Path(n).name.split('.')[0]:n for n in z.namelist()}
 m=z.read(names['201-m1']);samples=z.read(names['201-v1'])+z.read(names['201-v2'])
(b/'M1.bin').write_bytes(m);(b/'V.bin').write_bytes(samples)
subprocess.run(['g++','-O2','-std=c++17','-I'+str(v),str(a.audio_tools/'tools/render_sound_z80.cpp'),*[str(v/f'ymfm/src/ymfm_{n}.cpp')for n in ['opn','ssg','adpcm']],'-o',str(b/'render')],check=True)
subprocess.run([str(b/'render'),str(b/'M1.bin'),str(b/'V.bin'),'10fa','3',str(b/'10fa.raw')],check=True)
raw=(b/'10fa.raw').read_bytes();x=np.frombuffer(raw,dtype='<i2').reshape(-1,2).mean(axis=1)
x=resample_poly(x,2304,15625);nz=np.flatnonzero(abs(x)>32);assert len(nz)
x=x[max(0,int(nz[0])-8):min(len(x),int(nz[-1])+80)]
data=np.clip(np.rint(x/256),-128,127).astype(np.int8).tobytes()
header=(g/'audio_data.h').read_text();records=[list(map(int,s.split(',')))for s in re.findall(r'^\{([^}]+)\}',header,re.M)]
effects=bytearray((g/'audio-effects.bin').read_bytes());existing=[r for r in records if r[0]==0x10fa]
if existing:
 r=existing[0];assert r[1]==0 and effects[r[3]:r[3]+r[4]]==data,'existing command differs'
else:
 records.append([0x10fa,0,1,len(effects),len(data),0xffffffff,0,0,0]);effects.extend(data)
 (g/'audio-effects.bin').write_bytes(effects)
 pre=header.split('#define AUDIO_ASSET_COUNT')[0]
 (g/'audio_data.h').write_text(pre+f'#define AUDIO_ASSET_COUNT {len(records)}\nstatic const struct AudioAsset audio_assets[]={{\n'+''.join('{'+','.join(map(str,r))+'},\n'for r in sorted(records))+'};\n#endif\n')
report=dict(command='10fa',samples=len(data),sample_rate=8192,format='pcm8',source='Original supplied M1/V; standalone pinned Z80/YM2610 offline',
 m1_sha256=hashlib.sha256(m).hexdigest(),v_sha256=hashlib.sha256(samples).hexdigest(),render_sha256=hashlib.sha256(raw).hexdigest(),
 extractor_sha256=hashlib.sha256((a.audio_tools/'tools/render_sound_z80.cpp').read_bytes()).hexdigest(),
 pins={n:json.loads((v/n/'PIN.json').read_text())for n in ['chips','ymfm']})
(g/'actor-audio-source.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
