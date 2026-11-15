#!/usr/bin/env python3
"""Extract Slug body constants and ledger; no gameplay execution.

Handler bodies are hand-written in src/slug_body.c. The ledger explicitly
separates executable body/mount/firing/destruction states from pending systems.
"""
import argparse,hashlib,json
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
IMPLEMENTED={
 'IDLE_FLAT':0x2b38c,'IDLE_SLOPE':0x2b4d2,'ACCEL_RIGHT':0x2b8d8,'ACCEL_LEFT':0x2ba3e,
 'BRAKE_RIGHT':0x2bd00,'BRAKE_LEFT':0x2be32,'CRUISE_RIGHT':0x2bf64,'CRUISE_LEFT':0x2c07a,
 'SLOPE_IDLE_ENTER':0x2b6f4,'SLOPE_MOUNT':0x2b7da,
 'DRIVE':0x2e6c2,'DRIVE_B':0x2e7fa,'TURN_TO_DRIVE':0x2e924,'DRIVE_ALT':0x2e9c8,
 'TURN_TO_DRIVE_ALT':0x2eb14,'CROUCH_BRAKE':0x2ebb0,'STALL':0x2ecc2,
 'CROUCH_ACCEL_RIGHT':0x2ee78,'CROUCH_ACCEL_LEFT':0x2efaa,'DECEL_RIGHT':0x2f202,'DECEL_LEFT':0x2f312,
 'CROUCH_CRUISE_RIGHT':0x2f422,'CROUCH_CRUISE_LEFT':0x2f4fc,'SET_SPEED':0x2f5d6,
 'JUMP_NEUTRAL':0x2c9c4,'JUMP_RIGHT':0x2c9d4,'JUMP_LEFT':0x2c9f0,'JUMP_CROUCH':0x2c908,
 'JUMP_LAUNCH':0x2ca0c,'JUMP_AIR':0x2ca8a,'JUMP_AIR_ANGLE':0x2cc02,'JUMP_LAND':0x2cc1a,
 'LAND_SLOPE':0x2ce1c,'LAND_SLOPE_FRAME':0x2ce7e,'FALL_START':0x2cffa,'FALL':0x2d02e,
 'FALL_LOOP':0x2d0c4,'FALL_ANGLE':0x2d26e,
 'HIT_GROUND_A':0x2d63e,'HIT_GROUND_B':0x2d67c,'HIT_GROUND_FRAME':0x2d6ce,
 'HIT_AIR_A':0x2d736,'HIT_AIR_B':0x2d802,'HIT_LAND_A':0x2d8c8,'HIT_LAND_B':0x2d980,
 'IDLE_ENTER_A':0x2dd20,'IDLE_ENTER_B':0x2dd74,'IDLE_ENTER_C':0x2ddc8,'UNOCCUPIED_IDLE':0x2de18,
 'UNOCCUPIED_ANGLE':0x2deb4,'UNOCCUPIED_FALL':0x2e092,'UNOCCUPIED_FALL_KEEP':0x2e04a,
 'IDLE_ANGLED_B':0x2e244,'HUNKER':0x2e3b0,'PLAYER_MOUNT':0x2e496,
 'FIRE_FLAT':0x2c24a,'FIRE_SLOPE':0x2c71e,
 'FIRE_JUMP_CROUCH':0x2d286,'FIRE_JUMP_AIR':0x2d298,'FIRE_AIR':0x2d394,'FIRE_AIR_ANGLE':0x2d380,
 'FIRE_IDLE':0x2c190,'FIRE_RECOIL':0x2c432,'FIRE_FLAT_RESUME':0x2c554,
 'FIRE_MOVE_RIGHT':0x2c572,'FIRE_MOVE_LEFT':0x2c648,'FIRE_AIR_LOOP':0x2d49e,'FIRE_AIR_RESUME':0x2d38a,
 'DESTROYED_SLIDE':0x2bbf2,'KNOCKED':0x2f0e6,'DESTROYED':0x2fc70,
 'DEATH_START':0x2dcc0,'DEATH_EXPLODE':0x2da38,'DEATH_LAUNCH':0x2db52,'RETIRE':0x2dcbc,
 'DEATH_FADE':0x2dc5c,'RETIRED_RESET':0x2dcaa,'INACTIVE':0x400,
 'DESTROYED_SLIDE_RESUME':0x2bba4,'DESTROYED_SLIDE_LOOP':0x2bc78,'KNOCKED_RESUME':0x2f0dc,
 'DESTROYED_RESUME':0x2fc8e,'DESTROYED_STOP':0x2fc98,'DESTROYED_FRAME':0x2fd1a,'SELF_DESTRUCT':0x2fe6a,
 'DROP_INIT':0x2aef8,'DROP_FRAME':0x2b004,'DROP_DESCEND':0x2b05e,'DROP_DESCEND_FRAME':0x2b09e,
 'BOSS_ARENA':0x2add6,'BOSS_DROP_INIT':0x2b166,'BOSS_DROP_FRAME':0x2b20a,
 'BOSS_DESCEND':0x2b264,'BOSS_DESCEND_FRAME':0x2b2aa,'BOSS_DESCEND_STEADY':0x2b2be,
}
PENDING={}
ROUTES={'slug_body_damage_ground':(0x2a060,3),'slug_body_damage_air':(0x2a06c,3),
 'slug_body_down':(0x2a078,5),'slug_body_landing_down':(0x2a08c,5),'slug_body_jump':(0x2c9b0,5)}
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args()
 raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('P-ROM fingerprint')
 c=AnimationCompiler(raw);names={v:k for k,v in {**IMPLEMENTED,**PENDING}.items()}
 rows=[];decl=[];ledger=[]
 for name,(at,count) in ROUTES.items():
  pointers=[c.long(at+4*i) for i in range(count)]
  vals=['SB_STATE_COUNT' if v==0xffffffff else 'SB_'+names[v] for v in pointers]
  decl.append(f'extern const uint8_t {name}[{count}];')
  rows.append(f'const uint8_t {name}[{count}]={{'+','.join(vals)+'};')
  ledger.append(dict(name=name,source=at,pointers=pointers,states=vals))
 for name,at,count in [('slug_body_steer',0x2c900,4),('slug_body_accel_right_sounds',0x2b8ce,5),
                      ('slug_body_accel_left_sounds',0x2ba34,5),('slug_body_bounds',0x27964e,6),
                      ('slug_body_wide_bounds',0x27965a,6),('slug_body_destroyed_sounds',0x2bb9a,5)]:
  vals=[c.signed(at+2*i) for i in range(count)]
  decl.append(f'extern const int16_t {name}[{count}];')
  rows.append(f'const int16_t {name}[{count}]={{'+','.join(map(str,vals))+'};')
  ledger.append(dict(name=name,source=at,values=vals))
 (R/'generated/slug-body-data.h').write_text('#ifndef SLUG_BODY_DATA_H\n#define SLUG_BODY_DATA_H\n#include <stdint.h>\n'+'\n'.join(decl)+'\n#endif\n')
 (R/'generated/slug-body-data.c').write_text('#include "../src/slug_body.h"\n#include "slug-body-data.h"\n'+'\n'.join(rows)+'\n')
 result=dict(prom_sha256=digest,scope='hand-written locomotion, mount, firing, hit-reaction, destruction, ordinary/boss drop and arena groups; source-only world wiring',
  implemented=[dict(state='SB_'+k,source=v) for k,v in IMPLEMENTED.items()],
  pending_destinations=[dict(state='SB_'+k,source=v) for k,v in PENDING.items()],data=ledger,
  alias_rules={'drop_init_arguments_0_through_4':[0x2aef8,0x2af1c,0x2af40,0x2af64,0x2af88],
   'boss_init_arguments_0_and_1':[0x2b166,0x2b1a2],
   'boss_retirement_aliases':[0x2b18a,0x2b196],
   'boss_constructor_footer':'$02A242 calls ordinary chassis $03060A then RTS; no turret/wheel/marker',
   'boss_arena_footer':'$02AE36 schedules $02D02E for the next tick',
   'drop_descend_angle_frame':'$02B15A writes constant 20, not 20 plus angle frame',
   'acceleration_argument_1':'entry+6, skip dust',
   'stall_argument_1':0x2eca6,'stall_argument_2':0x2ecb4,
   'set_speed_parameter':'$02F614 and subsequent wrappers set VX then enter $02F5D6',
   'land_slope_frame':'$02CE7E bypasses the 17-tick timer/animation entry',
   'fall_start':'retains animation and sets angle status on each frame',
   'retire':'$02DC8C cleanup + heartbeat now, $02DCAA fixed-slot reset next tick',
   'destroyed_slide_resume':'$02BBA4 skips kill/timer and enters $02BC78 directly',
   'knocked_resume':'$02F0DC skips kill/timer but retains the delayed speed wrapper',
   'destroyed_resume':'$02FC8E skips kill/timer; $02FC98 instead clears the slope impulse'},
  not_implemented=['Scene placement/callers and anchor-claiming enemies',
   'Sprite pool counters and all-pose rendering'],
  validation='source extraction and ARM compile only; no gameplay tests')
 (R/'generated/slug-body-source.json').write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps(dict(implemented_states=len(IMPLEMENTED),pending_destinations=len(PENDING),tables=len(ledger))))
if __name__=='__main__':main()
