#!/usr/bin/env python3
"""All authored sounds -> GBA mono IMA + source replies/loops, offline only.

Uses the pinned upstream standalone sound renderer as an asset extractor, not
a gameplay test. No CPU/chip emulator is shipped in the native cartridge.
Capture is never capped and then labelled complete: absence of natural end or
two observed loop boundaries fails the extraction. Existing old ROM assets
stay untouched. Low-rate premixed voices are a GBA adaptation, not YM emulation.
"""
import argparse, concurrent.futures, hashlib, importlib, json, math, re, subprocess, sys, wave, zipfile
from pathlib import Path
import numpy as np
from scipy.signal import resample_poly
R=Path(__file__).resolve().parents[1]
DEFAULT_RATE=6000
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def renderer(up,b):
 src=up/'mrom/tools/sndrender';s=(src/'sndrender.cpp').read_text()
 def change(old,new):
  nonlocal s
  if s.count(old)!=1:raise ValueError('Pinned renderer structure changed: '+old[:80])
  s=s.replace(old,new)
 change('uint8_t latch = 0;', 'uint8_t latch = 0;\n    uint64_t capture_sample=0;\n    FILE*capture=nullptr;')
 change('default: break;   // 0x0C = reply to the 68000 (ignored)',
        'case 0x0C: if(capture) fprintf(capture,"R %llu %u\\n",(unsigned long long)capture_sample,v); break;\n        default: break;')
 change('const char *outp = argv[4];','const char *outp = argv[4];\n    std::string cp=std::string(outp)+".events";board.capture=fopen(cp.c_str(),"w");if(!board.capture)return 2;')
 change('int loop_count[14] = {0};','int loop_count[22] = {0};')
 change('int t = (int(board.cpu.iy) - 0xFBFC) / 0x23;',
        'int base=board.cpu.iy>=0xFDF2?0xFDF2:0xFBFC;\n                int t = (int(board.cpu.iy) - base) / 0x23+(base==0xFDF2?14:0);')
 change('ran = s + 1;', 'ran = s + 1;board.capture_sample=s;')
 change('board.latch = cmds[next_cmd++];','board.latch = cmds[next_cmd++];\n            fprintf(board.capture,"C %llu %u\\n",(unsigned long long)s,board.latch);')
 change('if (board.cpu.iy >= 0xFBFC && t < 14 && (board.cpu.iy - 0xFBFC) % 0x23 == 0) {',
        'if (board.cpu.iy >= 0xFBFC && t < 22 && (board.cpu.iy - base) % 0x23 == 0) {')
 change('if (loop_count[t]++ == 0 && ++looped == loop_tracks) last_track = t;',
        'fprintf(board.capture,"L %llu %d\\n",(unsigned long long)s,t);\n                    if (loop_count[t]++ == 0 && ++looped == loop_tracks) last_track = t;')
 change('fade_s = s;                       // pass N complete: fade out from here',
        'fade_s = s;\n                        fprintf(board.capture,"E %llu %d\\n",(unsigned long long)s,t);')
 change('if (++silent > sr) break;', 'if (++silent > sr) {fprintf(board.capture,"S %llu 0\\n",(unsigned long long)s);break;}')
 change('fclose(f);\n    if (vgm_path)', 'fclose(f);fprintf(board.capture,"W %llu %llu\\n",(unsigned long long)warm,(unsigned long long)written);fclose(board.capture);\n    if (vgm_path)')
 (b/'capture.cpp').write_text(s)
 subprocess.run(['gcc','-O2','-c',str(src/'third_party/z80/z80.c'),'-o',str(b/'z80.o')],check=True)
 subprocess.run(['g++','-O2','-std=c++14','-I'+str(src/'third_party/ymfm'),'-I'+str(src/'third_party/z80'),str(b/'capture.cpp'),*[str(src/'third_party/ymfm'/('ymfm_'+n+'.cpp'))for n in ('opn','adpcm','ssg')],str(b/'z80.o'),'-o',str(b/'capture')],check=True)
 subprocess.run(['g++','-O2','-std=c++14',str(R/'tools/encode_native_audio.cpp'),'-o',str(b/'encode')],check=True)
 for name in ['capture','encode']:(b/name).chmod(0o700)
 return {'upstream_renderer':sha(src/'sndrender.cpp'),'capture_renderer':sha(b/'capture.cpp'),
         'dependencies':{str(p.relative_to(src)):sha(p)for p in sorted((src/'third_party').rglob('*'))if p.is_file()}}
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--upstream',type=Path,required=True);ap.add_argument('--rom',type=Path,required=True);ap.add_argument('--workers',type=int,default=4)
 ap.add_argument('--sample-rate',type=int,default=DEFAULT_RATE);a=ap.parse_args()
 if not 4000<=a.sample_rate<=16384:ap.error('sample rate must be 4000..16384 Hz')
 # Pinned source YM output clock is exactly 8MHz / 144. Its WAV header
 # rounds this rate; use the rational clock for audio AND every timestamp.
 divisor=math.gcd(a.sample_rate*144,8000000)
 up=a.sample_rate*144//divisor;down=8000000//divisor
 b=R/'build/full-audio';b.mkdir(parents=True,exist_ok=True);g=R/'generated'
 with zipfile.ZipFile(a.rom)as z:
  names={Path(n).name.split('.')[0]:n for n in z.namelist()}
  for n in ['201-m1','201-v1','201-v2']:(b/(n+'.bin')).write_bytes(z.read(names[n]))
 m=(b/'201-m1.bin').read_bytes();assert hashlib.md5(m).hexdigest()=='6449db0459bcf7584348792009e8f12c'
 sys.path.insert(0,str(a.upstream/'mrom/tools'));gd=importlib.import_module('gen_sound_data');mi=importlib.import_module('export_midi')
 pins=renderer(a.upstream,b);jobs=[]
 for typ,base,count in [(0,gd.MUSIC_TABLE,gd.MUSIC_N),(16,gd.SFXA_TABLE,gd.SFXA_N),(17,gd.SFXB_TABLE,gd.SFXB_N)]:
  for i in range(count):
   h=gd.p24(m,base+4*i)
   if h:jobs.append((typ<<8|i+32,h,typ==0,[typ,i+32]))
 # Real control sequences, not fabricated STOP/ACK implementations.
 jobs.extend([(2,0x24de,True,[2]),(15,0x24f7,False,[15])])
 def convert(job):
  sid,h,music,cmd=job;tracks=mi.header_tracks(m,h+(1 if music else 9));parsed=[mi.interpret(m,p)for p,_ in tracks]
  looping=sum(any(e[1]=='loop'for e in ev)for ev,_ in parsed)
  # Include enough room for two actual source loop passes even with tempo changes.
  maximum=max(120,math.ceil(max((n for _,n in parsed),default=0)/mi.TIMER_HZ*32+60))
  wav=b/f'{sid:04x}.wav';events=Path(str(wav)+'.events')
  signature=hashlib.sha256((pins['capture_renderer']+str(job)).encode()).hexdigest();stamp=b/f'{sid:04x}.stamp'
  if not(wav.exists()and events.exists()and stamp.exists()and stamp.read_text()==signature):
   opts=['--loop-tracks',str(looping),'--loops','2','--fade-len','0']if looping else['--auto-stop']
   p=subprocess.run([str(b/'capture'),*[str(b/(n+'.bin'))for n in ['201-m1','201-v1','201-v2']],str(wav),'0',*opts,'--max',str(maximum),*[f'{c:02x}'for c in cmd]],capture_output=True,text=True,check=True)
   stamp.write_text(signature)
  ev=[(s,int(t),int(v))for s,t,v in (l.split()for l in events.read_text().splitlines())]
  warm=next(t for s,t,v in ev if s=='W');start=[t for s,t,v in ev if s=='C'][-1]
  if looping:
   ends=[(t,v)for s,t,v in ev if s=='E'];assert len(ends)==1,(sid,'no loop completion')
   end,track=ends[0];points=[t for s,t,v in ev if s=='L'and v==track];assert len(points)>=2,(sid,'no two loops');loop=points[-2]
   assert start<loop<end
  else:
   assert any(s=='S'for s,t,v in ev),(sid,'audio did not end naturally')
   end=warm+next(v for s,t,v in ev if s=='W');loop=None
  with wave.open(str(wav),'rb')as f:assert f.getnchannels()==2 and f.getsampwidth()==2;raw=np.frombuffer(f.readframes(f.getnframes()),'<i2').reshape(-1,2).astype(np.float64).mean(axis=1)
  # Keep command-to-first-note latency and end tail. No silence-based start crop.
  mono=raw[start-warm:end-warm];pcm=np.clip(np.rint(resample_poly(mono,up,down)),-32768,32767).astype('<i2')
  lp=0xffffffff if loop is None else round((loop-start)*up/down)
  if lp!=0xffffffff:assert 0<lp<len(pcm)
  rawpath=b/f'{sid:04x}.pcm';rawpath.write_bytes(pcm.tobytes());adpcm=b/f'{sid:04x}.ima'
  enc=json.loads(subprocess.check_output([str(b/'encode'),str(rawpath),str(adpcm),str(lp)],text=True));assert enc['samples']==len(pcm)
  replies=[(round((t-start)*up/down),(v&0x7e)>>1 if v&128 else 255)for s,t,v in ev if s=='R'and start<=t<end]
  replies=[(min(t,len(pcm)-1),v)for t,v in replies]
  stops=[]if music else[int.from_bytes(m[h+i:h+i+2],'little')for i in range(0,8,2)]
  item=dict(command=sid,header=h,music=music,priority=0 if music else m[h+8],tracks=[c for p,c in tracks],stop=stops,
   samples=len(pcm),loop=lp,**{k:enc[k]for k in ['loop_predictor','loop_index']},replies=replies,
   source_events_sha256=sha(events),pcm_sha256=sha(rawpath),adpcm_sha256=sha(adpcm))
  print(f'audio {sid:04x}: {len(pcm)} samples, loop {lp if lp!=0xffffffff else "none"}, replies {len(replies)}',flush=True);return item,adpcm.read_bytes()
 with concurrent.futures.ThreadPoolExecutor(max_workers=a.workers)as ex:results=list(ex.map(convert,jobs))
 rows=[];events=[];blob=bytearray()
 for item,data in sorted(results,key=lambda r:r[0]['command']):
  item['offset']=len(blob);item['first_event']=len(events);events.extend(item['replies']);blob.extend(data);rows.append(item)
 (g/'native-audio.ima').write_bytes(blob)
 h='''#ifndef NATIVE_AUDIO_DATA_H\n#define NATIVE_AUDIO_DATA_H\n#include <stdint.h>\nstruct NativeAudioAsset {uint32_t offset,samples,loop;uint16_t command,first_event,event_count,stop[4];int16_t loop_predictor;uint8_t loop_index,music,group,track_count,priority[8];};\nstruct NativeAudioEvent {uint32_t sample;uint8_t value;};\n'''
 h+=f'#define NATIVE_AUDIO_SAMPLE_RATE {a.sample_rate}\n#define NATIVE_AUDIO_ASSET_COUNT {len(rows)}\n#define NATIVE_AUDIO_BYTES {len(blob)}\nextern const struct NativeAudioAsset native_audio_assets[{len(rows)}];\nextern const struct NativeAudioEvent native_audio_events[{len(events)}];\nextern const uint8_t native_audio_data[NATIVE_AUDIO_BYTES];\nextern const uint16_t native_audio_fade_steps[256];\n#endif\n'
 (g/'native-audio-data.h').write_text(h)
 c='#include "native-audio-data.h"\nconst struct NativeAudioAsset native_audio_assets[]={\n'
 for r in rows:
  c+='{'+','.join(str(r[k])for k in ['offset','samples','loop','command','first_event'])+f',{len(r["replies"])},{{'+','.join(map(str,r['stop']or[0]*4))+'},'+f'{r["loop_predictor"]},{r["loop_index"]},{int(r["music"])},{r["priority"]},{len(r["tracks"])},{{'+','.join(map(str,(r['tracks']+[0]*8)[:8]))+'}},\n'
 c+='};\nconst struct NativeAudioEvent native_audio_events[]={'+','.join('{'+f'{t},{v}'+'}'for t,v in events)+'};\n'
 #4F88: CPL index applies to the pointer? Read byte at1500+original A's log index.
 # The source LD L,(HL) uses HL=1500+input; CPL A is unused. Preserve that!
 fades=[65535]+[m[0x1500+i]*16 for i in range(1,256)]
 c+='const uint16_t native_audio_fade_steps[]={'+','.join(map(str,fades))+'};\n';(g/'native-audio-data.c').write_text(c)
 report=dict(sample_rate=a.sample_rate,resample_ratio=[up,down],format='mono IMA 4-bit; individual sounds premixed offline',source_commit='574a517547f05bc8c1539fab1b7d566035543496',
  rom_sha256={n:sha(b/(n+'.bin'))for n in ['201-m1','201-v1','201-v2']},pins=pins,assets=rows,bytes=len(blob),
  limitations=['GBA premixed-voice adaptation, not per-YM-channel synthesis; music/effect channel stealing differs.',
   'Captured source replies and observed two-pass loop boundaries; no invented completion timers.',
   f'{a.sample_rate} Hz mono IMA changes fidelity/stereo. Random timbre inside a sound is captured once.',
   'Native07/08 uses extracted M1 curves/envelope and premixed resampling; pitch changes with rate. See native-audio-tempo-source.json.',
   'No gameplay/emulator regression/performance tests performed; these are offline asset extraction records.'])
 (g/'native-audio-source.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'assets':len(rows),'bytes':len(blob),'replies':len(events)}))
if __name__=='__main__':main()
