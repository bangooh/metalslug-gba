#!/usr/bin/env python3
"""Gun platform 133..136, both riders, local projectiles and collision stamps."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(GUN=0x290876,GUN_DAMAGED=0x290882,WRECK=0x290864,RIDER_A_IDLE=0x28e6b0,RIDER_A_ALERT=0x28e6bc,RIDER_A_FIRE=0x28e6c8,RIDER_B_IDLE=0x28e312,RIDER_B_ALERT=0x28e8f4,RIDER_B_FIRE=0x28ec54,RIDER_B_RETURN=0x28e934,SHOT=0x2908c8,SHOT_SLOW=0x29088e,SMOKE=0x2c4022)
TABLES=dict(WRECK=(0x290374,4),HATCH_HOLD=(0x290334,2),HATCH_FIRE=(0x29033c,2),HATCH_OPEN=(0x290344,2),HATCH_CLOSE=(0x29034c,2),BODY0=(0x290314,4),BODY1=(0x290324,4),SHIELD0=(0x290354,4),SHIELD1=(0x290364,4))
RANKS=dict(HP=0x2bd0da,SHIELD_HP=0x2bd15c,SHOT_SPEED=0x2bd1de,IDLE=0x2bd260,INTERVAL=0x2bd2e2,REPEATS=0x2bd364)
FIXED=dict(RIDER_A=('DEFENSE',0x290c20),RIDER_B=('DEFENSE',0x290c74),GUN=('ATTACK',0x290d70))
TEMPLATES={133:0x4bb9a,134:0x4bba4,135:0x4bbae,136:0x4bbb8}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',required=True,type=Path);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw)
 if any(c.long(0xe8000+4*i)!=at for i,at in TEMPLATES.items()):raise ValueError('template dispatch')
 if [c.long(at+4*i) for at in (0x290304,0x29030c) for i in range(2)]!=[0x290314,0x290324,0x290354,0x290364]:raise ValueError('double selector')
 entries={k:c.entry(v) for k,v in ROOTS.items()};tables={k:[65535 if c.long(at+4*i)==0xffffffff else c.entry(c.long(at+4*i)) for i in range(n)] for k,(at,n) in TABLES.items()};fixed={k:c.binding(*v) for k,v in FIXED.items()};c.compile()
 if c.fields or c.parameters or {x['source'] for x in c.callbacks}!={0x49472,0x494c8,0x283ca,0x4ca58,0x4ca7c}:raise ValueError('unreviewed graph')
 h=['#ifndef NATIVE_GUN_PLATFORM_PROGRAMS_H','#define NATIVE_GUN_PLATFORM_PROGRAMS_H','#include "../src/bosses.h"',
 'enum GunPlatformAnimation {'+','.join('GPA_'+k for k in ROOTS)+',GPA_COUNT};','enum GunPlatformCallback {'+','.join('GPC_'+format(x['source'],'X') for x in c.callbacks)+'};','enum GunPlatformBinding {'+','.join('GPB_'+k+'='+str(v) for k,v in fixed.items())+'};','enum GunPlatformRank {'+','.join('GPR_'+k for k in RANKS)+',GPR_COUNT};',
 'extern const struct AnimationProgram gun_platform_program;','extern const struct BossBindingSet gun_platform_bindings['+str(len(c.bindings))+'];','extern const uint16_t gun_platform_entries[GPA_COUNT],gun_platform_rank[GPR_COUNT][64];',
 *['extern const uint16_t gun_platform_'+k.lower()+'['+str(n)+'];' for k,(_,n) in TABLES.items()],
 'extern const uint8_t gun_platform_parts[3][4];','extern const int16_t gun_platform_offsets[4][2],gun_platform_hatch_y[4],gun_platform_bounds[4];',
 'struct GunPlatformStamp {const uint8_t*cells;int16_t dx,dy;uint16_t width,height;};','extern const struct GunPlatformStamp gun_platform_stamps[5];']
 out=c.c_source('gun_platform_program','gun-platform-programs.h');defs=[];attacks=[];sets=[];filters={}
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


 array('struct BossBindingSet','gun_platform_bindings[]',','.join(sets));array('uint16_t','gun_platform_entries[GPA_COUNT]',vals(entries.values()))
 for k,v in tables.items():array('uint16_t','gun_platform_'+k.lower()+'[]',vals(v))
 if any(c.word(at) not in (0,1) for at in RANKS.values()):raise ValueError('rank mode')
 array('uint16_t','gun_platform_rank[GPR_COUNT][64]',','.join('{'+vals(c.word(at+2+2*i) for i in range(64))+'}' for at in RANKS.values()))
 array('uint8_t','gun_platform_parts[3][4]',','.join('{'+vals(c.byte(at+i) for i in range(4))+'}' for at in (0x2902e8,0x2902ec,0x2902f0)))
 array('int16_t','gun_platform_offsets[4][2]',','.join('{'+vals(c.signed(0x290efe+4*i+2*j) for j in range(2))+'}' for i in range(4)))
 array('int16_t','gun_platform_hatch_y[4]',vals(c.signed(0x2902f6+4*i) for i in range(4)))
 array('int16_t','gun_platform_bounds[4]',vals(c.signed(0x290912+2*i) for i in range(4)))
 rows=[];stamps=[c.long(0x2902d8+4*i) for i in range(4)]+[0x290e6c]
 for i,at in enumerate(stamps):
  dims=[c.signed(at+2*j) for j in range(4)];data=[c.byte(at+8+j) for j in range(dims[2]*dims[3])];array('uint8_t',f'gun_platform_stamp{i}[]',vals(data));rows.append('{gun_platform_stamp'+str(i)+','+vals(dims)+'}')
 array('struct GunPlatformStamp','gun_platform_stamps[5]',','.join(rows));h.append('#endif')
 ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,tables=TABLES,rank_sources=RANKS,templates=TEMPLATES,stamp_sources=stamps,implementation=['src/gun_platform.c'],validation='Source conversion only; no gameplay tests',complete_port=False,source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in [(0x4bb9a,0x4cac4),(0x620da,0x6212a),(0x49472,0x4951e)]])
 for name,value in [('gun-platform-programs.h','\n'.join(h)),('gun-platform-programs.c','\n'.join(out)),('gun-platform-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),ranks=len(RANKS))))
if __name__=='__main__':main()
