#!/usr/bin/env python3
"""Source-only static placement assets: village destruction and shared props.
CPU identities are checked offline; runtime arrays contain semantic entries.
"""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(BREAK_A=0x297100,BREAK_B=0x297116,ONE_SHOT=0x29712c,SIGN=0x297174,SIGN_BREAK=0x29718a,SIGN_WRECK=0x2971ee,WALL=0x2973b6,WALL_HIT=0x2973cc,WALL2=0x297458,WALL2_HIT=0x297474,LARGE=0x297500,LARGE_WRECK=0x297516,EXPLOSIVE=0x297526,EXPLOSIVE_WRECK=0x29753c,HOUSE0=0x29754c,HOUSE1=0x297562,HOUSE2=0x297578,HUT0=0x29758e,HUT1=0x2975a4,HUT2=0x2975ba,HUT3=0x2975d0,HUT4=0x2975e6,TOWER=0x2975fc,TOWER2=0x297612,TOWER_WRECK=0x297622,GATE=0x297632,GATE2=0x297648,GATE_WRECK=0x297658,SWING_RIGHT=0x297668,SWING_LEFT=0x2976d2,SWING=0x297728,SWING_CHILD_LEFT=0x297750,SWING_CHILD_RIGHT=0x297798,SWING_CHILD=0x2977d0,CHUNK=0x2de4b0,LAMP=0x294280,LAMP_HIT=0x294296,LAMP_SPARK=0x294378,SENSOR=0x2948a6)
STATIC=[0x29498e,0x294214,0x29499e,0x298a68,0x298a78,0x298a98,0x298a88,0x29907c]
PATCHES=list(range(0x2982d8,0x298558,20))+list(range(0x29860c,0x2986fc,20))
PATTERNS=[0x297fc0,0x297fd2,0x297fe4,0x297ff6,0x298008,0x29801a,0x29802c,0x29803e,0x298050,0x298062,0x298074,0x298086,0x2980d8,0x2980ea]
FACTORIES={0x77e10:'BFX_SMALL',0x77f6a:'BFX_FIRE',0x77efe:'BFX_BLAST',0x7815a:'BFX_FIRE_BLUE',0x78130:'BFX_SMOKE_BLUE',0x528bc:'ONE_SHOT',0x78326:'WOOD_A',0x78344:'WOOD_B',0x78380:'WOOD_C',0x783bc:'WOOD_D',**{at:'BFX_DEBRIS'+str(i) for i,at in enumerate([0x78218,0x78236,0x78254,0x78272,0x78290,0x782ae,0x78740,0x7875e])}}
def vals(xs):return ','.join(map(str,xs))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);args=p.parse_args();raw=(args.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw);entries={k:c.entry(v) for k,v in ROOTS.items()};static=[c.entry(v) for v in STATIC];flying=[c.entry(c.long(0x2980fc+4*i)) for i in range(4)]
 debris=[c.entry(v) for v in [0x2ddf9e,0x2de014,0x2de100,0x2de1ec]]
 blast=c.binding('ATTACK',0x29874a);c.compile()
 if {(f['source_offset'],f['width']) for f in c.fields}!={(0x20,1)} or {v['source'] for v in c.callbacks}!={0x4fa40,0x4fa58,0x53d2c} or c.parameters:raise ValueError('unreviewed graph')
 h=['#ifndef NATIVE_VILLAGE_PROGRAMS_H','#define NATIVE_VILLAGE_PROGRAMS_H','#include "../src/bosses.h"','#include "../src/scene_video.h"','enum VillageAnimation {'+','.join('VGA_'+k for k in ROOTS)+',VGA_COUNT};','enum VillageCallback {'+','.join('VGC_'+format(v['source'],'X') for v in c.callbacks)+'};','enum {VGF_PHASE,VGB_BLAST='+str(blast)+'};','extern const struct AnimationProgram village_program;','extern const uint16_t village_entries[VGA_COUNT],village_static[8],village_flying[4],village_debris[4];','extern const struct BossBindingSet village_bindings['+str(len(c.bindings))+'];','extern const int16_t village_chunk_bounds[4],village_lamp_bounds[4],village_marker_xy[7][2];','extern const struct HitRect village_sensor;','extern const uint16_t village_hurt_sounds[5],village_break_patches[7][4];','extern const struct InteractionContact village_tower_contact;','extern const uint8_t village_house_cells[5];']
 out=c.c_source('village_program','village-programs.h')
 def array(ty,name,data,local=False):out.append(('static ' if local else '')+'const '+ty+' '+name+'={'+data+'};')
 defs=[];attacks=[];sets=[]
 for i,b in enumerate(c.bindings):
  at=b['source'];records=defs if b['kind']=='DEFENSE' else attacks;first=len(records)
  for n in range(32):
   if c.byte(at)==255:break
   rect='{'+vals(c.signed(at+10+2*j) for j in range(4))+'}'
   if b['kind']=='DEFENSE':
    f=c.long(at+2);name='0'
    if f!=0xffffffff:name=f'filter_{i}_{n}';array('uint8_t',name+'[36]',vals(c.byte(f+j) for j in range(36)),True)
    records.append('{'+rect+','+name+','+str(c.byte(at+1))+'}')
   elif b['kind']=='ATTACK':
    group,kind,damage,strength,shape,offscreen=struct.unpack_from('>BBhBBB',raw,c.offset(at))
    if group>4 or kind>34 or shape not in (0,4):raise ValueError('attack format')
    records.append('{'+rect+','+vals([damage,group,kind,strength,shape,offscreen])+'}')
   else:raise ValueError('binding kind')
   at+=80
  else:raise ValueError('hit terminator')
  sets.append('{'+('defs+'+str(first)+',0' if b['kind']=='DEFENSE' else '0,attacks+'+str(first))+','+str(n)+'}')
 array('struct HitDefense','defs[]',','.join(defs),True);array('struct HitAttack','attacks[]',','.join(attacks),True);array('struct BossBindingSet','village_bindings[]',','.join(sets))
 array('uint16_t','village_entries[VGA_COUNT]',vals(entries.values()));array('uint16_t','village_static[8]',vals(static));array('uint16_t','village_flying[4]',vals(flying));array('uint16_t','village_debris[4]',vals(debris))
 patch_rows=[]
 for i,at in enumerate(PATCHES):
  target=c.long(at);im=c.long(at+4);x,y,sx,sy,width,height=[c.signed(at+j) for j in range(8,20,2)];iw,ih=c.word(im),c.word(im+2);stream=c.long(im+4)
  if target not in (0x106f6c,0x107fe8) or min(sx,sy)<0 or min(width,height)<=0 or sx+width>iw or sy+height>ih:raise ValueError(('patch bounds',hex(at)))
  data=[c.word(stream+4*((sx+col)*ih+sy+row)+2*k) for col in range(width) for row in range(height) for k in range(2)]
  array('uint16_t',f'patch_cells{i}[{len(data)}]',vals(data),True);patch_rows.append('{patch_cells'+str(i)+','+vals([x,y,width,height,0 if target==0x106f6c else 1])+'}')
 h+=['enum VillagePatch {'+','.join('VGV_P'+format(at,'X') for at in PATCHES)+',VGV_COUNT};','extern const struct SceneTilePatch village_patches[VGV_COUNT];'];array('struct SceneTilePatch','village_patches[VGV_COUNT]',','.join(patch_rows))
 rows=[]
 for i in range(7):rows.append('{'+vals(65535 if c.long(0x297f50+i*16+j*4)==0xffffffff else PATCHES.index(c.long(0x297f50+i*16+j*4)) for j in range(4))+'}')
 array('uint16_t','village_break_patches[7][4]',','.join(rows))
 h+=['enum VillagePattern {'+','.join('VGP_P'+format(at,'X') for at in PATTERNS)+',VGP_COUNT};','enum VillageFactory {'+','.join('VFX_'+v for v in FACTORIES.values())+'};','extern const struct BossPatternData village_patterns[VGP_COUNT];','extern const uint8_t village_pattern_choices[VGP_COUNT][16];']
 rows=[];choices=[];fact=list(FACTORIES)
 for at in PATTERNS:
  random=c.byte(at+13);p=c.long(at+14);ids=[fact.index(c.long(p+4*i)) for i in range(16)] if random else [fact.index(p)]*16
  rows.append('{'+vals([c.signed(at+4),c.signed(at+8),c.word(at+6),c.word(at+10),c.word(at+2),c.byte(at),c.byte(at+1),random,ids[0]])+'}');choices.append('{'+vals(ids)+'}')
 array('struct BossPatternData','village_patterns[VGP_COUNT]',','.join(rows));array('uint8_t','village_pattern_choices[VGP_COUNT][16]',','.join(choices))
 array('int16_t','village_chunk_bounds[4]',vals(c.signed(0x298736+2*j) for j in range(4)));array('int16_t','village_lamp_bounds[4]',vals(c.signed(0x296254+2*j) for j in range(4)));array('struct HitRect','village_sensor',vals(c.signed(0x295fb0+2*j) for j in range(4)))
 array('uint16_t','village_hurt_sounds[5]',vals(c.word(0x5e766+2*j) for j in range(5)))
 at=0x2988ea
 if c.byte(at)!=128:raise ValueError('contact marker')
 array('struct InteractionContact','village_tower_contact','{'+vals(c.signed(at+6+2*j) for j in range(4))+'},'+vals([c.signed(at+2),c.byte(at+1)]))
 header=[c.signed(0x2986fc+2*j) for j in range(4)]
 h.append('extern const int16_t village_house_stamp[4];');array('int16_t','village_house_stamp[4]',vals(header))
 if header!=[8,31,5,1]:raise ValueError(('stamp size',header))
 array('uint8_t','village_house_cells[5]',vals(c.byte(0x298704+j) for j in range(5)))
 array('int16_t','village_marker_xy[7][2]',','.join('{'+vals([c.signed(0x7730a+40*i+8)*16,c.signed(0x7730a+40*i+10)*16+(16 if i in (0,4) else 32)])+'}' for i in range(7)))
 h.append('#endif');ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,static_roots=STATIC,pattern_sources=PATTERNS,patch_sources=PATCHES,implementation=['src/village.c','src/village_effects.c'],complete_port=False,validation='Source conversion only; no gameplay tests',source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in [(0x527ba,0x53a42),(0x53d2c,0x53f96),(0x4d6ec,0x4d8f2),(0x4f46a,0x4f4ee),(0x7749e,0x775ac)]])
 for name,v in [('village-programs.h','\n'.join(h)),('village-programs.c','\n'.join(out)),('village-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(v+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),patches=len(PATCHES),patterns=len(PATTERNS))))
if __name__=='__main__':main()
