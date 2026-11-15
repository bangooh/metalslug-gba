#!/usr/bin/env python3
"""Offline fortress assets. Source identities are provenance, never runtime PCs."""
import argparse,hashlib,json,struct,re
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(COLLAPSE=0x2eb2e4,SIGN=0x2ebfe6,SIGN_HIT=0x2ebffc,CAR=0x2ec180,CAR_HIT1=0x2ec19a,CAR_HIT2=0x2ec1ae,CAR_WRECK=0x2ec1c2,
 HELI=0x2ec226,HELI_ENTER=0x2ec23c,HELI_HIT1=0x2ec2ce,HELI_HIT2=0x2ec2de,HELI_WRECK=0x2ec2ee,LADDER=0x2ec2fe,
 CHUNK=0x2de4b0,QUAD0=0x2ec30e,QUAD1=0x2ec324,QUAD2=0x2ec33a,QUAD3=0x2ec350,
 BARRIER=0x2ec366,BARRIER_HIT1=0x2ec3de,BARRIER_HIT2=0x2ec3ee,BARRIER_HIT3=0x2ec3fe,BARRIER_WRECK=0x2ec40e,
 BLAST_HIGH=0x2dd594,BLAST_LOW=0x2dd37e,DEBRIS_A=0x2de262,DEBRIS_B=0x2de2d8,DEBRIS_C=0x2de34e,DEBRIS_D=0x2de3c4,DEBRIS_E=0x2de43a)
