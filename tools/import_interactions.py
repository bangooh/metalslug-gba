#!/usr/bin/env python3
"""Source-only POW/crate/gun program and contact-table conversion."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(POW_IDLE=0x3f070,POW_IDLE_WAVE=0x3f0cc,POW_STACK=0x3f0fa,POW_STRUGGLE=0x3f190,POW_TURN=0x3f194,POW_BREAK=0x3f20a,POW_GREET=0x3f216,POW_GREET_HIGH=0x3f25e,POW_BOUND=0x3f292,POW_FREED=0x3f39e,POW_CHEER=0x3f3c2,POW_WALK=0x3f40a,POW_STOP=0x3f488,POW_BOW=0x3f49e,POW_JUMP=0x3f596,POW_CLIMB=0x3f5ac,POW_CROUCH=0x3f636,POW_HOP=0x3f71e,POW_FALL=0x3f72a,POW_DESCEND=0x3f80e,POW_LAND=0x3f888,POW_RUN=0x3f89e,POW_WAVE=0x3f8f4,CRATE=0x2f772a,CRATE_BREAK=0x2f773c,GUN_EXPLOSION=0x2dd37e)
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw);entries={k:c.entry(v) for k,v in ROOTS.items()}
 fixed={'FREE':c.binding('DEFENSE',0x3f01c)}
 fixed.update({'STACK'+str(i):c.binding('DEFENSE',c.long(0x3fc28+4*i)) for i in range(4)})
 shells=[65535 if c.long(0x29d452+4*i)==0xffffffff else c.entry(c.long(0x29d452+4*i)) for i in range(16)]
 c.compile()
 expected_callbacks=[0x3f626,0x9b90a,0x3efec,0x3f62e]
 if c.fields!=[dict(source_offset=0x5c,width=4)] or c.parameters or [v['source'] for v in c.callbacks]!=expected_callbacks:raise ValueError('unreviewed owner operation')
 if set(n['op'] for n in c.nodes)-{'WAIT','DRAW','END','JUMP','LOOP','SET','CALL','DEFENSE','ATTACK','SOUND'}:raise ValueError('unreviewed opcode')
 if any(n['op']=='ATTACK' and n['value']!=0xffffffff for n in c.nodes):raise ValueError('unreviewed attack binding')
 templates={143:0x77224,172:0x3fe5a,173:0x3fcd8,192:0x3fc38,283:0x9a31a,293:0x9b3c6}
 if any(c.long(0xe8000+i*4)!=at for i,at in templates.items()):raise ValueError('template dispatch changed')
 def drawing(at):
  if at==0xffffffff:return 65535
  parts=[dict(map=at,flags=0,dx=0,dy_up=0,priority=65535)]
  return c.intern(c.drawings,c.draw_ids,json.dumps(parts,sort_keys=True),parts)
 # Long SET $5C is a raw sibling drawing, never a runtime address.
 maps={n['value']:drawing(n['value']) for n in c.nodes if n['op']=='SET' and c.fields[n['argument']]==dict(source_offset=0x5c,width=4)}
 for n in c.nodes:
  if n['op']=='SET' and c.fields[n['argument']]==dict(source_offset=0x5c,width=4):n['value']=maps[n['value']]
 gun=[]
 for i in range(9):
  at=c.long(0x2f6540+i*8);row=[]
  for j in range(5):
   overlay=c.long(at+j*8);row.append([drawing(c.long(at+j*8+4))]+[drawing(c.long(overlay+k*4)) for k in range(3)])
  gun.append(row)
 h=['#ifndef INTERACTION_PROGRAMS_H','#define INTERACTION_PROGRAMS_H','#include "../src/animation.h"','#include "../src/hitbox.h"',
 'enum InteractionAnimation {'+','.join('INAN_'+k for k in entries)+',INAN_COUNT};',
 'enum InteractionField {'+','.join('INF_'+format(f['source_offset'],'X') for f in c.fields)+'};',
 'enum InteractionCallback {'+','.join('INC_'+format(at['source'],'X') for at in c.callbacks)+'};',
 'enum InteractionDefense {'+','.join('IND_'+k+'='+str(v) for k,v in fixed.items())+'};',
 'struct InteractionDefenseSet {const struct HitDefense *records;uint16_t count;};',
 'extern const struct AnimationProgram interaction_program;',
 'extern const struct InteractionDefenseSet interaction_defenses['+str(len(c.bindings))+'];',
 'extern const uint16_t interaction_entries[INAN_COUNT],gun_shell_entries[16],gun_drawings[9][5][4],gun_palettes[3];',
 'extern const uint8_t gun_muzzle_angles[9];',
 'extern const int16_t pow_bounds[4],gun_bounds[4],gun_target_offsets[17],gun_muzzle_offsets[9][2];',
 'extern const struct HitAttackSet gun_shell_attack;',
 'struct InteractionContact {int16_t rect[4],bias;uint8_t kind;};',
 'extern const struct InteractionContact interaction_contacts[6];',
 'extern const uint16_t gun_rank_tables[2][8][4][2];',
 'extern const int16_t gun_explosion_bounds[4];','#endif']
 out=c.c_source('interaction_program','interaction-programs.h');records=[];sets=[];filters={}
 for b in c.bindings:
  if b['kind']!='DEFENSE':raise ValueError(b)
  first=len(records);q=b['source']
  for count in range(256):
   if c.byte(q)==255:break
   at=c.long(q+2);name='0'
   if at!=0xffffffff:
    if at not in filters:
     filters[at]='filter_'+str(len(filters));out.append('static const uint8_t '+filters[at]+'[36]={'+vals(c.byte(at+i) for i in range(36))+'};')
    name=filters[at]
   records.append('{{'+vals(c.signed(q+10+i*2) for i in range(4))+'},'+name+','+str(c.byte(q+1))+'}');q+=80
  else:raise ValueError('defense bound')
  sets.append('{defense_records+'+str(first)+','+str(count)+'}')
 out+=['static const struct HitDefense defense_records[]={'+','.join(records)+'};','const struct InteractionDefenseSet interaction_defenses[]={'+','.join(sets)+'};']
 attack=[];q=0x2f65f4
 for count in range(256):
  if c.byte(q)==255:break
  group,kind,damage,strength,shape,offscreen=struct.unpack_from('>BBhBBB',raw,c.offset(q))
  attack.append('{{'+vals(c.signed(q+10+i*2) for i in range(4))+'},'+vals([damage,group,kind,strength,shape,offscreen])+'}');q+=80
 else:raise ValueError('attack bound')
 out+=['static const struct HitAttack shell_attack_records[]={'+','.join(attack)+'};','const struct HitAttackSet gun_shell_attack={shell_attack_records,'+str(count)+'};']
 def array(ty,name,data):out.append('const '+ty+' '+name+'={'+data+'};')
 array('uint16_t','interaction_entries[INAN_COUNT]',vals(entries.values()));array('uint16_t','gun_shell_entries[16]',vals(shells))
 array('uint16_t','gun_drawings[9][5][4]',','.join('{'+','.join('{'+vals(v)+'}' for v in row)+'}' for row in gun))
 array('uint16_t','gun_palettes[3]',vals(c.word(0x2f65ee+2*i) for i in range(3)))
 array('uint8_t','gun_muzzle_angles[9]',vals(c.byte(0x2f6547+8*i) for i in range(9)))
 for name,at,n in [('pow_bounds',0x3efe4,4),('gun_bounds',0x9a0d8,4),('gun_target_offsets',0x2f6588,17)]:array('int16_t',name+'['+str(n)+']',vals(c.signed(at+2*i) for i in range(n)))
 array('int16_t','gun_muzzle_offsets[9][2]',','.join('{'+vals(c.signed(0x2f65aa+8*i+2*j) for j in range(2))+'}' for i in range(9)))
 array('struct InteractionContact','interaction_contacts[6]',','.join('{{'+vals(c.signed(at+6+2*i) for i in range(4))+'},'+str(c.signed(at+2))+','+str(c.byte(at+1))+'}' for at in (0x32500,0x32598,0x295b4,0x29600,0x2964c,0x9a672)))
 for at in (0x2f6648,0x2f66ca):
  if c.word(at)==2:raise ValueError('short rank format unreviewed')
 array('uint16_t','gun_rank_tables[2][8][4][2]',','.join('{'+','.join('{'+','.join('{'+vals(c.word(at+2+rank*16+flags*4+players*2) for players in range(2))+'}' for flags in range(4))+'}' for rank in range(8))+'}' for at in (0x2f6648,0x2f66ca)))
 array('int16_t','gun_explosion_bounds[4]',vals(c.signed(0x2df210+2*i) for i in range(4)))
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,raw_drawing_relocations=maps,fixed_defenses=fixed,
  templates=[dict(id=i,source=at) for i,at in templates.items()],
  source_ranges=[dict(start=x,end_exclusive=y,sha256=hashlib.sha256(raw[x:y]).hexdigest()) for x,y in ((0x3ee48,0x403c8),(0x9a0d8,0x9a5d8),(0x9b3c6,0x9b47a),(0x76fea,0x772a8),(0x28998,0x28c20))],
  native_implementation=dict(files=['src/pow.c','src/interactions.c','src/gun.c','src/body_contact.c'],
   pow_entries=4,pow_states=28,gun_states=8,contact_descriptors=6,climb_buffers=2,climb_capacity=8,
   families=['bound/stacked/idle/free POW, release/cheer/stack children','walk/turn/stop/climb/crouch-hop/descent/landing',
    'two-player rescue selection, WORD rescue counts, gift callback and departure','item crate hit/death/drop, moving-platform publication and body-contact flags',
    'detached/parent-local gun, target publication, owner overlay, aim acceleration and three-shot player burst',
    'original enemy aim/cooldown branch and rank-selected shell lifetime','gun explosion transition with explicit native cleanup API',
    'double-buffered climb targets, nearest/crossing/snap helpers and template 143 publisher']),
  adaptations=['World coordinates; original screen WORD comparisons reconstructed where needed',
   'Compiled drawing IDs replace raw sibling/map pointers; renderer output remains separate',
   'Scene supplies valid rank sub-index and player-count flag for gun tables; no default difficulty',
   'gun_destroy scene API explicitly deletes overlay descendants before source explosion handler, avoiding an orphaned native child',
   'Global projectile group has a zero-facing transform for the gun bullet backward phase; no gun/player attribution is invented'],
  pending=['Original scene/event placement and enemy gun-claiming callers','Non-template climb entry $077228 with arbitrary parent byte selector',
   'Other POW/enemy/vehicle families, rescue result/lives/credits/continue systems','All-pose renderer, physical palettes and scene rank controller',
   'Original damage-score dispatcher remains NativePlayServices.damage dependency','Integrated functional/gameplay validation after complete source connection'],
  validation='Offline extraction and ARM compilation only; no ROM/game/emulator/regression/performance tests',complete_port=False)
 for name,data in [('interaction-programs.h','\n'.join(h)),('interaction-programs.c','\n'.join(out)),('interaction-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(data+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),fields=c.fields,callbacks=[hex(v['source']) for v in c.callbacks],bindings=c.bindings,parameters=c.parameters)))
if __name__=='__main__':main()
