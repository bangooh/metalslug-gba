#!/usr/bin/env python3
"""Carrier 129/130/131 and connected crew: data conversion, never execution."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS={'COCKPIT': 3022600, 'EJECT': 3022622, 'HATCH': 3022658, 'OPEN': 3022748, 'OPEN2': 3022840, 'DROP': 3022892, 'FADE': 3008224, 'CANNON': 3023018, 'CANNON_FALL': 3023076, 'EMERGE': 3023494, 'JUMP': 3023592, 'TURN': 2736056, 'RUN': 2733892, 'STAND': 2734024, 'IDLE': 2734102, 'AIM': 2735924, 'FIRE': 2734704, 'RIDE': 2738044, 'LEAP': 2738112, 'LEAP_B': 2738264, 'LAND': 3008450, 'DIVE': 2738530, 'DIVE_LOOP': 2738626, 'DIVE_LAND': 2738578, 'GUN_HOLD': 3023144, 'GUN_POSE': 3023276, 'GUARD': 2736302, 'GUARD_B': 2736708, 'GUARD_HIT': 2736596, 'GUARD_B_HIT': 2736780, 'HULL_WRECK0': 3022282, 'HULL_WRECK1': 3022428, 'HULL_WRECK2': 3022574}
TABLES={'HULL_A': [3022162, 4], 'HULL_B': [3022308, 4], 'HULL_C': [3022454, 4], 'GUN_POSES': [3023252, 6]}
RANKS=dict(HP=0x2bcb42,COCKPIT_HP=0x2bcbc4,HP_B=0x2bcc46,COCKPIT_HP_B=0x2bccc8,SPAWN=0x2bcd4a,SPAWN_LATE=0x2bcdcc,CREW_WAIT=0x2bce4e,CREW_WAIT_LATE=0x2bced0)
FIXED=dict(BODY_INTRO=('DEFENSE',0x2e26b0),BODY=('DEFENSE',0x2e251c))
def vals(xs):return ','.join(map(str,xs))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw);entries={k:c.entry(v) for k,v in ROOTS.items()}
 tables={k:[c.entry(c.long(at+4*i)) for i in range(n)] for k,(at,n) in TABLES.items()}
 fixed={k:c.binding(*v) for k,v in FIXED.items()};c.compile()
 if {x['source'] for x in c.callbacks}!={0x4a110,0x29cb32,0x55bce} or c.parameters:raise ValueError('unreviewed carrier callback')
 if {(x['source_offset'],x['width']) for x in c.fields}!={(0x3b,1),(0x78,2),(0x20,1),(0x70,2),(0x5c,4),(0x3a,1)}:raise ValueError('unreviewed carrier field')
 templates={129:0x7dfc4,130:0x7e000,131:0x7e03c}
 if any(c.long(0xe8000+4*i)!=at for i,at in templates.items()):raise ValueError('template mapping')
 h=['#ifndef NATIVE_CARRIER_PROGRAMS_H','#define NATIVE_CARRIER_PROGRAMS_H','#include "../src/bosses.h"',
 'enum CarrierAnimation {'+','.join('CRA_'+k for k in entries)+',CRA_COUNT};',
 'enum CarrierCallback {'+','.join('CRC_'+format(v['source'],'X') for v in c.callbacks)+'};',
 'enum CarrierField {'+','.join('CRF_'+format(v['source_offset'],'X')+'_'+str(v['width']) for v in c.fields)+'};',
 'enum CarrierRank {'+','.join('CRR_'+k for k in RANKS)+',CRR_COUNT};',
 'enum CarrierBinding {'+','.join('CRB_'+k+'='+str(v) for k,v in fixed.items())+'};',
 'extern const struct AnimationProgram carrier_program;','extern const uint16_t carrier_entries[CRA_COUNT];',
 'extern const struct BossBindingSet carrier_bindings['+str(len(c.bindings))+'];','struct CarrierRankData {const uint16_t*values;const uint8_t*choices;};','extern const struct CarrierRankData carrier_rank[CRR_COUNT];',
 *['extern const uint16_t carrier_'+k.lower()+'['+str(n)+'];' for k,(_,n) in TABLES.items()],
 'extern const struct InteractionContact carrier_contacts[6];','extern const int16_t carrier_bounds[2][4],carrier_cannon_clocks[4];',
 'extern const uint8_t carrier_hatch_delays[4][6],carrier_arc[37],carrier_gunner_arc[128];']
 out=c.c_source('carrier_program','carrier-programs.h');defs=[];attacks=[];sets=[];filters={}
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
 out+=['static const struct HitDefense defs[]={'+','.join(defs)+'};']
 if attacks:out+=['static const struct HitAttack attacks[]={'+','.join(attacks)+'};']
 array('struct BossBindingSet','carrier_bindings[]',','.join(sets));array('uint16_t','carrier_entries[CRA_COUNT]',vals(entries.values()))
 for k,v in tables.items():array('uint16_t','carrier_'+k.lower()+'['+str(len(v))+']',vals(v))
 rank_rows=[]
 for i,at in enumerate(RANKS.values()):
  mode=c.word(at)
  if mode==2:
   choices=[c.byte(at+2+j) for j in range(64)]
   array('uint8_t',f'carrier_choices{i}[64]',vals(choices))
   array('uint16_t',f'carrier_values{i}[{(max(choices)+1)*8}]',vals(c.word(at+66+2*j) for j in range((max(choices)+1)*8)))
   rank_rows.append('{carrier_values'+str(i)+',carrier_choices'+str(i)+'}')
  else:
   if mode not in (0,1):raise ValueError('rank mode')
   array('uint16_t',f'carrier_values{i}[64]',vals(c.word(at+2+2*j) for j in range(64)))
   rank_rows.append('{carrier_values'+str(i)+',0}')
 array('struct CarrierRankData','carrier_rank[CRR_COUNT]',','.join(rank_rows))
 contacts=[]
 for at in (0x2e2332,0x2e237e,0x2e23ca,0x2e2416,0x2e2462,0x2e24ae):
  if c.byte(at)!=128:raise ValueError('contact format')
  contacts.append('{{'+vals(c.signed(at+6+2*i) for i in range(4))+'},'+vals([c.signed(at+2),c.byte(at+1)])+'}')
 array('struct InteractionContact','carrier_contacts[6]',','.join(contacts))
 array('int16_t','carrier_bounds[2][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x2e2ad8,0x2e2ae0)))
 array('int16_t','carrier_cannon_clocks[4]',vals(c.signed(0x2e1d4a+2*i) for i in range(4)))
 array('uint8_t','carrier_hatch_delays[4][6]',','.join('{'+vals(c.byte(c.long(0x2e1f7a+4*i)+j) for j in range(6))+'}' for i in range(4)))
 array('uint8_t','carrier_arc[37]',vals(c.byte(0x2e22fa+i) for i in range(37)))
 # $07FE98 uses unsigned shifts. Retain all 128 reachable byte indices,
 # including adjacent authored contact data for negative offsets.
 array('uint8_t','carrier_gunner_arc[128]',vals(c.byte(0x2e2320+i) for i in range(128)))
 h.append('#endif')
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,tables=TABLES,rank_sources=RANKS,templates=templates,
  source_files={p:hashlib.sha256((a.source/p).read_bytes()).hexdigest() for p in ['asm/enemies/bosses/boss2_crab_carrier_07a00a.s','asm/enemies/squad/deploy_module_07fbd2.s','asm/props/fx_explosions_platforms_crew_076000.s','asm/entity/anim_op08_callbacks.s','src/kernel/task_setters_w.c']},
  source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in ((0x7dfc4,0x8008a),(0x79b6e,0x79c70),(0x79d9a,0x79e9c),(0x19cb32,0x19cb6a))],
  implementation=['src/carrier.c','src/carrier_parts.c','src/carrier_crew.c'],validation='Source conversion only; not gameplay validation',complete_port=False)
 for name,data in [('carrier-programs.h','\n'.join(h)),('carrier-programs.c','\n'.join(out)),('carrier-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(data+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),fields=len(c.fields),ranks=len(RANKS))))
if __name__=='__main__':main()
