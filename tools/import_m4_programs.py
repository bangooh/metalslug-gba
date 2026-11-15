#!/usr/bin/env python3
"""M4 carrier and 104/105 attached free actors. Offline data only."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(BODY=0x2e78ce,BODY_FIRST_HIT=0x2e78b4,BODY_HIT=0x2e78de,TURRET=0x2e7908,TURRET_DEAD=0x2e7940,MOUNT=0x2e7a10,SPLASH=0x2e7950,WATER=0x2e79fa,FLOOR=0x2e7bfe,RECOIL=0x2e791e,
 FREE_FALL=0x28e75e,FREE_LAND=0x28e79c,FREE_WALK=0x28e394,FREE_JUMP=0x28e656,FREE_WAIT=0x28e6b0,FREE_THROW=0x28e6c8,FREE_LEAVE=0x28e6ee,FREE_STOP=0x28e4ee,FREE_STAND=0x28e536,FREE_FIRE=0x28e5b6,FREE_REPEAT=0x28e610,FREE_IDLE=0x28e632,FREE_EXIT=0x28e578,FREE_HAT=0x29ca36,FREE_BULLET=0x28f51c,FREE_FLASH=0x28f556)
TABLES=dict(WHEEL=(0x2e7bde,4),WHEEL_BACK=(0x2e7bee,4),DEBRIS_IDLE=(0x2e7b2e,4),DEBRIS_HIT=(0x2e7b3e,4))
RANKS=dict(HP=0x2c029a,RECOIL=0x2c031c,RISE=0x2c039e,DROP=0x2c0420,SPEED=0x2bfb2e,WAIT=0x2bfc32,REPEAT_WAIT=0x2bfcb4,REPEATS=0x2bfd36,BULLET=0x2bfdb8)
FIXED=dict(BODY=('DEFENSE',0x2e93fa),TURRET=('DEFENSE',0x2e94a2),BLAST=('ATTACK',0x2e9790),PUFF=('ATTACK',0x2e96ec),FLOOR=('ATTACK',0x2e97e4),FREE=('DEFENSE',0x28df42),FREE_WAIT=('DEFENSE',0x28e03e),FREE_THROW=('DEFENSE',0x28e092),FREE_STAND=('DEFENSE',0x28dfea))
ROOTS.update({'FS_INTRO': 2684096, 'FS_IDLE': 2679570, 'FS_WALK': 2679700, 'FS_RUN': 2679886, 'FS_BACK': 2679966, 'FS_WAIT': 2680764, 'FS_STOP': 2679830, 'FS_TURN': 2680920, 'FS_FALL': 2680670, 'FS_LAND': 2680732, 'FS_AIM0': 2681076, 'FS_AIM1': 2681108, 'FS_ROTATE0': 2681204, 'FS_ROTATE1': 2681520, 'FS_RECOVER0': 2681140, 'FS_RECOVER1': 2681172, 'FS_LOWER': 2680406, 'FS_THROW': 2680520, 'FS_RAISE': 2680558, 'FS_AIR_IDLE': 2682122, 'FS_AIR_LAND': 2682636, 'FS_PARACHUTE': 2682708, 'FS_CHUTE_LAND': 2683852, 'FS_CHUTE_HIT': 2684024, 'FS_RUNNER': 2686940, 'FS_CART': 2687120})
TABLES.update(FS_FIRE0=(0x28e1ca,16),FS_FIRE1=(0x28e20a,16),FS_AIR_FIRE=(0x28e24a,9),FS_CHUTE_FIRE=(0x28e26e,9))
RANKS.update(FS_RUN=0x2bfbb0,FS_AIR_WAIT=0x2bff0c,FS_AIR_SPEED=0x2bff8e,FS_CHUTE_HP=0x2bfe8a,FS_RUNNER=0x2bdc88)
FIXED.update(FS_AIR=("DEFENSE",0x28df96),FS_CHUTE=("DEFENSE",0x28e0e6),FS_CART=("DEFENSE",0x29024e),FS_CART_ATTACK=("ATTACK",0x290106))
TEMPLATES={104:0x483e2,105:0x483d2,263:0x859d4,264:0x857f2,265:0x858d0,266:0x8577a,268:0x85190}
TEMPLATES.update({96:0x478fc,97:0x4790c,98:0x4791c,99:0x4792c,100:0x4793c,101:0x4794c,102:0x4795c,103:0x4796c,106:0x48898,107:0x488a0,110:0x49baa})
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',required=True,type=Path);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw,{2681206: 17, 2681522: 13, 2682124: 17, 2682710: 17})
 if any(c.long(0xe8000+4*i)!=at for i,at in TEMPLATES.items()):raise ValueError('template dispatch')
 entries={k:c.entry(v) for k,v in ROOTS.items()};tables={k:[65535 if c.long(at+4*i)==0xffffffff else c.entry(c.long(at+4*i)) for i in range(n)] for k,(at,n) in TABLES.items()};fixed={k:c.binding(*v) for k,v in FIXED.items()};c.compile()
 if {x['source'] for x in c.callbacks}!={0x85bd8,0x85b7e,0x49472,0x4951e,0x493fa,0x283ca} or c.parameters or {(x['source_offset'],x['width']) for x in c.fields}!={(0x3b,1),(0x5c,4),(0x78,2),(0x3a,1)}:raise ValueError('unreviewed graph')
 if any(n['op'] in ('SET','AND','OR','XOR') and c.fields[n['target']]['source_offset']==0x5c for n in c.nodes):raise ValueError('unexpected delay pointer write')
 free_delays=[[c.byte(c.long(0x28e354+4*i)+j) for j in range(12)] for i in range(4)]
 h=['#ifndef NATIVE_M4_PROGRAMS_H','#define NATIVE_M4_PROGRAMS_H','#include "../src/bosses.h"','#include "../src/scene_video.h"',
 'enum M4Animation {'+','.join('M4A_'+k for k in ROOTS)+',M4A_COUNT};','enum M4Callback {'+','.join('M4C_'+format(x['source'],'X') for x in c.callbacks)+'};','enum M4Field {'+','.join('M4F_'+format(x['source_offset'],'X') for x in c.fields)+'};','enum M4Binding {'+','.join('M4B_'+k+'='+str(v) for k,v in fixed.items())+'};','enum M4Rank {'+','.join('M4R_'+k for k in RANKS)+',M4R_COUNT};','extern const struct AnimationProgram m4_program;','extern const struct BossBindingSet m4_bindings['+str(len(c.bindings))+'];','extern const uint16_t m4_entries[M4A_COUNT],m4_rank[M4R_COUNT][64];',
 *['extern const uint16_t m4_'+k.lower()+'['+str(n)+'];' for k,(_,n) in TABLES.items()]]
 out=c.c_source('m4_program','m4-programs.h');defs=[];attacks=[];sets=[];filters={}
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
 out+=['static const struct HitDefense defs[]={'+','.join(defs)+'};','static const struct HitAttack attacks[]={'+','.join(attacks)+'};']

 array('struct BossBindingSet','m4_bindings[]',','.join(sets));array('uint16_t','m4_entries[M4A_COUNT]',vals(entries.values()))
 for k,v in tables.items():array('uint16_t','m4_'+k.lower()+'[]',vals(v))
 if any(c.word(at) not in (0,1) for at in RANKS.values()):raise ValueError('rank mode')
 array('uint16_t','m4_rank[M4R_COUNT][64]',','.join('{'+vals(c.word(at+2+2*i) for i in range(64))+'}' for at in RANKS.values()))
 array('uint8_t','m4_free_delays[4][12]',','.join('{'+vals(v)+'}' for v in free_delays));h.append('extern const uint8_t m4_free_delays[4][12];')
 h+=['extern const struct BossPatternData m4_patterns[7];','extern const struct InteractionContact m4_contact;','extern const int16_t m4_bounds[6][4],m4_boxes[3][4],m4_free_offsets[16][2],m4_floor[11];']
 pats=[]
 for i in range(7):
  at=0x2e994e+18*i;p=c.long(at+14)
  if c.byte(at+13) or p not in (0x77fd6,0x77f6a,0x8573a,0x77e10):raise ValueError('pattern factory')
  pats.append('{'+vals([c.signed(at+4),c.signed(at+8),c.word(at+6),c.word(at+10),c.word(at+2),c.byte(at),c.byte(at+1),0,[0x77fd6,0x77f6a,0x8573a,0x77e10].index(p)])+'}')
 array('struct BossPatternData','m4_patterns[7]',','.join(pats))
 array('struct InteractionContact','m4_contact','{'+vals(c.signed(0x2eb0b8+6+2*i) for i in range(4))+'},'+vals([c.signed(0x2eb0ba),c.byte(0x2eb0b9)]))
 array('int16_t','m4_bounds[6][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x2e986a,0x2e9874,0x28e196,0x28e19e,0x2902c4,0x2902ce)))
 array('int16_t','m4_boxes[3][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x2eb104,0x2eb10e,0x2eb118)))
 array('int16_t','m4_free_offsets[16][2]',','.join('{'+vals(c.signed(0x28e292+4*i+2*j) for j in range(2))+'}' for i in range(16)))
 h+=['extern const int16_t m4_free_air_offsets[16][2],m4_free_boxes[7][4],m4_free_distances[4][4];']
 array('int16_t','m4_free_air_offsets[16][2]',','.join('{'+vals(c.signed(0x28e2d2+4*i+2*j) for j in range(2))+'}' for i in range(16)))
 array('int16_t','m4_free_boxes[7][4]',','.join('{'+vals(c.signed(at+2*j) for j in range(4))+'}' for at in [0x28e1a6,0x28e1ae,0x28e1b6]+[c.long(0x2bfe3a+4*i) for i in range(4)]))
 array('int16_t','m4_free_distances[4][4]',','.join('{'+vals(c.signed(0x2bfe6a+8*i+2*j) for j in range(4))+'}' for i in range(4)))
 if [c.long(0x28e1be+4*i) for i in range(3)]!=[0xffffffff,0x47bac,0x47c50]:raise ValueError('idle state table')
 array('int16_t','m4_floor[11]',vals(c.signed(0xec882+2*i) for i in range(11)))
 if c.word(0xec896)!=65535:raise ValueError('height terminator')
 patches=[0x2ea858,0x2ea86c]+[c.long(0x2eaa10+4*i) for i in range(20)];rows=[]
 for i,at in enumerate(patches):
  target=c.long(at);im=c.long(at+4);x,y,sx,sy,width,height=[c.signed(at+j) for j in range(8,20,2)];iw,ih=c.word(im),c.word(im+2);stream=c.long(im+4)
  if target not in (0x106f6c,0x107fe8) or min(sx,sy)<0 or min(width,height)<=0 or sx+width>iw or sy+height>ih:raise ValueError('patch bounds')
  data=[c.word(stream+4*((sx+col)*ih+sy+row)+2*k) for col in range(width) for row in range(height) for k in range(2)]
  array('uint16_t',f'm4_patch_cells{i}[{len(data)}]',vals(data));rows.append('{m4_patch_cells'+str(i)+','+vals([x,y,width,height,0 if target==0x106f6c else 1])+'}')
 array('struct SceneTilePatch','m4_patches[22]',','.join(rows));h+=['extern const struct SceneTilePatch m4_patches[22];','extern const uint8_t m4_stamp_cells['+str(c.word(0x2eaf06)*c.word(0x2eaf08))+'];','extern const int16_t m4_stamp[4];']
 dims=[c.signed(0x2eaf02+2*i) for i in range(4)];array('int16_t','m4_stamp[4]',vals(dims));array('uint8_t','m4_stamp_cells[]',vals(c.byte(0x2eaf0a+i) for i in range(dims[2]*dims[3])));h.append('#endif')
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,tables=TABLES,rank_sources=RANKS,delay_sources=[c.long(0x28e354+4*i) for i in range(4)],patch_sources=patches,templates=TEMPLATES,implementation=['src/m4.c','src/m4_scene.c','src/free104.c','src/free96.c'],validation='Source conversion only; no gameplay tests',complete_port=False,source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in [(0x85190,0x85ee8),(0x483d2,0x48898),(0x489f8,0x48d0e),(0x48ea6,0x48f54),(0x48fb0,0x48fcc),(0x492a4,0x492ce),(0x4932c,0x4954a),(0x478fc,0x483d2),(0x48898,0x489f8),(0x48a90,0x48ba0),(0x48d0e,0x48ea6),(0x48f54,0x48fb0),(0x48fcc,0x4932c),(0x49baa,0x49d6e)]])
 for name,value in [('m4-programs.h','\n'.join(h)),('m4-programs.c','\n'.join(out)),('m4-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),patches=len(patches))))
if __name__=='__main__':main()
