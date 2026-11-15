#!/usr/bin/env python3
"""Translate all original arm-selection functions into ordinary C branches.

This is a bounded offline semantic translation, not a CPU runtime. Only the
reviewed patterns in these leaf functions are accepted; all fields have named
C members and animation/attack addresses become typed native table indices.
Any unrecognized instruction, condition, call or data pointer stops conversion.
"""
import argparse,hashlib,json,re,struct
from pathlib import Path
from capstone import Cs,CS_ARCH_M68K,CS_MODE_BIG_ENDIAN,CS_MODE_M68K_000
R=Path(__file__).resolve().parents[1]
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--source',type=Path,required=True);a=ap.parse_args()
 raw=(a.source/'build/mslug_prom.bin').read_bytes()
 catalog=json.loads((R/'generated/player-source-catalog.json').read_text())
 if hashlib.sha256(raw).hexdigest()!=catalog['prom_sha256']:raise ValueError('P-ROM fingerprint')
 index=json.loads((R/'generated/source-index.json').read_text())
 roots=sorted((r for r in index['routines'] if r['name'].startswith('PlayerArm_') and 'SpriteTbl' not in r['name'] and
  (0x398a8<=r['address']<0x3c62a or r['address'] in [0x39460,0x3c632,0x3c67e,0x3c6c4])),key=lambda r:r['address'])
 # This entry lies between named sections in the matching ASM file. The
 # original body stream points here; omitting it loses running-air aim poses.
 roots.append(dict(address=0x3a824,name='PlayerArm_AirRunningAim_03a824'))
 roots.sort(key=lambda r:r['address'])
 root_ids={r['address']:i for i,r in enumerate(roots)}
 md=Cs(CS_ARCH_M68K,CS_MODE_BIG_ENDIAN|CS_MODE_M68K_000)
 tables={t['address']:i for i,t in enumerate(catalog['tables'])};all_read=set();used_tables=set()
 def name(r):
  if r['address']==0x3c28e:return 'CROUCH_FIRE'
  if r['address']==0x3c2d4:return 'CROUCH_GRENADE'
  return re.sub(r'_[0-9a-f]{6}$','',r['name'].removeprefix('PlayerArm_')).upper()
 def number(s):return int(s.removeprefix('#').split('(')[0].split('.')[0].removeprefix('$'),16)
 fields={'$72(a0)':'c->body->actor.aim','$72(a6)':'c->layer->aim',
         '$79(a0)':'c->body->direction_change','$78(a0)':'c->body->aim_direction',
         '$30(a6)':'c->layer->melee_selector'}
 def translate(at,ctx,trail=()):
  out=[]
  while True:
   if trail and at in root_ids:return out+[f'return select_{root_ids[at]}(c);']
   if at in trail:raise ValueError(f'loop in arm leaf {at:06x}')
   trail=trail+(at,);all_read.add(at)
   ins=next(md.disasm(raw[at:at+10],at));op=ins.mnemonic;args=ins.op_str
   following=at+ins.size
   if op=='rts':return out+['return !c->fault;']
   if op in ('bra.w','bra.b','jmp'):
    at=number(args);continue
   if op in ('beq.w','bne.w','beq.b','bne.b','bcs.w','bcc.w'):
    z=ctx.get('C' if op.startswith(('bcs','bcc')) else 'Z')
    if z is None:raise ValueError(f'unresolved condition {at:06x}')
    if op.startswith(('bne','bcc')):z=not z if isinstance(z,bool) else f'!({z})'
    target=number(args)
    if isinstance(z,bool):at=target if z else following;continue
    yes=translate(target,dict(ctx),trail);no=translate(following,dict(ctx),trail)
    return out+[f'if({z}){{']+[' '+s for s in yes]+['}else{']+[' '+s for s in no]+['}']
   if op=='bclr.b' and args=='#$2, $8c(a6)':out.append('c->layer->action_flags&=(uint8_t)~4u;')
   elif op=='bset.b' and args=='#$0, $13(a6)':out.append('c->layer->hit.flags|=1;')
   elif op=='addq.b' and args=='#$1, $30(a6)':out.append('c->layer->melee_selector++;')
   elif op=='clr.w' and args=='$72(a6)':out.append('c->layer->aim=0;');ctx['Z']=True;ctx['C']=False
   elif op in ('cmpi.w','cmpi.b'):
    value,target=args.split(', ');value=number(value)
    if target not in fields:raise ValueError(f'unknown compare field {args}')
    ctx['Z']=f'{fields[target]}=={value}';ctx['C']=f'{fields[target]}<{value}'
   elif op=='btst.b':
    bit,target=args.split(', ');bit=number(bit)
    if target not in fields:raise ValueError(f'unknown tested field {args}')
    ctx['Z']=f'!({fields[target]}&{1<<bit})'
   elif op=='movea.l' and args=='$c(a6), a0':ctx['pointer']='parent'
   elif op=='move.b' and args=='$80(a0), d0':
    if ctx.get('pointer')!='parent':raise ValueError('non-parent grenade read')
    ctx['Z']='c->body->grenades==0';ctx['C']=False
   elif op=='lea.l' and args.endswith(', a0'):
    value=number(args.split(', ')[0]);ctx['pointer']=value
   elif op=='movea.l' and args=='(a0, d0.w), a0':
    if ctx.get('pointer') not in tables:raise ValueError(f'unknown animation table {at:06x}')
    ctx['animation_table']=ctx['pointer'];ctx['pointer']='animation'
   elif op=='move.l' and args=='a0, $4c(a6)':
    if ctx.get('pointer') not in (0x392a4,0x39250):raise ValueError('unknown arm attack')
    out.append('c->layer->attack=&player_arm_'+('knife' if ctx['pointer']==0x392a4 else 'probe')+';')
   elif op=='jsr':
    target=number(args)
    if target==0x3933a:ctx['pointer']='parent'
    elif target==0x28cd4:
     if ctx.get('pointer')!='animation':raise ValueError('unresolved animation pointer')
     table=ctx['animation_table'];used_tables.add(table)
     out.append(f'if(!player_action_begin(c,{tables[table]}))return 0;')
    elif target==0x283ca:out.append('hit_begin_attack(&c->layer->hit);')
    elif target==0x2abcc:ctx['C']=False # actual two-instruction helper clears C/X only
    elif target==0x39354:
     out.append('c->body->arm_busy=player_action_step(c)?0:255;')
     out.append('if(c->fault||c->layer->animation.fault)return 0;')
    elif target==0x39416:
     out.append('c->body->actor.action_flags=(c->body->actor.action_flags&251)|(c->layer->action_flags&4);')
    else:raise ValueError(f'unreviewed arm call {at:06x}->{target:06x}')
   else:raise ValueError(f'unreviewed arm instruction {at:06x}: {op} {args}')
   at=following
 h=['#ifndef PLAYER_ARM_SELECT_H','#define PLAYER_ARM_SELECT_H','#include "../src/player_actions.h"','enum PlayerArmSelection {']
 h += [f' ARM_{name(r)}={i},' for i,r in enumerate(roots)]+[f' PLAYER_ARM_SELECTIONS={len(roots)}','};',
 'extern const struct HitAttack player_arm_probe,player_arm_knife;',
 'int player_arm_select(struct PlayerActionContext*,enum PlayerArmSelection);','#endif']
 c=['/* Generated semantic C selection rules; no CPU execution state. */','#include "player-arm-select.h"']
 c += [f'static int select_{i}(struct PlayerActionContext*);' for i in range(len(roots))]
 for label,at in [('probe',0x39250),('knife',0x392a4)]:
  group,kind,damage,strength,shape=struct.unpack_from('>BBhBB',raw,at);rect=struct.unpack_from('>4h',raw,at+10)
  c.append('const struct HitAttack player_arm_'+label+'={{'+','.join(map(str,rect))+'},'+','.join(map(str,[damage,group,kind,strength,shape,raw[at+6]]))+'};')
 ledger=[]
 for i,r in enumerate(roots):
  body=translate(r['address'],{})
  c += [f'/* Source ${r["address"]:06X}: {r["name"]} */',f'static int select_{i}(struct PlayerActionContext*c){{']+[' '+s for s in body]+['}']
  ledger.append(dict(source=r['address'],native_selection=i,name=name(r),implementation='generated/player-arm-select.c',implemented=True,runtime_connected=False))
 c+=['int player_arm_select(struct PlayerActionContext*c,enum PlayerArmSelection selection){',' switch(selection){']
 c += [f'  case ARM_{name(r)}:return select_{i}(c);' for i,r in enumerate(roots)]
 c+=['  default:c->fault=9;return 0;',' }','}']
 (R/'generated/player-arm-select.h').write_text('\n'.join(h)+'\n');(R/'generated/player-arm-select.c').write_text('\n'.join(c)+'\n')
 (R/'generated/player-arm-source.json').write_text(json.dumps(dict(prom_sha256=catalog['prom_sha256'],selections=ledger,
  native_tables_used=sorted(tables[t] for t in used_tables),source_instructions_read=sorted(all_read),
  complete=False,status='All inventoried arm selector entry points translated to C; parent scheduler and renderer integration pending; no tests run'),indent=2)+'\n')
 print(json.dumps(dict(selectors=len(roots),tables=len(used_tables),instructions=len(all_read),complete=False)))
if __name__=='__main__':main()
