#!/usr/bin/env python3
"""Compile reviewed combat streams to native frames/events, offline only.

Deliberately bounded: unknown opcodes/callbacks fail conversion. This is not
an emulator and does not infer NPC logic from an animation's visual sequence.
"""
import argparse,hashlib,importlib.util,json,re,struct,sys
from pathlib import Path
from PIL import Image
import numpy as np
R=Path(__file__).resolve().parents[1]
SHA='1b8eee0891b6f8c837348e60ea2923ae229e0fb603d34ae62ae06131332f0d3d'

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--source',type=Path,required=True);ap.add_argument('--approved-art',type=Path,required=True);args=ap.parse_args()
 sys.path.insert(0,str(args.source/'assets/tools'));import sprites as s
 p=s.PRom(str(args.source/'build/mslug_prom.bin'));assert hashlib.sha256(p.d).hexdigest()==SHA
 tiles=s.Tiles();palettes=s.palettes(p);g=R/'generated';frames=[];blob=bytearray();sequences=[];proof=[]
 # Codes are native game events, never executable source addresses.
 callbacks={0x3942e:1,0x3c776:2,0x3c7fc:2,0x3c7aa:2,0x9b90a:3}
 def decode(start,actor=0):
  at=start;seen={};out=[];hold=1;events=[];selection=False
  for _ in range(1000):
   op=p.B(at);arg=p.B(at+1)
   if op==0:hold=arg;at+=2
   elif op==1:at=p.L(at+2)
   elif op==0x17:selection=True;at+=2
   elif op==2:
    if at in seen:return out,seen[at]
    seen[at]=len(out);parts=[];origin=at
    while p.B(at) in (2,0x18):
     if p.B(at)==0x18:selection=True;at+=2;continue
     fl=p.B(at+1);dx,dy=(0,0) if fl&8 else (p.SW(at+8),p.SW(at+10))
     if not selection or bool(fl&16)==bool(actor):parts.append((p.L(at+2),fl,dx,dy))
     at+=8 if fl&8 else 12
    out.append(dict(address=origin,parts=parts,hold=hold,events=events));events=[]
   elif op==8:
    callback=p.L(at+2)
    # $03D640 returns immediately outside scene 3; this conversion is M1-only.
    if callback==0x3d640:at+=6;continue
    assert callback in callbacks,hex(callback)
    events.append(callbacks[callback]);at+=6
   elif op==9:events.append(4 if p.L(at+2)!=0xffffffff else 5);at+=6
   elif op in (13,16):
    assert p.W(at+2)==0x8c and arg in (4,251),(hex(at),op,arg)
    events.append(6 if op==16 else 7);at+=4
   elif op==0x16:return out,-1
   else:raise ValueError(f'unreviewed animation opcode {op:02x} at {at:06x}')
  raise ValueError('animation budget')
 def pack(idx):
  out=bytearray()
  for ty in range(8):
   for tx in range(8):
    for y in range(8):
     for x in range(0,8,2):out.append(int(idx[ty*8+y,tx*8+x])|(int(idx[ty*8+y,tx*8+x+1])<<4))
  return bytes(out)
 def render(parts,pal):
  layers,box=s.compose(p,tiles,parts)
  # Fixed source origin maps exactly to pixel (32,56) after approved 75%.
  canvas=(-48,-80,80,48);assert box and box[0]>=canvas[0] and box[1]>=canvas[1] and box[2]<=canvas[2] and box[3]<=canvas[3],box
  idx=s.draw(tiles,layers,box,canvas)
  scaled=Image.fromarray(idx).resize((192,192),Image.Resampling.NEAREST).resize((96,96),Image.Resampling.NEAREST)
  image=np.array(scaled);crop=image[4:68,4:68]
  check=np.zeros((96,96),np.uint8);check[4:68,4:68]=crop
  assert np.array_equal(check,image),'sprite would be clipped'
  raw=pack(crop);frame=len(frames);blob.extend(raw);frames.append((32,56,pal))
  return frame,s.rgba(crop,palettes[0x176 if pal==0 else pal])
 # Approved stand-fire pixels stay byte-identical. The source stream supplies
 # their timing and callback positions; no old game code is imported.
 approved=args.approved_art/'generated';raw=(approved/'players.bin').read_bytes();header=(approved/'combat_motion.h').read_text()
 origins=[list(map(int,re.search(r'player_origin_'+axis+r'\[\]=\{([^}]+)',header)[1].split(','))) for axis in ('x','y')]
 specs=[('PISTOL',0x27bb20,0x2796a4,0,94),('KNIFE_A',0x27c4de,0x279d56,0,None),
        ('KNIFE_B',0x27c3f2,0x279d56,0,None),('CRATE',0x2f772a,0,0x1ab,None),('CRATE_BREAK',0x2f773c,0,0x1ab,None),
        ('CROUCH_PISTOL',0x27ba34,0,0,71),('UP_PISTOL',0x27bc22,0x2796a4,0,902),('AIM_UP_IDLE',0x27ad6a,0x2796a4,0,890)]
 previews=[]
 for name,start,lower,pal,old in specs:
  seq,loop=decode(start);lo=decode(lower)[0][0]['parts'] if lower else []
  native=[]
  for n,frame in enumerate(seq):
   if old is not None:
    f=len(frames);blob.extend(raw[(old+n)*2048:(old+n+1)*2048]);frames.append((origins[0][old+n],origins[1][old+n],pal))
   else:
    f,im=render(lo+frame['parts'],pal);previews.append((name,n,im))
   mask=sum(1<<e for e in frame['events']);native.append((f,frame['hold'],mask))
  sequences.append((name,native,loop));proof.append(dict(name=name,source=f'{start:06X}',frames=seq,loop=loop,approved_pixels=old is not None))
 bullet_frame,_=render([(0x224d04,8,0,-1)],2)
 (g/'combat.tiles').write_bytes(blob)
 # The existing player bank is approved; use that palette for new poses too.
 palette=bytearray((g/'player.pal').read_bytes())
 for pal in [0x1ab,2]:
  base=0x14e00+pal*64
  for n in range(16):
   rr,gg,bb,_=p.d[base+n*4:base+n*4+4];palette+=struct.pack('<H',rr|(gg<<5)|(bb<<10))
 (g/'combat.pal').write_bytes(palette)
 h=['#ifndef COMBAT_ASSETS_H','#define COMBAT_ASSETS_H','#include <stdint.h>','#include "../src/hitbox.h"',
    'struct CombatPose {uint16_t frame;uint8_t hold,events;};',
    'struct CombatSequence {uint16_t first;uint8_t count;int8_t loop;};',
    'extern const uint8_t combat_tiles[];','extern const uint16_t combat_palettes[];',
    'extern const struct CombatPose combat_poses[];','extern const struct CombatSequence combat_sequences[];',
    'extern const uint8_t combat_origins[][3];',
    'extern const struct HitAttack attack_probe,attack_knife,attack_pistol;',
    'extern const struct HitDefense defense_crate;']
 h.append(f'#define COMBAT_BULLET_FRAME {bullet_frame}')
 c=['#include "combat-assets.h"','const uint8_t combat_origins[][3]={'+','.join('{%d,%d,%d}'%(x,y,{0:0,0x1ab:1,2:2}[pal]) for x,y,pal in frames)+'};']
 allposes=[];defs=[]
 for i,(name,poses,loop) in enumerate(sequences):
  h.append(f'#define COMBAT_{name} {i}');defs.append((len(allposes),len(poses),loop));allposes+=poses
 c.append('const struct CombatPose combat_poses[]={'+','.join('{%d,%d,%d}'%v for v in allposes)+'};')
 c.append('const struct CombatSequence combat_sequences[]={'+','.join('{%d,%d,%d}'%v for v in defs)+'};')
 for name,at in [('probe',0x325e4),('knife',0x392a4),('pistol',0x30804)]:
  group,kind,damage,strength,shape=struct.unpack_from('>BBhBB',p.d,at);rect=struct.unpack_from('>4h',p.d,at+10)
  c.append('const struct HitAttack attack_'+name+'={{'+','.join(map(str,rect))+'},'+','.join(map(str,[damage,group,kind,strength,shape,p.B(at+6)]))+'};')
 at=0x9a61e;filter_at=p.L(at+2);vals=[p.B(filter_at+i) for i in range(36)];rect=[p.SW(at+10+i*2) for i in range(4)]
 c.append('static const uint8_t crate_filter[]={'+','.join(map(str,vals))+'};')
 c.append('const struct HitDefense defense_crate={{'+','.join(map(str,rect))+'},crate_filter,'+str(p.B(at+1))+'};')
 h+=['#endif'];(g/'combat-assets.h').write_text('\n'.join(h)+'\n');(g/'combat-assets.c').write_text('\n'.join(c)+'\n')
 (g/'combat-source.json').write_text(json.dumps(dict(prom_sha256=SHA,sequences=proof,
    limits='Combat fixture assets only; no enemy or mission completion claim. New knife poses use original pixels at approved 75% scale, without inferred facial edits.'),indent=2)+'\n')
 sheet=Image.new('RGBA',(8*128,((len(previews)+7)//8)*128),(40,44,48,255))
 for i,(_,_,im) in enumerate(previews):sheet.alpha_composite(im.resize((128,128),Image.Resampling.NEAREST),((i%8)*128,(i//8)*128))
 sheet.convert('RGB').save(R/'build/combat-art.png')
 print(json.dumps(dict(frames=len(frames),bytes=len(blob),sequences=len(sequences))))
if __name__=='__main__':main()
