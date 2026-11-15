#!/usr/bin/env python3
"""Bind the35 approved complete poses to explicit source body/arm frames.

No inferred pixel edits: runtime substitutes byte-identical approved art only
when both independently scheduled layers match the reviewed composition.
"""
import hashlib,json
from pathlib import Path
R=Path(__file__).resolve().parents[1];G=R/'generated'
def main():
 pixels=(G/'player.tiles').read_bytes();sha=hashlib.sha256(pixels).hexdigest()
 assert sha=='ca7febc19eeda7cf62af36f036cdb91f290bf8be8ac279344e03bf269c74be32'
 d=json.loads((G/'player-program-source.json').read_text());commands=d['commands']
 def sequence(entry):
  seen=set();out=[]
  while entry not in seen and entry!=65535:
   seen.add(entry);c=commands[entry]
   if c['op']=='DRAW':out.append((entry,c['target'],c['source']))
   if c['op']=='END':break
   entry=c['target']if c['op']=='JUMP'else c['next']
  return out
 rows=[]
 for name,root,count,dy in [('idle',0x27aaf0,6,0),('walk',0x27b42c,12,-1),('standing_jump',0x27b872,11,0),('running_jump',0x27b966,6,0)]:
  entry=next(c['native_index']for c in commands if c['source']==root and c['actor']==0)
  arms=sequence(entry);body=sequence(d['lower_streams'][name][0]);assert len(arms)>=count,(name,len(arms))
  # Idle's later weapon-check branch is outside the six approved poses.
  arms=arms[:count]
  assert len(body)in(1,count)
  for i,(cursor,drawing,source)in enumerate(arms):rows.append(dict(frame=len(rows),lower=body[i%len(body)][1],upper=drawing,cursor=cursor,offset_y=dy,source=source))
 assert len(rows)==35 and len(pixels)==35*2048
 (G/'approved-player-data.h').write_text('#ifndef NATIVE_APPROVED_PLAYER_DATA_H\n#define NATIVE_APPROVED_PLAYER_DATA_H\n#include <stdint.h>\nstruct NativeApprovedPlayer {uint16_t lower,upper,cursor;int8_t offset_y;};\nextern const struct NativeApprovedPlayer native_approved_players[35];\nextern const uint8_t native_approved_player_pixels[35*2048];\n#endif\n')
 (G/'approved-player-data.c').write_text('#include "approved-player-data.h"\nconst struct NativeApprovedPlayer native_approved_players[]={'+','.join('{'+','.join(str(r[k])for k in ['lower','upper','cursor','offset_y'])+'}'for r in rows)+'};\n')
 report=dict(approved_sha256=sha,bytes=len(pixels),bindings=rows,policy='Exact paired body/arm graph and relative transform, palette and reflection required; other source poses use full source catalog. No guessed repairs.',gameplay_executed=False)
 (G/'approved-player-source.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'bindings':len(rows),'approved_sha256':sha}))
if __name__=='__main__':main()
