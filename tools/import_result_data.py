#!/usr/bin/env python3
"""Result/end titles, defaults, glyphs, rank strings and orbit drawing IDs."""
import argparse,hashlib,json
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
STRINGS={'ALL':0x59362,'BLANK_ALL':0x59366,'MISSION':0x5936a,'BLANK_MISSION':0x59372,'OVER':0x5937a,'BLANK_OVER':0x59380,'P1':0x59386,'P2':0x59393,'SCORE':0x593a0,'CONTINUE':0x593a6,'PEACE':0x59b5a}
def vals(v):return ','.join(map(str,v))
def apply_default_names(defs):
 """Keep native initials when importing the original score/owner/rank data."""
 names=json.loads((R/'art/ranking-defaults.json').read_text())['names']
 if len(names)>len(defs) or any(len(n)!=3 or not all('A'<=c<='Z' for c in n) for n in names):
  raise ValueError('Ranking names must contain three uppercase letters')
 for entry,name in zip(defs,names):
  entry[1:4]=[0xb00+((ord(c)&8)<<5)+((ord(c)&0x77)<<1) for c in name]
 return names
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw)
 def bytes_until(at,end):
  row=[]
  for i in range(2048):
   v=c.byte(at+i);row.append(v)
   if v==end:return row
  raise ValueError('unterminated string')
 strings=[bytes_until(at,255) for at in STRINGS.values()];stream=bytes_until(0x593b5,254);ranks=[bytes_until(c.long(0x2f5440+4*i),254) for i in range(11)]
 defs=[]
 for i in range(10):
  at=0x2f53b2+12*i;defs.append([c.long(at+2),*[c.word(at+6+2*j) for j in range(3)],c.byte(at),c.byte(at+1)])
 native_names=apply_default_names(defs)
 drawings=[]
 for at in [0x24e208,0x24e030]:
  parts=[dict(map=at,flags=8,dx=0,dy_up=0,priority=65535)];drawings.append(c.intern(c.drawings,c.draw_ids,json.dumps(parts,sort_keys=True),parts))
 inputs=[[c.long(0x2f552c+4*i),c.long(0x2f5534+4*i)] for i in range(2)]
 if inputs!=[[0x10e203,0x10e204],[0x10e209,0x10e20a]]:raise ValueError(('input provenance',inputs))
 h=['#ifndef NATIVE_RESULT_DATA_H','#define NATIVE_RESULT_DATA_H','#include "../src/results.h"','#include "../src/animation.h"','enum ResultString {'+','.join('RSTR_'+k for k in STRINGS)+',RSTR_COUNT};','struct ResultBytes {const uint8_t*bytes;uint16_t count;};','extern const struct ResultBytes result_strings[RSTR_COUNT],result_rank_suffixes[11],result_prisoner_stream;','extern const struct HighScoreRecord result_defaults[10];','extern const uint16_t result_alphabet[41],result_rank_glyphs[11],result_prisoner_words[6],result_palettes[8][2],result_orbit_drawings[2];','extern const struct AnimationProgram result_program;','#endif']
 out=c.c_source('result_program','result-data.h')
 for prefix,rows in [('string',strings),('rank',ranks),('prisoner',[stream])]:
  for i,row in enumerate(rows):out.append('static const uint8_t '+prefix+str(i)+'[]={'+vals(row)+'};')
 out+=['const struct ResultBytes result_strings[]={'+','.join('{string'+str(i)+','+str(len(v))+'}' for i,v in enumerate(strings))+'};','const struct ResultBytes result_rank_suffixes[]={'+','.join('{rank'+str(i)+','+str(len(v))+'}' for i,v in enumerate(ranks))+'};','const struct ResultBytes result_prisoner_stream={prisoner0,'+str(len(stream))+'};','const struct HighScoreRecord result_defaults[]={'+','.join('{'+str(v[0])+',{'+vals(v[1:4])+'},'+vals(v[4:])+'}' for v in defs)+'};']
 h.insert(-1,'extern const uint16_t result_large_digits[17];')
 for name,at,count in [('result_alphabet',0x2f54b2,41),('result_rank_glyphs',0x2f542a,11),('result_prisoner_words',0x593cc,6),('result_large_digits',0x5d864,17)]:out.append('const uint16_t '+name+'[]={'+vals(c.word(at+2*i) for i in range(count))+'};')
 palettes=[]
 for i in range(16):
  at=0x593d8+4*i
  if c.word(at)==65535:break
  palettes.append((c.word(at),c.word(at+2)))
 if len(palettes)!=8:raise ValueError(('palettes',palettes))
 out+=['const uint16_t result_palettes[8][2]={'+','.join('{'+vals(v)+'}' for v in palettes)+'};','const uint16_t result_orbit_drawings[]={'+vals(drawings)+'};']
 ledger=c.ledger();ledger.update(prom_sha256=digest,strings=STRINGS,highscore_defaults=0x2f53b2,alphabet=0x2f54b2,input_sources=inputs,implementation=['src/results.c','src/result_text.c','src/result_highscore.c'],source_semantics=['595AE d7 is byte count, not count-minus-one; score8digits and numeric3digits','97ABC clrb(a6) does not clear an old table owner; source code pointers high byte already0','97AAA signed packed-BCD comparison and unchanged rank at insertion','59A1A setsD1 but2308 readsD0.low=1: unique15B6/15F4 parent refreshes45 to2 and scheduler decrements to1 before child; emits0A21'],pending=['Outer result snapshot/boot/mission transition callers,physical renderer/palette/audio/input/persistence'],complete_port=False,validation='Source extraction and compile only; no gameplay tests',source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[c.offset(lo):c.offset(hi-1)+1]).hexdigest()) for lo,hi in [(0x15b6,0x1604),(0xfc6,0xffe),(0x656,0x6ee),(0x5934e,0x59bd2),(0x4737e,0x47482),(0x477d4,0x478ae),(0x9773c,0x977c2),(0x97a60,0x97b74),(0x97c02,0x981e8),(0x2f53b2,0x2f553c)]])
 ledger['native_default_names']=native_names
 for name,value in [('result-data.h','\n'.join(h)),('result-data.c','\n'.join(out)),('result-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(drawings=len(c.drawings),strings=len(strings),rank_suffixes=len(ranks),alphabet=41,defaults=len(defs),palettes=len(palettes))))
if __name__=='__main__':main()
