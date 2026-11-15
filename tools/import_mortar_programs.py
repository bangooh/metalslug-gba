#!/usr/bin/env python3
"""Mortar/cannon 60..63, attached crew, shells and destruction."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS={'MT_IDLE': 2906336, 'MT_IDLE_HURT': 2906366, 'MT_DRIVE': 2906152, 'MT_DRIVE_HURT': 2906198, 'MT_DEAD': 2906392, 'CREW_TURN': 2735924, 'CREW_FIRE': 2734704, 'CREW_DEAD': 307510, 'CN_WAIT': 2906802, 'CN_OPEN': 2906822, 'CN_IDLE_A': 2907024, 'CN_IDLE_B': 2907004, 'CN_IDLE_HURT_A': 2907064, 'CN_IDLE_HURT_B': 2907044, 'CN_TURN_A': 2907084, 'CN_TURN_B': 2907176, 'CN_TURN_HURT_A': 2907268, 'CN_TURN_HURT_B': 2907360, 'CN_FIRE_A': 2907532, 'CN_FIRE_B': 2907452, 'CN_FIRE_HURT_A': 2907692, 'CN_FIRE_HURT_B': 2907612, 'CN_DEAD': 2907772, 'CN_SHELL': 2907794, 'CN_MUZZLE': 2908254}
ROOTS.update(CRATE_IDLE=0x29c536,CRATE_DEAD=0x29c54c)
TABLES={'MT_CREW': [2905752, 4]}
SELECTORS={2907796: 25}
RANKS=dict(MT_HP=0x2b8610,CREW_WAIT=0x2b8818,CN_HP=0x2b8b64,CN_COUNT=0x2b8be6,CN_WAIT=0x2b8c68,CN_PERIOD=0x2b8cea,CRATE_HP=0x2bdc06)
FIXED=dict(MT_BODY=("DEFENSE",0x2c5356),MT_DRIVE=("ATTACK",0x2c544e),MT_WRECK=("ATTACK",0x2c5542),CN_BODY_A=("DEFENSE",0x2c5958),CN_BODY_B=("DEFENSE",0x2c59ac),CN_SHELL=("ATTACK",0x2c5a00))
TEMPLATES={60:0x6361e,61:0x63626,62:0x63ec6,63:0x63ed8,122:0x60ce8,123:0x60cde,124:0x60cd4}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',required=True,type=Path);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw,SELECTORS)
 if [c.long(0x2c2b80+4*i) for i in range(3)]!=[0x60e0c,0x60e1a,0x60e44] or [c.long(0x2c2b8c+4*i) for i in range(3)]!=[0x60e28,0x60e36,0x60e44]:raise ValueError("crate stamp dispatch")
 if any(c.long(0xe8000+4*i)!=at for i,at in TEMPLATES.items()):raise ValueError('template dispatch')
 entries={k:c.entry(v) for k,v in ROOTS.items()};tables={k:[65535 if c.long(at+4*i)==0xffffffff else c.entry(c.long(at+4*i)) for i in range(n)] for k,(at,n) in TABLES.items()};fixed={k:c.binding(*v) for k,v in FIXED.items()};c.compile()
 if {x['source'] for x in c.callbacks}!={0x4a110,0x4a0d4,0x644e8,0x6449e,0x64526,0x63df2,0x63e2c,0x4a1ba,0x63db2,0x55bce} or c.parameters or {(x['source_offset'],x['width']) for x in c.fields}!={(0x3b,1),(0x34,2),(0x74,2),(0x28,2),(0x3a,1)}:raise ValueError('unreviewed graph')
 h=['#ifndef NATIVE_MORTAR_PROGRAMS_H','#define NATIVE_MORTAR_PROGRAMS_H','#include "../src/bosses.h"',
 'enum MortarAnimation {'+','.join('MTA_'+k for k in ROOTS)+',MTA_COUNT};','enum MortarCallback {'+','.join('MTC_'+format(x['source'],'X') for x in c.callbacks)+'};','enum MortarField {'+','.join('MTF_'+format(x['source_offset'],'X') for x in c.fields)+'};','enum MortarBinding {'+','.join('MTB_'+k+'='+str(v) for k,v in fixed.items())+'};','enum MortarRank {'+','.join('MTR_'+k for k in RANKS)+',MTR_COUNT};',
 'extern const struct AnimationProgram mortar_program;','extern const struct BossBindingSet mortar_bindings['+str(len(c.bindings))+'];','extern const uint16_t mortar_entries[MTA_COUNT],mortar_rank[MTR_COUNT][64];',
 *['extern const uint16_t mortar_'+k.lower()+'['+str(n)+'];' for k,(_,n) in TABLES.items()],
 'extern const int16_t mortar_bounds[4][4],mortar_crew_offsets[4],mortar_crew_launch[4][3],mortar_shell[4][3];',
 'extern const struct InteractionContact mortar_contacts[2];','struct MortarStamp {const uint8_t*cells;int16_t dx,dy;uint16_t width,height;};','extern const struct MortarStamp mortar_stamps[8];',
 'struct MortarCrew {int16_t x,y;uint8_t kind;};','extern const struct MortarCrew mortar_crews[4][4];','extern const uint8_t mortar_crew_counts[4];']
 out=c.c_source('mortar_program','mortar-programs.h');defs=[];attacks=[];sets=[];filters={}
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


 array('struct BossBindingSet','mortar_bindings[]',','.join(sets));array('uint16_t','mortar_entries[MTA_COUNT]',vals(entries.values()))
 for k,v in tables.items():array('uint16_t','mortar_'+k.lower()+'[]',vals(v))
 if any(c.word(at) not in (0,1) for at in RANKS.values()):raise ValueError('rank mode')
 array('uint16_t','mortar_rank[MTR_COUNT][64]',','.join('{'+vals(c.word(at+2+2*i) for i in range(64))+'}' for at in RANKS.values()))
 array('int16_t','mortar_bounds[4][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x2c55e6,0x2c5a54,0x2c5a5c,0x2c2c60)))
 array('int16_t','mortar_crew_offsets[4]',vals(c.signed(0x2c5820+2*i) for i in range(4)))
 for name,at in [('mortar_crew_launch',0x2c5718),('mortar_shell',0x2c5a9a)]:array('int16_t',name+'[4][3]',','.join('{'+vals(c.signed(at+6*i+2*j) for j in range(3))+'}' for i in range(4)))
 contacts=[]
 for at in (0x2c55ee,0x2c563a):
  if c.byte(at)!=128:raise ValueError('contact format')
  contacts.append('{{'+vals(c.signed(at+6+2*i) for i in range(4))+'},'+vals([c.signed(at+2),c.byte(at+1)])+'}')
 array('struct InteractionContact','mortar_contacts[2]',','.join(contacts))
 crews=[];counts=[]
 for i in range(4):
  at=c.long(0x2c56a8+4*i);row=[]
  while c.long(at)!=0xffffffff:
   kind=c.long(at)
   if kind not in (0x63960,0x639b4) or len(row)>=4:raise ValueError('crew type/bound')
   row.append('{'+vals([c.signed(at+4),c.signed(at+6),int(kind==0x63960)])+'}');at+=8
  counts.append(len(row));crews.append('{'+','.join(row+['{0,0,0}']*(4-len(row)))+'}')
 array('struct MortarCrew','mortar_crews[4][4]',','.join(crews));array('uint8_t','mortar_crew_counts[4]',vals(counts))
 rows=[]
 for i,at in enumerate((0x2c5730,0x2c576c,0x2c57a8,0x2c57e4,0x2c2b98,0x2c2bc4,0x2c2bf0,0x2c2c28)):
  dims=[c.signed(at+2*j) for j in range(4)];data=[c.byte(at+8+j) for j in range(dims[2]*dims[3])];array('uint8_t',f'mortar_stamp{i}[]',vals(data));rows.append('{mortar_stamp'+str(i)+','+vals(dims)+'}')
 array('struct MortarStamp','mortar_stamps[8]',','.join(rows));h.append('#endif')
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,tables=TABLES,rank_sources=RANKS,selector_counts=SELECTORS,templates=TEMPLATES,implementation=['src/mortar.c'],validation='Source conversion only; no gameplay tests',complete_port=False,source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in [(0x60cd4,0x60e62),(0x6361e,0x64550),(0x6a7d6,0x6a7e8),(0x57226,0x5724e)]])
 for name,value in [('mortar-programs.h','\n'.join(h)),('mortar-programs.c','\n'.join(out)),('mortar-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),ranks=len(RANKS))))
if __name__=='__main__':main()
