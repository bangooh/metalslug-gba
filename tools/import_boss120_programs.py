#!/usr/bin/env python3
"""120/121 multi-part flight controller: original source data conversion only."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS={'BODY': 2647016, 'BURST': 2647864, 'BURST_END': 2649636, 'AIM': 2649972, 'AIM_SCRIPTED': 2649978, 'HURT': 2650786, 'WRECK_FALL': 2650852, 'WRECK_BOUNCE': 2650924, 'WRECK_GROUND': 2650974, 'WRECK_HIT': 2650994, 'MUZZLE': 2654720, 'GROUND': 2651660, 'TRAIL': 2651780, 'BOMB': 2651822, 'MISSILE_MUZZLE': 2651912, 'MISSILE': 2652074, 'MISSILE_FLY': 2652956, 'FRAGMENT': 2655008, 'BLOOD_GROUND': 309970, 'BLOOD_HIT': 309744, 'FLASH': 3004114}
TABLES={'PART': [2646876, 8], 'COMMAND': [2646908, 16], 'FIRE': [2646412, 6], 'SCRIPTED_FIRE': [2646436, 6], 'SHELL': [2646460, 6]}
SELECTORS={2649980: 11, 2652076: 16, 2647872: 12, 2652976: 32, 2652368: 16, 2648092: 12, 2653556: 32, 2652660: 16, 2648312: 12, 2654136: 32, 2648532: 12, 2648752: 12, 2648972: 12, 2649192: 12, 2649412: 12}
RANKS={'HP': 2865078, 'THRESHOLD': 2865208, 'WAIT': 2865338, 'WAIT_LATE': 2865468, 'DASH_SPEED': 2865598, 'DASH_SPEED_LATE': 2865728, 'DASH_WAIT': 2865858, 'DASH_WAIT_LATE': 2865988, 'SHELL_SPEED': 2866118, 'SHELL_SPEED_LATE': 2866248, 'DASH_COUNT': 2866378, 'DASH_COUNT_LATE': 2866508, 'DASH_RELOAD': 2866638, 'DASH_RELOAD_LATE': 2866768, 'BOMB_CONFIG': 2866898, 'BOMB_CONFIG_LATE': 2867028, 'BOMB_HP': 2867190, 'MISSILE_HP': 2867320, 'MISSILE_COUNT': 2867450, 'MISSILE_COUNT_LATE': 2867580, 'MISSILE_CONFIG': 2867710, 'MISSILE_CONFIG_LATE': 2867840, 'MISSILE_RELOAD': 2868002, 'MISSILE_RELOAD_LATE': 2868132, 'BURST_COUNT': 2868262, 'BURST_COUNT_LATE': 2868392, 'BURST_RELOAD': 2868522, 'BURST_RELOAD_LATE': 2868652, 'CHOICE_HIGH': 2868782, 'CHOICE_HIGH_LATE': 2868912, 'CHOICE_MID': 2869042, 'CHOICE_MID_LATE': 2869172, 'CHOICE_BACK': 2869302, 'CHOICE_BACK_LATE': 2869432, 'CHOICE_FRONT': 2869562, 'CHOICE_FRONT_LATE': 2869692, 'REENTRY': 2869822, 'REENTRY_LATE': 2869952, 'SHELL_ALT_SPEED': 2859090}
FIXED=dict(BODY=('DEFENSE',0x285d16),WRECK=('DEFENSE',0x285dbe),SHELL=('DEFENSE',0x285e12),SHELL_ATTACK=('ATTACK',0x285fb6),SHELL_IMPACT=('ATTACK',0x28605e))
TEMPLATES={120:0x403e4,121:0x40d6c}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',required=True,type=Path);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw,SELECTORS)
 if any(c.long(0xe8000+4*i)!=at for i,at in TEMPLATES.items()):raise ValueError('template dispatch')
 if [c.long(0x2863bc+4*i) for i in range(11)]!=[0x40f26,0x40fee,0x410ba,0x410c4,0x410ee,0x41196,0x411f8,0x4127e,0x41328,0x41390,0x40ef2]:raise ValueError('part dispatch')
 if [c.long(0x28633c+4*i) for i in range(6)]!=[0x405b2,0x40640,0x4080a,0x409a8,0x40a38,0x40b12]:raise ValueError('body dispatch')
 if any((c.byte(0x28631c+i)&3)>=3 for i in range(16)):raise ValueError('missile position bound')
 entries={k:c.entry(v) for k,v in ROOTS.items()};tables={k:[65535 if c.long(at+4*i)==0xffffffff else c.entry(c.long(at+4*i)) for i in range(n)] for k,(at,n) in TABLES.items()};fixed={k:c.binding(*v) for k,v in FIXED.items()};c.compile()
 if {x['source'] for x in c.callbacks}!={0x422ea,0x42206,0x420f6,0x420e8,0x283ca,0x42040,0x4233e,0x42366,0x42324} or c.parameters:raise ValueError('unreviewed graph')
 if {(x['source_offset'],x['width']) for x in c.fields}!={(0x9a,2),(0x76,2),(0x80,1),(0x34,2)}:raise ValueError('unreviewed fields')
 h=['#ifndef NATIVE_BOSS120_PROGRAMS_H','#define NATIVE_BOSS120_PROGRAMS_H','#include "../src/bosses.h"',
 'enum Boss120Animation {'+','.join('B120A_'+k for k in ROOTS)+',B120A_COUNT};',
 'enum Boss120Callback {'+','.join('B120C_'+format(x['source'],'X') for x in c.callbacks)+'};',
 'enum Boss120Field {'+','.join('B120F_'+format(x['source_offset'],'X')+'_'+str(x['width']) for x in c.fields)+'};',
 'enum Boss120Binding {'+','.join('B120B_'+k+'='+str(v) for k,v in fixed.items())+'};',
 'enum Boss120Rank {'+','.join('B120R_'+k for k in RANKS)+',B120R_COUNT};',
 'extern const struct AnimationProgram boss120_program;','extern const struct BossBindingSet boss120_bindings['+str(len(c.bindings))+'];','extern const uint16_t boss120_entries[B120A_COUNT];',
 'struct Boss120RankData {const uint16_t*values;const uint8_t*choices;};','extern const struct Boss120RankData boss120_rank[B120R_COUNT];',
 *['extern const uint16_t boss120_'+k.lower()+'['+str(n)+'];' for k,(_,n) in TABLES.items()],
 'extern const uint8_t boss120_part_offsets[8][4],boss120_missile_choices[16];',
 'extern const uint16_t boss120_palettes[8];',
 'extern const int16_t boss120_bounds[3][4],boss120_targets[12][2],boss120_speeds[4],boss120_muzzles[6][2],boss120_angles[6],boss120_bomb_configs[8][2],boss120_missile_configs[8][2],boss120_missile_offsets[3][2],boss120_trail[4][16][3];',
 'extern const struct FlightProgram boss120_flight_program;','extern const uint16_t boss120_paths[5];']
 out=c.c_source('boss120_program','boss120-programs.h');defs=[];attacks=[];sets=[];filters={}
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
 if attacks:out.append('static const struct HitAttack attacks[]={'+','.join(attacks)+'};')


 array('struct BossBindingSet','boss120_bindings[]',','.join(sets));array('uint16_t','boss120_entries[B120A_COUNT]',vals(entries.values()))
 for k,v in tables.items():array('uint16_t','boss120_'+k.lower()+'[]',vals(v))
 rank_rows=[]
 for i,at in enumerate(RANKS.values()):
  mode=c.word(at)
  if mode==2:
   choices=[c.byte(at+2+j) for j in range(64)]
   array('uint8_t',f'boss120_choices{i}[64]',vals(choices))
   array('uint16_t',f'boss120_values{i}[{(max(choices)+1)*8}]',vals(c.word(at+66+2*j) for j in range((max(choices)+1)*8)))
   rank_rows.append('{boss120_values'+str(i)+',boss120_choices'+str(i)+'}')
  else:
   if mode not in (0,1):raise ValueError('rank mode')
   array('uint16_t',f'boss120_values{i}[64]',vals(c.word(at+2+2*j) for j in range(64)))
   rank_rows.append('{boss120_values'+str(i)+',0}')
 array('struct Boss120RankData','boss120_rank[B120R_COUNT]',','.join(rank_rows))
 array('uint8_t','boss120_part_offsets[8][4]',','.join('{'+vals(c.byte(0x28615c+4*i+j) for j in range(4))+'}' for i in range(8)))
 array('uint8_t','boss120_missile_choices[16]',vals(c.byte(0x28631c+i) for i in range(16)))
 array('uint16_t','boss120_palettes[8]',vals(c.word(0x28617c+2*i) for i in range(8)))
 array('int16_t','boss120_bounds[3][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x286102,0x28610a,0x2df1fc)))
 for name,at,n,m in [('targets',0x286124,12,2),('muzzles',0x2861d4,6,2),('bomb_configs',0x2bbfd6,8,2),('missile_configs',0x2bc302,8,2),('missile_offsets',0x286310,3,2)]:
  array('int16_t',f'boss120_{name}[{n}][{m}]',','.join('{'+vals(c.signed(at+2*(i*m+j)) for j in range(m))+'}' for i in range(n)))
 for name,at,n in [('speeds',0x286154,4),('angles',0x2861ec,6)]:array('int16_t',f'boss120_{name}[{n}]',vals(c.signed(at+2*i) for i in range(n)))
 array('int16_t','boss120_trail[4][16][3]',','.join('{'+','.join('{'+vals(c.signed(c.long(0x2861f8+4*i)+6*j+2*k) for k in range(3))+'}' for j in range(16))+'}' for i in range(4)))
 pending=[];seen={};rows=[];calls=[];fields=[]
 def entry(at):
  if at not in seen:seen[at]=len(pending);pending.append(at)
  return seen[at]
 paths=[entry(0x288382)]+[entry(c.long(0x28632c+4*i)) for i in range(4)]
 for at in pending:
  op=c.byte(at);value=target=length=width=increment=angle=0;nxt=65535
  if op==0:kind='ANGLE';angle=c.byte(at+1);nxt=entry(at+2)
  elif op==1:kind='DISTANCE';length=c.word(at+2);nxt=entry(at+4)
  elif op==2:kind='ARC';length=c.word(at+4);increment=struct.unpack('b',bytes([c.byte(at+1)]))[0];angle=c.byte(at+2);nxt=entry(at+6)
  elif op==3:kind='WAIT';length=c.word(at+2);nxt=entry(at+4)
  elif op==4:kind='JUMP';target=entry(c.long(at+2))
  elif op in (5,6,7):
   kind='FIELD';width={5:4,6:2,7:1}[op];value=c.byte(at+1) if width==1 else c.word(at+2) if width==2 else c.long(at+4);key=(c.word(at+(4 if width==2 else 2)),width)
   if key==(0,4):raise ValueError('unreviewed path handler')
   if key not in fields:fields.append(key)
   target=fields.index(key);nxt=entry(at+(4 if width==1 else 6 if width==2 else 8))
  elif op==8:
   kind='CALL';callback=c.long(at+2)
   if callback not in calls:calls.append(callback)
   target=calls.index(callback);nxt=entry(at+6)
  elif op==9:kind='SOUND';value=c.word(at+2);nxt=entry(at+4)
  elif op==10:kind='FINISH'
  else:raise ValueError(f'path operation {op} at {at:x}')
  rows.append('{'+vals([value,nxt,target,length])+',FP_'+kind+','+vals([width,increment,angle])+'}')
 if fields or calls:raise ValueError('unreviewed boss120 path')
 print('path',len(rows),'fields',fields,'calls',[hex(x) for x in calls])
 if fields:h.append('enum Boss120PathField {'+','.join('B120PF_'+format(x,'X')+'_'+str(y) for x,y in fields)+'};')
 if calls:h.append('enum Boss120PathCallback {'+','.join('B120PC_'+format(x,'X') for x in calls)+'};')
 out.append('static const struct FlightCommand boss120_flight_commands[]={'+','.join(rows)+'};')
 array('struct FlightProgram','boss120_flight_program','boss120_flight_commands,'+str(len(rows)));array('uint16_t','boss120_paths[5]',vals(paths));h.append('#endif')
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,tables=TABLES,rank_sources=RANKS,templates=TEMPLATES,selector_bounds=SELECTORS,path_commands=[dict(source=at,native_index=i) for at,i in seen.items()],path_fields=fields,path_callbacks=calls,implementation=['src/boss120.c','src/boss120_parts.c','src/boss120_effects.c'],implemented_unreachable_aliases=['0420E4/042188 standalone alternate shell callback'],pending_aliases=['040554 ambient-D5 steering branch: no established normal entry; native rejects X>272 in intro-open'],validation='Source conversion only; no gameplay tests',complete_port=False,source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in [(0x403e4,0x42390),(0x8121c,0x81332),(0x4aad2,0x4aba8)]])
 for name,value in [('boss120-programs.h','\n'.join(h)),('boss120-programs.c','\n'.join(out)),('boss120-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),ranks=len(RANKS))))
if __name__=='__main__':main()
