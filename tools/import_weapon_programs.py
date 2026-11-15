#!/usr/bin/env python3
"""Compile the complete weapon/projectile drawing dependency group offline."""
import argparse,hashlib,json,re,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
DIRECT={
 'SHOTGUN_HORIZONTAL':0x2f865c,'SHOTGUN_VERTICAL':0x2f86ec,
 'FLAME_VERTICAL':0x2f83a4,'FLAME_HORIZONTAL':0x2f84ae,
 'ROCKET_VERTICAL':0x2f7a70,'ROCKET_HORIZONTAL':0x2f7ac6,'ROCKET_ALT':0x2f7aac,
 'GRENADE_SPIN':0x28d3e2,'BODY_FX':0x2f7d3a,
 'AIR_IMPACT':0x2dd8b6,
 'PISTOL_CASING':0x3d3f6,'PISTOL_CASING_UP':0x3d470,
 'ROCKET_CASING':0x3d54a,'ROCKET_CASING_UP':0x3d5b0,
 'SCENE3_DEBRIS':0x3d7a0,'DEATH_BLOOD':0x3d894,
 'ROCKET_IMPACT_VERTICAL':0x2f7cc0,'ROCKET_IMPACT_HORIZONTAL':0x2f7c46,
 'FLAME_IMPACT_VERTICAL':0x2f8416,'FLAME_IMPACT_HORIZONTAL':0x2f8520,
 'GRENADE_BIG_BLAST':0x29e76c,'GRENADE_SMOKE':0x28d52e,
 'BLAST_A':0x2dd4ba,'BLAST_B':0x2dd594,'BLAST_C':0x2dd37e,
 'DROPPED_WEAPON':0x27a28e,
 'DROP_IMPACT':0x2dd2d4,
 'PLAYER_SPLASH':0x27a182,'PLAYER_SPLASH_END':0x27a228,'PLAYER_RIPPLE':0x27a0fe,
 'PLAYER_DEATH_ALT':0x279f9a,'SMALL_SMOKE':0x2ded26,
 'PLAYER_ICON1':0x321c2,'PLAYER_ICON2':0x32224,
 'SLUG_BUBBLE':0x3232a,
 'PARACHUTE_RELEASE':0x28fe2c,'PARACHUTE_FALL':0x38ec8,
}
FIELDS={(0x70,4):'TRAIL_DRAWING',(0x74,2):'TRAIL_ANGLE',(0x2a,2):'VELOCITY_Y',
        (0x70,2):'EFFECT_X',(0x72,2):'EFFECT_Y',(0x71,1):'PARACHUTE_SLOW',(0x5c,2):'ATTACHED_OFFSET'}
