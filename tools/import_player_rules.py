#!/usr/bin/env python3
"""Extract original inventory/action constants, preserving actual weapon order."""
import argparse,hashlib,json,struct
from pathlib import Path
R=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args()
 b=(a.source/'build/mslug_prom.bin').read_bytes();catalog=json.loads((R/'generated/player-source-catalog.json').read_text())
 if hashlib.sha256(b).hexdigest()!=catalog['prom_sha256']:raise ValueError('P-ROM fingerprint')
 ammo=struct.unpack_from('>10H',b,0x329d4);limits=list(b[0x329e8:0x329ed]);sounds=struct.unpack_from('>5H',b,0x32d28)
 h=['#ifndef PLAYER_RULES_DATA_H','#define PLAYER_RULES_DATA_H','#include <stdint.h>',
 'static const uint16_t player_default_ammo[5]={'+','.join(map(str,ammo[1::2]))+'};',
 'static const uint8_t player_shot_limits[5]={'+','.join(map(str,limits))+'};',
 'static const uint16_t player_weapon_announcements[5]={'+','.join(map(str,sounds))+'};','#endif']
 (R/'generated/player-rules-data.h').write_text('\n'.join(h)+'\n')
 (R/'generated/player-rules-source.json').write_text(json.dumps(dict(prom_sha256=catalog['prom_sha256'],weapons=catalog['weapons'],
  ammo_table=dict(source=0x329d4,values=ammo),shot_limits=dict(source=0x329e8,values=limits),
  announcements=dict(source=0x32d28,values=sounds),implementation='src/player_rules.c',
  scope='Inventory arithmetic, projectile accounting, aim direction, attack selection; body state scheduler and pickups not yet connected',complete=False),indent=2)+'\n')
 print(json.dumps(dict(default_ammo=ammo[1::2],shot_limits=limits,weapons=catalog['weapons'])))
if __name__=='__main__':main()
