#!/usr/bin/env python3
"""Compile all16 original scene entries to typed scroll/map commands.

Inline callbacks are recognized by exact reviewed encodings and reduced to
scene conditions or assignments. Runtime data contains no CPU instructions.
"""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
LENGTHS={0:22,1:12,2:2,3:6,4:8,5:6,7:6,8:6,9:6,11:6,12:4,13:14,15:12,16:6,17:10,18:4,19:4,21:6,22:4}
OPS=['BIND','FIELDS','FRAME','WARP','CURSOR','DETACH','WAIT','DIRTY','HIDE','DRAW','PALETTES','CLEAR','MODE','CALL','RESERVED','LIMITS','CENTER','TRIGGER','AXIS','SOUND','WRITE','POSITION','PROGRESS']
PLANES=[0x106f6c,0x107fe8,0x108064,0x1080e0]
FIELDS={0x106f50:('X',2),0x106f54:('Y',2),0x10e39a:('INTRO',1),0x10a2cf:('FADE_DONE',1),0x106e88:('ENEMIES',2),0x10e39b:('SIGNAL',1),0x10e39c:('SIEGE',1),0x106f64:('SEED_Y',4)}
CALLS={0x52756:'PALETTE_CYCLE',0x52796:'COLLISION_ROW',0x52776:'FADE_OUT',0x5276c:'FADE_IN',0x52780:'DELETE_ACTORS',0x22c8:'AUDIO_STOP'}
def vals(v):return ','.join(str(int(x)) for x in v)
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw);W,L,B,S=c.word,c.long,c.byte,c.signed
 descriptors=[(L(0x916c8+8*i),L(0x916cc+8*i)) for i in range(16)]
 commands=[];index={};callbacks=[];palette_pairs=[];images=[];collisions=[];triggers=[];initial=[];script_entries={};initial_entries={}
 def image(width,height,source,collision=False):
  if not source:return 65535
  if not 0<width*height<200000:raise ValueError('map dimensions')
  if collision and width*height*4>32768:raise ValueError('signed collision source offset requires review')
  target=collisions if collision else images;key=(width,height,source)
  if key not in target:target.append(key)
  return target.index(key)
 def trigger(at):
  for i,t in enumerate(triggers):
   if t['source']==at:return i
  rows=[]
  for j in range(2048):
   threshold=W(at+4*j);value=W(at+4*j+2);rows.append((threshold,value))
   if threshold==65535:break
  else:raise ValueError('unterminated column controls')
  if rows[0][0]!=0:raise ValueError('column controls need preceding entry review')
  triggers.append(dict(source=at,rows=rows));return len(triggers)-1
 for start in sorted(set(v[0] for v in descriptors)):
  script_entries[start]=len(commands);pc=start
  for budget in range(4096):
   op=W(pc);v=[0]*6;plane=0;nxt=pc+LENGTHS.get(op,0);idx=len(commands);index[pc]=idx
   if op==0:
    plane=PLANES.index(L(pc+2));v=[image(W(pc+6),W(pc+8),L(pc+10)),image(W(pc+6),W(pc+8),L(pc+14),True),W(pc+18),W(pc+20),0,0]
    if plane and v[1]!=65535:raise ValueError('nonprimary collision map')
   elif op==1:plane=PLANES.index(L(pc+2));v[:3]=[B(pc+6),W(pc+8),W(pc+10)]
   elif op in (2,):pass
   elif op in (3,16,21):v[:2]=[W(pc+2),W(pc+4)]
   elif op==4:plane=PLANES.index(L(pc+2));v[:2]=[B(pc+6),B(pc+7)]
   elif op in (5,7,8,9,11):plane=PLANES.index(L(pc+2))
   elif op==6:
    at=pc+2;encoding=W(at);width=2 if encoding==0xc79 else 1 if encoding==0xc39 else 0;address=L(at+4);field=list(FIELDS).index(address);cond=W(at+8)
    if not width or FIELDS[address][1]!=width or cond not in (0x55c0,0x52c0,0x56c0,0x57c0) or W(at+10)!=0x43fa or W(at+14)!=0x4e75:raise ValueError('unknown wait callback')
    nxt=at+12+S(at+12)
    if nxt!=at+16:raise ValueError('nonlocal wait successor')
    v[:3]=[field,(0x55c0,0x52c0,0x56c0,0x57c0).index(cond),W(at+2)&(255 if width==1 else 65535)];callbacks.append(dict(source=at,kind='WAIT',field=FIELDS[address][0],condition=v[1],value=v[2],next=nxt))
   elif op==10:
    v[0]=len(palette_pairs);nxt=pc+2
    while W(nxt)!=65535:palette_pairs.append((W(nxt),W(nxt+2)));nxt+=4
    v[1]=len(palette_pairs)-v[0];nxt+=2
   elif op==12:v[0]=B(pc+2)
   elif op==13:
    fn=L(pc+2)
    if fn not in CALLS:raise ValueError('unknown scene call')
    v=[list(CALLS).index(fn),W(pc+6),W(pc+8),W(pc+10),W(pc+12),0]
   elif op==15:v[:5]=[W(pc+2+2*i) for i in range(5)]
   elif op==17:plane=PLANES.index(L(pc+2));v[0]=trigger(L(pc+6))
   elif op==18:v[0]=B(pc+2)
   elif op in (19,22):v[0]=W(pc+2)
   elif op==20:
    at=pc+2;encoding=W(at)
    if encoding==0x13fc:value=W(at+2)&255;address=L(at+4);end=at+8;width=1
    elif encoding==0x23fc:value=L(at+2);address=L(at+6);end=at+10;width=4
    else:raise ValueError('unknown assignment callback')
    if address not in FIELDS or FIELDS[address][1]!=width or W(end)!=0x41fa or W(end+4)!=0x4e75:raise ValueError('assignment field/return')
    nxt=end+2+S(end+2)
    if nxt!=end+6:raise ValueError('nonlocal assignment successor')
    v[:2]=[list(FIELDS).index(address),value];callbacks.append(dict(source=at,kind='WRITE',field=FIELDS[address][0],value=value,next=nxt))
   else:raise ValueError(f'unreviewed scene opcode {op:X} at{pc:X}')
   commands.append(dict(source=pc,op=op,plane=plane,values=v,next_source=nxt))
   if op==2:commands[-1]['next_source']=pc;break
   pc=nxt
  else:raise ValueError('scene command budget')
 for at in sorted(set(v[1] for v in descriptors)):
  first=len(initial);pc=at
  while B(pc)!=2:
   if B(pc) not in (0,1):raise ValueError('map sprite pool')
   initial.append([PLANES.index(L(pc+2)),B(pc),*[S(pc+6+2*i) for i in range(4)]]);pc+=14
  initial_entries[at]=(first,len(initial)-first,B(pc+1))
 if len(callbacks)!=191:raise ValueError('callback inventory')
 spawn_rows=[];spawn_tables=[]
 for mission in range(6):
  at=L(0x4cb44+4*mission);first=len(spawn_rows)
  for j in range(256):
   pc=at+16*j
   if W(pc)==65535:break
   spawn_rows.append([W(pc),W(pc+16),B(pc+2),B(pc+3),W(pc+4),W(pc+6),W(pc+8),W(pc+10),B(pc+12),B(pc+13),B(pc+14)])
  else:raise ValueError('unterminated spawn position table')
  spawn_tables.append((first,len(spawn_rows)-first))
 relink=0x2c84ba
 relink_image=image(W(relink),W(relink+2),L(relink+4))
 relink_collision=image(W(relink),W(relink+2),0x20a000,True)
 h=['#ifndef NATIVE_SCENE_PROGRAMS_H' ,'#define NATIVE_SCENE_PROGRAMS_H','#include <stdint.h>','enum SceneOp {'+','.join('SCOP_'+x for x in OPS)+'};','enum SceneField {'+','.join('SCF_'+v[0] for v in FIELDS.values())+'};','enum SceneCall {'+','.join('SCC_'+v for v in CALLS.values())+'};','enum SceneCondition {SCCOND_LT,SCCOND_GT,SCCOND_NE,SCCOND_EQ};','struct SceneCommand {uint32_t value[6];uint16_t next;uint8_t op,plane;};','struct SceneDescriptor {uint16_t command,first;uint8_t count,allocation_mode;};','struct SceneInitialPlane {uint8_t plane,pool;int16_t x,y,width,height;};','struct SceneImage {const uint16_t*cells;uint16_t width,height;};','struct SceneCollisionImage {const uint8_t*cells;uint16_t width,height;};','struct SceneColumnControl {uint16_t threshold,value;};','struct SceneColumnControls {const struct SceneColumnControl*rows;uint16_t count;};']
 h+=['struct SceneSpawnPosition {uint16_t threshold,next_threshold;uint8_t relative_x,relative_y;int16_t x,y;uint16_t mode,spacing;uint8_t item,quantity,lifetime;};','struct SceneSpawnTable {uint16_t first,count;};','#define SCENE_M5_RELINK_IMAGE '+str(relink_image),'#define SCENE_M5_RELINK_COLLISION '+str(relink_collision)]
 out=['#include "scene-programs.h"']
 def array(ty,name,data,static=False):out.append(('static ' if static else '')+'const '+ty+' '+name+'={'+data+'};')
 def export(ty,name,count,data):h.append('extern const '+ty+' '+name+'['+str(count)+'];');array(ty,name+'[]',data)
 export('struct SceneSpawnPosition','scene_spawn_positions',len(spawn_rows),','.join('{'+vals(v)+'}' for v in spawn_rows))
 export('struct SceneSpawnTable','scene_spawn_tables',6,','.join('{'+vals(v)+'}' for v in spawn_tables))
 export('struct SceneCommand','scene_commands',len(commands),','.join('{{'+vals(v['values'])+'},'+vals([index[v['next_source']],v['op'],v['plane']])+'}' for v in commands))
 export('struct SceneDescriptor','scene_descriptors',16,','.join('{'+vals([script_entries[v[0]],*initial_entries[v[1]]])+'}' for v in descriptors))
 export('struct SceneInitialPlane','scene_initial_planes',len(initial),','.join('{'+vals(v)+'}' for v in initial))
 export('uint16_t','scene_palette_pairs',2*len(palette_pairs),vals(x for row in palette_pairs for x in row))
 for collision,table in [(False,images),(True,collisions)]:
  prefix='collision' if collision else 'map';ctype='struct SceneCollisionImage' if collision else 'struct SceneImage';rows=[]
  for i,(width,height,at) in enumerate(table):
   name=prefix+'_cells'+str(i);count=width*height*4 if collision else width*height*2
   array('uint8_t' if collision else 'uint16_t',name+'[]',vals(B(at+j) if collision else W(at+2*j) for j in range(count)),True);rows.append('{'+name+','+vals([width,height])+'}')
  export(ctype,'scene_'+prefix+'_images',len(rows),','.join(rows))
 rows=[]
 for i,t in enumerate(triggers):
  name='column_controls'+str(i);array('struct SceneColumnControl',name+'[]',','.join('{'+vals(v)+'}' for v in t['rows']),True);rows.append('{'+name+','+str(len(t['rows']))+'}')
 export('struct SceneColumnControls','scene_column_controls',len(rows),','.join(rows));h.append('#endif')
 ledger=dict(prom_sha256=digest,descriptors=descriptors,commands=commands,callbacks=callbacks,initial_planes=initial,spawn_tables=spawn_tables,spawn_rows=spawn_rows,relink_image=relink_image,relink_collision=relink_collision,images=images,collisions=collisions,triggers=triggers,complete_port=False,implementation=['src/scene_camera.c','src/scene_maps.c','src/scene_runtime.c','src/source_audio.c'],validation='Source decoding and ARM compilation only; physical rendering, full scene controllers and released game integration remain pending')
 for name,value in [('scene-programs.h','\n'.join(h)),('scene-programs.c','\n'.join(out)),('scene-program-source.json',json.dumps(ledger,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(commands),callbacks=len(callbacks),images=len(images),collision_images=len(collisions),triggers=len(triggers),initial_planes=len(initial))))
if __name__=='__main__':main()
