import zipfile,struct,collections,json,zlib
from unicorn import *
from unicorn.m68k_const import *
z=zipfile.ZipFile('scene-fixes/reference/neogeo.zip');raw=z.read('neo-epo.bin');b=bytearray(raw);b[::2],b[1::2]=raw[1::2],raw[::2]
u=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);u.ctl_set_cpu_model(UC_CPU_M68K_M68000);u.mem_map(0,0x1000000);u.mem_write(0xc00000,bytes(b));u.mem_write(0,open('upstream/build/mslug_prom.bin','rb').read()[:0x100000]);u.mem_write(0x300000,b'\xff'*0x100);u.mem_write(0x320000,b'\xff'*0x100)
u.mem_write(0x340000,b'\xff'*0x100);u.mem_write(0x380000,b'\xff'*0x100)
u.reg_write(UC_M68K_REG_SR,0x2700);u.reg_write(UC_M68K_REG_A7,struct.unpack_from('>I',b,0)[0]);pc=struct.unpack_from('>I',b,4)[0]
vram=[0]*65536;addr=0;inc=1;waits=0;steps=0;counts=collections.Counter();sounds=[]
def write(uc,access,a,size,val,ctx):
 global addr,inc
 if a==0x320000:sounds.append([waits,size,val])
 elif a==0x3c0000:addr=val&65535
 elif a==0x3c0002:vram[addr]=val&65535;addr=(addr+inc)&65535
 elif a==0x3c0004:inc=val

def read(uc,access,a,size,val,ctx):
 if a==0x3c0002:uc.mem_write(a,struct.pack('>H',vram[addr]))
 elif a==0x3c0006:uc.mem_write(a,struct.pack('>H',((steps//32)%264)<<7&65535))
def code(uc,a,size,ctx):
 global steps,waits
 if a==0xc16dda and uc.mem_read(0x10fe8c,1)==b'\x01':
  waits+=1
  sp=uc.reg_read(UC_M68K_REG_A7)-6;sr=uc.reg_read(UC_M68K_REG_SR)
  uc.mem_write(sp,struct.pack('>HI',sr,a));uc.reg_write(UC_M68K_REG_A7,sp);uc.reg_write(UC_M68K_REG_SR,0x2700);uc.reg_write(UC_M68K_REG_PC,0xc00438)
  open(f'scene-fixes/boot-animation/{waits:04d}.bin','wb').write(zlib.compress(struct.pack('>65536H',*vram)+bytes(uc.mem_read(0x400000,8192))))
  if waits>=330:uc.emu_stop()
 if uc.mem_read(a,2)==b'\x4e\x73':
  sp=uc.reg_read(UC_M68K_REG_A7);sr,ret=struct.unpack('>HI',uc.mem_read(sp,6));uc.reg_write(UC_M68K_REG_A7,sp+6);uc.reg_write(UC_M68K_REG_SR,sr);uc.reg_write(UC_M68K_REG_PC,ret)
 steps+=1;counts[a]+=1
u.hook_add(UC_HOOK_MEM_WRITE,write,begin=0x320000,end=0x3c000f);u.hook_add(UC_HOOK_MEM_READ,read,begin=0x3c0000,end=0x3c000f);u.hook_add(UC_HOOK_CODE,code)
try:u.emu_start(pc,0xffffff,count=2000000)
except UcError as e:print(e)
print('pc',hex(u.reg_read(UC_M68K_REG_PC)),counts.most_common(8));print('fix',[(hex(i),hex(x)) for i,x in enumerate(vram[0x7000:0x7500],0x7000) if x][:20])
open('scene-fixes/bios-vram.bin','wb').write(struct.pack('>65536H',*vram))

open('scene-fixes/boot-animation/sounds.json','w').write(json.dumps(sounds));print('sounds',sounds)
