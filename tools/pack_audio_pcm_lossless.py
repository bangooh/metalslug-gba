"""Share exact PCM duplicates and zero tail/head overlaps; no sample changes."""
from pathlib import Path
import json,re,hashlib
r=Path(__file__).resolve().parents[1]/'generated'
a=json.loads((r/'native-audio-source.json').read_text())['assets']
h=r/'native-audio-pcm.h';text=h.read_text();offsets=list(map(int,re.findall(r'(\d+)u',text)));b=(r/'native-audio-pcm.s8').read_bytes();pool=bytearray();new=[]
for asset,off in zip(a,offsets):
 if off==0xffffffff:new.append(off);continue
 chunk=b[off:off+asset['samples']];assert len(chunk)==asset['samples'];at=pool.find(chunk)
 if at<0:
  overlap=min(len(pool)-len(pool.rstrip(b'\0')),len(chunk)-len(chunk.lstrip(b'\0')));at=len(pool)-overlap;pool.extend(chunk[overlap:])
 new.append(at);assert pool[at:at+len(chunk)]==chunk
it=iter(new);h.write_text(re.sub(r'\d+u',lambda m:str(next(it))+'u',text));(r/'native-audio-pcm.s8').write_bytes(pool)
f=r/'native-audio-runtime.h';f.write_text(re.sub(r'#define NATIVE_AUDIO_PCM_BYTES \d+u',f'#define NATIVE_AUDIO_PCM_BYTES {len(pool)}u',f.read_text()))
f=r/'native-audio-pcm.json';report=json.loads(f.read_text());report.update(bytes=len(pool),sha256=hashlib.sha256(pool).hexdigest(),lossless_shared_bytes=len(b)-len(pool));f.write_text(json.dumps(report,indent=2)+'\n')
print('PCM bytes saved without changing any sample:',len(b)-len(pool))
