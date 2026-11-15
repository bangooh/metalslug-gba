#!/usr/bin/env python3
"""Shield soldiers 108/109 and M4 lift/rescue objects 267/275, source data."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(IDLE=0x2c7d1e,GUARD=0x2c7ee6,FALL=0x2c7f54,WALK=0x2c7da4,STOP=0x2c7e2a,BACK=0x2c7e92,CROUCH=0x2c7fb0,CROUCH_FIRE=0x2c802a,RISE=0x2c806e,AIM=0x2c80e4,FIRE=0x2c8146,RECOVER=0x2c82a0,HIT_SMALL=0x2c834c,HIT=0x2c82ec,LOST=0x29bfc4,SHIELD_FALL=0x2c836c,BULLET=0x2c83f2,MUZZLE=0x2c8428,LIFT_UP=0x2e7828,LIFT_DOWN=0x2e7866,LIFT_STOP=0x2e78a4,RESCUE=0x2e6bf0)
RANKS=dict(SHIELD_HP=0x2bf59a,SPEED=0x2bf61c,DISTANCE=0x2bf6de,BULLET=0x2bf7a0,CROUCH_COUNT=0x2bf822,CROUCH_COOLDOWN=0x2bf8a4,CROUCH_WAIT=0x2bf926,AIM_COUNT=0x2bf9a8,AIM_COOLDOWN=0x2bfa2a,AIM_WAIT=0x2bfaac)
FIXED=dict(BODY=('DEFENSE',0x2c79e2),WALK=('DEFENSE',0x2c7a8a),SHIELD=('DEFENSE',0x2c7a36),RESCUE0=('DEFENSE',0x2e92aa),RESCUE1=('DEFENSE',0x2e92fe),RESCUE2=('DEFENSE',0x2e9352))
TEMPLATES={108:0x670d2,109:0x676ba,267:0x84f26,275:0x84db0}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',required=True,type=Path);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw)
 if any(c.long(0xe8000+4*i)!=at for i,at in TEMPLATES.items()):raise ValueError('template dispatch')
 entries={k:c.entry(v) for k,v in ROOTS.items()};poses=[65535 if c.long(0x2c7cfe+4*i)==0xffffffff else c.entry(c.long(0x2c7cfe+4*i)) for i in range(8)];fixed={k:c.binding(*v) for k,v in FIXED.items()};c.compile()
 if {x['source'] for x in c.callbacks}!={0x68206,0x4a120,0x283ca} or c.parameters or {(x['source_offset'],x['width']) for x in c.fields}!={(0x80,1),(0x3b,1),(0x81,1),(0x94,4),(0x16,2),(0x18,2),(0x1a,2),(0x3a,1)}:raise ValueError('unreviewed graph')
 h=['#ifndef NATIVE_SHIELD_PROGRAMS_H','#define NATIVE_SHIELD_PROGRAMS_H','#include "../src/bosses.h"',
 'enum ShieldAnimation {'+','.join('SHA_'+k for k in ROOTS)+',SHA_COUNT};','enum ShieldField {'+','.join('SHF_'+format(x['source_offset'],'X') for x in c.fields)+'};','enum ShieldCallback {'+','.join('SHC_'+format(x['source'],'X') for x in c.callbacks)+'};','enum ShieldBinding {'+','.join('SHB_'+k+'='+str(v) for k,v in fixed.items())+'};','enum ShieldRank {'+','.join('SHR_'+k for k in RANKS)+',SHR_COUNT};',
 'extern const struct AnimationProgram shield_program;','extern const struct BossBindingSet shield_bindings['+str(len(c.bindings))+'];','extern const uint16_t shield_entries[SHA_COUNT],shield_poses[8];','struct ShieldRankData {const uint16_t*values;const uint8_t*choices;};','extern const struct ShieldRankData shield_rank[SHR_COUNT];','extern const uint8_t shield_delays[4][12];','extern const int16_t shield_bounds[5][4],shield_boxes[4][4];','extern const struct InteractionContact shield_contact;','extern const struct BossPatternData shield_rescue_patterns[3];']
 out=c.c_source('shield_program','shield-programs.h');defs=[];attacks=[];sets=[];filters={}
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


 array('struct BossBindingSet','shield_bindings[]',','.join(sets));array('uint16_t','shield_entries[SHA_COUNT]',vals(entries.values()));array('uint16_t','shield_poses[8]',vals(poses))
 rank_rows=[]
 for i,at in enumerate(RANKS.values()):
  mode=c.word(at)
  if mode==2:
   choices=[c.byte(at+2+j) for j in range(64)]
   array('uint8_t',f'shield_choices{i}[64]',vals(choices))
   array('uint16_t',f'shield_values{i}[{(max(choices)+1)*8}]',vals(c.word(at+66+2*j) for j in range((max(choices)+1)*8)))
   rank_rows.append('{shield_values'+str(i)+',shield_choices'+str(i)+'}')
  else:
   if mode not in (0,1):raise ValueError('rank mode')
   array('uint16_t',f'shield_values{i}[64]',vals(c.word(at+2+2*j) for j in range(64)))
   rank_rows.append('{shield_values'+str(i)+',0}')
 array('struct ShieldRankData','shield_rank[SHR_COUNT]',','.join(rank_rows))

 array('uint8_t','shield_delays[4][12]',','.join('{'+vals(c.byte(c.long(0x2c7d64+4*i)+j) for j in range(12))+'}' for i in range(4)))
 array('int16_t','shield_bounds[5][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x2c7ce6,0x2e9860,0x2e9838,0x2e9842,0x2e984c)))
 array('int16_t','shield_boxes[4][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x2c7cce,0x2c7cd6,0x2c7cee,0x2c7cf6)))
 if c.byte(0x2c7c82)!=128:raise ValueError('contact format')
 array('struct InteractionContact','shield_contact','{'+vals(c.signed(0x2c7c88+2*i) for i in range(4))+'},'+vals([c.signed(0x2c7c84),c.byte(0x2c7c83)]))
 rows=[]
 for at in (0x2e9c12,0x2e9c24,0x2e9c36):
  if c.byte(at)!=1 or c.byte(at+1)!=255 or c.word(at+2)!=1 or c.byte(at+13) or c.long(at+14)!=0x3fec6:raise ValueError('rescue factory')
  rows.append('{'+vals([c.signed(at+4),c.signed(at+8),c.word(at+6),c.word(at+10),c.word(at+2),c.byte(at),c.byte(at+1),0,0])+'}')
 array('struct BossPatternData','shield_rescue_patterns[3]',','.join(rows));h.append('#endif')
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,rank_sources=RANKS,templates=TEMPLATES,implementation=['src/shield.c','src/m4_lift.c'],validation='Source conversion only; no gameplay tests',complete_port=False,source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in [(0x670d2,0x682d0),(0x84db0,0x84ec2),(0x84f26,0x85190),(0x85ee8,0x85fb0)]])
 for name,value in [('shield-programs.h','\n'.join(h)),('shield-programs.c','\n'.join(out)),('shield-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),ranks=len(RANKS))))
if __name__=='__main__':main()
