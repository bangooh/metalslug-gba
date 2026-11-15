#!/usr/bin/env python3
"""Original mission results, two roster dictionaries, and four banner layouts."""
import argparse,hashlib,json
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
STRINGS={'P1':0x282a3e,'P2':0x282a42,'WINNER':0x282a4c,'BLANK_WINNER':0x282a54,'DRAW':0x282a5c,'BLANK_DRAW':0x282a62,'TITLE':0x282a68,'NO_PRISONER':0x282a7c,'BLANK_NO_PRISONER':0x282a88}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw);entries={name:c.entry(at) for name,at in [('PRIZE',0x28295a),('COLUMN',0x282966),('VEHICLE',0x28dc6e)]}
 letters=[c.entry(c.long(0x2df684+4*i)) for i in range(14)];numbers=[c.entry(c.long(0x2df71c+4*i)) for i in range(6)];c.compile()
 if c.fields or c.callbacks or c.bindings or c.parameters:raise ValueError('unreviewed animation dependency')
 def terminated(at,end):
  row=[]
  for i in range(2048):
   v=c.byte(at+i);row.append(v)
   if v==end:return row
  raise ValueError('unterminated text')
 strings=[terminated(at,255) for at in STRINGS.values()];bonus=terminated(0x282a96,254)
 names=[];banks=[];weights=[]
 for base,first,last,table,size in [(0x2831b2,1,8,0x282de0,0x3b2),(0x282d88,2,11,0x282c3c,0x14c)]:
  rows=[]
  for length in range(first,last+1):
   at=base+8*(length-first);ptr=c.long(at);count=c.long(at+4);offset=len(names)
   names.extend(c.byte(ptr+i) for i in range(length*count));rows.append([offset,count,length])
  banks.append(rows);weights.append([c.byte(table+i) for i in range(size)])
 layouts=[];layout_rows=[]
 for start,end in [(0x7acd0,0x7af50),(0x7af50,0x7b28e),(0x7b28e,0x7b598),(0x7b598,0x7b960)]:
  pos=start;first=len(layout_rows)
  while pos<end-2:
   if c.word(pos)!=0x43fa or c.word(pos+4)!=0x4eb9 or c.long(pos+6)!=0x4ae or c.word(pos+10)!=0x4eb9 or c.long(pos+12)!=0x5dd02:raise ValueError(hex(pos))
   target=pos+2+c.signed(pos+2);pos+=16;v={0x20:255}
   while c.word(pos) in [0x117c,0x317c]:
    value=c.word(pos+2);field=c.word(pos+4);v[field]=value;pos+=6
   if target not in [0x7aae0,0x7ab5c] or set(v)!=set([0x20,0x21,0x74,0x70,0x72]):raise ValueError((target,v))
   layout_rows.append([v[0x70],v[0x72],v[0x74],v[0x20],v[0x21],int(target==0x7ab5c)])
  if pos!=end-2 or c.word(pos)!=0x4e75:raise ValueError(hex(pos))
  layouts.append([first,len(layout_rows)-first])
 h=['#ifndef NATIVE_MISSION_RESULT_DATA_H','#define NATIVE_MISSION_RESULT_DATA_H','#include "../src/result_internal.h"','enum MissionResultString {'+','.join('MRSTR_'+s for s in STRINGS)+',MRSTR_COUNT};','enum {'+','.join('MRA_'+k+'='+str(v) for k,v in entries.items())+'};','struct MissionNameBank {uint16_t first,count;uint8_t length;};','struct MissionBannerLetter {int16_t x,y;uint16_t delay;uint8_t glyph,blink,number;};','extern const struct AnimationProgram mission_result_program;','extern const struct ResultBytes mission_result_strings[MRSTR_COUNT],mission_result_bonus,mission_name_weights[2];','extern const struct MissionNameBank mission_name_banks[2][10];',f'extern const uint8_t mission_name_bytes[{len(names)}],mission_roster_prefix[64][4];','extern const uint16_t mission_roster_suffix[64],mission_roster_offsets[16],mission_result_delays[2][8],mission_prize_x[2][16],mission_result_digits[10],mission_banner_sounds[2][8],mission_banner_letters[14],mission_banner_numbers[6],mission_banner_layouts[4][2];',f'extern const struct MissionBannerLetter mission_banner_layout[{len(layout_rows)}];','extern const int16_t mission_banner_bounds[4];','#endif']
 out=c.c_source('mission_result_program','mission-result-data.h')
 for pre,rows in [('text',strings),('bonus',[bonus]),('weight',weights)]:
  for i,row in enumerate(rows):out.append('static const uint8_t '+pre+str(i)+'[]={'+vals(row)+'};')
 out+=['const struct ResultBytes mission_result_strings[]={'+','.join('{text'+str(i)+','+str(len(row))+'}'for i,row in enumerate(strings))+'};','const struct ResultBytes mission_result_bonus={bonus0,'+str(len(bonus))+'};','const struct ResultBytes mission_name_weights[]={'+','.join('{weight'+str(i)+','+str(len(row))+'}'for i,row in enumerate(weights))+'};','const struct MissionNameBank mission_name_banks[2][10]={'+','.join('{'+','.join('{'+vals(row)+'}'for row in bank)+'}'for bank in banks)+'};','const uint8_t mission_name_bytes[]={'+vals(names)+'};']
 for name,at,n in [('mission_roster_suffix',0x282bbc,64),('mission_roster_offsets',0x282a9c,16),('mission_result_digits',0x28dee0,10)]:out.append('const uint16_t '+name+'[]={'+vals(c.word(at+2*i)for i in range(n))+'};')
 for name,starts,n in [('mission_result_delays',[0x28293a,0x28294a],8),('mission_prize_x',[0x2829fe,0x282a1e],16),('mission_banner_sounds',[0x28df00,0x28df10],8)]:out.append('const uint16_t '+name+'[2]['+str(n)+']={'+','.join('{'+vals(c.word(at+2*i)for i in range(n))+'}'for at in starts)+'};')
 out+=['const uint8_t mission_roster_prefix[64][4]={'+','.join('{'+vals(c.byte(0x282abc+4*i+j)for j in range(4))+'}'for i in range(64))+'};','const uint16_t mission_banner_letters[]={'+vals(letters)+'};','const uint16_t mission_banner_numbers[]={'+vals(numbers)+'};','const uint16_t mission_banner_layouts[4][2]={'+','.join('{'+vals(row)+'}'for row in layouts)+'};','const struct MissionBannerLetter mission_banner_layout[]={'+','.join('{'+vals(row)+'}'for row in layout_rows)+'};','const int16_t mission_banner_bounds[]={'+vals(c.signed(0x2df734+2*i)for i in range(4))+'};']
 ledger=c.ledger();ledger.update(prom_sha256=digest,strings=STRINGS,roster_bytes=len(names),roster_banks=banks,banner_layouts=layouts,implementation=['src/mission_result.c','src/mission_result_roster.c','src/mission_banner.c'],source_semantics=['3E084/0D0/1A0/208 accelerate only clock>1 then decrement and trigger signed<0; distinct root<=0 waits','516DA sums rescued LOW BYTES with saturation; 516BA live status/progress snapshot stays separate','4772A zero tens leaves prior cells untouched;4768A always draws both digits','2C66 palette bit8 stored as private_palette_flags, not a sound command;236E selects palettes1BE/1AA/0','7ACD0/7AF50/7B28E/7B598 spawn70 FIRST children across14/18/17/21-letter layouts'],pending=['Outer17C8/172E result boot caller,mission start callers,raw start214/21A producer,physical renderer/palette/audio and full main integration'],complete_port=False,validation='Source extraction and ARM compile only; no gameplay tests',source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[c.offset(lo):c.offset(hi-1)+1]).hexdigest())for lo,hi in [(0x3dbc8,0x3ee48),(0x7a970,0x7ba2e),(0x2acfc,0x2add6),(0x47656,0x477d4),(0x28293a,0x285c84),(0x28dc6e,0x28df20),(0x2df5a4,0x2df73c)]])
 for name,value in [('mission-result-data.h','\n'.join(h)),('mission-result-data.c','\n'.join(out)),('mission-result-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),roster_bytes=len(names),banner_letters=len(layout_rows))))
if __name__=='__main__':main()
