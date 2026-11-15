#!/usr/bin/env python3
"""Offline conversion of the source-labelled FinalBoss group, templates 114..116.
The labels do not establish a canonical in-game boss name or mission number.
"""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(IDLE=0x2d9332,REVERSE=0x2d9362,TURN=0x2d9392,WAIT=0x2d93cc,RESUME=0x2d93ee,
 STAND=0x2d9428,STAND_B=0x2d9462,DIE=0x2d94f0,LIMB0=0x2d9508,LIMB1=0x2d961c,LIMB2=0x2d9714,LIMB3=0x2d9828,LIMB4=0x2d990a,
 HEAD=0x2da2d0,HEAD_A=0x2da2e2,HEAD_B=0x2da380,HEAD_C=0x2da41e,HEAD_DIE=0x2c41ea,
 CANNON=0x2d9cbe,CANNON_B=0x2d9d66,SHELL=0x2d9e36,MUZZLE=0x2d9bb8,CASING=0x2c4022,
 FLAME=0x2d997a,BEAM=0x2d9a1c,BEAM_FLIGHT=0x2d9a9c,BEAM_B=0x2d9aea,BEAM_B_FLIGHT=0x2d9b6a,
 FLAME_FOLLOW=0x2da526,FLAME_FLIGHT=0x2da58a,HEAD_SHARD=0x29ca36,PUFF=0x2c3f0a,FLASH=0x67014)
TABLES={f'PART{i}':(at,5) for i,at in enumerate((0x2d92a6,0x2d92ba,0x2d92ce,0x2d92e2,0x2d92f6))}
RANKS=dict(LEFT_HP=0x2ba79e,RIGHT_HP=0x2ba820,ATTACK_REST=0x2ba8a2,ATTACK_WAIT=0x2ba924,ATTACK_COUNT=0x2ba9a6,
 BEAM_SPEED=0x2baa28,BEAM_B_SPEED=0x2baaaa,SECOND_REST=0x2bab2c,SECOND_WAIT=0x2babae,SECOND_COUNT=0x2bac30,
 FLAME_SPEED=0x2bacb2,FLAME_ACCEL=0x2bad34,HEAD_RESPAWN=0x2badb6)
