#!/usr/bin/env python3
"""Extract reviewed Slug support data; never execute a game or CPU model."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args()
 raw=(a.source/'build/mslug_prom.bin').read_bytes();sha=hashlib.sha256(raw).hexdigest()
 if sha!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('P-ROM fingerprint')
 b=AnimationCompiler(raw);rows=[];programs=[];ledger=[]
 for start,end in [(0x2f886,0x2f8ae),(0x2f8ae,0x2fa3c),(0x2fa3c,0x2faca)]:
  nodes={};pending=[0]
  for pos in pending:
   if pos in nodes:continue
   if not 0<=pos<end-start:raise ValueError('damage program range')
   op=b.byte(start+pos);stride=b.byte(start+pos+1)
   if op>3 or stride!=(2 if op==3 else 4):raise ValueError('unreviewed damage command')
   value=b.word(start+pos+2) if op!=3 else 0
   if op==1 and value not in (0x16,0x18,0x1a,0x1c):raise ValueError('unreviewed palette field')
   nxt=value if op==2 else pos+stride if op!=3 else pos
   nodes[pos]=(op,nxt,(value-0x16)//2 if op==1 else value)
   if nxt not in nodes:pending.append(nxt)
  if sorted(nodes)!=list(range(0,4*len(nodes),4)):raise ValueError('cross-program damage cursor requires a new mapping')
  base=len(rows);ids={pos:base+i for i,pos in enumerate(sorted(nodes))}
  for pos in sorted(nodes):
   op,nxt,value=nodes[pos];rows.append((ids[nxt],op,value>>8 if op==0 else 0,value&255))
  programs.append((base,len(nodes)));ledger.append(dict(source=start,bytes=end-start,offsets=sorted(nodes)))
 offsets=[b.signed(at+2) for at in (0x279668,0x27966e,0x279674)]
 if b.word(0x279666)!=0:raise ValueError('contact correction mode requires review')
 angle=[b.byte(0x2a558+i) for i in range(-64,65)]
 if set(angle)-{0,1}:raise ValueError('slope classification')
 sprite=[struct.unpack('b',bytes([b.byte(0x2b0bee+i)]))[0] for i in range(65)]
 h=['#ifndef SLUG_SUPPORT_DATA_H','#define SLUG_SUPPORT_DATA_H','#include <stdint.h>',
  'enum SlugDisplayOp {SDISPLAY_WAIT,SDISPLAY_PALETTE,SDISPLAY_JUMP,SDISPLAY_END};',
  'struct SlugDisplayCommand {uint16_t next;uint8_t op,duration,value;};',
  'struct SlugDisplayProgram {uint16_t first,count;};',
  'extern const struct SlugDisplayCommand slug_display_commands[];',
  'extern const struct SlugDisplayProgram slug_display_programs[3];',
  'extern const int16_t slug_contact_offsets[3];',
  'extern const uint8_t slug_flat_angles[129];',
  'extern const int8_t slug_angle_sprites[65];','#endif']
 c=['#include "slug-support-data.h"',
  'const struct SlugDisplayCommand slug_display_commands[]={'+','.join('{'+','.join(map(str,r))+'}' for r in rows)+'};',
  'const struct SlugDisplayProgram slug_display_programs[3]={'+','.join('{'+','.join(map(str,r))+'}' for r in programs)+'};',
  'const int16_t slug_contact_offsets[3]={'+','.join(map(str,offsets))+'};',
  'const uint8_t slug_flat_angles[129]={'+','.join(map(str,angle))+'};',
  'const int8_t slug_angle_sprites[65]={'+','.join(map(str,sprite))+'};']
 (R/'generated/slug-support-data.h').write_text('\n'.join(h)+'\n');(R/'generated/slug-support-data.c').write_text('\n'.join(c)+'\n')
 (R/'generated/slug-support-source.json').write_text(json.dumps(dict(prom_sha256=sha,damage_programs=ledger,
  contact_offsets=offsets,correction_mode=0,angle_table=0x2a558,angle_range=[-64,64],sprite_table=0x2b0bee,
  status='Offline support-data extraction; full Slug state/graphics integration pending; no gameplay tests'),indent=2)+'\n')
 print(json.dumps(dict(damage_commands=len(rows),damage_programs=3,contact_offsets=offsets,angle_entries=129,sprite_entries=65)))
if __name__=='__main__':main()
