#!/usr/bin/env python3
"""Predecode short SFX and selected music to rounded signed8 PCM.

The GBA output is signed8. Reducing the existing signed16 SFX cache frees
enough ROM to remove the music IMA decoder in the expensive late missions.
Samples, loops and reply timestamps remain at the source catalog's rate.
"""
import hashlib,json,re
from pathlib import Path
R=Path(__file__).resolve().parents[1]
PCM_MUSIC={0x25,0x26,0x28}
def main():
 catalog=json.loads((R/'generated/native-audio-source.json').read_text());assets=catalog['assets'];rate=catalog['sample_rate']
 source=(R/'generated/native-audio.ima').read_bytes()
 text=(R/'generated/audio_ima_fast.h').read_text();table=[int(n) for n in re.findall(r'\d+',text.split('{',1)[1].split('}',1)[0])];assert len(table)==1424
 data=bytearray();offsets=[];selected=[]
 for a in assets:
  if not ((a['music'] and a['command'] in PCM_MUSIC) or
          (not a['music'] and a['samples']<=rate*8 and a['loop']==0xffffffff)):
   offsets.append(0xffffffff);continue
  offsets.append(len(data));predictor=index=0
  for sample in range(a['samples']):
   value=source[a['offset']+sample//2];n=value&15 if sample&1 else value>>4
   packed=table[index*16+n];delta=packed&0x3ffff
   if delta&0x20000:delta-=0x40000
   predictor=max(-32768,min(32767,predictor+delta));index=packed>>18
   # Nearest signed8 level; retain the signed saturation asymmetry of
   # the hardware. Never resample: event/loop positions stay unchanged.
   data.append(max(-128,min(127,(predictor+128)//256))&255)
  selected.append(a['command'])
 # Runtime keeps IMA only where no PCM copy exists. Original catalog
 # and original IMA remain offline/reference inputs, never rewritten.
 runtime=bytearray();runtime_offsets=[];removed=0
 for a,pcm_offset in zip(assets,offsets):
  size=(a['samples']+1)//2
  if pcm_offset!=0xffffffff:runtime_offsets.append(0xffffffff);removed+=size;continue
  runtime_offsets.append(len(runtime));runtime+=source[a['offset']:a['offset']+size]
 for a,offset in zip(assets,runtime_offsets):
  if offset!=0xffffffff:assert runtime[offset:offset+(a['samples']+1)//2]==source[a['offset']:a['offset']+(a['samples']+1)//2]
 (R/'generated/native-audio-runtime.ima').write_bytes(runtime)
 (R/'generated/native-audio-runtime.h').write_text('#ifndef NATIVE_AUDIO_RUNTIME_H\n#define NATIVE_AUDIO_RUNTIME_H\n#include <stdint.h>\n#define NATIVE_AUDIO_RUNTIME_BYTES '+str(len(runtime))+'u\n#define NATIVE_AUDIO_PCM_BYTES '+str(len(data))+'u\nextern const uint8_t native_audio_runtime_data[];\nstatic const uint32_t native_audio_runtime_offsets[192]={'+','.join(str(x)+'u'for x in runtime_offsets)+'};\n#endif\n')
 (R/'generated/native-audio-runtime.json').write_text(json.dumps(dict(bytes=len(runtime),removed_duplicate_ima_bytes=removed,original_bytes=len(source),original_sha256=hashlib.sha256(source).hexdigest(),runtime_sha256=hashlib.sha256(runtime).hexdigest(),offsets=runtime_offsets),indent=2)+'\n')
 out=R/'generated/native-audio-pcm.s8';out.write_bytes(data)
 # This obsolete artifact is not linked after the signed8 migration.
 (R/'generated/native-audio-pcm.s16le').unlink(missing_ok=True)
 (R/'generated/native-audio-pcm.h').write_text('#ifndef NATIVE_AUDIO_PCM_H\n#define NATIVE_AUDIO_PCM_H\n#include <stdint.h>\nextern const int8_t native_audio_pcm[];\nstatic const uint32_t native_audio_pcm_offsets[192]={'+','.join(str(x)+'u' for x in offsets)+'};\n#endif\n')
 report=dict(assets=len(selected),commands=selected,music_commands=sorted(PCM_MUSIC),bytes=len(data),sample_rate=rate,sha256=hashlib.sha256(data).hexdigest(),source_ima_sha256=hashlib.sha256(source).hexdigest(),format=f'rounded signed8 PCM; non-looping SFX<=8seconds and selected music at{rate}Hz; no rate/event/loop changes')
 (R/'generated/native-audio-pcm.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='commands'}))
if __name__=='__main__':main()
