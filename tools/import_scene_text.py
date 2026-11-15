#!/usr/bin/env python3
"""Gameplay fixed-layer GO timelines and continue labels; karaoke ending removed."""
import argparse,hashlib,json
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw);roots=[]
 if c.fields or c.callbacks or c.parameters or c.bindings:raise ValueError('unreviewed text animation')
 text=[]
 timelines=[];starts=[];pending=[];ids={}
 def timeline(at):
  if at in ids:return ids[at]
  ids[at]=len(timelines);starts.append(at);timelines.append(None);pending.append(at);return ids[at]
 go=[timeline(at) for at in [0x2f3574,0x2f35fe,0x2f3688]]
 for at in pending:
  rows=[]
  for i in range(256):
   pc=at+8*i;tile=c.word(pc)
   if tile==65535:rows.append((65535,0,0,0,65535));break
   if tile==0:rows.append((0,0,0,0,timeline(c.long(pc+2))));break
   row=tuple(c.word(pc+2*j) for j in range(4))
   if not row[1] or not row[2]:raise ValueError('empty fixed timeline rectangle')
   rows.append((*row,65535))
  else:raise ValueError('unterminated timeline')
  timelines[ids[at]]=rows
 labels=[]
 for at in [0x2f4a12,0x2f4a22]:
  row=[]
  for i in range(256):
   v=c.byte(at+i);row.append(v)
   if v==255:break
  else:raise ValueError('unterminated label')
  labels.append(row)
 if len(labels[0])!=16 or len(labels[1])!=16:raise ValueError('label length')
 h=['#ifndef NATIVE_SCENE_TEXT_DATA_H','#define NATIVE_SCENE_TEXT_DATA_H','#include "../src/animation.h"','struct FixedTimelineFrame {uint16_t tile,width,height,ticks,jump;};','struct FixedTimeline {const struct FixedTimelineFrame*frames;uint16_t count;};',f'extern const struct FixedTimeline scene_text_timelines[{len(timelines)}];','extern const uint16_t scene_go_timelines[3];','extern const uint8_t scene_fixed_hex[16],scene_continue_labels[2][16];','#endif']
 out=['#include "scene-text-data.h"'];out+=['const uint16_t scene_go_timelines[]={'+vals(go)+'};','const uint8_t scene_fixed_hex[]={'+vals(c.byte(0x5d71c+i) for i in range(16))+'};','const uint8_t scene_continue_labels[2][16]={'+','.join('{'+vals(row)+'}' for row in labels)+'};']
 for i,rows in enumerate(timelines):out.append('static const struct FixedTimelineFrame timeline'+str(i)+'[]={'+','.join('{'+vals(row)+'}' for row in rows)+'};')
 out.append('const struct FixedTimeline scene_text_timelines[]={'+','.join('{timeline'+str(i)+','+str(len(rows))+'}' for i,rows in enumerate(timelines))+'};')
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=roots,text_source=0x2f4712,text_words=len(text),timeline_sources=starts,timeline_frames=sum(map(len,timelines)),implementation=['src/scene_text.c','src/scene_driver.c'],source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[c.offset(lo):c.offset(hi-1)+1]).hexdigest()) for lo,hi in [(0x8c820,0x8ccc6),(0x8ce64,0x8d184),(0x967fe,0x96b7e),(0x5db58,0x5dc00),(0x2f3574,0x2f374a),(0x2f4712,0x2f4a32)]],pending=['Outer boot transition, results, physical FIXED/sprite renderer, audio transport, released main loop'],complete_port=False,validation='Source extraction and ARM compilation only; no gameplay tests')
 for name,value in [('scene-text-data.h','\n'.join(h)),('scene-text-data.c','\n'.join(out)),('scene-text-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),text_words=len(text),timelines=len(timelines),timeline_frames=sum(map(len,timelines)))))
if __name__=='__main__':main()
