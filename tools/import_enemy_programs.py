#!/usr/bin/env python3
"""Compile shared human death and enemy ordnance, without executing gameplay."""
import argparse,hashlib,json,struct,re
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(DEATH_GROUND=0x4acfe,DEATH_SIDE=0x4ad90,DEATH_BACK=0x4ae22,BURST=0x4af08,BURST_LAND=0x4af9e,
 CHARRED_AIR=0x4b078,CHARRED_LAND=0x4b0c6,DEATH_AIR=0x4b136,DEATH_PALETTE=0x4b13c,
 KNOCK_BACK=0x4b1f6,KNOCK_FORWARD=0x4b28c,COLLAPSE=0x4b34a,COLLAPSE_A=0x4b57c,COLLAPSE_B=0x4b61a,
 TUMBLE_LAND=0x4b6b8,LOW_FALL=0x4b74c,HIGH_FALL=0x4b78c,KNOCK_LAND=0x4b7b8,LAUNCH=0x4b7ec,
 BURN=0x4b82a,BURN_LAND=0x4b898,FLAME=0x4b926,BLOOD_B=0x4b9f0,BLOOD_A=0x4bad2,
 SMOKE_BODY=0x4bb44,SMOKE=0x4bb4a,BURN_RUN=0x2b604a,
 GRENADE=0x2c72ba,GRENADE_ANGLE=0x2c72c0,BOUNCE=0x29cda4,BOUNCE_REST=0x29cdb6,
 BOUNCE_REST2=0x29cfa8,BOUNCE_FIZZLE=0x29cfd2,BOUNCE_BLAST=0x29d01a,MORTAR=0x56378,ROLLER=0x56670,
 GRENADE_BLAST=0x2df180,SHELL_BLAST=0x2dd594)
ROOTS.update(dict(GUARD=0x29b8ee,GUARD_FIXED=0x288394,GUARD_ATTACK=0x29bb4c,GUARD_THROW_A=0x2b6596,GUARD_THROW_B=0x2b6644,
 GUARD_RECOVER=0x288414,GUARD_RETREAT=0x2883f4,WALK_A=0x427fa,WALK_B=0x4287e,RUN=0x29b744,
 IDLE=0x29b816,IDLE_ARMED=0x2b5c10,MELEE=0x2b70d2,THROW=0x29ba70,BOUNCE_THROW=0x29b9ce,
 MORTAR_THROW=0x29bbec,ROLLER_THROW=0x29bc64,LOWER=0x29bd84,RAISE=0x29b956,
 EXIT_WAIT=0x2b6ef8,EXIT_STAND=0x29bd40,EXIT_BRAKE=0x2b5cfa,EXIT_LEAP=0x2b5d58,
 EXIT_FALL=0x2b5b92,EXIT_LAND_VERTICAL=0x2b5c22,EXIT_LAND_HORIZONTAL=0x29b7c8))
ROOTS.update(dict(GRAB_LEAP=0x2b6922,GRAB_TOUCH=0x2b6a02,GRAB_ALIGN=0x2b6a60,GRAB_CANNON=0x2b6d64,GRAB_RIDER=0x2b6aa4,GRAB_TREAD=0x2b6832,GRAB_HIT=0x2b6892,GRAB_SHAKE=0x2b6c32,GRAB_SHAKE_TREAD=0x2b6bc0,GRAB_THROWN=0x2b6ca4,GRAB_RELEASE=0x2b6d00,SLUG_LEAP=0x2b6f40,SLUG_HIT=0x2b6fba,SLUG_LAND=0x2b7030,STOP_BACK=0x2b5d14,SLOPE_IDLE=0x2b5f28,STEP_FRONT=0x2b5c56,STEP_FRONT_SLOPE=0x2b5f56,STEP_BACK=0x2b5ca8,STEP_BACK_SLOPE=0x2b5fd0,LOOK=0x29bf34,CROUCH_STEP=0x2b5bcc,JUMP_BACK=0x2b5e68,CLIMB_UP_START=0x2b60ce,CLIMB_UP=0x2b63f6,CLIMB_UP_END=0x2b6124,CLIMB_DISMOUNT=0x2b6204,CLIMB_DOWN_START=0x2b6284,CLIMB_DOWN=0x2b648a,CLIMB_DOWN_END=0x2b63b4,TAUNT_END=0x29bfc4,JUMP_IN=0x2b69d8,GRAB_JUMP_IN=0x2b69aa,SLUG_JUMP_IN=0x2b6fee,TAUNT_0=0x29bde8,TAUNT_1=0x29be2c,TAUNT_2=0x29be70,TAUNT_3=0x29bf04))
ROOTS.update(dict(DEPLOY_ENTER=0x2e2286,DEPLOY_TURN=0x29bfb8,DEPLOY_IDLE_A=0x29b85e,DEPLOY_IDLE_B=0x29b8a6))
ROOTS.update(dict(RING_WALKER=0x5a696))
ROOTS.update(dict(STATIONARY_A=0x29ca0a,STATIONARY_B=0x29ca20))
ROUTES={0x4a3f8:'TUMBLE',0x4a604:'AIR',0x4a568:'KNOCK',0x4a702:'RANDOM_BURST',0x4a7b0:'CHARRED',
 0x4a72e:'BURST',0x4a68c:'GROUND',0x4a88c:'GROUND_CHARRED',0x4a89a:'HARD_HIT',0x4a8fa:'VERTICAL',
 0x4a948:'BURN',0x4a5a6:'COLLAPSE',0x4a5b4:'SIDE',0x4a680:'RANDOM_GROUND'}
