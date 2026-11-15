#!/usr/bin/env python3
"""Reviewed151 structure, mini-script, parts and projectile source conversion."""
import argparse,hashlib,json,struct
from pathlib import Path
from animation_compiler import AnimationCompiler
R=Path(__file__).resolve().parents[1]
ROOTS=dict(ROOT=0x75270,WALL=0x7527c,WALL_OPEN=0x75300,TOWER_A=0x753a2,TOWER_A_LOW=0x75466,TOWER_B=0x75288,TOWER_B_LOW=0x7534c,FIRE_A=0x75a82,FIRE_B=0x754b2,DOOR_A_OPEN=0x75bd8,DOOR_A_LOOP=0x75c34,DOOR_A_CLOSE=0x75cd0,DOOR_B_OPEN=0x75d2c,DOOR_B_LOOP=0x75dac,DOOR_B_CLOSE=0x75e76,COLLAPSE=0x75bcc,FLAME=0x75704,FLAME_RETURN=0x75688,SMOKE=0x75b98,WINDOW=0x76862,WINDOW_FALL=0x7686e,ROOF=0x769a2,ROOF_WRECK=0x769ae,CREW=0x76c74,FRAGMENT=0x76d6a,TRAIL=0x76d96)
FIXED=dict(ROOT=('DEFENSE',0x7466e),ROOT_IDLE=('DEFENSE',0x746be),FLAME_PULSE=('ATTACK',0x76576),SMOKE_PULSE=('ATTACK',0x765ca),FLAME=('ATTACK',0x766c2),SMOKE=('ATTACK',0x7661e),NUKE=('ATTACK',0x2ecec4))
RANKS=dict(HP=0x2bf004,WAIT=0x2bf086,BURST_WAIT=0x2bf108,BURST_B=0x2bf18a,BURST_A=0x2bf20c,SMOKE_SPEED=0x2bf28e,FRAGMENT_TIME=0x2bf310,LOW_WAIT=0x2bf392)
PATCHES=[0x747b2,0x747c6,0x747da,0x747ee,0x748b6,0x74852,0x7487a,0x74866,0x7483e,0x7482a,0x74816,0x74802,0x7488e,0x748a2]
PATTERNS=list(range(0x74922,0x749e8,18))
# Inline code has been reviewed and reduced to game operations. No native
# instruction addresses, CPU registers or memory bus appear in runtime data.
INLINE={
 0x749f0:('EVENTS',0xe9638,0x74a16),
 0x74a16:('HP_WAIT',0x80,0x74a2a),
 0x74a2e:('PATCH',0x74852,0x74a50),
 0x74a50:('HP_WAIT',0x66,0x74a64),
 0x74a68:('PATCH',0x7487a,0x74a8a),
 0x74a8a:('HP_WAIT',0x4c,0x74a9e),
 0x74aa6:('PATCH',0x74866,0x74ac8),
 0x74ac8:('PATCH',0x7483e,0x74aea),
 0x74aea:('PATCH',0x7482a,0x74b0c),
 0x74b0c:('HP_WAIT',0x33,0x74b20),
 0x74b24:('SOUND',0x1076,0x74b46),
 0x74b46:('PATCH',0x74816,0x74b68),
 0x74b68:('HP_WAIT',0x19,0x74b7c),
 0x74b80:('PATCH',0x74802,0x74ba2),
 0x74ba2:('HP_ZERO',0x0,0x74bb6),
 0x74bbe:('NUKE',0x8656e,0x74be0),
 0x74be0:('FX',0x749a0,0x74c02),
 0x74c06:('NOP',0x0,0x74c1e),
 0x74c24:('NUKE',0x8656e,0x74c46),
 0x74c46:('FX',0x74922,0x74c68),
 0x74c68:('FX',0x74934,0x74c8a),
 0x74c8a:('FX',0x74946,0x74cac),
 0x74cac:('FX',0x74958,0x74cce),
 0x74cce:('FX',0x7496a,0x74cf0),
 0x74cf0:('FX',0x7497c,0x74d12),
 0x74d12:('FX',0x7498e,0x74d34),
 0x74d38:('NOP',0x0,0x74d50),
 0x74d56:('NUKE',0x8656e,0x74d78),
 0x74d90:('FX',0x749b2,0x74db2),
 0x74db6:('NOP',0x0,0x74dce),
 0x74dd4:('NUKE',0x8656e,0x74df6),
 0x74dfa:('CLEAR',0x748ca,0x74e1a),
 0x74e1a:('PATCH',0x7488e,0x74e3c),
 0x74e42:('FX',0x749c4,0x74e64),
 0x74e72:('STAMP',0x0,0x74e88),
 0x74e88:('CLEAR',0x748d6,0x74ea8),
 0x74ea8:('PATCH',0x748a2,0x74eca),
 0x74ed2:('EVENTS',0xe9686,0x74ef8),
 0x74ef8:('NOP',0x0,0x74f10),
 0x74f16:('NUKE',0x8656e,0x74f38),
 0x74f42:('UNLOCK',0x0,0x74f58),
}
def vals(v):return ','.join(str(int(x)) for x in v)
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();raw=(a.source/'build/mslug_prom.bin').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest!=json.loads((R/'generated/player-source-catalog.json').read_text())['prom_sha256']:raise ValueError('PROM fingerprint')
 c=AnimationCompiler(raw)
 if c.long(0x278008)!=0x100120 or c.long(0x27800c)!=0xa7c:raise ValueError('projectile enclosing owner')
 entries={k:c.entry(v) for k,v in ROOTS.items()};fixed={k:c.binding(*v) for k,v in FIXED.items()};c.compile()
 if c.long(0xe8000+4*151)!=0x75100:raise ValueError('template mapping')
 if {v['source'] for v in c.callbacks}!={0x283ca,0x283d8,0x75ee8,0x76536,0x434ce,0x7650e,0x76dd4} or {(v['source_offset'],v['width']) for v in c.fields}!={(0x7a,2),(0x74,4),(0x78,2),(0x20,1)} or c.parameters:raise ValueError('unreviewed graph')
 overlays={}
 def drawing(source):
  if source==0xffffffff:return 65535
  parts=[dict(map=source,flags=8,dx=0,dy_up=0,priority=65535)];v=c.intern(c.drawings,c.draw_ids,json.dumps(parts,sort_keys=True),parts);overlays[source]=v;return v
 for n in c.nodes:
  if n['op']=='SET' and n['width']==4:n['value']=drawing(n['value'])
 raw_draw=[drawing(x) for x in [0x2435e6,0x24369e]]
 h=['#ifndef NATIVE_SCRIPT151_PROGRAMS_H','#define NATIVE_SCRIPT151_PROGRAMS_H','#include "../src/bosses.h"','#include "../src/scene_video.h"','enum Script151Animation {'+','.join('S151A_'+k for k in ROOTS)+',S151A_COUNT};','enum Script151Field {'+','.join('S151F_'+format(v['source_offset'],'X') for v in c.fields)+'};','enum Script151Callback {'+','.join('S151C_'+format(v['source'],'X') for v in c.callbacks)+'};','enum Script151Binding {'+','.join('S151B_'+k+'='+str(v) for k,v in fixed.items())+'};','enum Script151Rank {'+','.join('S151R_'+k for k in RANKS)+',S151R_COUNT};','extern const struct AnimationProgram script151_program;','extern const uint16_t script151_entries[S151A_COUNT],script151_raw_draw[2];','extern const struct BossBindingSet script151_bindings['+str(len(c.bindings))+'];','struct Script151RankData {const uint16_t*values;};','extern const struct Script151RankData script151_ranks[S151R_COUNT];','extern const struct InteractionContact script151_contact;','extern const struct HitRect script151_target[2];','extern const uint16_t script151_palettes[2][9][2];','extern const uint8_t script151_palette_tail_lifecycle[2];','struct Script151Clear {int16_t x,y;uint16_t width,height;uint8_t plane;};','extern const struct Script151Clear script151_clear[2];','extern const struct SceneTilePatch script151_patches['+str(len(PATCHES))+'];','struct Script151Stamp {int16_t dx,dy;uint16_t width,height;const uint8_t*cells;};','extern const struct Script151Stamp script151_stamp;']
 out=c.c_source('script151_program','script151-programs.h')
 def array(ty,name,data):out.append('const '+ty+' '+name+'={'+data+'};')
 defs=[];attacks=[];sets=[]
 for i,b in enumerate(c.bindings):
  at=b['source'];records=defs if b['kind']=='DEFENSE' else attacks;first=len(records)
  for n in range(32):
   if c.byte(at)==255:break
   rect='{'+vals(c.signed(at+10+2*j) for j in range(4))+'}'
   if b['kind']=='DEFENSE':
    f=c.long(at+2);name='0'
    if f!=0xffffffff:
     name='filter_'+str(i)+'_'+str(n);out.append('static const uint8_t '+name+'[36]={'+vals(c.byte(f+j) for j in range(36))+'};')
    records.append('{'+rect+','+name+','+str(c.byte(at+1))+'}')
   elif b['kind']=='ATTACK':
    group,kind,damage,strength,shape,offscreen=struct.unpack_from('>BBhBBB',raw,c.offset(at))
    if group>4 or kind>34 or shape not in (0,4):raise ValueError('attack format')
    records.append('{'+rect+','+vals([damage,group,kind,strength,shape,offscreen])+'}')
   else:raise ValueError('binding kind')
   at+=80
  else:raise ValueError('hit terminator')
  sets.append('{'+('defs+'+str(first)+',0' if b['kind']=='DEFENSE' else '0,attacks+'+str(first))+','+str(n)+'}')
 out+=['static const struct HitDefense defs[]={'+','.join(defs)+'};','static const struct HitAttack attacks[]={'+','.join(attacks)+'};'];array('struct BossBindingSet','script151_bindings[]',','.join(sets))

 array('uint16_t','script151_entries[S151A_COUNT]',vals(entries.values()));array('uint16_t','script151_raw_draw[2]',vals(raw_draw))
 ranks=[]
 for i,at in enumerate(RANKS.values()):
  mode=c.word(at)
  if mode not in (0,1):raise ValueError('rank mode')
  array('uint16_t',f'rank_{i}[64]',vals(c.word(at+2+2*j) for j in range(64)));ranks.append('{rank_'+str(i)+'}')
 array('struct Script151RankData','script151_ranks[S151R_COUNT]',','.join(ranks))
 array('uint8_t','script151_palette_tail_lifecycle[2]',vals(c.byte(at+36+0x5b) for at in [0x7475e,0x74784]))
 at=0x74712
 if c.byte(at)!=128:raise ValueError('contact format')
 array('struct InteractionContact','script151_contact','{'+vals(c.signed(at+6+2*i) for i in range(4))+'},'+vals([c.signed(at+2),c.byte(at+1)]))
 array('struct HitRect','script151_target[2]',','.join('{'+vals(c.signed(at+2*i) for i in range(4))+'}' for at in [0x75f34,0x75f3c]))
 array('uint16_t','script151_palettes[2][9][2]',','.join('{'+','.join('{'+vals([c.word(at+4*i),c.word(at+4*i+2)])+'}' for i in range(9))+'}' for at in [0x7475e,0x74784]))
 patches=[]
 for i,at in enumerate(PATCHES):
  target=c.long(at);im=c.long(at+4);x,y,sx,sy,width,height=[c.signed(at+j) for j in range(8,20,2)];iw,ih=c.word(im),c.word(im+2);stream=c.long(im+4)
  if target!=0x106f6c or min(sx,sy)<0 or min(width,height)<=0 or sx+width>iw or sy+height>ih:raise ValueError('patch bounds')
  data=[c.word(stream+4*((sx+col)*ih+sy+row)+2*k) for col in range(width) for row in range(height) for k in range(2)]
  array('uint16_t',f'patch_cells{i}[]',vals(data));patches.append('{patch_cells'+str(i)+','+vals([x,y,width,height,0])+'}')
 array('struct SceneTilePatch','script151_patches[]',','.join(patches))
 clears=[]
 for at in [0x748ca,0x748d6]:
  if c.long(at)!=0x106f6c:raise ValueError('clear plane')
  clears.append('{'+vals([*[c.signed(at+4+2*i) for i in range(4)],0])+'}')
 array('struct Script151Clear','script151_clear[2]',','.join(clears))
 at=0x74f5a;dx,dy,width,height=[c.signed(at+2*i) for i in range(4)];array('uint8_t','stamp_cells[]',vals(c.byte(at+8+i) for i in range(width*height)));array('struct Script151Stamp','script151_stamp',vals([dx,dy,width,height])+',stamp_cells')
 # Compile reviewed mixed word-command/inline-action stream to a typed array.
 kinds=['END','TIMER','WAIT','SOUND','FLAG','HP_WAIT','HP_ZERO','EVENTS','PATCH','CLEAR','NUKE','FX','STAMP','UNLOCK','NOP'];rows=[];at=0x749e8;ledger=[]
 for budget in range(256):
  op=c.word(at);value=0;nxt=at+2;pause=0
  if op==0:kind='END'
  elif op==1:kind='TIMER';value=c.word(at+2);nxt=at+4
  elif op==2:kind='WAIT'
  elif op==7:kind='SOUND';value=c.word(at+2);nxt=at+4
  elif op in (5,6):kind='FLAG';value=[0x70,0x71,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c].index(c.word(at+2));value|=256 if op==5 else 0;nxt=at+4
  elif op in (3,4):
   kind,value,nxt=INLINE[at];pause=op==3
   if kind=='PATCH':value=PATCHES.index(value)
   elif kind=='CLEAR':value=[0x748ca,0x748d6].index(value)
   elif kind=='FX':value=PATTERNS.index(value)
   elif kind=='EVENTS':value=[0xe9638,0xe9686].index(value)
   elif kind=='NUKE':value=0
  else:raise ValueError('mini operation')
  ledger.append(dict(source=at,kind=kind,value=value,next=nxt,pause=pause));rows.append('{'+vals([value,kinds.index(kind),pause])+'}')
  if op==0:break
  at=nxt
 else:raise ValueError('unterminated mini script')
 h+=['enum Script151Op {'+','.join('S151OP_'+k for k in kinds)+'};','struct Script151Command {uint16_t value;uint8_t op,pause;};','extern const struct Script151Command script151_commands['+str(len(rows))+'];',f'enum {{SCRIPT151_END={len(rows)-1}}};','#endif'];array('struct Script151Command','script151_commands[]',','.join(rows))
 catalog=c.ledger();catalog.update(prom_sha256=digest,templates={151:0x75100},roots=ROOTS,ranks=RANKS,patches=PATCHES,mini_script=ledger,overlay_maps=overlays,implementation=['src/script151.c','src/script151_parts.c','src/script151_effects.c'],complete_port=False,validation='Source conversion and compilation only',projectile_owner=dict(boot_slot=0x100120,handler=0xa7c,idle=0xb90,initial_hp=-1,initial_cutoff=-1),pending=['Full renderer and scene consumers'],source_ranges=[dict(start=lo,end_exclusive=hi,sha256=hashlib.sha256(raw[c.offset(lo):c.offset(hi-1)+1]).hexdigest()) for lo,hi in [(0x745da,0x76fce),(0x8656e,0x865da)]])
 for name,value in [('script151-programs.h','\n'.join(h)),('script151-programs.c','\n'.join(out)),('script151-source.json',json.dumps(catalog,indent=2))]:(R/'generated'/name).write_text(value+'\n')
 print(json.dumps(dict(commands=len(c.nodes),drawings=len(c.drawings),callbacks=len(c.callbacks),mini_commands=len(rows),patches=len(PATCHES))))
if __name__=='__main__':main()
