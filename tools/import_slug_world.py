#!/usr/bin/env python3
"""Record ordinary Slug construction and fixed-slot ordering from source bytes.

Offline provenance only: never run an emulated instruction or gameplay tick.
"""
import argparse,hashlib,json,struct
from pathlib import Path
R=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args()
 raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('P-ROM fingerprint')
 def word(at):return struct.unpack_from('>H',raw,at)[0]
 def long(at):return struct.unpack_from('>I',raw,at)[0]
 slots=[]
 for n in range(8):
  at=0xa7c+18*n
  if word(at)!=0x41f9 or word(at+6)!=0x43f9 or raw[at+12:at+18]!=bytes.fromhex('4eb90000050e'):raise ValueError('fixed-slot installation')
  slots.append(dict(source=at,slot=long(at+2),handler=long(at+8)))
 thunks=[]
 for at in [0x32406,0x3240c,0x29582]:
  if word(at)!=0x4eba or word(at+4)!=0x4e75:raise ValueError('fixed-slot thunk')
  target=at+2+struct.unpack_from('>h',raw,at+2)[0]
  thunks.append(dict(source=at,target=target))
 variants=[long(0x2ae90+i*4) for i in range(5)]
 if variants!=[long(0x2aee4+i*4) for i in range(5)]:raise ValueError('ordinary drop variant tables differ')
 spans=[(0x2a0f8,0x2a1aa),(0x2ae50,0x2af88),(0x2af88,0x2b166),
  (0x3060a,0x30696),(0x8f6d2,0x8f738),(0x8f8e2,0x8f8fe),(0x5dc1c,0x5dca4),
  (0x138fe,0x13906),(0x32ea4,0x32ed0)]
 data=dict(prom_sha256=digest,scope='ordinary five-variant Slug construction and NativePlayWorld wiring',
  fixed_slots_install_order=slots,fixed_slots_runtime_order=list(reversed(slots)),thunks=thunks,
  drop_tables=[0x2ae90,0x2aee4],variants=variants,
  source_ranges=[dict(start=x,end_exclusive=y,sha256=hashlib.sha256(raw[x:y]).hexdigest()) for x,y in spans],
  native_functions=['play_world_request_slug','slug_request_tick','slug_tick','contacts_tick','drop_init','drop_frame','slug_body_reset'],
  runtime_child_order=['wheel','marker','turret and descendants','contacts B/A/C','surface follower and descendants'],
  observations=[
   '$00050E prepends fixed slots; runtime is Slug, clear player $8D, P2, P1, clear player $88, projectiles, enemies',
   '$03240C calls $032EBA, not $032EBC; $88 is crouch-trigger flags, not arm-animation completion',
   '$0437DA calls $08F8E2 before gameplay: clear the opposite-phase Slug mask; no invented nibble shift',
   '$08F85C/$08F884/$08F8C2 anchor actors claim low bits for the next frame; high nibbles inhibit claims',
   '$02AE50 waits if the fixed slot is alive; $02AEA4 discards; both clamp byte variants >=5 to 4',
   '$02A0F8 writes $1E then $0138FE adds $74 in the fourth palette-record alias $1C; preserve $0092 through three-slot release',
   '$05DC1C fills $10..$9F with $FF before $05DC34 selective initialization; contact surface/previous-position sentinels are consumed before first child tick',
   'Turret $80 shot phase starts $FF; variants 0..3 clear $81 sequence, variant 4 leaves it $FF',
   'Source $3B is Animation.pose for body, surface delays and turret recoil; it is not PlayerTransform.display_group'],
  adaptations=['World-space coordinates with source word wrapping',
   'Three contacts are embedded and scheduled through one group callback in original B/A/C order',
   'Source physical palette handles/sprite metadata await renderer; no Neo Geo address runtime'],
  pending=['Original scene placement and anchor-claiming actor families',
   'All-pose renderer and sprite budget input','Released main/game loop connection'],
  validation='Source inspection/extraction and ARM compilation only; no functional tests or ROM build',complete=False)
 (R/'generated/slug-world-source.json').write_text(json.dumps(data,indent=2)+'\n')
 print(json.dumps(dict(fixed_slots=len(slots),drop_variants=len(variants),thunks=thunks)))
if __name__=='__main__':main()
