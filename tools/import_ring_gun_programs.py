#!/usr/bin/env python3
"""Standalone attachment guns171/193: reviewed animation, overlay and ordnance data."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(IDLE=0x2b732c,FIRE=0x2b7340,WAIT=0x2b737c,BULLET=0x2b7390)
TABLES=dict(IDLE=(0x5a13e,5),ACTIVE=(0x5a152,5),FIRE=(0x5a166,5),SHELL=(0x5a17a,5))
FIXED=dict(BULLET=('ATTACK',0x2b73bc),ENEMY_SHELL=('ATTACK',0x5a40e),PLAYER_SHELL=('ATTACK',0x5a462),SHELL_DEFENSE=('DEFENSE',0x5a506))
TEMPLATES={171:0x59c2e,193:0x5a25a}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw);entries={k:c.entry(v) for k,v in ROOTS.items()};tables={k:[c.entry(c.long(at+4*i)) for i in range(n)] for k,(at,n) in TABLES.items()};fixed={k:c.binding(*v) for k,v in FIXED.items()}
 if any(c.long(0xe8000+4*i)!=at for i,at in TEMPLATES.items()):raise ValueError('template dispatch')
 c.compile()
 if {x['source'] for x in c.callbacks}!={0x59d84} or {(x['source_offset'],x['width']) for x in c.fields}!={(0x72,4),(0x5c,4)} or c.parameters:raise ValueError('unreviewed graph')
 overlay_maps={}
 for n in c.nodes:
  if n['op']=='SET':
   if n['width']!=4:raise ValueError('overlay field width')
   source=n['value'];parts=[dict(map=source,flags=8,dx=0,dy_up=0,priority=65535)]
   n['value']=c.intern(c.drawings,c.draw_ids,json.dumps(parts,sort_keys=True),parts);overlay_maps[source]=n['value']
 h=['#ifndef NATIVE_RING_GUN_PROGRAMS_H','#define NATIVE_RING_GUN_PROGRAMS_H','#include "../src/bosses.h"','enum RingGunAnimation {'+','.join('RGA_'+k for k in ROOTS)+',RGA_COUNT};','enum RingGunField {RGF_OVERLAY171,RGF_OVERLAY193};','enum RingGunBinding {'+','.join('RGB_'+k+'='+str(v) for k,v in fixed.items())+'};','extern const struct AnimationProgram ring_gun_program;','extern const uint16_t ring_gun_entries[RGA_COUNT];','extern const struct BossBindingSet ring_gun_bindings['+str(len(c.bindings))+'];','extern const int16_t ring_gun_bounds[2][4],ring_gun_anchor[5],ring_gun_muzzle[5][2];','extern const uint16_t ring_gun_input_angles[16];',f'enum {{RING_GUN193_PRIORITY={c.word(0x4000)|4}}};',*['extern const uint16_t ring_gun_'+k.lower()+'[5];' for k in TABLES],'#endif']
 out=c.c_source('ring_gun_program','ring-gun-programs.h')
 def array(ty,name,data):out.append('const '+ty+' '+name+'={'+data+'};')
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
 out+=['static const struct HitDefense defs[]={'+','.join(defs)+'};','static const struct HitAttack attacks[]={'+','.join(attacks)+'};'];array('struct BossBindingSet','ring_gun_bindings[]',','.join(sets))

 array('uint16_t','ring_gun_entries[RGA_COUNT]',vals(entries.values()))
 for k,rows in tables.items():array('uint16_t','ring_gun_'+k.lower()+'[5]',vals(rows))
 array('int16_t','ring_gun_bounds[2][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in (0x2b73b4,0x5a236)))
 array('int16_t','ring_gun_anchor[5]',vals(c.signed(0x5a18e+4*i+2) for i in range(5)))
 array('int16_t','ring_gun_muzzle[5][2]',','.join('{'+vals([c.signed(0x5a1a2+4*i+2),c.signed(0x5a1a2+4*i)])+'}' for i in range(5)))
 array('uint16_t','ring_gun_input_angles[16]',vals(c.word(0x5d326+2*i) for i in range(16)))
 ledger=c.ledger();ledger.update(prom_sha256=digest,templates=TEMPLATES,roots=ROOTS,tables=TABLES,fixed=FIXED,overlay_maps=overlay_maps,priority_rom_word=0x4000,implementation=['src/ring_guns.c'],complete_port=False,validation='Source extraction and compilation only',source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[c.offset(lo):c.offset(hi-1)+1]).hexdigest()) for lo,hi in [(0x59bee,0x5a696),(0x8f3a6,0x8f6b6),(0x5d326,0x5d346)]])
 for name,value in [('ring-gun-programs.h','\n'.join(h)),('ring-gun-programs.c','\n'.join(out)),('ring-gun-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),bindings=len(c.bindings),templates=len(TEMPLATES))))
if __name__=='__main__':main()
