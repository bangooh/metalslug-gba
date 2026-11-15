#!/usr/bin/env python3
"""Ground nest vehicles 50..52, crew escape, local ordnance and terrain stamps."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS={'DRIVE': 2897938, 'REVERSE': 2898048, 'AVOID': 2898186, 'IDLE': 2898332, 'BRAKE': 2898394, 'BRAKE_BACK': 2898466, 'RESUME': 2898538, 'RESUME_BACK': 2898668, 'AIM': 2898798, 'FIRE': 2898912, 'FIRE_FAST': 2899060, 'HIT_BIG': 2899180, 'HIT': 2899322, 'DEAD': 2899364, 'WRECK': 2899528, 'DEAD_BIG': 2899612, 'CREW_A': 2900282, 'CREW_NEXT': 2900360, 'CREW_HELMET': 2900230, 'CREW_B': 2900164, 'CREW_DEAD': 2900458, 'CREW_DEAD_BACK': 2900596, 'ROCKET': 2901252, 'PIECE': 2900888, 'HELMET': 2901070, 'MUZZLE': 2900742, 'FLAME': 2899722, 'FLASH': 3004910, 'SMOKE': 2900002}
TABLES={}
RANKS=dict(HP=0x2b7a22,SHOT=0x2b7aa4,WAIT=0x2b7b26,REPEATS=0x2b7ba8,PERIOD=0x2b7c2a,AVOID=0x2b7cac)
FIXED=dict(BODY=("DEFENSE",0x2c3458),DRIVE=("ATTACK",0x2c3554),WRECK=("ATTACK",0x2c3650))
TEMPLATES={50:0x619ca,51:0x619d6,52:0x619e4}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',required=True,type=Path);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw)
 if any(c.long(0xe8000+4*i)!=at for i,at in TEMPLATES.items()):raise ValueError('template dispatch')
 entries={k:c.entry(v) for k,v in ROOTS.items()};tables={k:[65535 if c.long(at+4*i)==0xffffffff else c.entry(c.long(at+4*i)) for i in range(n)] for k,(at,n) in TABLES.items()};fixed={k:c.binding(*v) for k,v in FIXED.items()};c.compile()
 if {x['source'] for x in c.callbacks}!={0x627ca,0x62774,0x62816,0x628ac,0x283ca,0x62910,0x6289c,0x6288a,0x62872} or c.parameters or {(x['source_offset'],x['width']) for x in c.fields}!={(0x3b,1),(0x5c,4),(0x2e,2),(0x3a,1),(0x2a,2)}:raise ValueError('unreviewed graph')
 h=['#ifndef NATIVE_GROUND_NEST_PROGRAMS_H','#define NATIVE_GROUND_NEST_PROGRAMS_H','#include "../src/bosses.h"',
 'enum GroundNestAnimation {'+','.join('GNA_'+k for k in ROOTS)+',GNA_COUNT};','enum GroundNestCallback {'+','.join('GNC_'+format(x['source'],'X') for x in c.callbacks)+'};','enum GroundNestField {'+','.join('GNF_'+format(x['source_offset'],'X') for x in c.fields)+'};','enum GroundNestBinding {'+','.join('GNB_'+k+'='+str(v) for k,v in fixed.items())+'};','enum GroundNestRank {'+','.join('GNR_'+k for k in RANKS)+',GNR_COUNT};',
 'extern const struct AnimationProgram ground_nest_program;','extern const struct BossBindingSet ground_nest_bindings['+str(len(c.bindings))+'];','extern const uint16_t ground_nest_entries[GNA_COUNT],ground_nest_rank[GNR_COUNT][64];',
 *['extern const uint16_t ground_nest_'+k.lower()+'['+str(n)+'];' for k,(_,n) in TABLES.items()],
 'extern const uint8_t ground_nest_rank_choices[64];','extern const uint16_t ground_nest_rank_values[8][8];',
 'extern const uint8_t ground_nest_delays[2][4][6],ground_nest_avoid_delays[4][13],ground_nest_palettes[4];','extern const int16_t ground_nest_bounds[4];','extern const struct InteractionContact ground_nest_contact;','struct GroundNestStamp {const uint8_t*cells;int16_t dx,dy;uint16_t width,height;};','extern const struct GroundNestStamp ground_nest_stamps[2];']
 out=c.c_source('ground_nest_program','ground-nest-programs.h');defs=[];attacks=[];sets=[];filters={}
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


 array('struct BossBindingSet','ground_nest_bindings[]',','.join(sets));array('uint16_t','ground_nest_entries[GNA_COUNT]',vals(entries.values()))
 for k,v in tables.items():array('uint16_t','ground_nest_'+k.lower()+'[]',vals(v))
 if any(c.word(at) not in (0,1) for at in RANKS.values()):raise ValueError('rank mode')
 array('uint16_t','ground_nest_rank[GNR_COUNT][64]',','.join('{'+vals(c.word(at+2+2*i) for i in range(64))+'}' for at in RANKS.values()))
 if c.word(0x2b7d2e)!=2:raise ValueError('random rank mode')
 array('uint8_t','ground_nest_rank_choices[64]',vals(c.byte(0x2b7d30+i) for i in range(64)))
 array('uint16_t','ground_nest_rank_values[8][8]',','.join('{'+vals(c.word(0x2b7d70+16*i+2*j) for j in range(8))+'}' for i in range(8)))
 array('uint8_t','ground_nest_delays[2][4][6]',','.join('{'+','.join('{'+vals(c.byte(c.long(at+4*i)+j) for j in range(6))+'}' for i in range(4))+'}' for at in (0x2c37ea,0x2c3858)))
 array('uint8_t','ground_nest_avoid_delays[4][13]',','.join('{'+vals(c.byte(c.long(0x2c38c6+4*i)+j) for j in range(13))+'}' for i in range(4)))
 palette_offsets=[c.word(0x2c3750+2*i) for i in range(4)]
 if any(v not in (0x16,0x18,0x1a) for v in palette_offsets):raise ValueError('palette slot')
 array('uint8_t','ground_nest_palettes[4]',vals((v-0x16)//2 for v in palette_offsets))
 array('int16_t','ground_nest_bounds[4]',vals(c.signed(0x2c36f4+2*i) for i in range(4)))
 if c.byte(0x2c3704)!=128:raise ValueError('contact format')
 array('struct InteractionContact','ground_nest_contact','{'+vals(c.signed(0x2c370a+2*i) for i in range(4))+'},'+vals([c.signed(0x2c3706),c.byte(0x2c3705)]))
 rows=[]
 for i,at in enumerate((0x2c379a,0x2c37c2)):
  dims=[c.signed(at+2*j) for j in range(4)];data=[c.byte(at+8+j) for j in range(dims[2]*dims[3])];array('uint8_t',f'ground_nest_stamp{i}[]',vals(data));rows.append('{ground_nest_stamp'+str(i)+','+vals(dims)+'}')
 array('struct GroundNestStamp','ground_nest_stamps[2]',','.join(rows));h.append('#endif')
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,tables=TABLES,rank_sources=RANKS,random_rank_source=0x2b7d2e,templates=TEMPLATES,implementation=['src/ground_nest.c'],validation='Source conversion only; no gameplay tests',complete_port=False,source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in [(0x619ca,0x6293e),(0x5e5a8,0x5e64c),(0x58fe2,0x59002)]])
 for name,value in [('ground-nest-programs.h','\n'.join(h)),('ground-nest-programs.c','\n'.join(out)),('ground-nest-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),ranks=len(RANKS))))
if __name__=='__main__':main()
