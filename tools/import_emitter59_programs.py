#!/usr/bin/env python3
"""Template59 emitter, combat actor, shells and breakup: source conversion only."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS={'INTRO': 2902644, 'OPEN': 2902690, 'IDLE': 2902840, 'TURN': 2902998, 'RELEASE': 2904304, 'FALL': 2904418, 'LANDED': 2904522, 'DEAD': 2904630, 'SHELL': 2904648, 'MUZZLE': 2923616, 'RELEASED': 3023494, 'SMOKE': 3003262, 'EXHAUST': 2904762, 'FRAGMENT_A': 3006366, 'FRAGMENT_B': 3006484, 'METAL_A': 3007546, 'METAL_B': 3007988, 'SPARK': 3004202, 'PUFF': 3004428}
TABLES={'FIRE': (2902508, 4), 'FAST_FIRE': (2902524, 4)}
SELECTORS={2902842: 4, 2903000: 8, 2902918: 4, 2903148: 8}
RANKS=dict(HP=0x2b80fc,BURST=0x2b817e,WAIT=0x2b8200,FIRE_WAIT=0x2b8282,TRAJECTORY=0x2b8304,TURN=0x2b8386,COUNT0=0x2b8408,COUNT1=0x2b848a,COUNT2=0x2b850c,COUNT3=0x2b858e)
FIXED=dict(BODY=("DEFENSE",0x2c459c),WRECK=("ATTACK",0x2c4920))
TEMPLATES={59:0x6293e}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',required=True,type=Path);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw,SELECTORS)
 if any(c.long(0xe8000+4*i)!=at for i,at in TEMPLATES.items()):raise ValueError('template dispatch')
 entries={k:c.entry(v) for k,v in ROOTS.items()};tables={k:[65535 if c.long(at+4*i)==0xffffffff else c.entry(c.long(at+4*i)) for i in range(n)] for k,(at,n) in TABLES.items()};fixed={k:c.binding(*v) for k,v in FIXED.items()};c.compile()
 if {x['source'] for x in c.callbacks}!={406944, 406820, 164810, 406574, 406774, 407000, 406876, 406654} or c.parameters:raise ValueError('unreviewed graph')
 if {(x['source_offset'],x['width']) for x in c.fields}!={(46, 2), (52, 2), (124, 2)}:raise ValueError('unreviewed fields')
 h=['#ifndef NATIVE_EMITTER59_PROGRAMS_H','#define NATIVE_EMITTER59_PROGRAMS_H','#include "../src/bosses.h"',
 'enum Emitter59Animation {'+','.join('E59A_'+k for k in ROOTS)+',E59A_COUNT};',
 'enum Emitter59Callback {'+','.join('E59C_'+format(x['source'],'X') for x in c.callbacks)+'};',
 'enum Emitter59Field {'+','.join('E59F_'+format(x['source_offset'],'X')+'_'+str(x['width']) for x in c.fields)+'};',
 'enum Emitter59Binding {'+','.join('E59B_'+k+'='+str(v) for k,v in fixed.items())+'};',
 'enum Emitter59Rank {'+','.join('E59R_'+k for k in RANKS)+',E59R_COUNT};',
 'extern const struct AnimationProgram emitter59_program;','extern const struct BossBindingSet emitter59_bindings['+str(len(c.bindings))+'];','extern const uint16_t emitter59_entries[E59A_COUNT];',
 'struct Emitter59RankData {const uint16_t*values;const uint8_t*choices;};','extern const struct Emitter59RankData emitter59_rank[E59R_COUNT];',
 *['extern const uint16_t emitter59_'+k.lower()+'['+str(n)+'];' for k,(_,n) in TABLES.items()],
 'extern const int16_t emitter59_bounds[2][4],emitter59_shake[4],emitter59_muzzles[4][2],emitter59_vectors[4][4][3];',
 'extern const uint8_t emitter59_release_choices[4];',
 'struct Emitter59Stamp {const uint8_t*cells;int16_t dx,dy;uint16_t width,height;};','extern const struct Emitter59Stamp emitter59_stamp;']
 out=c.c_source('emitter59_program','emitter59-programs.h');defs=[];attacks=[];sets=[];filters={}
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
 out+=['static const struct HitDefense defs[]={'+','.join(defs)+'};']
 if attacks:out.append('static const struct HitAttack attacks[]={'+','.join(attacks)+'};')


 array('struct BossBindingSet','emitter59_bindings[]',','.join(sets));array('uint16_t','emitter59_entries[E59A_COUNT]',vals(entries.values()))
 for k,v in tables.items():array('uint16_t','emitter59_'+k.lower()+'[]',vals(v))
 rank_rows=[]
 for i,at in enumerate(RANKS.values()):
  mode=c.word(at)
  if mode==2:
   choices=[c.byte(at+2+j) for j in range(64)]
   array('uint8_t',f'emitter59_choices{i}[64]',vals(choices))
   array('uint16_t',f'emitter59_values{i}[{(max(choices)+1)*8}]',vals(c.word(at+66+2*j) for j in range((max(choices)+1)*8)))
   rank_rows.append('{emitter59_values'+str(i)+',emitter59_choices'+str(i)+'}')
  else:
   if mode not in (0,1):raise ValueError('rank mode')
   array('uint16_t',f'emitter59_values{i}[64]',vals(c.word(at+2+2*j) for j in range(64)))
   rank_rows.append('{emitter59_values'+str(i)+',0}')
 array('struct Emitter59RankData','emitter59_rank[E59R_COUNT]',','.join(rank_rows))
 array('int16_t','emitter59_bounds[2][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x2c49c4,0x2c49cc)))
 array('int16_t','emitter59_shake[4]',vals(c.signed(0x2c49d4+2*i) for i in range(4)))
 array('int16_t','emitter59_muzzles[4][2]',','.join('{'+vals(c.signed(0x2c49dc+4*i+2*j) for j in range(2))+'}' for i in range(4)))
 array('int16_t','emitter59_vectors[4][4][3]',','.join('{'+','.join('{'+vals(c.signed(c.long(0x2c4a0c+4*i)+6*j+2*k) for k in range(3))+'}' for j in range(4))+'}' for i in range(4)))
 choices=[0x6313c,0x63142,0x6314e];array('uint8_t','emitter59_release_choices[4]',vals(choices.index(c.long(0x2c4a44+4*i)) for i in range(4)))
 at=0x2c4a54;dims=[c.signed(at+2*i) for i in range(4)];array('uint8_t','emitter59_stamp_cells[]',vals(c.byte(at+8+i) for i in range(dims[2]*dims[3])));array('struct Emitter59Stamp','emitter59_stamp','emitter59_stamp_cells,'+vals(dims));h.append('#endif')
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,templates=TEMPLATES,rank_sources=RANKS,selector_bounds=SELECTORS,implementation=['src/emitter59.c','src/emitter59_effects.c'],pending=['063106 reads incoming D1 rather than RNG D0; automatic source selector is not yet resolved'],complete_port=False,validation='Source conversion only; no gameplay tests',source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in [(0x6293e,0x6361e)]])
 for name,value in [('emitter59-programs.h','\n'.join(h)),('emitter59-programs.c','\n'.join(out)),('emitter59-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),ranks=len(RANKS))))
if __name__=='__main__':main()
