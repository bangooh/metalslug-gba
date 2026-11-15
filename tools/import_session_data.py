#!/usr/bin/env python3
"""Extract campaign/controller data, not an emulated boot instruction stream."""
import argparse, hashlib, json
from pathlib import Path
from animation_compiler import AnimationCompiler
R = Path(__file__).resolve().parents[1]
TEXT = {'INSERT':0x2785c4,'START':0x2785d0,'CONTINUE':0x2785dc,
        'OVER':0x2785e8,'WAIT':0x2785f4,'TIMEUP':0x28debc,
        'BLANK_TIMEUP':0x28dec4,'CENTER':0x28de36,'BLANK_CENTER':0x28de40}
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args()
 raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw);initial=c.entry(0x28de2a)
 digits=[c.entry(c.long(0x28dd96+4*i)) for i in range(10)];c.compile()
 if c.fields or c.callbacks or c.bindings or c.parameters:raise ValueError('unreviewed continue animation')
 def term(at,end):
  out=[]
  for i in range(2048):
   v=c.byte(at+i);out.append(v)
   if v==end:return out
  raise ValueError('text terminator')
 texts=[term(at,255) for at in TEXT.values()]
 palettes=[]
 for base,offset in [(0x9fa,0x74),(0x9b4,0)]:
  at=base
  while c.word(at)!=65535:
   slot,pal=c.word(at),c.word(at+2);palettes.append([(slot&255)+offset,pal if pal==65535 or offset else 1024+pal,64 if pal==65535 else (slot>>8)|1]);at+=4
  if len(palettes)>64:raise ValueError('palette termination')
 # The dispatcher swaps the logical third/fourth missions. Do not sort IDs.
 scripts=[c.long(0xc0a+8*i) for i in range(6)]
 scenes=[]
 for script in scripts:
  init=c.long(script+4)
  if c.word(init)!=0x13fc or c.long(init+4)!=0x106ece:raise ValueError(hex(init))
  scenes.append(c.word(init+2))
 if scenes!=[0,1,3,2,4,5]:raise ValueError(scenes)
 def vals(v):return ','.join(map(str,v))
 h=['#ifndef NATIVE_SESSION_DATA_H','#define NATIVE_SESSION_DATA_H','#include "../src/result_internal.h"',
    'enum SessionText {'+','.join('SST_'+k for k in TEXT)+',SST_COUNT};',
    f'enum {{ SESSION_CONTINUE_INITIAL={initial}, SESSION_PALETTE_COUNT={len(palettes)} }};',
    'extern const struct AnimationProgram session_program;',
    'extern const struct ResultBytes session_texts[SST_COUNT],session_up[2];',
    'extern const uint8_t session_scenes[6],session_continue_scale[60];',
    'extern const uint16_t session_time_settings[4],session_time_digits[10],session_continue_digits[10],session_palettes[SESSION_PALETTE_COUNT][3];','#endif']
 out=c.c_source('session_program','session-data.h')
 for i,row in enumerate(texts):out.append(f'static const uint8_t text{i}[]={{'+vals(row)+'};')
 out.append('const struct ResultBytes session_texts[]={'+','.join('{text'+str(i)+','+str(len(row))+'}' for i,row in enumerate(texts))+'};')
 ups=[term(at,254) for at in [0x2785b8,0x2785be]]
 for i,row in enumerate(ups):out.append(f'static const uint8_t up{i}[]={{'+vals(row)+'};')
 out.append('const struct ResultBytes session_up[]={'+','.join('{up'+str(i)+','+str(len(row))+'}' for i,row in enumerate(ups))+'};')
 for name,typ,row in [('session_scenes','uint8_t',scenes),('session_continue_scale','uint8_t',[c.byte(0x28de54+i) for i in range(60)]),('session_time_settings','uint16_t',[c.word(0x79968+2*i) for i in range(4)]),('session_time_digits','uint16_t',[c.word(0x28decc+2*i) for i in range(10)]),('session_continue_digits','uint16_t',digits)]:out.append('const '+typ+' '+name+'[]={'+vals(row)+'};')
 out.append('const uint16_t session_palettes[SESSION_PALETTE_COUNT][3]={'+','.join('{'+vals(row)+'}' for row in palettes)+'};')
 ledger=c.ledger();ledger.update(prom_sha256=digest,scene_order=scenes,strings=TEXT,palettes=palettes,
  implementation=['src/session.c','src/session_player.c','src/session_ui.c','src/session_hud.c'],
  adaptations=['Explicit GBA shared-credit/freeplay configuration and typed join requests replace BIOS credit-bank calls'],
  pending=['Physical GBA input/render/audio, global credits/highscore HUD/rank policy, title/attract return, three exceptional inputs, integrated gameplay validation'],
  complete_port=False,validation='Static extraction and ARM compile only',
  source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[c.offset(lo):c.offset(hi-1)+1]).hexdigest()) for lo,hi in [(0x9b4,0x1e70),(0x25488,0x267a0),(0x46608,0x46a96),(0x47482,0x47676),(0x516ba,0x5178e),(0x2785b8,0x278600),(0x28dd96,0x28dee0)]])
 for name,value in [('session-data.h','\n'.join(h)),('session-data.c','\n'.join(out)),('session-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),scene_order=scenes,palettes=len(palettes))))
if __name__=='__main__':main()