ROOTS.update(dict(ALLEN_WALK=0x296894,ALLEN_IDLE=0x296bd8,ALLEN_BACK=0x296908,ALLEN_TURN=0x296954,
 ALLEN_TAUNT=0x296b76,ALLEN_FIRE_LEVEL=0x29658a,ALLEN_FIRE_DIAGONAL=0x29668c,ALLEN_LAND=0x296a04,
 ALLEN_AIR=0x2969bc,ALLEN_KNIFE=0x296c82,ALLEN_DEATH_GROUND=0x296a4c,ALLEN_DEATH_AIR=0x296abc,
 ALLEN_DEATH_LAND=0x296adc,ALLEN_STAGGER=0x296b24,ALLEN_FLASH_LEVEL=0x29663a,ALLEN_FLASH_DIAGONAL=0x296784,
 ALLEN_GRENADE=0x51208,ALLEN_BLAST=0x29e76c,ALLEN_BULLET=0x2967d6,ALLEN_IMPACT_LEVEL=0x296d26,ALLEN_IMPACT_DIAGONAL=0x296d78))
FIXED={'GRENADE':('ATTACK',0x29cbfe),'BOUNCE':('ATTACK',0x29cca6),'MORTAR':('ATTACK',0x562ce),
 'ROLLER':('ATTACK',0x565c8),'ROLLER_DEFENSE':('DEFENSE',0x5661c),'FALLING_BODY':('DEFENSE',0x4aeb4)}
