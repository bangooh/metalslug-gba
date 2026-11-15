#!/usr/bin/env python3
"""Native player body hitboxes, motion selectors, input vectors and death routes."""
import argparse,hashlib,json,struct
from pathlib import Path
R=Path(__file__).resolve().parents[1]
MOTIONS={
 'STAND':0x2796a4,'STAND_UP':0x2796c4,
 'STAND_FIRE':0x2796e4,'STAND_UP_FIRE':0x2796ee,
 'STAND_FIRE_FROM_UP':0x279716,'STAND_FIRE_TO_UP':0x279720,
 'STAND_FIRE_ALT':0x27972a,'STAND_UP_FIRE_ALT':0x279734,
 'SHOTGUN_RELOAD':0x2796f8,'SHOTGUN_RELOAD_UP':0x279702,
 'IDLE_SPECIAL':0x27973e,'IDLE_LOW_AMMO':0x27981e,'EMPTY_WEAPON':0x279828,
 'WALK':0x279882,'WALK_UP':0x279878,
 'WALK_FIRE':0x2798e0,'WALK_UP_FIRE':0x2798ea,
 'WALK_FIRE_FROM_UP':0x2798f4,'WALK_FIRE_TO_UP':0x2798fe,
 'WALK_FIRE_ALT':0x279908,'WALK_UP_FIRE_ALT':0x279912,
 'WALK_LOOP':0x279926,'WALK_LOOP_UP':0x27991c,
 'TURN':0x279a3c,'TURN_UP':0x279a5a,'STOP':0x279a96,
 'GROUND_MELEE':0x279d56,'GROUND_MELEE_RIGHT':0x279d76,'GROUND_MELEE_LEFT':0x279e30,
 'GROUND_MELEE_ALT':0x279efe,'GROUND_GRENADE':0x279d2e,'WALK_GRENADE':0x279d38,
 'CROUCH_ENTER':0x279864,'CROUCH_EXIT':0x27986e,'CROUCH_IDLE':0x27983c,
 'CROUCH_AFTER_SHOT':0x279850,'CROUCH_AFTER_THROW':0x27985a,
 'CRAWL':0x279a82,'CRAWL_TURN':0x279a78,'CROUCH_EMPTY':0x279832,
 'CROUCH_SHOOT':0x279d06,'CROUCH_GRENADE':0x279d1a,
 'CROUCH_MELEE':0x279eea,'CROUCH_RELOAD':0x27970c,
 'JUMP_RUNNING':0x279c78,'JUMP_STANDING':0x279b2c,
 'AIR_RUNNING_NEUTRAL':0x279b22,'AIR_STANDING_NEUTRAL':0x279b18,
 'AIR_RUNNING_DOWN':0x279b0e,'AIR_STANDING_DOWN':0x279b04,
 'AIR_MELEE':0x279d56,'AIR_GRENADE_FORWARD':0x279d42,'AIR_GRENADE_BACK':0x279d4c,
 'AIR_RUNNING_DOWN_FIRE':0x279c6e,'AIR_STANDING_DOWN_FIRE':0x279afa,
 'AIR_RUNNING_UP_FIRE':0x279c3c,'AIR_STANDING_UP_FIRE':0x279ad2,
 'AIR_RUNNING_BACK_FIRE':0x279c32,'AIR_STANDING_BACK_FIRE':0x279ac8,
 'AIR_RUNNING_FORWARD_FIRE':0x279c28,'AIR_STANDING_FORWARD_FIRE':0x279abe,
 'DEATH_GENERIC':0x279f30,'DEATH_AIR':0x279f44,'DEATH_LANDED':0x279f4e,
 'DEATH_AIR_FX':0x279f58,'DEATH_LANDED_FX':0x279f62,'DEATH_TIMED':0x279f6c,
 'DEATH_SMOKE':0x279f76,'DEATH_ALT':0x279f80,'SPAWN_FALL':0x279f8a,
 'ATTACH':0x279f08,'ATTACH_JUMP':0x279690,'SLUG_POSE':0x279f1c,
}
DEFENSES={'STAND':0x326e0,'CROUCH':0x32734,'MELEE':0x32788,'CROUCH_MELEE':0x327dc,
 'AIR':0x32830,'FALL':0x32884,'DEATH':0x328d8,'SLUG':0x3292c,'ARM':0x32980}
