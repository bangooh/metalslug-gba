#!/usr/bin/env python3
"""Extract M1 tempo curves and initial music tempos; no sound execution."""
import argparse,hashlib,json,zipfile
from pathlib import Path
R=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--rom',type=Path,required=True);a=p.parse_args()
 with zipfile.ZipFile(a.rom)as z:
  n=next(n for n in z.namelist()if Path(n).name.split('.')[0]=='201-m1');m=z.read(n)
 ledger=json.loads((R/'generated/native-audio-source.json').read_text())
 assert hashlib.sha256(m).hexdigest()==ledger['rom_sha256']['201-m1']
 rows=ledger['assets'];tempo=[m[0x4b84+m[r['header']]]if r['music']else 0 for r in rows]
 h='#ifndef NATIVE_AUDIO_TEMPO_DATA_H\n#define NATIVE_AUDIO_TEMPO_DATA_H\n#include <stdint.h>\nextern const uint8_t native_audio_curve[2][256];\nextern const uint8_t native_audio_initial_tempo['+str(len(rows))+'];\n#endif\n'
 c='#include "native-audio-tempo.h"\nconst uint8_t native_audio_curve[2][256]={'+','.join('{'+','.join(map(str,m[at:at+256]))+'}'for at in [0x1400,0x1500])+'};\nconst uint8_t native_audio_initial_tempo[]={'+','.join(map(str,tempo))+'};\n'
 (R/'generated/native-audio-tempo.h').write_text(h);(R/'generated/native-audio-tempo.c').write_text(c)
 report={'m1_sha256':hashlib.sha256(m).hexdigest(),'music_initial_tempos':{str(r['command']):tempo[i]for i,r in enumerate(rows)if r['music']},'source_functions':['2170','4D77','4F5B'],'adaptation':'Original curve and fixed-point envelope; resample premixed music, including its replies. Pitch changes together with tempo. SFX unaffected.','executed':False}
 (R/'generated/native-audio-tempo-source.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
if __name__=='__main__':main()
