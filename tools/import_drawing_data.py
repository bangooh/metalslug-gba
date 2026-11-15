#!/usr/bin/env python3
"""Compile every authored native drawing and lossless GBA graphics assets.

Uses the original ROM fingerprint and existing animation provenance ledgers.
No gameplay, emulator, preview/replay or runtime pixel composition.
"""
import argparse,hashlib,json,re,struct,zipfile
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1]
PROM_SHA='1b8eee0891b6f8c837348e60ea2923ae229e0fb603d34ae62ae06131332f0d3d'
MD5={'201-s1.bin':'59b0200b7ed4f9704e6f34fa6d05796e','201-c1.bin':'db70f526ea5eef2f8d95687542c5216c',
 '201-c2.bin':'9c1cdd4ddef664cc00dd9bafe7fc7d48','201-c3.bin':'96af10f773441ca60bacfd626d385caa','201-c4.bin':'530c64ea8ac47bb8e4ec527b18fa6c5f'}
def off(at):
 if 0<=at<0x100000:return at
 if 0x200000<=at<0x300000:return at-0x100000
 raise ValueError(f'non-ROM drawing map {at:08x}')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--source',type=Path,required=True);ap.add_argument('--rom',type=Path,required=True);args=ap.parse_args()
 raw=(args.source/'build/mslug_prom.bin').read_bytes();assert hashlib.sha256(raw).hexdigest()==PROM_SHA
 g=R/'generated';programs=[];map_sources=set()
 for path in sorted(g.glob('*.json')):
  if path.name=='scene-text-source.json':continue # Removed karaoke animation set.
  ledger=json.loads(path.read_text())
  if not isinstance(ledger,dict) or not isinstance(ledger.get('drawings'),list):continue
  if ledger.get('prom_sha256')!=PROM_SHA:raise ValueError('animation fingerprint '+path.name)
  stem=path.stem.removesuffix('-source').removesuffix('-program')
  candidates=[p for p in [g/(stem+'-programs.c'),g/(stem+'-data.c')] if p.exists()]
  if len(candidates)!=1:raise ValueError('program mapping '+path.name)
  content=candidates[0].read_text();symbols=re.findall(r'const struct AnimationProgram (\w+)=\{commands,(?:choices|0),(\d+),(\d+)\}',content)
  if len(symbols)!=1 or int(symbols[0][1])!=len(ledger['commands']):raise ValueError('program size '+path.name)
  commands={x['native_index']:x for x in ledger['commands']}
  if set(commands)!=set(range(len(commands))):raise ValueError('command indices')
  # Establish that each ledger describes the actual compiled command table.
  rows=re.findall(r'\{(\d+)u,(\d+),(\d+),AN_(\w+),(\d+),(\d+),0\}',content)
  expected=[(str(c['value']),str(c['next']),str(c['target']),c['op'],str(c['width']),str(c['argument'])) for _,c in sorted(commands.items())]
  if rows!=expected:raise ValueError('stale compiled animation '+path.name)
  programs.append((symbols[0][0],ledger['drawings'],path.name))
  map_sources.update(p['map'] for d in ledger['drawings'] for p in d if p['map']!=0xffffffff)
 # Retain four referenced legacy font maps to preserve stable v191 IDs.
 map_sources.update([2553288,2553342,2553544,2553600])
 ordered=sorted(map_sources);map_ids={at:i for i,at in enumerate(ordered)}
 if len(ordered)>=65535:raise ValueError('map ID capacity')
 maps=[];pieces=[];tilewords=[];map_audit=[]
 for at in ordered:
  cursor=off(at);start=cursor;count,attributes=raw[cursor:cursor+2];cursor+=2
  if not 1<=count<=255 or attributes&15 or attributes>>4>1:raise ValueError(f'map header {at:x}')
  first=len(pieces)
  for _ in range(count):
   x,y,width,height=struct.unpack_from('>hhBB',raw,cursor);cursor+=6
   if not width or not 1<=height<=32:raise ValueError(f'map dimensions {at:x}')
   n=width*height;cells=struct.unpack_from('>'+str(n)+'H',raw,cursor);cursor+=2*n
   pieces.append((len(tilewords),x,y,width,height));tilewords.extend(cells)
  # Do not impose the exploratory sprite viewer's +/-512 coordinate filter:
  # source map271B44 legitimately contains a distant x944 tile0 piece.
  maps.append((first,count,attributes));map_audit.append(dict(source=at,id=map_ids[at],bytes=cursor-start,sha256=hashlib.sha256(raw[start:cursor]).hexdigest()))
 parts=[];drawing_rows=[];sets=[];decl=[];program_audit=[]
 for number,(symbol,drawings,ledger) in enumerate(programs):
  first=len(drawing_rows)
  for drawing in drawings:
   drawing_rows.append((len(parts),len(drawing)))
   for p in drawing:
    parts.append((p['dx'],p['dy_up'],65535 if p['map']==0xffffffff else map_ids[p['map']],p['priority'],p['flags']))
  sets.append(f'{{&{symbol},&drawings[{first}],{len(drawings)}}}');decl.append(f'extern const struct AnimationProgram {symbol};')
  program_audit.append(dict(symbol=symbol,ledger=ledger,count=len(drawings),first=first))
 def rows(values):return ',\n'.join('{'+','.join(map(str,row))+'}' for row in values)
 c=['#include "drawing-data.h"',*decl,
  'static const struct NativeDrawing drawings[]={'+rows(drawing_rows)+'};',
  'const struct NativeDrawPart native_draw_parts[]={'+rows(parts)+'};',
  'const struct NativeDrawMap native_draw_maps[]={'+rows(maps)+'};',
  'const struct NativeDrawPiece native_draw_pieces[]={'+rows(pieces)+'};',
  'const uint16_t native_draw_tiles[]={'+','.join(map(str,tilewords))+'};',
  'const struct NativeDrawingSet native_drawing_sets[]={'+','.join(sets)+'};']
 palette=[]
 for base,count in [(0x14e00,1024),(0x1ce00,512)]:
  for i in range(count):
   entries=[]
   for j in range(16):
    red,green,blue,pad=raw[base+i*64+j*4:base+i*64+j*4+4]
    if max(red,green,blue)>31:raise ValueError('palette component')
    entries.append(red|(green<<5)|(blue<<10))
   palette.append(entries)
 from presentation_logo_repairs import nazca_palette
 palette[0x120]=nazca_palette(palette[0x120])
 c.append('const uint16_t native_source_palettes[NATIVE_PALETTE_COUNT][16]={'+rows(palette)+'};')
 h=['#ifndef NATIVE_DRAWING_DATA_H','#define NATIVE_DRAWING_DATA_H','#include "../src/drawing.h"',
  f'enum {{NATIVE_DRAWING_SET_COUNT={len(sets)},NATIVE_DRAW_PART_COUNT={len(parts)},NATIVE_DRAW_MAP_COUNT={len(maps)},NATIVE_DRAW_PIECE_COUNT={len(pieces)},NATIVE_DRAW_TILE_COUNT={len(tilewords)},NATIVE_SPRITE_TILE_COUNT=131072,NATIVE_FIXED_TILE_COUNT=4096,NATIVE_PALETTE_COUNT=1536}};',
  'extern const struct NativeDrawingSet native_drawing_sets[NATIVE_DRAWING_SET_COUNT];',
  'extern const struct NativeDrawPart native_draw_parts[NATIVE_DRAW_PART_COUNT];',
  'extern const struct NativeDrawMap native_draw_maps[NATIVE_DRAW_MAP_COUNT];',
  'extern const struct NativeDrawPiece native_draw_pieces[NATIVE_DRAW_PIECE_COUNT];',
  'extern const uint16_t native_draw_tiles[NATIVE_DRAW_TILE_COUNT],native_source_palettes[NATIVE_PALETTE_COUNT][16];',
  'extern const uint8_t native_sprite_pixels[],native_fixed_pixels[];','#endif']
 inputs={};roms={}
 with zipfile.ZipFile(args.rom) as z:
  for name,digest in MD5.items():
   data=z.read(name)
   if hashlib.md5(data).hexdigest()!=digest:raise ValueError('graphics fingerprint '+name)
   roms[name]=data;inputs[name]=dict(bytes=len(data),md5=digest,sha256=hashlib.sha256(data).hexdigest())
 # Decode source bitplanes to unchanged4bit color indices, then pack native
 # GBA8x8 quadrants. Chunking keeps temporary conversion memory bounded.
 with (g/'native-sprites.4bpp').open('wb') as output:
  for bank in range(2):
   odd,even=roms[f'201-c{bank*2+1}.bin'],roms[f'201-c{bank*2+2}.bin']
   for first in range(0,65536,4096):
    a=np.frombuffer(odd[first*64:(first+4096)*64],np.uint8).reshape(-1,4,8,2)
    b=np.frombuffer(even[first*64:(first+4096)*64],np.uint8).reshape(-1,4,8,2)
    planes=np.concatenate([a,b],axis=-1)
    bits=(planes[...,None]>>np.arange(8))&1
    pixels=(bits*(1<<np.arange(4))[:,None]).sum(axis=-2).astype(np.uint8)
    quadrants=pixels[:,[2,0,3,1],:,:] # TL,TR,BL,BR
    packed=quadrants[...,0::2]|(quadrants[...,1::2]<<4)
    output.write(packed.tobytes())
 fix=np.frombuffer(roms['201-s1.bin'],np.uint8).reshape(-1,4,8)[:,[2,3,0,1],:]
 (g/'native-fixed.4bpp').write_bytes(fix.transpose(0,2,1).copy().tobytes())
 (g/'drawing-data.c').write_text('\n'.join(c)+'\n');(g/'drawing-data.h').write_text('\n'.join(h)+'\n')
 audit=dict(prom_sha256=PROM_SHA,graphics_inputs=inputs,programs=program_audit,maps=map_audit,
  counts=dict(programs=len(sets),drawings=len(drawing_rows),parts=len(parts),maps=len(maps),pieces=len(pieces),tile_cells=len(tilewords)),
  conversion='Lossless nativeGBA4bpp source inventory;75percent conversion and physical integration are described in port-status.json',
  assets={name:dict(bytes=(g/name).stat().st_size,sha256=hashlib.sha256((g/name).read_bytes()).hexdigest()) for name in ['native-sprites.4bpp','native-fixed.4bpp']},
  gameplay_executed=False,physical_renderer_complete=False)
 (g/'drawing-source.json').write_text(json.dumps(audit,indent=2)+'\n');print(json.dumps(audit['counts']))
if __name__=='__main__':main()