# Both shell sectors are angle >> 3; each contains 32 contiguous 18-byte draw/jump rows.
SELECTORS={0x2d9e44:32,0x2da088:32}
FIXED=dict(BODY_DEF=('DEFENSE',0x2d898c),BODY_ATK=('ATTACK',0x2d9068),SHELL_DEF=('DEFENSE',0x2d8a84),DEATH_ATK=('ATTACK',0x2d8f1c))
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);args=p.parse_args()
 raw=(args.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw,SELECTORS);entries={k:c.entry(v) for k,v in ROOTS.items()}
 tables={k:[65535 if c.long(at+4*i)==0xffffffff else c.entry(c.long(at+4*i)) for i in range(n)] for k,(at,n) in TABLES.items()}
 fixed={k:c.binding(*v) for k,v in FIXED.items()};c.compile()
 expected_calls={0x72de8,0x72f64,0x72f40,0x72e14,0x72e80,0x72da4,0x283ca,0x72f88,0x72d78,0x72d28,0x72ee8}
 if {v['source'] for v in c.callbacks}!=expected_calls or c.parameters:raise ValueError('unreviewed callback/parameter')
 if {(v['source_offset'],v['width']) for v in c.fields}!={(0x82,1),(0x34,2)}:raise ValueError('unreviewed field')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),fields=c.fields,callbacks=c.callbacks,bindings=c.bindings,parameters=c.parameters)))
 templates={114:0x71068,115:0x7114e,116:0x71216}
 if any(c.long(0xe8000+4*i)!=at for i,at in templates.items()):raise ValueError('template mapping')
 h=['#ifndef NATIVE_FINAL_PROGRAMS_H','#define NATIVE_FINAL_PROGRAMS_H','#include "../src/bosses.h"',
 'enum FinalAnimation {'+','.join('FBA_'+k for k in entries)+',FBA_COUNT};',
 'enum FinalCallback {'+','.join('FBC_'+format(v['source'],'X') for v in c.callbacks)+'};',
 'enum FinalField {'+','.join('FBF_'+format(v['source_offset'],'X')+'_'+str(v['width']) for v in c.fields)+'};',
 'enum FinalRank {'+','.join('FBR_'+k for k in RANKS)+',FBR_COUNT};',
 'enum FinalBinding {'+','.join('FBB_'+k+'='+str(v) for k,v in fixed.items())+'};',
 'extern const struct AnimationProgram final_program;','extern const uint16_t final_entries[FBA_COUNT];',
 'extern const struct BossBindingSet final_bindings['+str(len(c.bindings))+'];',
 'extern const uint16_t final_rank[FBR_COUNT][64];',
 *['extern const uint16_t final_'+k.lower()+'['+str(n)+'];' for k,(_,n) in TABLES.items()],
 'extern const uint8_t final_limb_commands[7],final_head_commands[6];',
 'extern const int16_t final_offsets[4][16][2],final_bounds[2][4],final_head_proximity[4],final_slug_box[4],final_ballistics[2][16][3];',
 'extern const struct InteractionContact final_contact;',
 'struct FinalTerrainStamp {int16_t dx,dy;uint16_t width,height;const uint8_t*cells;};','extern const struct FinalTerrainStamp final_stamp;','#endif']
 out=c.c_source('final_program','final-programs.h');defs=[];attacks=[];sets=[];filters={}
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
 array('struct BossBindingSet','final_bindings[]',','.join(sets));array('uint16_t','final_entries[FBA_COUNT]',vals(entries.values()))
 for k,v in tables.items():array('uint16_t','final_'+k.lower()+'['+str(len(v))+']',vals(v))
 if any(c.word(at)!=0 for at in RANKS.values()):raise ValueError('rank mode')
 array('uint16_t','final_rank[FBR_COUNT][64]',','.join('{'+vals(c.word(at+2+2*i) for i in range(64))+'}' for at in RANKS.values()))
 limb=[0xffffffff,0x7170e,0x71786,0x717be,0x717f6,0x71866,0x7182e];head=[0xffffffff,0x71976,0x71a4e,0x71a64,0x71a7a,0xffffffff]
 if [c.long(0x2d9272+4*i) for i in range(7)]!=limb or [c.long(0x2d928e+4*i) for i in range(6)]!=head:raise ValueError('part dispatch')
 array('uint8_t','final_limb_commands[7]',vals([0,1,2,3,4,5,6]));array('uint8_t','final_head_commands[6]',vals([0,1,2,3,4,0]))
 # $072BEA masks (pose-1) with 15; preserve the 4 neighbouring rows
 # beyond each 12-row authored table, including the pre-first-pose path.
 array('int16_t','final_offsets[4][16][2]',','.join('{'+','.join('{'+vals(c.signed(at+4*i+2*j) for j in range(2))+'}' for i in range(16))+'}' for at in (0x2d9138,0x2d9168,0x2d9198,0x2d91c8)))
 array('int16_t','final_bounds[2][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x2d90bc,0x2d90c4)))
 array('int16_t','final_head_proximity[4]',vals(c.signed(0x2d90cc+2*i) for i in range(4)))
 array('int16_t','final_slug_box[4]',vals(c.signed(0x2d9130+2*i) for i in range(4)))
 array('int16_t','final_ballistics[2][16][3]',','.join('{'+','.join('{'+vals(c.signed(at+6*i+2*j) for j in range(3))+'}' for i in range(16))+'}' for at in (0x2bae38,0x2bae98)))
 at=0x2d90e4
 if c.byte(at)!=128:raise ValueError('contact format')
 array('struct InteractionContact','final_contact','{'+vals(c.signed(at+6+2*i) for i in range(4))+'},'+vals([c.signed(at+2),c.byte(at+1)]))
 at=0x2d920a;sw=c.word(at+4);sh=c.word(at+6)
 array('uint8_t',f'final_stamp_cells[{sw*sh}]',vals(c.byte(at+8+i) for i in range(sw*sh)))
 array('struct FinalTerrainStamp','final_stamp',vals([c.signed(at),-c.signed(at+2),sw,sh])+',final_stamp_cells')
 files=['asm/enemies/bosses/gunship_m5boss_finalboss_06dfe8.s','asm/enemies/bosses/m5tank_finalboss_scriptedprop_071ffc.s']
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,tables=TABLES,rank_sources=RANKS,templates=templates,
  source_files={p:hashlib.sha256((args.source/p).read_bytes()).hexdigest() for p in files},source_ranges=[dict(start=0x71068,end_exclusive=0x72fc8,sha256=hashlib.sha256(raw[0x71068:0x72fc8]).hexdigest())],
  implementation=['src/final_boss.c','src/final_parts.c'],validation='Source conversion only, not gameplay validation',complete_port=False)
 for name,data in [('final-programs.h','\n'.join(h)),('final-programs.c','\n'.join(out)),('final-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(data+'\n')
if __name__=='__main__':main()