FIXED.update(RING_WALKER=('DEFENSE',0x29b4a4))
FIXED.update(STATIONARY=('DEFENSE',0x8b950))
FIXED.update(MELEE_PROBE=('ATTACK',0x57440),MELEE=('ATTACK',0x57494))
FIXED.update(GRAB_TOUCH_DEF=('DEFENSE',0x2b67de),GRAB_P1=('ATTACK',0x2b71dc),GRAB_P2=('ATTACK',0x2b7230),RIDER_P1=('ATTACK',0x2b7284),RIDER_P2=('ATTACK',0x2b72d8))
SELECTORS={0x56676+i*58:3 for i in range(8)}
FIXED.update(ALLEN_BODY=('DEFENSE',0x2963c4),ALLEN_SENSOR0=('DEFENSE',0x2964e2),
 ALLEN_SENSOR1=('DEFENSE',0x296536),ALLEN_SENSOR2=('DEFENSE',0x29643a),ALLEN_SENSOR3=('DEFENSE',0x29648e),
 ALLEN_KNIFE_PROBE=('ATTACK',0x296dca),ALLEN_GRENADE=('ATTACK',0x51070),ALLEN_BLAST=('ATTACK',0x51114),ALLEN_BULLET=('ATTACK',0x5148c))
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);args=p.parse_args()
 raw=(args.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw,SELECTORS);entries={k:c.entry(v) for k,v in ROOTS.items()}
 tumble=[[c.entry(c.long(at+i*4)) for i in range(5)] for at in (0x4a408,0x4a4b0)]
 fixed={k:c.binding(*v) for k,v in FIXED.items()};c.compile()
 expected={0x4a0d4,0x4a0ea,0x4a128,0x4a1ba,0x4b052,0x267e2,0x283ca,
 0x4a110,0x4a108,0x4a120,0x56066,0x5e8da,0x56438,0x55bce,0x56844,0x4a118,0x2b6114,0x2b611c,0x283d8}
 expected.update((0x5080a,0x50816,0x50822,0x5087c,0x5063e))
 if {v['source'] for v in c.callbacks}!=expected or c.parameters:raise ValueError('unreviewed callback/parameter')
 if {(f['source_offset'],f['width']) for f in c.fields}!={(0x74,2),(0x3b,1),(0x76,4),(0x28,2),(0x70,2),(0x3a,1),(0x20,1)}:raise ValueError('unreviewed field')
 if set(n['op'] for n in c.nodes)-{'WAIT','DRAW','END','SELECT','JUMP','LOOP','SET','AND','XOR','CALL','DEFENSE','ATTACK','SOUND','WAIT_TABLE'}:raise ValueError('unreviewed operation')
 h=['#ifndef ENEMY_PROGRAMS_H','#define ENEMY_PROGRAMS_H','#include "../src/animation.h"','#include "../src/hitbox.h"',
 'enum EnemyAnimation {'+','.join('EAN_'+k for k in entries)+',EAN_COUNT};',
 'enum EnemyField {'+','.join('ENF_'+format(f['source_offset'],'X') for f in c.fields)+'};',
 'enum EnemyCallback {'+','.join('ENC_'+format(v['source'],'X') for v in c.callbacks)+'};',
 'enum HumanDeathRoute {'+','.join('HDR_'+v for v in ROUTES.values())+'};',
 'enum EnemyBinding {'+','.join('ENB_'+k+'='+str(v) for k,v in fixed.items())+'};',
 'struct EnemyBindingSet {const struct HitDefense*defense;const struct HitAttack*attack;uint16_t count;};',
 'extern const struct AnimationProgram enemy_program;',
 'extern const struct EnemyBindingSet enemy_bindings['+str(len(c.bindings))+'];',
 'extern const uint16_t enemy_entries[EAN_COUNT],human_tumble_entries[2][5];',
 'extern const uint8_t human_death_routes[4][34],human_gib_delays[8][6];',
 'extern const int16_t enemy_grenade_trajectories[24][4],enemy_projectile_bounds[4],enemy_blast_bounds[2][4];',
 'extern const uint16_t enemy_mortar_hp[8][4][2],enemy_grenade_palettes[4];',
 'extern const uint16_t infantry_rank[2][8][4][2],infantry_speeds[3][2][8];',
 'extern const uint8_t infantry_speed_choices[3][8][4][2];',
 'extern const int16_t infantry_bounds[2][4];',
 'extern const uint32_t enemy_hit_scores[32];','extern const uint16_t deploy_rank[64],deploy_idle[4];',
 'extern const uint16_t allen_rank[4][8][4][2],allen_stage_palettes[5],allen_voices[4];',
 'extern const int16_t allen_bounds[4],allen_target_bounds[4];','#endif']
 out=c.c_source('enemy_program','enemy-programs.h');defenses=[];attacks=[];sets=[];filters={}
 for b in c.bindings:
  q=b['source'];records=defenses if b['kind']=='DEFENSE' else attacks;first=len(records)
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
    if group>4 or kind>34 or shape not in (0,4):raise ValueError('unreviewed attack')
    records.append('{'+rect+','+vals([damage,group,kind,strength,shape,offscreen])+'}')
   else:raise ValueError(b)
   q+=80
  else:raise ValueError('unterminated hit records')
  sets.append('{'+('defenses+'+str(first)+',0' if b['kind']=='DEFENSE' else '0,attacks+'+str(first))+','+str(count)+'}')
 out+=['static const struct HitDefense defenses[]={'+','.join(defenses)+'};','static const struct HitAttack attacks[]={'+','.join(attacks)+'};',
 'const struct EnemyBindingSet enemy_bindings[]={'+','.join(sets)+'};']
 def array(ty,name,data):out.append('const '+ty+' '+name+'={'+data+'};')
 array('uint16_t','enemy_entries[EAN_COUNT]',vals(entries.values()))
 if c.word(0x2b7716)!=0:raise ValueError('deploy rank mode')
 array('uint16_t','deploy_rank[64]',vals(c.word(0x2b7718+2*i) for i in range(64)))
 array('uint16_t','deploy_idle[4]',vals(c.entry(c.long(0x28dc36+4*i)) for i in range(4)))
 array('uint16_t','human_tumble_entries[2][5]',','.join('{'+vals(v)+'}' for v in tumble))
 array('uint8_t','human_death_routes[4][34]',','.join('{'+','.join('HDR_'+ROUTES[c.long(c.long(0x49faa+4*k)+4*i)] for i in range(34))+'}' for k in range(4)))
 array('uint8_t','human_gib_delays[8][6]',','.join('{'+vals(c.byte(c.long(0x4af5a+4*k)+i) for i in range(6))+'}' for k in range(8)))
 array('int16_t','enemy_grenade_trajectories[24][4]',','.join('{'+vals(c.signed(0x55c08+8*k+2*i) for i in range(4))+'}' for k in range(24)))
 array('int16_t','enemy_projectile_bounds[4]',vals(c.signed(0x55cc8+2*i) for i in range(4)))
 array('int16_t','enemy_blast_bounds[2][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x2df1fc,0x2df206)))
 array('uint16_t','enemy_grenade_palettes[4]',vals({0x16:0x15d,0x18:0x164,0x1a:0x165}[c.word(0x55cd0+2*i)] for i in range(4)))
 if c.word(0x2b7632)==2:raise ValueError('unreviewed rank format')
 array('uint16_t','enemy_mortar_hp[8][4][2]',','.join('{'+','.join('{'+vals(c.word(0x2b7634+16*r+4*f+2*p) for p in range(2))+'}' for f in range(4))+'}' for r in range(8)))
 array('uint32_t','enemy_hit_scores[32]',vals(c.long(0x51890+i*4) for i in range(32)))
 allen_tables=(0x296e7a,0x296efc,0x296f7e,0x297000)
 if any(c.word(at)!=0 for at in allen_tables):raise ValueError('Allen rank format changed')
 array('uint16_t','allen_rank[4][8][4][2]',','.join('{'+','.join('{'+','.join('{'+vals(c.word(at+2+16*r+4*f+2*p) for p in range(2))+'}' for f in range(4))+'}' for r in range(8))+'}' for at in allen_tables))
 array('uint16_t','allen_stage_palettes[5]',vals(c.word(0x296e72+2*i) for i in range(5)))
 array('uint16_t','allen_voices[4]',vals(c.word(0x509d6+2*i) for i in range(4)))
 array('int16_t','allen_bounds[4]',vals(c.signed(0x50726+2*i) for i in range(4)))
 array('int16_t','allen_target_bounds[4]',vals(c.signed(0x503ea+2*i) for i in range(4)))
 rank_tables=(0x2b74ec,0x2b746a);speed_tables=(0x2b75d0,0x2b76b4,0x2b756e)
 if any(c.word(at)!=1 for at in rank_tables) or any(c.word(at)!=2 for at in speed_tables):raise ValueError('rank format changed')
 array('uint16_t','infantry_rank[2][8][4][2]',','.join('{'+','.join('{'+','.join('{'+vals(c.word(at+2+16*r+4*f+2*p) for p in range(2))+'}' for f in range(4))+'}' for r in range(8))+'}' for at in rank_tables))
 array('uint8_t','infantry_speed_choices[3][8][4][2]',','.join('{'+','.join('{'+','.join('{'+vals(c.byte(at+2+8*r+2*f+p) for p in range(2))+'}' for f in range(4))+'}' for r in range(8))+'}' for at in speed_tables))
 if any(c.byte(at+2+i)>1 for at in speed_tables for i in range(64)):raise ValueError('speed row bound')
 array('uint16_t','infantry_speeds[3][2][8]',','.join('{'+','.join('{'+vals(c.word(at+66+16*k+2*i) for i in range(8))+'}' for k in range(2))+'}' for at in speed_tables))
 array('int16_t','infantry_bounds[2][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x28838c,0x2b71d4)))
 templates={0:0x427ca,27:0x4258e,28:0x425b6,249:0x4290c,281:0x42d42,282:0x42d3c,194:0x5a72e}
 templates.update(dict(enumerate([0x573a2,0x573ae,0x573de,0x5736a,0x57374,0x57396,0x57386,0x5729a,0x572a6,0x572b2,0x572be,0x572d2,0x572e2,0x572f2,0x57302,0x5731a,0x57326,0x5733a,0x5734e,0x57276,0x57282,0x5728e],1)));templates[152]=0x8f18c
 templates.update({168:0x8b9a2,169:0x8b9aa,170:0x8b9b2})
 templates[328]=0x508d6
 templates[228]=templates[229]=0x46140
 if [c.long(at+4*i) for at in (0x8ba9e,0x8baae) for i in range(4)]!=[0x8baf6]*8:raise ValueError('stationary hit routes')
 if [c.long(0x8b944+4*i) for i in range(3)]!=[0xffffffff,0x8bada,0x8babe]:raise ValueError('stationary hit dispatch')
 if any(c.long(0xe8000+4*i)!=at for i,at in templates.items()):raise ValueError('enemy template dispatch')
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,selectors=SELECTORS,routes={hex(k):v for k,v in ROUTES.items()},fixed_bindings=FIXED,
  source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in
   ((0x423ac,0x42e8a),(0x46140,0x46322),(0x463c2,0x463da),(0x49d8a,0x4bb8e),(0x5029c,0x515fa),(0x55bce,0x56a38),(0x56acc,0x59332),(0x8b944,0x8bb8c),(0x8f18c,0x8f308),(0x8f738,0x8f8e2),(0x5e9b6,0x5ea1c),(0x51914,0x5199a),(0x8f308,0x8f3a6),(0x5a696,0x5a824),(0x8f470,0x8f69c))],
  implementation=['src/enemy_common.c','src/human_death.c','src/enemy_ordnance.c','src/infantry.c','src/soldier.c','src/enemy_environment.c','src/allen.c','src/deploy.c','src/stationary_guard.c','src/ring_actors.c'],templates=templates,
  native_states={name:[v.strip() for v in re.search(r'enum State \{(.*?)\};',(R/'src'/name).read_text(),re.S).group(1).split(',')] for name in ('human_death.c','enemy_ordnance.c','infantry.c','soldier.c','allen.c')},
  allen=dict(template=328,entry=0x508d6,rank_tables=allen_tables,
   data_range=dict(start=0x2963c4,end_exclusive=0x297082,sha256=hashlib.sha256(raw[c.offset(0x2963c4):c.offset(0x297082)]).hexdigest()),
   semantic_corrections=['Level/diagonal MG names follow actual callback vectors, not assembly names',
    'The five HP segments change private palette contents; no health-bar HUD call exists here',
    '$05E770 selects an impact sound, not a flash animation',
    '$05033A repeats on remaining_x AND remaining_y, not OR',
    '$050F10 stagger has no incoming reference in the verified PROM; retained entry is not counted as reachable AI',
    '$05158A BCS is not taken after the FF-sentinel CMP in $0283D8; do not invent impact behavior from attack flags'],
   required_inputs=['rank and player-count','RNG','secondary camera Y','terrain','sound'],
   pending=['Mission event caller and complete mission flow','All-pose renderer and private palette allocation','Integrated gameplay validation']),
  stationary_guard=dict(templates=[168,169,170],hit_tables=[0x8b944,0x8ba9e,0x8baae],rng_on_hit='one unrotated word; both four-way tables have identical destinations',native_states=['NEW','IDLE_A','IDLE_B','TAUNT','HIT','HIT_FRAME','DELETE']),
  adaptations=['Q8 world positions; source screen WORD comparisons reconstructed at their consumers',
   'One-frame death position ring deliberately retains screen coordinates across camera movement',
   'Explicit scene rank/player-count/damage/ordnance inputs; physical palettes and visibility supplied by the future renderer',
   'Ordinary soldier full Think and escape use separate semantic state, preserving source word/byte writes and ordered RNG calls',
   'Unplaced fixed P2 uses allocator FF coordinate words for the original no-live-player fallback',
   'Allen private palette mutations publish the original palette template IDs for the future renderer',
   'Per-hit scores use decimal storage with the existing exact packed-BCD addition/cap adapter'],
  pending=['Remaining weapon-crew and specialized soldier families',
   'Remaining enemy/vehicle/boss families beyond Allen 328 and scene/event callers',
   'Original renderer, rank controller, score HUD and released-loop integration',
   'Integrated functional validation only after full source connection'],
  validation='Source extraction and ARM compilation only; no gameplay/emulator/regression/performance tests',complete_port=False)
 for name,data in [('enemy-programs.h','\n'.join(h)),('enemy-programs.c','\n'.join(out)),('enemy-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(data+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),defenses=len(defenses),attacks=len(attacks))))
if __name__=='__main__':main()
