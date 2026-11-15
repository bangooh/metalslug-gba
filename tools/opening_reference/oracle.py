"""Execute original 68000 cinematic tasks offline; no emulated CPU in GBA ROM."""
import struct,json,sys
from pathlib import Path
from unicorn import *
from unicorn.m68k_const import *
R=Path(__file__).resolve().parents[1]
u=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);u.ctl_set_cpu_model(UC_CPU_M68K_M68000);u.mem_map(0,0x400000)
b=(R/'upstream/build/mslug_prom.bin').read_bytes();u.mem_write(0,b[:0x100000]);u.mem_write(0x200000,b[0x100000:]);u.reg_write(UC_M68K_REG_SR,0x2000)
D=[UC_M68K_REG_D0+i for i in range(8)];A=[UC_M68K_REG_A0+i for i in range(8)]
def rd(a,n=2):return int.from_bytes(u.mem_read(a,n),'big')
def wr(a,v,n=2):u.mem_write(a,int(v&((1<<(8*n))-1)).to_bytes(n,'big'))
def reg(r):return u.reg_read(r)
def setr(r,v):u.reg_write(r,v&0xffffffff)
def ret():
 sp=reg(A[7]);pc=rd(sp,4);setr(A[7],sp+4);setr(UC_M68K_REG_PC,pc)
actors=[];live=set();pal={};cycles={};draws=[];sounds=[];frame=0;seed=1;global_fade=None

def alloc(handler,parent=0):
 a=0x101000+len(actors)*0xa0
 assert a<0x106000
 u.mem_write(a,b'\xff'*160)
 for o,n in [(0x14,2),(0x1e,2),(0x12,1),(0x38,2),(0x13,1),(0x5a,1),(0x69,1),(0x5b,1),(0x6b,1),(0x3a,1),(0x28,4),(0x2c,4),(0x26,2),(0x59,1),(0x45,1),(0x44,1),(0x6a,1)]:wr(a+o,0,n)
 wr(a+0x13,64,1);wr(a,handler,4);wr(a+12,parent,4);wr(a+0x10,rd(parent+0x10,1)+1 if parent else 0,1)
 actors.append(a);live.add(a)
 if parent in order:order.insert(order.index(parent)+1,a)
 else:order.append(a)
 return a
order=[];root=alloc(0x8f91a)
# Source scene loader global defaults (fixed camera; no mission gameplay).
wr(0x106ed2,255,1)
noops={0x43568,0x437da,0x51ed6,0x5da9c,0x5da56,0x47872,0x46ac6,0x91618,0x91514,0x91558,0x9152c,0x5d6a0}
# Palette values are copied from the normalized original P-ROM.
def sourcepal(n):
 off=(0x14e00+n*64) if n<1024 else (0x1ce00+(n-1024)*64)
 return [list(b[off+i*4:off+i*4+3]) for i in range(16)]
def fade_start(period,down,alternate):
 global global_fade
 # Native source palette fade rates are 1/1, 2/1, 4/1, 8/1 for 0..3.
 global_fade=[31 if down else 0, -[31,4,2,1][period] if down else [31,4,2,1][period],alternate]
 wr(0x10a2cf,0,1)

