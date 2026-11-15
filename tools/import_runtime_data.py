#!/usr/bin/env python3
"""Extract input replay and fixed HUD bytes; no game/test execution."""
import argparse,hashlib,json
from pathlib import Path
R=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args()
 raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 addresses=[int.from_bytes(raw[0xe7c00+4*i:0xe7c04+4*i],'big') for i in range(2)]
 if addresses!=[0xe7800,0xe7a00]:raise ValueError(addresses)
 data={name:(at,n) for name,at,n in [('credits',0x52230,10),('level',0x522ea,6),('pause',0x13d32,5),('pause_blank',0x13d38,5)]}
 h=['#ifndef NATIVE_RUNTIME_DATA_H','#define NATIVE_RUNTIME_DATA_H','#include <stdint.h>','extern const uint8_t runtime_demo[2][513];']
 out=['#include "runtime-data.h"','const uint8_t runtime_demo[2][513]={']
 for at in addresses:out.append('{'+','.join(map(str,raw[at:at+513]))+'},')
 out.append('};')
 for name,(at,n) in data.items():
  h.append(f'extern const uint8_t runtime_{name}[{n}];')
  out.append(f'const uint8_t runtime_{name}[{n}]={{'+','.join(map(str,raw[at:at+n]))+'};')
 #799DE increments A0 by2, then799A4 calls2AC4C. On Slug presence it
 #copies ROM byte[A0+68] into the calling actor's68 owner byte. This is
 #NOT necessarily a rank value or Slug owner. Some choice tables spill
 #into their following source data here; preserve the fingerprinted byte.
 groups={}
 for name in ['air_formation','assault76','boss117','boss120','carrier','emitter59','final',
  'ground-nest','gun-platform','light-tank','m4','m5','mortar','script151','shield','shore','tank','turret142','walker']:
  ledger=json.loads((R/f'generated/{name}-source.json').read_text())
  if ledger['prom_sha256']!=digest:raise ValueError('rank ledger fingerprint')
  entries=ledger.get('rank_sources',ledger.get('ranks'))
  groups[name.replace('-','_')]=list(entries.values()) if isinstance(entries,dict) else entries
 groups['ground_nest'].append(0x2b7d2e)
 groups.update(airship=[0x2c06aa],deploy=[0x2b7716],allen=[0x296e7a,0x296efc,0x296f7e,0x297000],
  infantry=[0x2b74ec,0x2b746a,0x2b75d0,0x2b76b4,0x2b756e],gun=[0x2f6648,0x2f66ca],
  opening=[0x2c0010],late_guard=[0x2b79a0],boss2=[0x2bcf54,0x2bcfd6,0x2bd058],
  crab=[0x2bda80,0x2bdb02],fort=[0x2c0628,0x2c05a6],urban=[0x2c0196,0x2c0114],
  scene_prop=[0x2c0092],siege=[0x2c0218],enemy_ordnance=[0x2b7632])
 def off(at):return at-0x100000 if at>=0x200000 else at
 groups['gunship']=[0x2b91fe,0x2b9488,0x2b950a]+[int.from_bytes(raw[off(0x2d4a4e+4*i):off(0x2d4a52+4*i)],'big') for i in range(4)]
 owners={}
 for name,entries in groups.items():
  if any(int.from_bytes(raw[off(at):off(at)+2],'big') not in (0,1,2) for at in entries):raise ValueError('rank mode '+name)
  values=[raw[off(at)+0x6a] for at in entries]
  h.append(f'extern const uint8_t runtime_owner_{name}[{len(values)}];')
  out.append(f'const uint8_t runtime_owner_{name}[{len(values)}]={{'+','.join(map(str,values))+'};')
  owners[name]=[dict(table=at,byte_address=at+0x6a,owner=value) for at,value in zip(entries,values)]
 h.append('#endif')
 (R/'generated/runtime-data.h').write_text('\n'.join(h)+'\n');(R/'generated/runtime-data.c').write_text('\n'.join(out)+'\n')
 (R/'generated/runtime-source.json').write_text(json.dumps(dict(prom_sha256=digest,demo_addresses=addresses,demo_bytes=513,texts=data,rank_owner_sources=owners,source_only=True),indent=2)+'\n')
if __name__=='__main__':main()
