#!/usr/bin/env python3
"""Reuse approved title pixels and converted audio, not the old game runtime."""
import argparse,hashlib,json,re,shutil
from pathlib import Path
R=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--approved-art',type=Path,required=True);a=p.parse_args();src=a.approved_art/'generated';g=R/'generated'
 report={}
 for old,new in [('prototype-title-tiles.bin','title.tiles'),('prototype-title-maps.bin','title.maps'),('prototype-title.pal','title.pal'),('audio-effects.bin','audio-effects.bin'),('audio_ima_fast.h','audio_ima_fast.h')]:
  shutil.copyfile(src/old,g/new);report[new]=dict(approved_source=old,sha256=hashlib.sha256((g/new).read_bytes()).hexdigest())
 header=(src/'audio_data.h').read_text();records=[];packed=bytearray();music=(src/'audio-music.bin').read_bytes()
 for s in re.findall(r'^\{([^}]+)\}',header,re.M):
  v=list(map(int,s.split(',')))
  if v[0]<0x1000:
   if v[0] not in (0x20,0x21,0x28,0x2b):continue
   data=music[v[3]:v[3]+(v[4]+1)//2];assert len(data)==(v[4]+1)//2
   v[3]=len(packed);packed.extend(data)
  records.append(v)
 (g/'audio-music.bin').write_bytes(packed[:0x100000]);(g/'audio-music-tail.bin').write_bytes(packed[0x100000:])
 pre=header.split('#define AUDIO_ASSET_COUNT')[0]
 (g/'audio_data.h').write_text(pre+f'#define AUDIO_ASSET_COUNT {len(records)}\nstatic const struct AudioAsset audio_assets[]={{\n'+''.join('{'+','.join(map(str,v))+'},\n' for v in records)+'};\n#endif\n')
 oldmanifest=json.loads((src/'audio-manifest.json').read_text())
 report.update(music_bytes=len(packed),commands=[v[0] for v in records],sample_rate=8192,music_sample_rate=7168,
               source_assets=[v for v in oldmanifest['assets'] if int(v['command'],16) in {r[0] for r in records}],
               scope='M1/title music wired; effects, boss and clear music packed for later gameplay integration')
 (g/'presentation-source.json').write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(dict(music_bytes=len(packed),assets=len(records))))
if __name__=='__main__':main()
