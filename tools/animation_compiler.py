"""Offline compiler for original drawing streams to typed native animation data.

Source CPU addresses exist only in this converter and its provenance ledger.
The runtime receives indices, drawing records and owner-defined field/callback
IDs. Dynamic selectors require an explicit reviewed bound for each source site.
"""
import json,struct
class AnimationCompiler:
 def __init__(self,data,selector_counts=None):
  self.data=data;self.selector_counts=selector_counts or {}
  self.nodes=[];self.seen={};self.pending=[];self.drawings=[];self.draw_ids={}
  self.fields=[];self.field_ids={};self.callbacks=[];self.callback_ids={}
  self.bindings=[];self.binding_ids={};self.parameters=[];self.choices=[]
 def offset(self,at):
  if 0<=at<0x100000:return at
  if 0x200000<=at<0x300000:return at-0x100000
  raise ValueError(f'non-ROM animation reference {at:06X}')
 def byte(self,at):return self.data[self.offset(at)]
 def word(self,at):return struct.unpack_from('>H',self.data,self.offset(at))[0]
 def signed(self,at):return struct.unpack_from('>h',self.data,self.offset(at))[0]
 def long(self,at):return struct.unpack_from('>I',self.data,self.offset(at))[0]
 @staticmethod
 def intern(rows,ids,key,row):
  if key not in ids:ids[key]=len(rows);rows.append(row)
  return ids[key]
 def field(self,at,width):
  if at<0 or at+width>0xa0:raise ValueError('animation owner field range')
  return self.intern(self.fields,self.field_ids,(at,width),dict(source_offset=at,width=width))
 def entry(self,at,actor=0):
  key=(at,actor)
  if key not in self.seen:
   self.seen[key]=len(self.nodes);self.nodes.append(None);self.pending.append(key)
  return self.seen[key]
 def binding(self,kind,at):
  if at==0xffffffff:return at
  return self.intern(self.bindings,self.binding_ids,(kind,at),dict(kind=kind,source=at))
 def compile(self):
  b,w,sw,l=self.byte,self.word,self.signed,self.long
  for at,actor in self.pending:
   op,arg=b(at),b(at+1)
   c=dict(op='',value=0,next=65535,target=0,width=0,argument=0)
   def nxt(size):c['next']=self.entry(at+size,actor)
   if op==0:c.update(op='WAIT',argument=arg);nxt(2)
   elif op==1:c.update(op='JUMP',target=self.entry(l(at+2),actor))
   elif op in (2,0x17,0x1e):
    q=at;parts=[];selected=False;alternate=op==0x1e
    while b(q) in (2,0x17,0x18,0x1e):
     operation=b(q)
     if operation in (0x17,0x18):selected=True;q+=2;continue
     if operation==0x1e and q!=at:break
     flags=b(q+1);length=8 if flags&8 else 12
     if not selected or bool(flags&16)==bool(actor):
      parts.append(dict(map=l(q+2),flags=flags&15,dx=0 if alternate or flags&8 else sw(q+8),
                        dy_up=0 if alternate or flags&8 else sw(q+10),priority=65535 if alternate else w(q+6)))
     selected=False;q+=length
    key=json.dumps(parts,sort_keys=True)
    drawing=self.intern(self.drawings,self.draw_ids,key,parts)
    c.update(op='DRAW',target=drawing,argument=int(alternate),next=self.entry(q,actor))
   elif op==3:c.update(op='LOOP',argument=arg,target=self.entry(l(at+2),actor));nxt(6)
   elif op in (4,31):c.update(op='SOUND' if op==4 else 'SOUND_STOP',value=w(at+2));nxt(4)
   elif op in (5,6,7) or 13<=op<=21:
    if op<=7:kind='SET';width=(1,2,4)[op-5]
    else:kind=('AND','OR','XOR')[(op-13)//3];width=(1,2,4)[(op-13)%3]
    value=arg if width==1 else w(at+2) if width==2 else l(at+2)
    source_field=w(at+(2 if width==1 else 4 if width==2 else 6))
    if source_field+width>0xa0:raise ValueError(f'owner field {source_field:04X}/{width} at {at:06X}')
    target=self.field(source_field,width)
    c.update(op=kind,width=width,target=target,value=value);nxt(4 if width==1 else 6 if width==2 else 8)
   elif op==8:
    callback=self.intern(self.callbacks,self.callback_ids,l(at+2),dict(source=l(at+2)))
    c.update(op='CALL',target=callback);nxt(6)
   elif op in (9,10,11):
    kind={9:'DEFENSE',10:'ATTACK',11:'CONTACT'}[op]
    c.update(op=kind,value=self.binding(kind,l(at+2)));nxt(6)
   elif op==12:c.update(op='PALETTE',target=self.field(arg,2));nxt(2)
   elif op==22:c.update(op='END')
   elif op==25:
    self.parameters.append(self.data[self.offset(at+2):self.offset(at+10)].hex())
    c.update(op='PARAMETERS',value=len(self.parameters)-1);nxt(10)
   elif op==26:c.update(op='WAIT_TABLE',target=self.field(arg,4),argument=self.field(0x3b,1));nxt(2)
   elif op in (27,28):
    if at not in self.selector_counts:raise ValueError(f'bounded selector required at {at:06X}')
    count=self.selector_counts[at];first=len(self.choices);stride=18 if op==27 else 28
    self.choices += [self.entry(at+2+i*stride,actor) for i in range(count)]
    c.update(op='SELECT',target=first,argument=self.field(arg,2),value=count)
   elif op==29:
    if arg&15>1:raise ValueError('unreviewed back selector')
    c.update(op='JUMP',target=self.entry(at-l(0x293e4+(arg&15)*4),actor))
   else:raise ValueError(f'unknown animation opcode {op:02X} at {at:06X}')
   self.nodes[self.seen[(at,actor)]]=c
  if max(len(self.nodes),len(self.drawings),len(self.choices))>=65535:raise ValueError('native animation index capacity')
  return self
 def c_source(self,symbol,header):
  c=[f'#include "{header}"','static const struct AnimationCommand commands[]={']
  c += ['{'+f"{n['value']}u,{n['next']},{n['target']},AN_{n['op']},{n['width']},{n['argument']},0"+'},' for n in self.nodes]
  c+=['};']
  if self.choices:c+=['static const uint16_t choices[]={'+','.join(map(str,self.choices))+'};']
  c += [f'const struct AnimationProgram {symbol}={{commands,'+('choices' if self.choices else '0')+f',{len(self.nodes)},{len(self.choices)}'+'};']
  return c
 def ledger(self):
  return dict(fields=self.fields,callbacks=self.callbacks,bindings=self.bindings,parameters=self.parameters,
   choices=self.choices,drawings=self.drawings,
   commands=[dict(native_index=i,source=at,actor=actor,**self.nodes[i]) for (at,actor),i in self.seen.items()])