TABLES=dict(PILL=(0x2eb2c0,9),CRATE=(0x2eb6ba,5),HATCH_OPEN=(0x2ebfae,7),HATCH_CLOSE=(0x2ebfca,7),TURRET=(0x2ec21a,3))
FIXED=dict(ROOT=('DEFENSE',0x2ec41e),PROP=('DEFENSE',0x2ec726),ROOT_BLAST=('ATTACK',0x2ecc88),CAR_BLAST=('ATTACK',0x2ecc34))
PATCHES={f'P{x:X}':x for x in range(0x2ed738,0x2edc10,20)}
STAMPS=dict(ROOT0=0x2edcb0,ROOT1=0x2edcba,ROOT2=0x2edcfe,ROOT3=0x2edd0c,ROOT4=0x2edd20,PILL=0x2edd48,CAR0=0x2edd56,CAR1=0x2edd68,HELI=0x2edd7e,HELI_WRECK=0x2edd92)
PATTERNS={f'P{x:X}':x for x in [0x2ecf68,0x2ecf7a,0x2ed04a,0x2ed05c,0x2ed0ae,0x2ecfde,0x2ecff0,0x2ed002,0x2ed014,0x2ed026,0x2ed038,0x2ed0c0,0x2ed0d2,0x2ed0e4,0x2ecfcc,0x2ed176,0x2ed188,0x2ed19a,0x2ed1ac,0x2ed1be,0x2ed1d0,0x2ed1e2]}
FACTORIES={0x78066:'BLAST_HIGH',0x7808a:'BLAST_LOW',0x7857e:'DEBRIS72_A',0x785ba:'DEBRIS72_B',0x785d8:'DEBRIS88_B',0x78614:'DEBRIS72_C',0x7866e:'DEBRIS72_D',0x786aa:'DEBRIS72_E',0x786e6:'DEBRIS88_E',0x87f4c:'CHUNK',0x77e10:'SMALL',0x77f6a:'FIRE',0x77efe:'BLAST',0x77fd6:'SMOKE',0x78506:'DEBRIS_AD_A',0x78542:'DEBRIS_AD_B',0x78632:'DEBRIS77_A',0x786c8:'DEBRIS77_B',**{x:'DEBRIS'+str(i) for i,x in enumerate([0x78218,0x78236,0x78254,0x78272,0x78290,0x782ae,0x78740,0x7875e])}}
def vals(v):return ','.join(map(str,v))
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw)
 if c.long(0xe8000+269*4)!=0x87b26:raise ValueError('template 269 mapping')
 entries={k:c.entry(v) for k,v in ROOTS.items()};tables={k:[c.entry(c.long(at+4*i)) if c.long(at+4*i)!=0xffffffff else 65535 for i in range(n)] for k,(at,n) in TABLES.items()};fixed={k:c.binding(*v) for k,v in FIXED.items()}
 crate_defs=[c.binding('DEFENSE',c.long(0x2ec712+4*i)) for i in range(5)]
 wall_defs=[c.binding('DEFENSE',v) for v in [0x2ec822,0x2ec876,0x2ec822,0x2ec822,0x2ec8ca,0x2ec8ca,0x2ec91e,0x2ec972,*([0x2ec9c6]*5)]]
 c.compile()
 if {(x['source_offset'],x['width']) for x in c.fields}!={(0x20,1)} or c.parameters:raise ValueError('unreviewed owner field')
 callback_patches=[]
 for cb in c.callbacks:
  at=cb['source'];data=raw[at:at+14]
  if data[:2]!=bytes.fromhex('45f9') or data[6:]!=bytes.fromhex('4eb90005022a4e75'):raise ValueError(('unreviewed callback',hex(at)))
  callback_patches.append(c.long(at+2))
 # Each callback and field is implemented by fort.c; unexpected additions fail.
 h=['#ifndef NATIVE_FORT_PROGRAMS_H','#define NATIVE_FORT_PROGRAMS_H','#include "../src/bosses.h"','#include "../src/scene_video.h"','#include "../src/fort.h"',
 'enum FortAnimation {'+','.join('FTA_'+k for k in entries)+',FTA_COUNT};',
 'enum FortCallback {'+','.join('FTC_'+format(x['source'],'X') for x in c.callbacks)+'};',
 'enum FortField {'+','.join('FTF_'+format(x['source_offset'],'X')+'_'+str(x['width']) for x in c.fields)+'};',
 'enum FortBinding {'+','.join('FTB_'+k+'='+str(v) for k,v in fixed.items())+'};',
 'enum FortPattern {'+','.join('FTP_'+k for k in PATTERNS)+',FTP_COUNT};',
 'enum FortFactory {'+','.join('FTFX_'+v for v in FACTORIES.values())+'};',
 'extern const struct AnimationProgram fort_program;','extern const uint16_t fort_entries[FTA_COUNT];',
 'extern const struct BossBindingSet fort_bindings['+str(len(c.bindings))+'];',
 *['extern const uint16_t fort_'+k.lower()+'['+str(n)+'];' for k,(_,n) in TABLES.items()],
 'extern const uint16_t fort_crate_defs[5],fort_wall_defs[13];',
 'extern const struct BossPatternData fort_patterns[FTP_COUNT];','extern const uint8_t fort_pattern_choices[FTP_COUNT][16];',
 'extern const uint16_t fort_rank[2][64];','extern const int16_t fort_chunk_bounds[4],fort_blast_bounds[2][4];',
 'struct FortStamp {const uint8_t*cells;int16_t dx,dy;uint16_t width,height;};',
 'enum FortStampId {'+','.join('FTS_'+k for k in STAMPS)+',FTS_COUNT};','extern const struct FortStamp fort_stamps[FTS_COUNT];',
 'extern const struct InteractionContact fort_contacts[2];']
 out=c.c_source('fort_program','fort-programs.h');defs=[];attacks=[];sets=[];filters={}
 def array(ty,name,data):out.append(('static ' if name.startswith(('patch_cells','stamp_cells')) else '')+'const '+ty+' '+name+'={'+data+'};')
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
 array('struct BossBindingSet','fort_bindings[]',','.join(sets));array('uint16_t','fort_entries[FTA_COUNT]',vals(entries.values()))
 for k,v in tables.items():array('uint16_t','fort_'+k.lower()+'['+str(len(v))+']',vals(v))
 array('uint16_t','fort_crate_defs[5]',vals(crate_defs));array('uint16_t','fort_wall_defs[13]',vals(wall_defs))
 patch_ids={};patch_rows=[]
 def patch(at):
  if at in patch_ids:return patch_ids[at]
  i=len(patch_ids);patch_ids[at]=i;target=c.long(at);im=c.long(at+4);x,y,sx,sy,width,height=[c.signed(at+j) for j in range(8,20,2)]
  iw,ih=c.word(im),c.word(im+2);stream=c.long(im+4)
  if target not in (0x106f6c,0x107fe8) or min(sx,sy)<0 or min(width,height)<=0 or sx+width>iw or sy+height>ih:raise ValueError(('patch bounds',hex(at)))
  data=[c.word(stream+4*((sx+col)*ih+sy+row)+2*k) for col in range(width) for row in range(height) for k in range(2)]
  array('uint16_t',f'patch_cells{i}[{len(data)}]',vals(data));patch_rows.append('{patch_cells'+str(i)+','+vals([x,y,width,height,0 if target==0x106f6c else 1])+'}');return i
 named={k:patch(at) for k,at in PATCHES.items()}
 cb_ids=[patch(at) for at in callback_patches]
 h.append('extern const uint16_t fort_callback_patches['+str(len(cb_ids))+'];')
 array('uint16_t','fort_callback_patches[]',vals(cb_ids))
 h+=['enum FortPatchId {'+','.join('FTV_'+k+'='+str(v) for k,v in named.items())+'};','extern const struct SceneTilePatch fort_patches['+str(len(patch_ids))+'];']
 array('struct SceneTilePatch','fort_patches[]',','.join(patch_rows))
 stamp_rows=[]
 for i,at in enumerate(STAMPS.values()):
  dx,dy,width,height=[c.signed(at+2*j) for j in range(4)]
  if width<=0 or height<=0:raise ValueError('stamp dimensions')
  array('uint8_t',f'stamp_cells{i}[{width*height}]',vals(c.byte(at+8+j) for j in range(width*height)))
  stamp_rows.append('{stamp_cells'+str(i)+','+vals([dx,dy,width,height])+'}')
 array('struct FortStamp','fort_stamps[]',','.join(stamp_rows))
 patterns=[];choices=[];fact=list(FACTORIES)
 for at in PATTERNS.values():
  random=c.byte(at+13);p=c.long(at+14);ids=[fact.index(c.long(p+4*i)) for i in range(16)] if random else [fact.index(p)]*16
  patterns.append('{'+vals([c.signed(at+4),c.signed(at+8),c.word(at+6),c.word(at+10),c.word(at+2),c.byte(at),c.byte(at+1),random,ids[0]])+'}');choices.append('{'+vals(ids)+'}')
 array('struct BossPatternData','fort_patterns[]',','.join(patterns));array('uint8_t','fort_pattern_choices[FTP_COUNT][16]',','.join(choices))
 ranks=[]
 for at in [0x2c0628,0x2c05a6]:
  if c.word(at)!=0:raise ValueError('rank mode')
  ranks.append('{'+vals(c.word(at+2+2*i) for i in range(64))+'}')
 array('uint16_t','fort_rank[2][64]',','.join(ranks))
 array('int16_t','fort_chunk_bounds[4]',vals(c.signed(0x298736+2*i) for i in range(4)))
 array('int16_t','fort_blast_bounds[2][4]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in [0x2df206,0x2df210]))
 contacts=[]
 for at in [0x2ede46,0x2eddfa]:
  if c.byte(at)!=0x80:raise ValueError('contact marker')
  contacts.append('{{'+vals(c.signed(at+6+2*i) for i in range(4))+'},'+vals([c.signed(at+2),c.byte(at+1)])+'}')
 array('struct InteractionContact','fort_contacts[2]',','.join(contacts))
 # Explicit typed scene constructors; placements are not a scheduler.
 handlers={0x865dc:'ROOT0',0x86624:'ROOT1',0x86666:'ROOT2',0x866ae:'ROOT3',0x866f0:'ROOT4',0x8716a:'PROP0',0x871fa:'PROP1',0x87280:'PROP2',0x872fa:'SIGN',0x87366:'WALL',0x86f1c:'HATCH5',0x86f40:'HATCH6',0x87fbc:'QUAD',0x87b1c:'HELI',0x88114:'BARRIER',0x877d4:'CAR'}
 rows=[]
 for i in range(17):
  at=0x972cc+20*i
  if c.byte(at)!=1 or c.long(at+6) not in handlers:raise ValueError('unreviewed placement')
  rows.append('{'+vals([c.signed(at+2),c.signed(at+4),c.word(at+10),c.word(at+12)])+',FORT_'+handlers[c.long(at+6)]+','+str(c.byte(at+1))+'}')
 if c.word(0x972cc+17*20)!=65535:raise ValueError('scene4 terminator')
 h.append('struct FortPlacement {int16_t x,y;uint16_t config,priority;uint8_t entry,flags;};')
 h.append('extern const struct FortPlacement fort_scene4[17];')
 array('struct FortPlacement','fort_scene4[17]',','.join(rows))
 h.append('#endif');ledger=c.ledger();ledger.update(prom_sha256=digest,roots=ROOTS,tables=TABLES,patch_sources=patch_ids,stamp_sources=STAMPS,pattern_sources=PATTERNS,validation='Source conversion only; executable coverage tracked separately',complete_port=False,templates={269:0x87b26},implementation=['src/fort.c','src/fort_structures.c','src/fort_vehicles.c','src/fort_effects.c'],source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[lo:hi]).hexdigest()) for lo,hi in [(0x865dc,0x88f58),(0x77228,0x772a8),(0x78066,0x780ae),(0x7857e,0x78704)]])
 for name,data in [('fort-programs.h','\n'.join(h)),('fort-programs.c','\n'.join(out)),('fort-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(data+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),patches=len(patch_ids),stamps=len(STAMPS))))
if __name__=='__main__':main()