CALLBACKS={0x283ca:'CLEAR_ATTACK',0x283d8:'ATTACK',0x9bdc6:'BLAST_A',0x9bdd4:'BLAST_B',0x9bde2:'BLAST_C',0x32ed0:'PLAYER_LIFE_SOUND'}
ATTACKS={'BULLET':0x30804,'FLAME':0x2f85b8,'ROCKET_HORIZONTAL':0x2f7ddc,
         'ROCKET_VERTICAL':0x2f7ed4,'ROCKET_SEEK_HORIZONTAL':0x2f7e80,'ROCKET_SEEK_VERTICAL':0x2f7f78,
         'GRENADE':0x28d152,'GRENADE_BLAST':0x28d1f6,'GRENADE_BIG_BLAST':0x28d29a}
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args()
 raw=(a.source/'build/mslug_prom.bin').read_bytes();source=json.loads((R/'generated/player-source-catalog.json').read_text())
 if hashlib.sha256(raw).hexdigest()!=source['prom_sha256']:raise ValueError('P-ROM fingerprint')
 c=AnimationCompiler(raw);entries={name:c.entry(at) for name,at in DIRECT.items()};roots=dict(DIRECT)
 tables={}
 # Constructor index is (angle & $F8) >> 4: exactly sixteen choices.
 for name,at,count in [('pistol',0x29d452,16),('hmg',0x29d4d2,16),('ground_impact',0x31c6a,2),
                       ('surface_bullet',0x2dca6a,2),('surface_heavy',0x2dca72,2),
                       ('surface_vehicle',0x2dca7a,2),('surface_large',0x2dca82,2),
                       ('parachute_open',0x28f8d4,10),('parachute_swing',0x28fdbc,9)]:
  values=[c.long(at+4*n) for n in range(count)]
  tables[name]=[65535 if v==0xffffffff else c.entry(v) for v in values]
 # Exact LEA + animation-begin instruction pairs in this dependency group.
 # Child constructors are included even when not reached by a current route.
 ranges=[(0x3d396,0x3d944),(0x9bec2,0x9c608),(0x28d55a,0x28db5a),(0x31c72,0x31d1e)]
 for lo,hi in ranges:
  for at in range(lo,hi,2):
   off=c.offset(at);opcode=raw[off:off+2]
   if opcode==bytes.fromhex('41f9'):target=c.long(at+2);end=at+6
   elif opcode==bytes.fromhex('41fa'):target=at+2+c.signed(at+2);end=at+4
   else:continue
   if raw[c.offset(end):c.offset(end)+6]!=bytes.fromhex('4eb900028cd4'):continue
   if target not in roots.values():
    name=f'AUX_{len(roots)}';roots[name]=target;entries[name]=c.entry(target)
 c.compile()
 fixed_attacks={name:c.binding('ATTACK',at) for name,at in ATTACKS.items()}
 # Long writes to field $70 are trail graphic maps, never runtime ROM
 # addresses. Convert each map to the same native drawing ID space.
 field_names=[FIELDS[(v['source_offset'],v['width'])] for v in c.fields]
 pointer_fixups=[]
 for node in c.nodes:
  if node['op']=='SET' and field_names[node['target']]=='TRAIL_DRAWING':
   at=node['value'];parts=[dict(map=at,flags=8,dx=0,dy_up=0,priority=65535)]
   drawing=c.intern(c.drawings,c.draw_ids,json.dumps(parts,sort_keys=True),parts)
   node['value']=drawing;pointer_fixups.append(dict(source_graphic=at,native_drawing=drawing))
 h=['#ifndef WEAPON_PROGRAMS_H','#define WEAPON_PROGRAMS_H','#include "../src/animation.h"','#include "../src/hitbox.h"','enum WeaponAnimation {']
 h += [f' WANI_{name}={i},' for i,name in enumerate(entries)]
 h += ['};','extern const struct AnimationProgram weapon_animation_program;','extern const uint16_t weapon_animation_entries[];']
 for name,values in tables.items():h.append(f'extern const uint16_t weapon_{name}_entries[{len(values)}];')
 h+=['enum WeaponAnimationField {']+[f' WAF_{name}={i},' for i,name in enumerate(field_names)]+['};',
      'enum WeaponAnimationCallback {']+[f" WAC_{CALLBACKS[v['source']]}={i}," for i,v in enumerate(c.callbacks)]+['};',
      f'#define WEAPON_ATTACK_BINDINGS {len(c.bindings)}','extern const struct HitAttackSet weapon_attack_sets[];',
      'enum WeaponAttack {']+[f' WATTACK_{name}={value},' for name,value in fixed_attacks.items()]+['};',
      'extern const int16_t weapon_sine[256],weapon_cosine[256];','#endif']
 out=c.c_source('weapon_animation_program','weapon-programs.h')
 out+=['const uint16_t weapon_animation_entries[]={'+','.join(map(str,entries.values()))+'};']
 for name,values in tables.items():out.append(f'const uint16_t weapon_{name}_entries[]={{'+','.join(map(str,values))+'};')
 attacks=[];attack_sets=[]
 for index,binding in enumerate(c.bindings):
  if binding['kind']!='ATTACK':raise ValueError('unknown weapon binding')
  first=len(attacks);at=c.offset(binding['source'])
  for count in range(256):
   if raw[at]==255:break
   group,kind,damage,strength,shape,offscreen=struct.unpack_from('>BBhBBB',raw,at)
   if shape not in (0,4) or group>=5:raise ValueError('unported hit shape/group')
   rect=struct.unpack_from('>4h',raw,at+10)
   attacks.append('{{'+','.join(map(str,rect))+'},'+','.join(map(str,[damage,group,kind,strength,shape,offscreen]))+'}');at+=80
  else:raise ValueError('unterminated attack set')
  binding['attack_count']=count
  attack_sets.append('{attack_records+'+str(first)+','+str(count)+'}')
 out+=['static const struct HitAttack attack_records[]={'+','.join(attacks)+'};',
       'const struct HitAttackSet weapon_attack_sets[]={'+','.join(attack_sets)+'};']
 for name,at in [('sine',0x2c072c),('cosine',0x2c07ac)]:
  out.append(f'const int16_t weapon_{name}[256]={{'+','.join(str(c.signed(at+2*i)) for i in range(256))+'};')
 (R/'generated/weapon-programs.h').write_text('\n'.join(h)+'\n');(R/'generated/weapon-programs.c').write_text('\n'.join(out)+'\n')
 ledger=c.ledger();ledger.update(prom_sha256=source['prom_sha256'],pointer_fixups=pointer_fixups,roots=[dict(name=name,source=roots[name],entry=entry) for name,entry in entries.items()],tables=tables,
  complete=False,status='Compiled full drawing graph; native projectile lifecycle/fields/callbacks/render integration in progress; no tests run')
 (R/'generated/weapon-program-source.json').write_text(json.dumps(ledger,indent=2)+'\n')
 print(json.dumps(dict(roots=len(roots),commands=len(c.nodes),drawings=len(c.drawings),fields=c.fields,callbacks=c.callbacks,bindings=c.bindings)))
if __name__=='__main__':main()
