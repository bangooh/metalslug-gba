#!/usr/bin/env python3
"""Offline, reviewed NPC drawings and hitboxes; never executable ROM code.

Unknown visual instructions fail the build. Per-pose effects are translated
to named C events; all source addresses remain in the provenance JSON only.
"""
import argparse,hashlib,json,struct,sys
from pathlib import Path
import numpy as np
from PIL import Image
R=Path(__file__).resolve().parents[1]
SHA='1b8eee0891b6f8c837348e60ea2923ae229e0fb603d34ae62ae06131332f0d3d'

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--source',type=Path,required=True);a=ap.parse_args()
 sys.path.insert(0,str(a.source/'assets/tools'));import sprites as s
 p=s.PRom(str(a.source/'build/mslug_prom.bin'));assert hashlib.sha256(p.d).hexdigest()==SHA
 tiles=s.Tiles();palettes=s.palettes(p);g=R/'generated'
 specs=[('WALK',0x29b744,14),('RUN_STOP',0x29b7c8,14),('IDLE',0x29b816,14),
        ('TURN',0x2b5cfa,14),('STEP_IN',0x2b5c56,14),('STEP_BACK',0x2b5ca8,14),
        ('MELEE',0x2b70d2,14),('DEATH',0x4acfe,0x2f),('STAND',0x29bd84,14),
        ('BIRD',0x2f4036,0x12a),('SIGN_A',0x2949ae,0x186),('SIGN_B',0x2949be,0x1b6),
        ('CHARGER_WALK',0x427fa,14),('THROW',0x29ba70,14),
        ('GRENADE',0x2c72ba,0x15d),('BURST',0x2df180,0x16c),
        ('FOOD_301',0x2f6cca,0x1c6),('FOOD_310',0x2f745e,0x1c6),
        ('POW_BOUND',0x3f292,0x182),('POW_FREE',0x3f39e,0x182),
        ('POW_IDLE',0x3f070,0x182),('POW_UNTIE',0x3f0cc,0x182),
        ('POW_WALK',0x3f40a,0x181),('POW_BOW',0x3f49e,0x181),
        ('POW_WAVE',0x3f8f4,0x181),('POW_EXIT',0x3f89e,0x181),('CHARGER_RUN',0x4287e,14)]
 palette_ids=list(dict.fromkeys(v[2] for v in specs));blob=bytearray();frames=[];poses=[];sequences=[];proof=[];images=[];dedup={};defenses={}
 def defense(at):
  if at==0xffffffff:return 255
  if at not in defenses:defenses[at]=len(defenses)
  return defenses[at]
 def decode(start):
  at=start;seen={};out=[];hold=1;events=0;def_id=254;sound=0;counter=0
  for _ in range(2000):
   op,arg=p.B(at),p.B(at+1)
   if op==0:hold=arg;at+=2
   elif op==1:at=p.L(at+2)
   elif op in (2,30):
    key=(at,counter)
    if key in seen:return out,seen[key]
    seen[key]=len(out);parts=[];origin=at
    while p.B(at) in (2,30):
     fl=p.B(at+1);dx,dy=(0,0) if fl&8 else (p.SW(at+8),p.SW(at+10))
     parts.append((p.L(at+2),fl,dx,dy-(p.B(at)==30)));at+=8 if fl&8 else 12
    out.append(dict(address=origin,parts=parts,hold=hold,events=events,defense=def_id,sound=sound))
    events=0;def_id=254;sound=0
   elif op==3:
    # $02902C: one byte loop counter, including ADDX saturation at zero.
    if not counter:counter=(arg+1)&255
    counter=max(0,counter-1);at=p.L(at+2) if counter else at+6
   elif op==4:sound=p.W(at+2);at+=4
   elif op==8:
    cb=p.L(at+2)
    if cb==0x5e8da:events|=1 # melee hit callback, not a periodic attack
    elif cb in (0x4a0d4,0x4a110):pass # selected palette baked into this sequence
    elif cb==0x55bce:events|=4 # grenade spawn from pose callback
    elif cb==0x283ca:events|=8 # clear attack latch on spin loop
    elif cb==0x3efec:events|=16 # typed POW gift spawn
    elif cb==0x4a1ba:assert sound # reviewed word $74 -> sound thunk
    else:raise ValueError(f'unreviewed callback {cb:06x}')
    at+=6
   elif op==9:def_id=defense(p.L(at+2));at+=6
   elif op==6:
    assert p.W(at+4)==0x74 and p.W(at+2)==0x1043
    sound=p.W(at+2);at+=6
   elif op==19:
    assert arg==1 and p.W(at+2)==0x3a
    events|=2;at+=4 # XOR facing, once on pose entry
   elif op==22:return out,-1
   else:raise ValueError(f'unreviewed opcode {op:02x} at {at:06x}')
  raise ValueError('animation budget')
 def render(parts,pal):
  layers,box=s.compose(p,tiles,parts);canvas=(-48,-80,80,48)
  assert box and box[0]>=-48 and box[1]>=-80 and box[2]<=80 and box[3]<=48,box
  idx=s.draw(tiles,layers,box,canvas)
  scaled=np.array(Image.fromarray(idx).resize((192,192),Image.Resampling.NEAREST).resize((96,96),Image.Resampling.NEAREST))
  crop=scaled[4:68,4:68];check=np.zeros_like(scaled);check[4:68,4:68]=crop
  assert np.array_equal(check,scaled),'NPC clipped'
  raw=bytearray()
  for ty in range(8):
   for tx in range(8):
    for y in range(8):
     for x in range(0,8,2):raw.append(int(crop[ty*8+y,tx*8+x])|(int(crop[ty*8+y,tx*8+x+1])<<4))
  key=(bytes(raw),pal)
  if key not in dedup:
   dedup[key]=len(frames);blob.extend(raw);frames.append((32,56,palette_ids.index(pal)))
   images.append(s.rgba(crop,palettes[pal]))
  return dedup[key]
 for name,at,pal in specs:
  seq,loop=decode(at);sequences.append((len(poses),len(seq),loop))
  assert len(seq)<=255 and -1<=loop<128,(name,len(seq),loop)
  for f in seq:poses.append((render(f['parts'],pal),f['sound'],f['hold'],f['events'],f['defense'],0))
  proof.append(dict(name=name,source=f'{at:06X}',palette=pal,loop=loop,poses=seq))
 # Player and soldier attack descriptors share the already source-tested core.
 defense(0x32788)
 h=['#ifndef ACTOR_ASSETS_H','#define ACTOR_ASSETS_H','#include <stdint.h>','#include "../src/hitbox.h"',
    'struct ActorPose {uint16_t frame,sound;uint8_t hold,events,defense,pad;};',
    'struct ActorSequence {uint16_t first;uint8_t count;int8_t loop;};',
    'extern const uint8_t actor_tiles[],actor_origins[][3];','extern const uint16_t actor_palettes[];',
    'extern const struct ActorPose actor_poses[];','extern const struct ActorSequence actor_sequences[];',
    'extern const struct HitDefense actor_defenses[];','extern const struct HitAttack soldier_probe,soldier_melee,soldier_grenade;',
    'extern const uint16_t soldier_walk_speeds[8];',
    'struct TerrainStamp {int16_t dx,dy;uint8_t width,height;const uint8_t *cells;};',
    'extern const struct TerrainStamp sign_stamps[2];','extern const int16_t grenade_trajectories[24][4];',
    'extern const uint32_t food_scores[16];']
 h += [f'#define ACTOR_{name} {i}' for i,(name,_,_) in enumerate(specs)]
 h += [f'#define ACTOR_PALETTES {len(palette_ids)}',f'#define PLAYER_DEFENSE {defenses[0x32788]}','#endif']
 c=['#include "actor-assets.h"',
    'const uint8_t actor_origins[][3]={'+','.join('{%d,%d,%d}'%v for v in frames)+'};',
    'const struct ActorPose actor_poses[]={'+','.join('{%d,%d,%d,%d,%d,%d}'%v for v in poses)+'};',
    'const struct ActorSequence actor_sequences[]={'+','.join('{%d,%d,%d}'%v for v in sequences)+'};']
 defs=[]
 for at,ix in defenses.items():
  fp=p.L(at+2)
  if fp!=0xffffffff:
   c.append(f'static const uint8_t filter_{ix}[]={{'+','.join(str(p.B(fp+n))for n in range(36))+'};')
  rect=[p.SW(at+10+n*2)for n in range(4)]
  defs.append('{{'+','.join(map(str,rect))+'},'+('0' if fp==0xffffffff else f'filter_{ix}')+','+str(p.B(at+1))+'}')
 c.append('const struct HitDefense actor_defenses[]={'+','.join(defs)+'};')
 for name,at in [('probe',0x57440),('melee',0x57494),('grenade',0x29cbfe)]:
  rect=[p.SW(at+10+n*2)for n in range(4)]
  c.append('const struct HitAttack soldier_'+name+'={{'+','.join(map(str,rect))+'},'+','.join(map(str,[p.SW(at+2),p.B(at),p.B(at+1),p.B(at+4),p.B(at+5),p.B(at+6)]))+'};')
 # $079A0E, row 0 speed choices; the initial M1 rows all select row 0.
 speeds=[p.W(0x2b756e+66+n*2)for n in range(8)]
 c.append('const uint16_t soldier_walk_speeds[8]={'+','.join(map(str,speeds))+'};')
 stamps=[]
 for i,at in enumerate([0x295f6e,0x295f94]):
  dx,dy,w,hh=p.SW(at),p.SW(at+2),p.W(at+4),p.W(at+6)
  c.append(f'static const uint8_t stamp_{i}[]={{'+','.join(str(p.B(at+8+n))for n in range(w*hh))+'};')
  stamps.append('{'+','.join(map(str,[dx,-dy,w,hh]))+f',stamp_{i}'+'}')
 c.append('const struct TerrainStamp sign_stamps[]={'+','.join(stamps)+'};')
 c.append('const int16_t grenade_trajectories[24][4]={'+','.join('{'+','.join(str(p.SW(0x55c08+i*8+n*2))for n in range(4))+'}'for i in range(24))+'};')
 c.append('const uint32_t food_scores[16]={'+','.join(str(int(f'{p.L(0x2f792c+i*4):08x}'))for i in range(16))+'};')
 pal=bytearray()
 for ix in palette_ids:
  for n in range(16):
   rr,gg,bb,_=p.d[0x14e00+ix*64+n*4:0x14e00+ix*64+n*4+4];pal+=struct.pack('<H',rr|(gg<<5)|(bb<<10))
 (g/'actor.tiles').write_bytes(blob);(g/'actor.pal').write_bytes(pal)
 (g/'actor-assets.h').write_text('\n'.join(h)+'\n');(g/'actor-assets.c').write_text('\n'.join(c)+'\n')
 (g/'actor-source.json').write_text(json.dumps(dict(prom_sha256=SHA,sequences=proof,defenses={hex(k):v for k,v in defenses.items()},
   scope='Reviewed original visual sequences/hitboxes, not proof of full NPC AI equivalence'),indent=2)+'\n')
 sheet=Image.new('RGBA',(8*96,((len(images)+7)//8)*96),(40,44,48,255))
 for i,im in enumerate(images):sheet.alpha_composite(im.resize((96,96),Image.Resampling.NEAREST),((i%8)*96,(i//8)*96))
 sheet.convert('RGB').save(R/'build/actor-art.png')
 print(json.dumps(dict(frames=len(frames),poses=len(poses),sequences=len(sequences),bytes=len(blob))))
if __name__=='__main__':main()
