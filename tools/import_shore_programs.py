#!/usr/bin/env python3
"""Source 94/95 reinforcement soldiers and barrel 141 with shared splash."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(PARA_ENTER=0x66dd8,PARA_IDLE=0x66e48,PARA_A=0x66e8a,PARA_B=0x66ec4,PARA_LEAVE=0x66efe,PARA_DIE=0x66f6e,PARA_BULLET=0x66fee,PARA_SMOKE=0x67014,BARREL_IDLE=0x2c756c,BARREL_WAIT=0x2c75e6,BARREL_RISE=0x2c7626,BARREL_FALL=0x2c765c,BARREL_FIRE=0x2c7688,BARREL_HIT=0x2c76e8,BARREL_SLIDE=0x2c7744,MINE_A=0x2c7798,MINE_B=0x2c77a8,FRAGMENT=0x2c77e6,SPLASH=0x2de7c2)
RANKS=dict(SPAWN=0x2b889a,IDLE=0x2b891c,REPEATS=0x2b8a60,PERIOD=0x2b89de,BULLET=0x2b8ae2,MINE=0x2bdb84)
FIXED=dict(PARA=('DEFENSE',0x66d7c),MINE=('DEFENSE',0x2c7944),MINE_ATTACK=('ATTACK',0x2c784c))
TEMPLATES={94:0x666ae,95:0x666b6,141:0x65f40}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',required=True,type=Path);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw)
 if any(c.long(0xe8000+4*i)!=at for i,at in TEMPLATES.items()):raise ValueError('template dispatch')
 entries={k:c.entry(v) for k,v in ROOTS.items()};fixed={k:c.binding(*v) for k,v in FIXED.items()};c.compile()
 if {x['source'] for x in c.callbacks}!={0x66c40,0x66c80,0x66cc0,0x283ca,0x66680} or c.parameters or c.fields:raise ValueError('unreviewed graph')
 h=['#ifndef NATIVE_SHORE_PROGRAMS_H','#define NATIVE_SHORE_PROGRAMS_H','#include "../src/bosses.h"',
 'enum ShoreAnimation {'+','.join('SA_'+k for k in ROOTS)+',SA_COUNT};','enum ShoreCallback {'+','.join('SC_'+format(x['source'],'X') for x in c.callbacks)+'};','enum ShoreBinding {'+','.join('SB_'+k+'='+str(v) for k,v in fixed.items())+'};','enum ShoreRank {'+','.join('SR_'+k for k in RANKS)+',SR_COUNT};',
 'extern const struct AnimationProgram shore_program;','extern const struct BossBindingSet shore_bindings['+str(len(c.bindings))+'];','extern const uint16_t shore_entries[SA_COUNT];','struct ShoreRankData {const uint16_t*values;const uint8_t*choices;};','extern const struct ShoreRankData shore_rank[SR_COUNT];','extern const int16_t shore_bounds[4][4],shore_boxes[2][4];']
 out=c.c_source('shore_program','shore-programs.h');defs=[];attacks=[];sets=[];filters={}
 def array(ty,name,data):out.append('const '+ty+' '+name+'={'+data+'};')
 for b in c.bindings:
  q=b['source'];records=defs if b['kind']=='DEFENSE' else attacks;first=len(records)
  for count in range(256):
   if c.byte(q)==255:break
   rect='{'+vals(c.signed(q+10+2*i) for i in range(4))+'}'
   if b['kind']=='DEFENSE':
    at=c.long(q+2);name='0'
    if at!=0xffffffff:
     if at not in filters:
      filters[at]='filter_'+str(len(filters));out.append('static const uint8_t '+filters[at]+'[36]={'+vals(c.byte(at+i) for i in range(36))+'};')
     name=filters[at]
    records.append('{'+rect+','+name+','+str(c.byte(q+1))+'}')
   elif b['kind']=='ATTACK':
    group,kind,damage,strength,shape,offscreen=struct.unpack_from('>BBhBBB',raw,c.offset(q))
    if group>4 or kind>34 or shape not in (0,4):raise ValueError('attack format')
    records.append('{'+rect+','+vals([damage,group,kind,strength,shape,offscreen])+'}')
   else:raise ValueError(b)
   q+=80
  else:raise ValueError('unterminated hit table')
  sets.append('{'+('defs+'+str(first)+',0' if b['kind']=='DEFENSE' else '0,attacks+'+str(first))+','+str(count)+'}')
 out+=['static const struct HitDefense defs[]={'+','.join(defs)+'};','static const struct HitAttack attacks[]={'+','.join(attacks)+'};']


 array('struct BossBindingSet','shore_bindings[]',','.join(sets));array('uint16_t','shore_entries[SA_COUNT]',vals(entries.values()))
 rank_rows=[]
 for i,at in enumerate(RANKS.values()):
  mode=c.word(at)
  if mode==2:
   choices=[c.byte(at+2+j) for j in range(64)]
   array('uint8_t',f'shore_choices{i}[64]',vals(choices))
   array('uint16_t',f'shore_values{i}[{(max(choices)+1)*8}]',vals(c.word(at+66+2*j) for j in range((max(choices)+1)*8)))
   rank_rows.append('{shore_values'+str(i)+',shore_choices'+str(i)+'}')
  else:
   if mode not in (0,1):raise ValueError('rank mode')
   array('uint16_t',f'shore_values{i}[64]',vals(c.word(at+2+2*j) for j in range(64)))
   rank_rows.append('{shore_values'+str(i)+',0}')
 array('struct ShoreRankData','shore_rank[SR_COUNT]',','.join(rank_rows))


 array('int16_t','shore_bounds[4][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x66dd0,0x2c7998,0x2c79a2,0x2df238)))
 array('int16_t','shore_boxes[2][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x2c79ac,0x2c79b6)))
 h.append('#endif')
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,rank_sources=RANKS,templates=TEMPLATES,implementation=['src/shore.c','src/paratrooper.c','src/barrel.c'],validation='Source conversion only; no gameplay tests',complete_port=False,source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in [(0x65f40,0x666ae),(0x666ae,0x670d2),(0x788c0,0x78908),(0x78998,0x789f0)]])
 for name,value in [('shore-programs.h','\n'.join(h)),('shore-programs.c','\n'.join(out)),('shore-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),ranks=len(RANKS))))
if __name__=='__main__':main()