def hook(uc,pc,size,_):
 global seed,global_fade
 a=reg(A[6])
 if pc==0xf0000:uc.emu_stop();return
 if pc in (0x4ae,0x6fe):
  child=alloc(reg(A[1]),a);setr(A[0],child);ret();return
 if pc in (0x518,0x5b6):
  if pc==0x518:live.discard(a);uc.emu_stop()
  else:
   for x in list(live):
    q=x
    while q and q!=0xffffffff:
     if rd(q+12,4)==a:live.discard(x);break
     q=rd(q+12,4)
   ret()
  return
 if pc==0x28134:
  pr=reg(D[0])&65535
  if ((pr-0x4000)&65535)<0x7fff:pr=(pr&0xe01f)|((255-(rd(a+0x24)&255))<<5)
  wr(a+0x38,pr);ret();return
 if pc==0x236e:
  slot=a;wr(a+0x14,a&65535);pal[a&65535]=sourcepal(reg(D[1])&65535);cycles.pop(a&65535,None);ret();return
 if pc in (0x2c30,0x2c26):
  slot=reg(D[1])&65535;target=(reg(D[2])&65535)+(1024 if pc==0x2c26 else 0)
  cycles[slot]=[target,max(1,reg(D[3])&65535),reg(D[4])&255,0,pal.get(slot,sourcepal(0)),0];ret();return
 if pc==0x2b58:
  p=reg(A[0]);
  for i in range(256):
   slot=rd(p);n=rd(p+2)
   if slot==65535:break
   pal[slot&255]=sourcepal(1024+n) if n!=65535 else sourcepal(1024);p+=4
  ret();return
 if pc in (0x5239e,0x523b2,0x523c6,0x523da):fade_start(reg(D[0])&65535,pc in (0x523b2,0x523da),pc in (0x523c6,0x523da));ret();return
 if pc in (0x2352,0x23c68,0x2308,0x2222):sounds.append([frame,pc,reg(D[0])&65535]);ret();return
 if pc==0x5e9b6:
  seed=(seed*1103515245+12345)&0xffffffff;setr(D[0],seed>>16);ret();return
 if pc in (0x5a9e2,0x5a9d6):
  mp=reg(A[0]);xy=reg(D[0]);pr=reg(D[1])&65535;slot=reg(D[2])&65535
  if mp!=0xffffffff:
   draws.append(dict(map=mp,x=(xy>>16),y=xy&65535,priority=pr,slot=slot,sx=reg(D[4])&255,sy=reg(D[3])&255,flags=reg(D[5])&3,forced=pc==0x5a9d6,actor=a))
  ret();return
 if pc in noops:ret();return
 if 0x8f91a<=pc<0x916c8 or 0x28c00<=pc<0x29400 or 0x5dd02<=pc<0x5dd22 or 0x91096<=pc<0x910a2:return
 if pc in (0x5dc1c,0x5dc34):raise RuntimeError('unexpected init')
 raise RuntimeError(f'Unhandled service {pc:06x}, frame {frame}, actor {a:x}, handler {rd(a,4):x}, return {rd(reg(A[7]),4):x}')
u.hook_add(UC_HOOK_CODE,hook)
frames=[]
for frame in range(420):
 draws=[];i=0
 while i<len(order):
  a=order[i];i+=1
  if a not in live:continue
  setr(A[6],a);setr(A[7],0x10ff00);wr(0x10ff00,0xf0000,4)
  try:u.emu_start(rd(a,4),0xf0000,count=100000)
  except Exception as e:
   print('FAIL',e,'pc',hex(reg(UC_M68K_REG_PC)));raise
 if global_fade:
  global_fade[0]=max(0,min(31,global_fade[0]+global_fade[1]))
  if global_fade[0] in (0,31):wr(0x10a2cf,255,1)
 for slot,c in list(cycles.items()):
  target,period,inc,timer,orig,progress=c;timer+=1
  if timer>=period:
   progress+=inc or 1;level=min(31,max(0,progress-1));dest=sourcepal(target)
   pal[slot]=[[v+(1 if goal>=v else -1)*(abs(goal-v)*level//31) for v,goal in zip(row,goalrow)] for row,goalrow in zip(orig,dest)]
   timer=0
  c[3]=timer;c[5]=progress
  if progress>=32:pal[slot]=sourcepal(target);del cycles[slot]
 # Store per-frame source RGB palettes. Keep original per-draw palette choices.
 frames.append(dict(frame=frame,backdrop=pal.get(255,sourcepal(0))[15],root_handler=rd(root,4),root_y=rd(root+0x24),root_phase=rd(root+0x21,1),draws=draws,palettes={str(d['slot']):pal.get(d['slot'],sourcepal(0)) for d in draws},fade=global_fade.copy() if global_fade else [31,0,False]))
 if frame%30==0:print(frame,hex(rd(root,4)),rd(root+0x24),len(draws),len(live),flush=True)
Path('opening-work/trace.json').write_text(json.dumps(dict(frames=frames,sounds=sounds),separators=(',',':')))
