#!/usr/bin/env python3
"""Template 117 body, weapons and destruction: data conversion, never execution."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS={'MOVE_LEFT': 3038000, 'MOVE_RIGHT': 3038074, 'CENTRAL': 3038360, 'CENTRAL_WRECK': 3038496, 'SIDE_IDLE': 3038508, 'SIDE_PREPARE': 3038868, 'SIDE_BURST': 3039050, 'SIDE_CLOSE': 3039392, 'SIDE_WRECK': 3039554, 'CENTRAL_ATTACHED': 3039996, 'SCATTER': 3041764, 'BEAM': 3039570, 'PROJECTILE': 3041544, 'PROJECTILE_FALL': 3041600, 'PROJECTILE_IMPACT': 3041622, 'SIDE_ATTACHED': 3041064, 'PELLET': 3073256, 'GROUND_IMPACT': 3041406, 'BALLISTIC': 3041520, 'PARACHUTE': 3042026, 'DEATH_OVERLAY': 3041682, 'GROUND_BLAST_A': 2743176, 'GROUND_BLAST_B': 2743318}
TABLES={'BODY': [3036190, 4], 'PLATE': [3036206, 6], 'PLATE_BROKEN': [3036230, 6], 'TROOP': [3036282, 10], 'SIDE': [3037234, 5]}
# The six shot-selector streams contain a duplicate final pose for shot=5
# during burst-to-close handoff; the 5C side-angle selector still has five.
SELECTORS={3038362: 7, 3038510: 5, 3039052: 6, 3041066: 6, 3040014: 7, 3039164: 6, 3041178: 6, 3039604: 7, 3040144: 7, 3039276: 6, 3041290: 6, 3040274: 7, 3039750: 7, 3040404: 7, 3040540: 7, 3040670: 7, 3040800: 7, 3040930: 7}
RANKS={'HP': 2875414, 'THRESHOLD_BOTH': 2875544, 'THRESHOLD_ONE': 2875674, 'SIDE_HP': 2875804, 'PLATE_HP': 2875934, 'PROJECTILE_HP': 2876064, 'CENTRAL_WAIT': 2876194, 'CENTRAL_WAIT_LATE': 2876324, 'SIDE_WAIT': 2876454, 'SIDE_WAIT_LATE': 2876584, 'CENTRAL_FIRE': 2876974, 'CENTRAL_FIRE_LATE': 2877104, 'SPEED': 2877234, 'SPEED_LATE': 2877364, 'BURST_COUNT': 2877494, 'BURST_COUNT_LATE': 2877624, 'BURST_WAIT': 2877754, 'BURST_WAIT_LATE': 2877884}
FIXED=dict(BODY=("DEFENSE",0x2e3f76),SIDE=("DEFENSE",0x2e3fca))
def vals(xs):return ','.join(map(str,xs))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw,SELECTORS);entries={k:c.entry(v) for k,v in ROOTS.items()}
 tables={k:[c.entry(c.long(at+4*i)) for i in range(n)] for k,(at,n) in TABLES.items()}
 fixed={k:c.binding(*v) for k,v in FIXED.items()};attack_table=[c.binding("ATTACK",c.long(0x2e545e+4*i)) for i in range(7)];c.compile()
 if {x['source'] for x in c.callbacks}!={0x8321c,0x831b8,0x283ca,0x83164,0x83254,0x832b6,0x434ce,0x83262,0x83270,0x831c2,0x8328c,0x83158} or c.parameters:raise ValueError('unreviewed boss117 callback')
 if {(x['source_offset'],x['width']) for x in c.fields}!={(0x5c,2),(0x5e,2)}:raise ValueError('unreviewed boss117 field')
 templates={117:0x81908}
 if any(c.long(0xe8000+4*i)!=at for i,at in templates.items()):raise ValueError('template mapping')
 h=['#ifndef NATIVE_BOSS117_PROGRAMS_H','#define NATIVE_BOSS117_PROGRAMS_H','#include "../src/bosses.h"',
 'enum Boss117Animation {'+','.join('B117A_'+k for k in entries)+',B117A_COUNT};',
 'enum Boss117Callback {'+','.join('B117C_'+format(v['source'],'X') for v in c.callbacks)+'};',
 'enum Boss117Field {'+','.join('B117F_'+format(v['source_offset'],'X')+'_'+str(v['width']) for v in c.fields)+'};',
 'enum Boss117Rank {'+','.join('B117R_'+k for k in RANKS)+',B117R_COUNT};',
 'enum Boss117Binding {'+','.join('B117B_'+k+'='+str(v) for k,v in fixed.items())+'};',
 'extern const struct AnimationProgram boss117_program;','extern const uint16_t boss117_entries[B117A_COUNT];',
 'extern const struct BossBindingSet boss117_bindings['+str(len(c.bindings))+'];','struct Boss117RankData {const uint16_t*values;const uint8_t*choices;};','extern const struct Boss117RankData boss117_rank[B117R_COUNT];',
 *['extern const uint16_t boss117_'+k.lower()+'['+str(n)+'];' for k,(_,n) in TABLES.items()],
 'extern const uint16_t boss117_attack[7],boss117_debris_palettes[8];',
 'extern const int16_t boss117_bounds[4],boss117_bob[3][16],boss117_troop_offsets[7][10][2],boss117_scatter_offsets[7][5],boss117_scatter_motion[7][16][3],boss117_blast_path[7][6][3];',
 'extern const int16_t boss117_projectile_offsets[6][2],boss117_projectile_motion[6][3],boss117_ground_offsets[6],boss117_ballistic_offsets[5][2],boss117_burst_offsets[6][2],boss117_burst_motion[6][3];',
 'enum Boss117Pattern {B117P_PLATE_FIRE0,B117P_PLATE_FIRE1,B117P_PLATE_FIRE2,B117P_PLATE_FIRE3,B117P_PLATE_FIRE4,B117P_PLATE_FIRE5,B117P_PLATE_DEBRIS0,B117P_PLATE_DEBRIS1,B117P_PLATE_DEBRIS2,B117P_PLATE_DEBRIS3,B117P_PLATE_DEBRIS4,B117P_PLATE_DEBRIS5,B117P_CENTRAL_DEBRIS,B117P_FINAL_DEBRIS,B117P_CENTRAL_FIRE,B117P_FIRE4,B117P_FIRE2,B117P_FINAL_FIRE,B117P_PARACHUTE,B117P_COUNT};',
 'extern const struct BossPatternData boss117_patterns[B117P_COUNT];']
 out=c.c_source('boss117_program','boss117-programs.h');defs=[];attacks=[];sets=[];filters={}
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
 array('struct BossBindingSet','boss117_bindings[]',','.join(sets));array('uint16_t','boss117_entries[B117A_COUNT]',vals(entries.values()))
 for k,v in tables.items():array('uint16_t','boss117_'+k.lower()+'['+str(len(v))+']',vals(v))
 rank_rows=[]
 for i,at in enumerate(RANKS.values()):
  mode=c.word(at)
  if mode==2:
   choices=[c.byte(at+2+j) for j in range(64)]
   array('uint8_t',f'boss117_choices{i}[64]',vals(choices))
   array('uint16_t',f'boss117_values{i}[{(max(choices)+1)*8}]',vals(c.word(at+66+2*j) for j in range((max(choices)+1)*8)))
   rank_rows.append('{boss117_values'+str(i)+',boss117_choices'+str(i)+'}')
  else:
   if mode not in (0,1):raise ValueError('rank mode')
   array('uint16_t',f'boss117_values{i}[64]',vals(c.word(at+2+2*j) for j in range(64)))
   rank_rows.append('{boss117_values'+str(i)+',0}')
 array('struct Boss117RankData','boss117_rank[B117R_COUNT]',','.join(rank_rows))
 array('uint16_t','boss117_attack[7]',vals(attack_table))
 array('uint16_t','boss117_debris_palettes[8]',vals(c.word(0x2e596a+2*i) for i in range(8)))
 array('int16_t','boss117_bounds[4]',vals(c.signed(0x2e5416+2*i) for i in range(4)))
 array('int16_t','boss117_ground_offsets[6]',vals(c.signed(0x2e588e+2*i) for i in range(6)))
 for name,at,n,m in [('bob',0x2e58ea,3,16),('projectile_offsets',0x2e5852,6,2),('projectile_motion',0x2e586a,6,3),('ballistic_offsets',0x2e589a,5,2),('burst_offsets',0x2e58ae,6,2),('burst_motion',0x2e58c6,6,3)]:
  array('int16_t',f'boss117_{name}[{n}][{m}]',','.join('{'+vals(c.signed(at+2*(i*m+j)) for j in range(m))+'}' for i in range(n)))
 for name,at,n,m in [('troop_offsets',0x2e54a2,10,2),('scatter_motion',0x2e560e,16,3),('blast_path',0x2e571a,6,3)]:
  array('int16_t',f'boss117_{name}[7][{n}][{m}]',','.join('{'+','.join('{'+vals(c.signed(c.long(at+4*i)+2*(j*m+k)) for k in range(m))+'}' for j in range(n))+'}' for i in range(7)))
 # $083164 writes the three indices into the parent, leaving child $5E=-1.
 # Retain the preceding word as well as the four authored offsets.
 array('int16_t','boss117_scatter_offsets[7][5]',','.join('{'+vals(c.signed(0x2e55d6+i*8+j*2) for j in range(-1,4))+'}' for i in range(7)))
 pattern_sources=[c.long(0x2e5a22+4*i) for i in range(6)]+[c.long(0x2e597a+4*i) for i in range(6)]+[0x2e59fe,0x2e5a10,0x2e5aa6,0x2e5ab8,0x2e5aca,0x2e5adc,0x2e5aee]
 kinds={0x77f6a:0,0x82c08:1,0x82b46:2};rows=[]
 for at in pattern_sources:
  if c.long(at+14) not in kinds or c.byte(at+13):raise ValueError('unreviewed boss117 pattern')
  rows.append('{'+vals([c.signed(at+4),c.signed(at+8),c.word(at+6),c.word(at+10),c.word(at+2),c.byte(at),c.byte(at+1),c.byte(at+13),kinds[c.long(at+14)]])+'}')
 array('struct BossPatternData','boss117_patterns[B117P_COUNT]',','.join(rows))
 h.append('#endif')
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,tables=TABLES,rank_sources=RANKS,templates=templates,selector_bounds=SELECTORS,pattern_sources=pattern_sources,
  source_files={p:hashlib.sha256((a.source/p).read_bytes()).hexdigest() for p in ['asm/enemies/squad/para_squad_module_081816.s','asm/enemies/squad/para_squad_helpers_08283c.s','asm/enemies/bosses/miniboss_module_083262.s'] if (a.source/p).is_file()},
  source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in ((0x81908,0x832d0),(0x6dce0,0x6de52),(0x56204,0x56256),(0x31d26,0x31d7c))],
  implementation=['src/boss117.c','src/boss117_parts.c','src/boss117_effects.c'],validation='Source conversion only; not gameplay validation',complete_port=False)
 for name,data in [('boss117-programs.h','\n'.join(h)),('boss117-programs.c','\n'.join(out)),('boss117-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(data+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),fields=len(c.fields),ranks=len(RANKS))))
if __name__=='__main__':main()
