#!/usr/bin/env python3
"""Roof/boat/signs, heading marker, manual platform and scene bookkeeping."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(STATIC317=0x2f73b8,ROOF=0x294254,ROOF_DIE=0x29426a,SIGN_A=0x2949ae,SIGN_B=0x2949be,BOAT=0x2949ce,BOAT_SPECIAL=0x2949de,HEADING=0x43354,FROG=0x2948f8,FROG_B=0x294936,FROG_JUMP=0x29494e)
FIXED=dict(ROOF_BLAST=('ATTACK',0x295fba),BOAT_A=('DEFENSE',0x295454),BOAT_B=('DEFENSE',0x2954a8),BOAT_BLAST=('ATTACK',0x2961a6))
TEMPLATES={317:0x9bb56,250:0x7926c,252:0x433be,259:0x4f2c2,260:0x4f7ca,261:0x4f70e,262:0x4f76c,272:0x60764,273:0x60a0e,274:0x4a802,319:0x605b6}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw);entries={k:c.entry(v) for k,v in ROOTS.items()};heading=[c.entry(c.long(0x4337e+4*i)) for i in range(16)];fixed={k:c.binding(*v) for k,v in FIXED.items()}
 if any(c.long(0xe8000+4*i)!=at for i,at in TEMPLATES.items()):raise ValueError('template dispatch')
 c.compile()
 if c.callbacks or {(x['source_offset'],x['width']) for x in c.fields}!={(0x3a,1)} or c.parameters:raise ValueError('unreviewed graph')
 parts=[dict(map=0x2507fa,flags=8,dx=0,dy_up=0,priority=65535)];platform=c.intern(c.drawings,c.draw_ids,json.dumps(parts,sort_keys=True),parts)
 h=['#ifndef NATIVE_SCENE_PROP_PROGRAMS_H','#define NATIVE_SCENE_PROP_PROGRAMS_H','#include "../src/bosses.h"','enum ScenePropAnimation {'+','.join('SPA_'+k for k in ROOTS)+',SPA_COUNT};','enum ScenePropField {SPF_FACING};','enum ScenePropBinding {'+','.join('SPB_'+k+'='+str(v) for k,v in fixed.items())+'};',f'enum {{SCENE_PROP_PLATFORM_DRAWING={platform}}};','extern const struct AnimationProgram scene_prop_program;','extern const uint16_t scene_prop_entries[SPA_COUNT],scene_prop_heading[16],scene_prop_boat_rank[64];','extern const struct BossBindingSet scene_prop_bindings['+str(len(c.bindings))+'];','extern const int16_t scene_prop_bounds[4][4],scene_prop_velocity[16][2];','extern const struct InteractionContact scene_prop_boat_contact;','struct ScenePropStamp {const uint8_t*cells;int16_t dx,dy;uint16_t width,height;};','extern const struct ScenePropStamp scene_prop_stamps[3];','#endif']
 out=c.c_source('scene_prop_program','scene-prop-programs.h')
 def array(ty,name,data):out.append(('static ' if name.startswith('stamp_cells') else '')+'const '+ty+' '+name+'={'+data+'};')
 defs=[];attacks=[];sets=[]
 for i,b in enumerate(c.bindings):
  at=b['source'];records=defs if b['kind']=='DEFENSE' else attacks;first=len(records)
  for n in range(32):
   if c.byte(at)==255:break
   rect='{'+vals(c.signed(at+10+2*j) for j in range(4))+'}'
   if b['kind']=='DEFENSE':
    f=c.long(at+2);name='0'
    if f!=0xffffffff:
     name='filter_'+str(i)+'_'+str(n);out.append('static const uint8_t '+name+'[36]={'+vals(c.byte(f+j) for j in range(36))+'};')
    records.append('{'+rect+','+name+','+str(c.byte(at+1))+'}')
   elif b['kind']=='ATTACK':
    group,kind,damage,strength,shape,offscreen=struct.unpack_from('>BBhBBB',raw,c.offset(at))
    if group>4 or kind>34 or shape not in (0,4):raise ValueError('attack format')
    records.append('{'+rect+','+vals([damage,group,kind,strength,shape,offscreen])+'}')
   else:raise ValueError('binding kind')
   at+=80
  else:raise ValueError('hit terminator')
  sets.append('{'+('defs+'+str(first)+',0' if b['kind']=='DEFENSE' else '0,attacks+'+str(first))+','+str(n)+'}')
 out+=['static const struct HitDefense defs[]={'+','.join(defs)+'};','static const struct HitAttack attacks[]={'+','.join(attacks)+'};'];array('struct BossBindingSet','scene_prop_bindings[]',','.join(sets))
 array('uint16_t','scene_prop_entries[SPA_COUNT]',vals(entries.values()));array('uint16_t','scene_prop_heading[16]',vals(heading))
 if c.word(0x2c0092)!=0:raise ValueError('rank mode')
 array('uint16_t','scene_prop_boat_rank[64]',vals(c.word(0x2c0094+2*i) for i in range(64)))
 array('int16_t','scene_prop_bounds[4][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in [0x29624a,0x29625e,0x296268,0x9a5f4]));array('int16_t','scene_prop_velocity[16][2]',','.join('{'+vals(c.signed(0x60576+4*i+2*j) for j in range(2))+'}' for i in range(16)))
 at=0x296356
 if c.byte(at)!=128:raise ValueError('contact format')
 array('struct InteractionContact','scene_prop_boat_contact','{'+vals(c.signed(at+6+2*i) for i in range(4))+'},'+vals([c.signed(at+2),c.byte(at+1)]))
 stamps=[]
 for i,at in enumerate([0x295f6e,0x295f94,0x9bb92]):
  dx,dy,width,height=[c.signed(at+2*j) for j in range(4)]
  if min(width,height)<=0:raise ValueError('stamp dimensions')
  array('uint8_t',f'stamp_cells{i}[{width*height}]',vals(c.byte(at+8+j) for j in range(width*height)));stamps.append('{stamp_cells'+str(i)+','+vals([dx,dy,width,height])+'}')
 array('struct ScenePropStamp','scene_prop_stamps[3]',','.join(stamps))
 ledger=c.ledger();ledger.update(prom_sha256=digest,templates=TEMPLATES,roots=ROOTS,implementation=['src/scene_props.c'],unreferenced_aliases=[0x4f5be],shared_dependencies=['Source60764/60A0E obstacle variants','Source4A802 launched human','Source77C7E shared FX patterns'],pending=['Source106F29 sound reply input, full renderer/camera/map and record106E92 consumer'],complete_port=False,validation='Source extraction and compilation only',source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[c.offset(lo):c.offset(hi-1)+1]).hexdigest()) for lo,hi in [(0x9bb56,0x9bdaa),(0x9a76a,0x9a7aa),(0x4f2c2,0x4f3ae),(0x4f5be,0x4fa40),(0x43354,0x4345e),(0x60576,0x6063a),(0x7926c,0x79298)]])
 for name,value in [('scene-prop-programs.h','\n'.join(h)),('scene-prop-programs.c','\n'.join(out)),('scene-prop-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),bindings=len(c.bindings),templates=len(TEMPLATES))))
if __name__=='__main__':main()
