#!/usr/bin/env python3
"""All eight source96B9C placement lists. Pending constructors stay explicit."""
import argparse,hashlib,json
from pathlib import Path
from animation_compiler import AnimationCompiler
from import_village_programs import STATIC
R=Path(__file__).resolve().parents[1]
VILLAGE={0x4d70c:'STATIC',0x4d6ec:'GROUND_STATIC',0x4d74a:'LAMP',0x4f46a:'SENSOR',0x527ba:'BREAKABLE',0x52902:'SIGN',0x52a26:'WALL',0x52b8a:'LARGE',0x52cea:'EXPLOSIVE',0x53306:'TOWER',0x534d8:'GATE',0x53964:'SWING',0x536ac:'BLINK',**{at:'HOUSE'+str(i) for i,at in enumerate([0x52e20,0x52e44,0x52e68,0x52e8c,0x52f16,0x52f3a])},**{0x530cc+42*i:'HUT'+str(i) for i in range(10)},**{at:'MARKER'+str(i) for i,at in enumerate([0x774ec,0x77500,0x77514,0x77528,0x7753e,0x77554,0x7756a])}}
SIEGE={0x834a4:'GATE',0x842d0:'BREAKABLE',0x83fa8:'ROW8',0x83596:'ARCH',0x84396:'FINALE',0x84ca4:'REWARD',0x835f2:'COLUMN0',0x8396a:'REACTIVE',0x8414c:'SINGLE'}
FORT={0x865dc:'ROOT0',0x86624:'ROOT1',0x86666:'ROOT2',0x866ae:'ROOT3',0x866f0:'ROOT4',0x8716a:'PROP0',0x871fa:'PROP1',0x87280:'PROP2',0x872fa:'SIGN',0x87366:'WALL',0x86f1c:'HATCH5',0x86f40:'HATCH6',0x87fbc:'QUAD',0x87b1c:'HELI',0x88114:'BARRIER',0x877d4:'CAR'}
AIRSHIP={0x88f74:'PROP0',0x88f8e:'PROP1',0x890cc:'CAMP',0x89e8e:'BARRELS',0x8a006:'ROCKRAIN',0x893ac:'BODY',0x8a10c:'TOWER_A',0x8a74e:'TOWER_B',0x89c44:'CRATE3',0x89e2c:'PROP2',0x89c58:'CRATE4'}
OPENING={0x4d8f2:'HUT',0x4dad2:'TOWER',0x4de40:'BUNKER',0x4df98:'BRIDGE',0x4e248:'NEST',0x4e38a:'SHED',0x4e512:'BARRIER'}
URBAN={0x53f96:'BUILDING',0x54160:'COLUMN',0x5447e:'COMPOUND',0x54282:'WALL',0x5481c:'FRAGILE',0x546f4:'STALL',0x54a1a:'HIT_STAGE',0x5495a:'BREAKABLE',0x54c0e:'RANKED',0x54b1c:'SMALL',0x5504a:'BLOCKER',**{at:'MULTISTAGE'+str(i) for i,at in enumerate([0x54cf2,0x54d12,0x54d32,0x54d52,0x54d72])}}
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);args=p.parse_args();raw=(args.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw);rows=[];lists=[];ledger=[];missing=[]
 for i,expected in enumerate([12,38,24,16,17,11,8,19]):
  start=c.long(0x96b9c+i*4);at=start;first=len(rows)
  while c.word(at)!=65535:
   kind,flags=c.byte(at),c.byte(at+1);entry=c.long(at+6);pal=c.word(at+14);graph=c.long(at+16);family=None;name=None
   for fam,mapping in [('URBAN',URBAN),('OPENING',OPENING),('VILLAGE',VILLAGE),('SIEGE',SIEGE),('FORT',FORT),('AIRSHIP',AIRSHIP),('PENDING',{})]:
    if entry in mapping:family=fam;name=mapping[entry];break
   if family is None or kind not in (0,1,2):raise ValueError(('unreviewed placement',hex(at),hex(entry)))
   if kind==0:
    if pal==65535 or family!='VILLAGE':raise ValueError('kind0 palette')
    graph_id=65535 if graph==0xffffffff else STATIC.index(graph)
   else:
    if pal!=65535 or graph!=0xffffffff:raise ValueError('unexpected metadata')
    graph_id=65535
   vals=[c.word(at+2),c.word(at+4),c.word(at+10),c.word(at+12),pal,graph_id,kind,flags]
   rows.append('{'+','.join(map(str,vals))+',PLACEMENT_'+family+','+('PENDING_' if family=='PENDING' else family+'_')+name+'}')
   item=dict(list=i,record=at,constructor=entry,family=family,entry=name,connected=family!='PENDING',kind=kind,x=vals[0],y=vals[1]);ledger.append(item)
   if family=='PENDING':missing.append(item)
   at+=20
  if len(rows)-first!=expected:raise ValueError('placement count')
  lists.append('{'+f'{first},{expected}'+'}')
 h=['#ifndef NATIVE_SCENE_PLACEMENTS_H','#define NATIVE_SCENE_PLACEMENTS_H','#include "../src/urban.h"','#include "../src/opening.h"','#include "../src/village.h"','#include "../src/siege.h"','#include "../src/fort.h"','#include "../src/airship.h"','enum ScenePlacementFamily {PLACEMENT_URBAN,PLACEMENT_OPENING,PLACEMENT_VILLAGE,PLACEMENT_SIEGE,PLACEMENT_FORT,PLACEMENT_AIRSHIP,PLACEMENT_PENDING};','struct ScenePlacement {uint16_t x,y,config,priority,palette,graph;uint8_t kind,flags,family,entry;};','struct ScenePlacementList {uint16_t first,count;};','extern const struct ScenePlacement scene_placements[145];','extern const struct ScenePlacementList scene_placement_lists[8];','#endif']
 out=['#include "scene-placements.h"','const struct ScenePlacement scene_placements[145]={'+',\n'.join(rows)+'};','const struct ScenePlacementList scene_placement_lists[8]={'+','.join(lists)+'};']
 data=dict(prom_sha256=digest,table=0x96b9c,records=ledger,record_count=len(rows),connected_records=len(rows)-len(missing),pending_records=missing,complete_port=False,validation='Static data and dispatch inventory only, not gameplay',source_sha256=hashlib.sha256(raw[0x96b9c:0x97720]).hexdigest())
 for name,v in [('scene-placements.h','\n'.join(h)),('scene-placements.c','\n'.join(out)),('scene-placement-source.json',json.dumps(data,indent=2))]:(R/'generated'/name).write_text(v+'\n')
 print(json.dumps(dict(records=len(rows),connected=len(rows)-len(missing),pending=len(missing))))
if __name__=='__main__':main()
