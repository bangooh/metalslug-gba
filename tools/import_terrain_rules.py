#!/usr/bin/env python3
"""Extract both source collision halves/profiles; no route or game execution."""
import argparse,hashlib,json,struct
from pathlib import Path
R=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args()
 raw=(a.source/'build/mslug_prom.bin').read_bytes()
 sha=hashlib.sha256(raw).hexdigest()
 if sha!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('P-ROM fingerprint')
 def offset(v):
  if 0<=v<0x100000:return v
  if 0x200000<=v<0x300000:return v-0x100000
  raise ValueError(f'unmapped data {v:x}')
 entries=[];rows=[]
 for index in range(64):
  halves=[];record=[]
  for half in range(2):
   at=0x278988+index*32+half*16;o=offset(at)
   pixels,profile=struct.unpack_from('>II',raw,o);po=offset(profile)
   mask=list(raw[offset(pixels):offset(pixels)+20])
   slope=struct.unpack_from('>h',raw,po)[0];heights=list(struct.unpack_from('>8h',raw,po+2))
   fractions=list(raw[po+18:po+26]);tag=struct.unpack_from('>h',raw,po+26)[0]
   flags=list(raw[o+8:o+14])
   halves.append('{'+','.join(['{'+','.join(map(str,mask))+'}',
    '{'+str(slope)+',{'+','.join(map(str,heights))+'},{'+','.join(map(str,fractions))+'},'+str(tag)+'}',
    str(flags[1]),str(flags[4]),str(flags[5]),str(flags[0]),str(flags[2]),'0'])+'}')
   record.append(dict(source=at,bitmap=pixels,profile=profile,rows=mask,slope=slope,heights=heights,fractions=fractions,tag=tag,flags=flags))
  rows.append('{{'+','.join(halves)+'}}');entries.append(record)
 (R/'generated/terrain-rules.h').write_text('#ifndef TERRAIN_RULES_H\n#define TERRAIN_RULES_H\n#include <stdint.h>\n'
  'struct SourceTerrainProfile {int16_t slope,heights[8];uint8_t fractions[8];int16_t tag;};\n'
  'struct SourceTerrainHalf {uint8_t rows[20];struct SourceTerrainProfile profile;uint8_t point_pass,filter,replacement,flags,alternate,pad;};\n'
  'struct SourceTerrainShape {struct SourceTerrainHalf halves[2];};\n'
  'extern const struct SourceTerrainShape source_terrain_shapes[64];\nextern const int16_t source_floor_probe_offsets[3][6][2];\n#endif\n')
 probes=[list(struct.unpack_from('>12h',raw,at)) for at in [0x26dc2,0x26dda,0x26df2]]
 probe_c=','.join('{'+','.join('{'+str(v[i])+','+str(v[i+1])+'}' for i in range(0,12,2))+'}' for v in probes)
 (R/'generated/terrain-rules.c').write_text('#include "terrain-rules.h"\nconst struct SourceTerrainShape source_terrain_shapes[64]={\n'+',\n'.join(rows)+'\n};\nconst int16_t source_floor_probe_offsets[3][6][2]={'+probe_c+'};\n')
 ledger=dict(prom_sha256=sha,source_table=0x278988,shapes=entries,status='Offline data extraction; full terrain/scene integration pending; no gameplay tests')
 (R/'generated/terrain-rules-source.json').write_text(json.dumps(ledger,indent=2)+'\n')
 print(json.dumps(dict(shapes=64,halves=128,profile_words=64*2*10,bitmap_bytes=64*2*20)))
if __name__=='__main__':main()
