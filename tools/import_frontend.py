#!/usr/bin/env python3
"""Original logo, ranking and tutorial graph/text data; never run the game."""
import argparse,hashlib,json
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes()
 sha=hashlib.sha256(raw).hexdigest();assert sha=='1b8eee0891b6f8c837348e60ea2923ae229e0fb603d34ae62ae06131332f0d3d'
 c=AnimationCompiler(raw)
 roots={'LOGO_PIECE':0x2f5576,'LOGO_CENTER':0x2f5582,'RANK_A':0x2f550c,'RANK_B':0x2f5518,
  'TUTORIAL':0x2f2dbe,'DIRECTION':0x2f2e0a,'BUTTON':0x2f2ed6,'PRESSED':0x2f2ee2,'RELEASED':0x2f2f02,
  'LIT1':0x2f2f22,'LIT2':0x2f2f2e,'LIT3':0x2f2f3a,'LIT4':0x2f2f46,'ANCHOR':0x2f2f52,'DROP':0x2ef80e}
 entries={k:c.entry(v)for k,v in roots.items()}
 directions=[65535 if c.long(0x2f2dca+4*i)==0xffffffff else c.entry(c.long(0x2f2dca+4*i))for i in range(16)]
 input_rows=[]
 for i in range(1024):
  at=0x2ef8aa+8*i;duration=c.word(at)
  input_rows.append([duration,c.word(at+2) if duration!=65535 else 0,c.byte(at+4)if duration!=65535 else 0,c.byte(at+5)if duration!=65535 else 0])
  if duration==65535:break
 else:raise ValueError('tutorial input terminator')
 streams=[];stream_ids={};pages=[]
 def stream(at):
  if at in stream_ids:return stream_ids[at]
  id=len(streams);stream_ids[at]=id;rows=[];streams.append(rows)
  for i in range(8192):
   q=at+8*i;duration=c.word(q)
   if duration==65535:rows.append([65535,0,65535]);break
   code=c.word(q+2);graph=c.long(q+4);rows.append([duration,code,c.entry(graph)if graph else 65535])
  else:raise ValueError(('text terminator',hex(at)))
  return id
 for base in [0x2efacc,0x2efc2c,0x2efd8c,0x2efeec]:
  pages.append([[stream(c.long(base+8*i)),c.long(base+8*i+4)-0x7000]for i in range(44)])
 from tutorial_button_labels import adapt
 adapt(streams)
 footer=[]
 for i in range(128):
  footer.append(c.byte(0x2f54a0+i))
  if footer[-1]==254:break
 else:raise ValueError('ranking footer terminator')
 c.compile()
 h=['#ifndef NATIVE_FRONTEND_DATA_H','#define NATIVE_FRONTEND_DATA_H','#include "../src/animation.h"',
  'enum FrontendAnimation {'+','.join('FRA_'+k for k in roots)+',FRA_COUNT};',
  'struct FrontendInput {uint16_t duration,page;uint8_t held,edges;};',
  'struct FrontendText {uint16_t duration,code,animation;};',
  'struct FrontendTextStream {const struct FrontendText*rows;uint16_t count;};',
  'extern const struct AnimationProgram frontend_program;',
  'extern const uint16_t frontend_entries[FRA_COUNT],frontend_directions[16],frontend_pages[4][44][2];',
  f'extern const struct FrontendInput frontend_input[{len(input_rows)}];',
  f'extern const struct FrontendTextStream frontend_text[{len(streams)}];',
  'extern const int16_t frontend_logo_pieces[9][2];','extern const uint8_t frontend_logo_zoom[22];',
  'extern const uint16_t frontend_ranking_headers[3][8];',
  f'extern const uint8_t frontend_ranking_footer[{len(footer)}];',
  'extern const int16_t frontend_drop_bounds[4],frontend_zoom[2][64];',
  '#include "../src/hitbox.h"','extern const struct HitAttack frontend_drop_attack;','#endif']
 out=c.c_source('frontend_program','frontend-data.h')
 out+=['const uint16_t frontend_entries[]={'+vals(entries.values())+'};','const uint16_t frontend_directions[]={'+vals(directions)+'};',
  'const struct FrontendInput frontend_input[]={'+','.join('{'+vals(r)+'}'for r in input_rows)+'};',
  'const uint16_t frontend_pages[4][44][2]={'+','.join('{'+','.join('{'+vals(r)+'}'for r in group)+'}'for group in pages)+'};']
 for i,rows in enumerate(streams):out.append(f'static const struct FrontendText text{i}[]={{'+','.join('{'+vals(r)+'}'for r in rows)+'};')
 out+=['const struct FrontendTextStream frontend_text[]={'+','.join('{text'+str(i)+','+str(len(rows))+'}'for i,rows in enumerate(streams))+'};',
  'const int16_t frontend_logo_pieces[9][2]={'+','.join('{'+vals(c.signed(0x2f553c+4*i+2*j)for j in range(2))+'}'for i in range(9))+'};',
  'const uint8_t frontend_logo_zoom[]={'+vals(c.byte(0x2f5560+i)for i in range(22))+'};',
  'const uint16_t frontend_ranking_headers[3][8]={'+','.join('{'+vals([c.word(at+2*i)for i in range(count)]+[65535]*(8-count))+'}'for at,count in [(0x2f547c,5),(0x2f5486,5),(0x2f5490,8)])+'};',
  'const uint8_t frontend_ranking_footer[]={'+vals(footer)+'};',
  'const int16_t frontend_drop_bounds[]={'+vals(c.signed(0x2ef8a0+2*i)for i in range(4))+'};',
  'const int16_t frontend_zoom[2][64]={'+','.join('{'+vals(c.signed(at+2*i)for i in range(64))+'}'for at in [0x2c072c,0x2c07ac])+'};',
  'const struct HitAttack frontend_drop_attack={{'+vals(c.signed(0x2ef856+2*i)for i in range(4))+'},100,0,6,2,4,255};']
 ledger=c.ledger();ledger.update(prom_sha256=sha,roots=roots,inputs=input_rows,page_sources=list(stream_ids),text_streams=len(streams),implementation=['src/frontend_actors.c','src/frontend_tutorial.c'],complete_port=False)
 for name,value in [('frontend-data.h','\n'.join(h)),('frontend-data.c','\n'.join(out)),('frontend-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),fields=c.fields,callbacks=c.callbacks,bindings=c.bindings,parameters=c.parameters,streams=len(streams),rows=sum(map(len,streams)))))
if __name__=='__main__':main()
