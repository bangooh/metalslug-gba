#!/usr/bin/env python3
"""Walker 66..74 body, ordnance, palettes, rank and terrain stamps."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(IDLE_A=0x2d428c,IDLE_B=0x2d4298,PAUSE=0x2d42a4,WAKE=0x2d42b0,FIRST=0x2d42e8,FIRE=0x2d431e,DEAD=0x2d4394,SHELL=0x2d45fc,TRAIL=0x2d4628,MUZZLE=0x2d455a,IMPACT=0x2dd37e)
TABLES={}
RANKS=dict(HP=0x2b8d6c,WAIT=0x2b8dee,PERIOD=0x2b8e70,REPEATS=0x2b8ef2,TRAJECTORY=0x2b8f74)
FIXED=dict(BODY=("DEFENSE",0x2d3bae),IMPACT=("ATTACK",0x2d3f8a))
TEMPLATES=dict(zip(range(66,75),(0x6d654,0x6d686,0x6d6ba,0x6d660,0x6d692,0x6d6c6,0x6d66e,0x6d6a0,0x6d6d4)))
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',required=True,type=Path);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw)
 if any(c.long(0xe8000+4*i)!=at for i,at in TEMPLATES.items()):raise ValueError('template dispatch')
 entries={k:c.entry(v) for k,v in ROOTS.items()};tables={k:[65535 if c.long(at+4*i)==0xffffffff else c.entry(c.long(at+4*i)) for i in range(n)] for k,(at,n) in TABLES.items()};fixed={k:c.binding(*v) for k,v in FIXED.items()};c.compile()
 if {x['source'] for x in c.callbacks}!={0x6e3a4,0x283ca,0x6e456,0x6e4cc,0x6e436,0x6e412} or c.parameters or {(x['source_offset'],x['width']) for x in c.fields}!={(0x20,1),(0x18,2),(0x16,2)}:raise ValueError('unreviewed graph')
 h=['#ifndef NATIVE_WALKER_PROGRAMS_H','#define NATIVE_WALKER_PROGRAMS_H','#include "../src/bosses.h"',
 'enum WalkerAnimation {'+','.join('WKA_'+k for k in ROOTS)+',WKA_COUNT};','enum WalkerCallback {'+','.join('WKC_'+format(x['source'],'X') for x in c.callbacks)+'};','enum WalkerField {'+','.join('WKF_'+format(x['source_offset'],'X') for x in c.fields)+'};','enum WalkerBinding {'+','.join('WKB_'+k+'='+str(v) for k,v in fixed.items())+'};','enum WalkerRank {'+','.join('WKR_'+k for k in RANKS)+',WKR_COUNT};',
 'extern const struct AnimationProgram walker_program;','extern const struct BossBindingSet walker_bindings['+str(len(c.bindings))+'];','extern const uint16_t walker_entries[WKA_COUNT],walker_rank[WKR_COUNT][64];',
 *['extern const uint16_t walker_'+k.lower()+'['+str(n)+'];' for k,(_,n) in TABLES.items()],
 'extern const uint16_t walker_palettes[4];','extern const int16_t walker_trajectories[5][4][3],walker_bounds[2][4];','extern const struct HitRect walker_filter;',
 'extern const struct InteractionContact walker_contact;','struct WalkerStamp {const uint8_t*cells;int16_t dx,dy;uint16_t width,height;};','extern const struct WalkerStamp walker_stamps[2];']
 out=c.c_source('walker_program','walker-programs.h');defs=[];attacks=[];sets=[];filters={}
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


 array('struct BossBindingSet','walker_bindings[]',','.join(sets));array('uint16_t','walker_entries[WKA_COUNT]',vals(entries.values()))
 for k,v in tables.items():array('uint16_t','walker_'+k.lower()+'[]',vals(v))
 if any(c.word(at) not in (0,1) for at in RANKS.values()):raise ValueError('rank mode')
 array('uint16_t','walker_rank[WKR_COUNT][64]',','.join('{'+vals(c.word(at+2+2*i) for i in range(64))+'}' for at in RANKS.values()))
 array('uint16_t','walker_palettes[4]',vals(c.word(0x2d4284+2*i) for i in range(4)))
 array('int16_t','walker_trajectories[5][4][3]',','.join('{'+','.join('{'+vals(c.signed(c.long(0x2d4186+4*i)+6*j+2*k) for k in range(3))+'}' for j in range(4))+'}' for i in range(5)))
 array('int16_t','walker_bounds[2][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x2d402e,0x2d404e)))
 array('struct HitRect','walker_filter',vals(c.signed(0x2d4036+2*i) for i in range(4)))
 if c.byte(0x2d4056)!=128:raise ValueError('contact format')
 array('struct InteractionContact','walker_contact','{'+vals(c.signed(0x2d405c+2*i) for i in range(4))+'},'+vals([c.signed(0x2d4058),c.byte(0x2d4057)]))
 rows=[]
 for i,at in enumerate((0x2d422a,0x2d424a)):
  dims=[c.signed(at+2*j) for j in range(4)];data=[c.byte(at+8+j) for j in range(dims[2]*dims[3])];array('uint8_t',f'walker_stamp{i}[]',vals(data));rows.append('{walker_stamp'+str(i)+','+vals(dims)+'}')
 array('struct WalkerStamp','walker_stamps[2]',','.join(rows));h.append('#endif')
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,tables=TABLES,rank_sources=RANKS,templates=TEMPLATES,implementation=['src/walker.c'],validation='Source conversion only; no gameplay tests',complete_port=False,source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in [(0x6d654,0x6dce0),(0x6e176,0x6e224),(0x6e31e,0x6e52a)]])
 for name,value in [('walker-programs.h','\n'.join(h)),('walker-programs.c','\n'.join(out)),('walker-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),ranks=len(RANKS))))
if __name__=='__main__':main()