ATTACKS={'ACTION_PROBE':0x325e4,'MELEE':0x32638}
DEATHS={'FALL':0x37684,'FALL_FX':0x378c6,'TIMED':0x37b00,'AIR':0x37778,
 'DEBUG':0x37af2,'PRIORITY':0x37a34,'GENERIC':0x37a3e}
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args()
 raw=(a.source/'build/mslug_prom.bin').read_bytes();catalog=json.loads((R/'generated/player-body-source.json').read_text())
 if hashlib.sha256(raw).hexdigest()!=catalog['prom_sha256']:raise ValueError('P-ROM fingerprint')
 motions={v['source']:i for i,v in enumerate(catalog['motions'])}
 h=['#ifndef PLAYER_BODY_DATA_H','#define PLAYER_BODY_DATA_H','#include "../src/hitbox.h"','enum PlayerNamedMotion {']
 h += [f' PMOTION_{name}={motions[at]},' for name,at in MOTIONS.items()]+['};','enum PlayerBodyDefense {']
 h += [f' PDEF_{name}={i},' for i,name in enumerate(DEFENSES)]+['};','enum PlayerBodyAttack {']
 h += [f' PATT_{name}={i},' for i,name in enumerate(ATTACKS)]+['};','enum PlayerDeathRoute {']
 h += [f' PDEATH_{name}={i},' for i,name in enumerate(DEATHS)]+['};',
  'extern const struct HitDefense player_body_defenses[];',
  'extern const struct HitAttack player_body_attacks[];',
  'extern const uint8_t player_death_routes[2][34];',
  'extern const int16_t player_input_velocity[5][4],player_ground_bounds[2][4];','#endif']
 c=['#include "player-body-data.h"'];rows=[];defs=[]
 for i,(name,at) in enumerate(DEFENSES.items()):
  flt=struct.unpack_from('>I',raw,at+2)[0];f=list(raw[flt:flt+36]);rect=list(struct.unpack_from('>4h',raw,at+10))
  c.append(f'static const uint8_t filter_{i}[36]={{'+','.join(map(str,f))+'};')
  rows.append('{{'+','.join(map(str,rect))+'},filter_'+str(i)+','+str(raw[at+1])+'}')
  defs.append(dict(name=name,source=at,filter=flt,rect=rect))
 c.append('const struct HitDefense player_body_defenses[]={'+','.join(rows)+'};');rows=[]
 for name,at in ATTACKS.items():
  group,kind,damage,strength,shape=struct.unpack_from('>BBhBB',raw,at);rect=struct.unpack_from('>4h',raw,at+10)
  if shape not in (0,4):raise ValueError('unreviewed player attack shape')
  if raw[at+80]!=255:raise ValueError('multi-record body attack needs a native set')
  rows.append('{{'+','.join(map(str,rect))+'},'+','.join(map(str,[damage,group,kind,strength,shape,raw[at+6]]))+'}')
 c.append('const struct HitAttack player_body_attacks[]={'+','.join(rows)+'};')
 death_ids={at:i for i,at in enumerate(DEATHS.values())}
 routes=[[death_ids[v] for v in struct.unpack_from('>34I',raw,at)] for at in [0x3338a,0x33412]]
 c.append('const uint8_t player_death_routes[2][34]={'+','.join('{'+','.join(map(str,row))+'}' for row in routes)+'};')
 vectors=[list(struct.unpack_from('>4h',raw,0x324d0+n*8)) for n in range(5)]
 c.append('const int16_t player_input_velocity[5][4]={'+','.join('{'+','.join(map(str,row))+'}' for row in vectors)+'};')
 bounds=[list(struct.unpack_from('>4h',raw,at)) for at in [0x324bc,0x324c6]]
 c.append('const int16_t player_ground_bounds[2][4]={'+','.join('{'+','.join(map(str,row))+'}' for row in bounds)+'};')
 (R/'generated/player-body-data.h').write_text('\n'.join(h)+'\n');(R/'generated/player-body-data.c').write_text('\n'.join(c)+'\n')
 (R/'generated/player-body-data-source.json').write_text(json.dumps(dict(prom_sha256=catalog['prom_sha256'],motions=MOTIONS,
  defenses=defs,attacks=ATTACKS,death_handlers=DEATHS,death_routes=routes,input_velocity=vectors,
  status='Original body data compiled; player state/graphics integration in progress; no game tests'),indent=2)+'\n')
 print(json.dumps(dict(motions=len(MOTIONS),defenses=len(DEFENSES),attacks=len(ATTACKS),death_routes=68)))
if __name__=='__main__':main()
