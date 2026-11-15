#!/usr/bin/env python3
"""Offline item/score data compiler. No executable ROM pointers in C."""
import argparse,hashlib,json
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS={'TAKEN':0x2f674c,'FLY':0x2f678a,'HMG':0x2f67dc,'ROCKET':0x2f67e8,
 'SHOTGUN':0x2f67f4,'FLAME':0x2f6800,'AMMO':0x2f680c,'GRENADE':0x2f685c,'REPAIR':0x2f68ac,
 'TIMED':0x2f6a1a,'TIMED_CHANGE':0x2f6a5c,'TIMED_END':0x2f6aa8,'ROT_WAIT':0x2f6f06,'ROT':0x2f6f34}
FOOD=[(0x2f6b78,0,0),(0x2f6b22,0,0),(0x2f6ae0,1,0),(0x2f68fc,0,0),
 (0x2f6bce,0,1),(0x2f6cca,0,1),(0x2f6e4c,0,1),(0x2f7002,1,0),(0x2f706c,0,0),
 (0x2f70b8,0,1),(0x2f7186,0,0),(0x2f7204,0,0),(0x2f7290,0,1),(0x2f73b8,0,1),
 (0x2f745e,0,1),(0x2f74a0,0,0),(0x2f75f2,2,1),(0x2f76e8,1,0),(0x2f755a,0,0),(0x2f75b0,0,1)]
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args()
 raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('P-ROM fingerprint')
 c=AnimationCompiler(raw);entries={k:c.entry(v) for k,v in ROOTS.items()}
 foods=[(c.entry(at),0x1c6 if pal else 0x1ac,kind) for at,kind,pal in FOOD]
 glyphs=[65535 if c.long(0x2f77a4+i*4)==0xffffffff else c.entry(c.long(0x2f77a4+i*4)) for i in range(32)]
 cycle_roots=[c.entry(c.long(0x2f7a28+i*4)) for i in range(4)]
 c.compile()
 if c.fields!=[dict(source_offset=24,width=2),dict(source_offset=152,width=1),dict(source_offset=59,width=1),dict(source_offset=22,width=2)] or c.callbacks or c.bindings or c.parameters:
  raise ValueError('unreviewed item animation owner operations')
 if any(n['op'] not in ('WAIT','DRAW','END','JUMP','SET','PALETTE','LOOP') for n in c.nodes):raise ValueError('unreviewed item animation opcode')
 handlers=[c.long(0xe8000+i*4) for i in range(284,316)]
 expected=[0x9a918,0x9a976,0x9a9d4,0x9aa32,0x9aa90,0x9abc2,0x9ac52,0x9ad54,0x2aea4,0x9b3c6,
  0x9aec4,0x9aed8,0x9b11e,0x9aeec,0x9af02,0x9af16,0x9af3e,0x9af2a,0x9b1ca,
  0x9af52,0x9af68,0x9af7c,0x9af90,0x9afa4,0x9afb8,0x9afcc,0x9afe0,0x9aff4,0x9b034,0x9b048,0x9b008,0x9b01e]
 if handlers!=expected:raise ValueError('template dispatch changed')
 template_kinds=[0,1,2,3,4,5,6,7,255,255,8,9,28,10,11,12,14,13,29,15,16,17,18,19,20,21,22,23,26,27,24,25]
 seqs=[]
 for i in range(11):
  at=c.long(0x2f79ce+i*4);row=[]
  for j in range(16):
   v=c.byte(at+j);row.append(v)
   if v==255:break
  if row[-1]!=255:raise ValueError('cycle bound')
  seqs.append(row)
 def bcd(at):
  value=c.long(at)
  if any((value>>(4*i)&15)>9 for i in range(8)):raise ValueError('BCD table')
  return value
 scores=[[bcd(at+4*i) for i in range(16)] for at in (0x2f792c,0x2f794c)]
 counts=[[c.byte(at+i) for i in range(16)] for at in (0x2f798c,0x2f7994)]
 notices=[]
 for i in range(4):
  at=c.long(0x2f79a4+i*4);n=c.byte(at)
  if n>15 or not n:raise ValueError('notice length')
  notices.append([n]+[c.byte(at+1+j) for j in range(n)])
 h=['#ifndef ITEM_PROGRAMS_H','#define ITEM_PROGRAMS_H','#include "../src/animation.h"',
  'enum ItemAnimation {'+','.join('IAN_'+k for k in entries)+',IAN_COUNT};',
  'struct ItemFoodData {uint16_t entry,palette;uint8_t kind;};',
  'extern const struct AnimationProgram item_program;',
  'extern const uint16_t item_entries[IAN_COUNT],item_glyphs[32],item_cycle_roots[4];',
  'extern const struct ItemFoodData item_foods[20];',
  'extern const uint8_t item_cycles[11][16],item_fade[8],item_cycle_weapons[4],item_cycle_ammo[4];',
  'extern const uint8_t item_score_counts[2][16],item_notices[4][16];',
  'extern const uint8_t item_template_kinds[32];',
  'extern const uint32_t item_score_bcd[2][16];',
  'extern const int16_t item_throw[2][4][3],item_bounds[4],item_popup_bounds[4];',
  'extern const uint16_t item_pickup_boxes[2][4];','#endif']
 out=c.c_source('item_program','item-programs.h')
 def array(ty,name,data):out.append('const '+ty+' '+name+'={'+data+'};')
 array('uint16_t','item_entries[IAN_COUNT]',vals(entries.values()))
 array('uint16_t','item_glyphs[32]',vals(glyphs));array('uint16_t','item_cycle_roots[4]',vals(cycle_roots))
 array('struct ItemFoodData','item_foods[20]',','.join('{'+vals(row)+'}' for row in foods))
 array('uint8_t','item_cycles[11][16]',','.join('{'+vals(row)+'}' for row in seqs))
 for name,at,n in [('fade',0x9ab54,8),('cycle_weapons',0x2f7a38,4),('cycle_ammo',0x2f7a3c,4)]:
  array('uint8_t','item_'+name+'['+str(n)+']',vals(c.byte(at+i) for i in range(n)))
 array('uint32_t','item_score_bcd[2][16]',','.join('{'+vals(row)+'}' for row in scores))
 array('uint8_t','item_score_counts[2][16]',','.join('{'+vals(row)+'}' for row in counts))
 array('uint8_t','item_notices[4][16]',','.join('{'+vals(row)+'}' for row in notices))
 array('uint8_t','item_template_kinds[32]',vals(template_kinds))
 array('int16_t','item_throw[2][4][3]',','.join('{'+','.join('{'+vals(c.signed(at+i*6+j*2) for j in range(3))+'}' for i in range(4))+'}' for at in (0x2f7a40,0x2f7a58)))
 for name,at in [('bounds',0x9a5f4),('popup_bounds',0x9a6c6)]:array('int16_t','item_'+name+'[4]',vals(c.signed(at+i*2) for i in range(4)))
 array('uint16_t','item_pickup_boxes[2][4]',','.join('{'+vals(c.word(at+i*2) for i in range(4))+'}' for at in (0x9a6ce,0x9a6d6)))
 spans=[(0x9a5f4,0x9bb56),(0x3ee48,0x3ef8c),(0x32b58,0x32cba),(0x27400,0x27464),(0x51a10,0x51a90)]
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,food_descriptors=FOOD,
  source_ranges=[dict(start=x,end_exclusive=y,sha256=hashlib.sha256(raw[x:y]).hexdigest()) for x,y in spans],
  templates=[dict(id=284+i,initializer=at,native_kind=template_kinds[i]) for i,at in enumerate(handlers)],
  spawn_handlers=[c.long(0x9a6de+i*4) for i in range(31)],score_bcd=scores,score_counts=counts,cycle_sequences=seqs,
  native_implementation=dict(file='src/items.c',pickup_kinds=31,ordinary_food_descriptors=20,special_food_kinds=3,
   weapon_kinds=4,cycling_weapon_sequences=11,supply_kinds=3,
   families=['world pickup overlap and two-player selection','weapon flash and ridden weapon sound',
    'ammo/grenade/Slug ammo/repair and score fallback','food lifetime, byte blink, timed animation/rot and thrown/bounce',
    'first-child table/value score popups, digit delays/ballistics/lift/hold/blink and deletion',
    'combo controller and signed-byte chain selection','counted fly-away wrapper and frame count rollover',
    'source template and parent-offset item creation']),
  adaptations=['World coordinates and explicit scene/camera services','Decimal score totals with packed BCD input and original saturation',
   'Semantic native palette IDs and drawing records; physical palette/render services remain pending'],
  pending=['Original scene placement/event command callers','Other original scene actor families (template 293 now delegates to src/interactions.c)',
   'Static debug template 317','Offset-helper $09B872 scene coordinate selector',
   'Explicit-value allocation aliases $09BA96/$09BAD2 (parent-field writes require caller-group port)',
   'Item/digit renderer and HUD score display'],
  validation='Offline extraction and ARM compilation only; no gameplay/functional tests',complete_port=False)
 for name,value in [('item-programs.h','\n'.join(h)),('item-programs.c','\n'.join(out)),('item-source.json',json.dumps(ledger,indent=2))]:
  (R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),fields=c.fields,callbacks=c.callbacks,bindings=c.bindings,parameters=c.parameters,counts=counts)))
if __name__=='__main__':main()
