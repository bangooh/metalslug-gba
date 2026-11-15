#!/usr/bin/env python3
"""Compile ALL inventoried player animation tables to native data graphs.

No ROM program counters or actor-memory offsets are used by the C reader.
Source mappings remain in the JSON implementation ledger. Typed owner bindings
and drawing conversion must be connected before this is a complete player port.
"""
import argparse,hashlib,json,struct
from pathlib import Path
from player_callbacks import CALLBACKS,FIELDS
R=Path(__file__).resolve().parents[1]
OPS=['WAIT','JUMP','DRAW','LOOP','SOUND','SOUND_STOP','SET','AND','OR','XOR','CALL','DEFENSE','ATTACK','CONTACT','PALETTE','END','PARAMETERS','WAIT_TABLE','SELECT']
FIELD_NAMES={0x12:'display_flags',0x13:'hit_flags',0x14:'palette',0x16:'base_palette',0x18:'alternate_palette',0x1a:'impact_palette',0x1c:'flash_palette',0x22:'world_x',0x24:'world_y_up',0x28:'velocity_x',0x2a:'velocity_y_up',0x2c:'acceleration_x',0x2e:'acceleration_y',0x30:'action_count',0x32:'zoom_x',0x33:'zoom_y',0x34:'angle',0x36:'angular_speed',0x38:'priority',0x3a:'facing',0x3b:'pose',0x44:'freeze',0x45:'invulnerability',0x46:'delay',0x47:'loop_counter',0x66:'hp',0x69:'animation_changed',0x6b:'type_flags',0x72:'aim',0x7a:'shot_phase',0x8c:'action_flags',0x91:'input_latch'}
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--source',type=Path,required=True);a=ap.parse_args()
 data=(a.source/'build/mslug_prom.bin').read_bytes();catalog=json.loads((R/'generated/player-source-catalog.json').read_text())
 if hashlib.sha256(data).hexdigest()!=catalog['prom_sha256']:raise ValueError('P-ROM fingerprint mismatch')
 def off(at):
  if at<0x100000:return at
  if 0x200000<=at<0x300000:return at-0x100000
  raise ValueError(f'non-ROM animation data {at:06X}')
 def b(at):return data[off(at)]
 def w(at):return struct.unpack_from('>H',data,off(at))[0]
 def sw(at):return struct.unpack_from('>h',data,off(at))[0]
 def l(at):return struct.unpack_from('>I',data,off(at))[0]
 nodes=[];seen={};pending=[];drawings=[];draw_ids={};fields=[];field_ids={};callbacks=[];callback_ids={};bindings=[];binding_ids={};params=[];op_counts={}
 def intern(items,ids,key,value):
  if key not in ids:ids[key]=len(items);items.append(value)
  return ids[key]
 def field(at,width):
  if at+width>0xa0:raise ValueError(f'invalid source actor field {at:x}/{width}')
  return intern(fields,field_ids,(at,width),dict(source_offset=at,width=width,name=FIELD_NAMES.get(at,f'owner_field_{at:02x}'),implemented=False))
 def node(at,actor):
  key=(at,actor)
  if key not in seen:seen[key]=len(nodes);nodes.append(None);pending.append(key)
  return seen[key]
 def bind(kind,at):
  if at==0xffffffff:return 0xffffffff
  return intern(bindings,binding_ids,(kind,at),dict(kind=kind,source=at,implemented=False))
 entries=[]
 for table in catalog['tables']:
  entries.append(dict(source=table['address'],actors=[[node(stream,actor) for stream in row] for actor,row in enumerate(table['streams'])]))
 # Body/leg streams used by the existing complete-body composition policy.
 lower={name:[node(at,actor) for actor in range(2)] for name,at in [('idle',0x2796a4),('walk',0x279926),('standing_jump',0x279b2c),('running_jump',0x279c78),('knife',0x279d56)]}
 initial=[node(0x27aaea,actor) for actor in range(2)]
 # Every lower/body stream header in the original bank carries its upper
 # selector immediately before the stream. Include unused variants too;
 # reconstructing only streams reached by a play route loses source poses.
 arm_source=json.loads((R/'generated/player-arm-source.json').read_text())
 selectors={v['source']:v for v in arm_source['selections']}
 body=[]
 for at in range(0x279680,0x27aaea,2):
  selector=l(at-4)
  if selector in selectors:
   if b(at) not in (0,1,2,5,0x17):raise ValueError(f'unreviewed body header {at:06x}')
   body.append(dict(source=at,selector=selectors[selector]['native_selection'],
                    selector_name=selectors[selector]['name'],entries=[node(at,actor) for actor in range(2)]))
 for at,actor in pending:
  op,arg=b(at),b(at+1);op_counts[op]=op_counts.get(op,0)+1
  c=dict(op=None,value=0,next=65535,target=0,width=0,argument=0,pad=0)
  def nxt(size):c['next']=node(at+size,actor)
  if op==0:c.update(op='WAIT',argument=arg);nxt(2)
  elif op==1:c.update(op='JUMP',target=node(l(at+2),actor))
  elif op in (2,0x17,0x1e):
   q=at;parts=[];selected=False;alternate=op==0x1e
   while b(q) in (2,0x17,0x18,0x1e):
    operation=b(q)
    if operation in (0x17,0x18):selected=True;q+=2;continue
    if operation==0x1e and q!=at:break
    flags=b(q+1);length=8 if flags&8 else 12
    # $028E9C clears the character-selection gate after one drawing.
    if not selected or bool(flags&16)==bool(actor):
     parts.append(dict(map=l(q+2),flags=flags&15,dx=0 if alternate or flags&8 else sw(q+8),dy_up=0 if alternate or flags&8 else sw(q+10),priority=65535 if alternate else w(q+6)))
    selected=False;q+=length
   key=json.dumps(parts,sort_keys=True);drawing=intern(drawings,draw_ids,key,parts)
   # $028FCA leaves the final drawing flags as its completion value. A few
   # body transitions use that byte when rebuilding the aim-change latch.
   c.update(op='DRAW',target=drawing,value=flags,argument=int(alternate),next=node(q,actor))
  elif op==3:c.update(op='LOOP',argument=arg,target=node(l(at+2),actor));nxt(6)
  elif op in (4,31):c.update(op='SOUND' if op==4 else 'SOUND_STOP',value=w(at+2));nxt(4)
  elif op in (5,6,7) or 13<=op<=21:
   if op<=7:kind='SET';width=(1,2,4)[op-5]
   else:kind=('AND','OR','XOR')[(op-13)//3];width=(1,2,4)[(op-13)%3]
   value=arg if width==1 else w(at+2) if width==2 else l(at+2)
   source_field=w(at+(2 if width==1 else 4 if width==2 else 6))
   c.update(op=kind,width=width,target=field(source_field,width),value=value);nxt(4 if width==1 else 6 if width==2 else 8)
  elif op==8:
   target=l(at+2);cb=intern(callbacks,callback_ids,target,dict(source=target,implemented=False))
   c.update(op='CALL',target=cb);nxt(6)
  elif op in (9,10,11):
   kind={9:'DEFENSE',10:'ATTACK',11:'CONTACT'}[op];c.update(op=kind,value=bind(kind,l(at+2)));nxt(6)
  elif op==12:c.update(op='PALETTE',target=field(arg,2));nxt(2)
  elif op==22:c.update(op='END')
  elif op==25:
   payload=data[off(at+2):off(at+10)].hex();params.append(payload);c.update(op='PARAMETERS',value=len(params)-1);nxt(10)
  elif op==26:c.update(op='WAIT_TABLE',target=field(arg,4),argument=field(0x3b,1));nxt(2)
  elif op==29:
   # Both entries in the actual $0293E4 table are 78-byte back offsets.
   selector=arg&15
   if selector>1:raise ValueError(f'unreviewed back selector {selector}')
   c.update(op='JUMP',target=node(at-l(0x293e4+selector*4),actor))
  elif op in (27,28):raise ValueError(f'dynamic selector needs bounded source table at {at:06X}, field {arg:02X}')
  else:raise ValueError(f'unreviewed player animation opcode {op:02X} at {at:06X}')
  nodes[seen[(at,actor)]]=c
 if len(nodes)>=65535 or len(drawings)>=65535:raise ValueError('native index capacity')
 symbol_names={row['address']:row['name'] for row in json.loads((R/'generated/source-index.json').read_text())['routines']}
 for cb in callbacks:
  cb['source_label']=symbol_names.get(cb['source'],'source label unavailable; inspect instructions')
  cb['native_name']=CALLBACKS[cb['source']]
  cb['implementation']='src/player_actions.c'
  cb['implemented']=True
  cb['runtime_connected']=False
 for f in fields:
  f['native_name']=FIELDS[(f['source_offset'],f['width'])]
  f['implementation']='src/player_actions.c';f['implemented']=True;f['runtime_connected']=False
 g=R/'generated';h=['#ifndef PLAYER_PROGRAMS_H','#define PLAYER_PROGRAMS_H','#include "../src/animation.h"','#include "../src/hitbox.h"',
 'extern const struct AnimationProgram player_animation_program;',
 f'#define PLAYER_PROGRAM_TABLES {len(entries)}',f'#define PLAYER_DRAWINGS {len(drawings)}',f'#define PLAYER_ANIMATION_FIELDS {len(fields)}',f'#define PLAYER_ANIMATION_CALLBACKS {len(callbacks)}',
 'extern const uint16_t player_animation_entries[][2][5];','extern const uint16_t player_lower_entries[][2];','extern const uint16_t player_arm_initial_entries[2];',
 f'#define PLAYER_BODY_MOTIONS {len(body)}','extern const uint16_t player_body_entries[][2];','extern const uint8_t player_body_selectors[];',
 f'#define PLAYER_ANIMATION_BINDINGS {len(bindings)}','extern const struct HitDefense player_animation_defenses[];']
 h+=['enum PlayerAnimationCallback {']+[f" PAC_{cb['native_name']}={i}," for i,cb in enumerate(callbacks)]+['};']
 h+=['enum PlayerAnimationField {']+[f" PAF_{f['native_name']}={i}," for i,f in enumerate(fields)]+['};','#endif']
 c=['#include "player-programs.h"','static const struct AnimationCommand commands[]={']
 c += ['{'+f"{v['value']}u,{v['next']},{v['target']},AN_{v['op']},{v['width']},{v['argument']},0"+'},' for v in nodes]
 c+=['};',f'const struct AnimationProgram player_animation_program={{commands,0,{len(nodes)},0}};',
 'const uint16_t player_animation_entries[][2][5]={'+','.join('{'+','.join('{'+','.join(map(str,row))+'}' for row in e['actors'])+'}' for e in entries)+'};',
 'const uint16_t player_lower_entries[][2]={'+','.join('{'+','.join(map(str,row))+'}' for row in lower.values())+'};',
 'const uint16_t player_arm_initial_entries[2]={'+','.join(map(str,initial))+'};',
 'const uint16_t player_body_entries[][2]={'+','.join('{'+','.join(map(str,row['entries']))+'}' for row in body)+'};',
 'const uint8_t player_body_selectors[]={'+','.join(str(row['selector']) for row in body)+'};']
 defense_rows=[]
 for i,binding in enumerate(bindings):
  if binding['kind']!='DEFENSE':raise ValueError('unreviewed player binding kind')
  at=binding['source'];filter_at=l(at+2)
  if filter_at==0xffffffff:filter_name='0'
  else:
   filter_name=f'defense_filter_{i}'
   c.append(f'static const uint8_t {filter_name}[36]={{'+','.join(str(b(filter_at+n)) for n in range(36))+'};')
  rect=[sw(at+10+n*2) for n in range(4)]
  defense_rows.append('{{'+','.join(map(str,rect))+'},'+filter_name+','+str(b(at+1))+'}')
  binding.update(implemented=True,runtime_connected=False,implementation='src/player_actions.c')
 c.append('const struct HitDefense player_animation_defenses[]={'+','.join(defense_rows)+'};')
 (g/'player-programs.h').write_text('\n'.join(h)+'\n');(g/'player-programs.c').write_text('\n'.join(c)+'\n')
 ledger=dict(prom_sha256=catalog['prom_sha256'],complete=False,status='Native animation command data compiled; owner field/callback/render integration in progress; no gameplay tests run',tables=entries,lower_streams=lower,
  fields=fields,callbacks=callbacks,bindings=bindings,parameters=params,drawings=drawings,
  commands=[dict(native_index=idx,source=at,actor=actor,**nodes[idx]) for (at,actor),idx in seen.items()],opcode_counts=op_counts)
 (g/'player-program-source.json').write_text(json.dumps(ledger,indent=2)+'\n')
 (g/'player-body-source.json').write_text(json.dumps(dict(prom_sha256=catalog['prom_sha256'],motions=body,
   complete=False,status='All original lower/body stream headers compiled; full body state scheduler still being translated'),indent=2)+'\n')
 print(json.dumps(dict(commands=len(nodes),drawings=len(drawings),fields=len(fields),callbacks=len(callbacks),bindings=len(bindings),opcode_counts=op_counts,complete=False)))
if __name__=='__main__':main()
