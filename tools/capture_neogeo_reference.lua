-- Input-only capture of the user's unmodified Neo Geo cartridge.
-- No CPU-register, RAM, damage, or camera injection.
local camera_ages={}
local targets={[0]=true,[1536]=true,[2336]=true,[2648]=true,[2880]=true,[3328]=true,[3648]=true,[3848]=true}
local heli_age=0
local n=0
local limit=tonumber(os.getenv("MS_REFERENCE_FRAMES")) or 1800
local extended=limit>1800
local fields={}
for tag,port in pairs(manager.machine.ioport.ports) do
 for name,field in pairs(port.fields) do
  fields[name]=field
  print('INPUT '..tag..' '..name)
 end
end
local function key(name,value)
 if fields[name] then fields[name]:set_value(value) end
end
local screen=manager.machine.screens[':screen']
local memory=manager.machine.devices[':maincpu'].spaces['program']
local out=os.getenv('MS_REFERENCE_OUT') or '.'
local log=assert(io.open(out..'/reference-state.csv','w'))
log:write('frame,camera_x,camera_y\n')
emu.register_frame_done(function()
 n=n+1
 key('Coin 1',((n>=700 and n<720) or (extended and n>1800 and n%180<20)) and 1 or 0)
 key('1 Player Start',((n>=800 and n<820) or (extended and n>1800 and n%180>=60 and n%180<80)) and 1 or 0)
 key('P1 Start',((n>=800 and n<820) or (extended and n>1800 and n%180>=60 and n%180<80)) and 1 or 0)
 local cx=memory:read_u16(0x106f50)
 if cx==1536 then heli_age=heli_age+1 else heli_age=0 end
 key('P1 Right',n>=900 and heli_age==0 and cx<3776 and 1 or 0)
 key('P1 Left',heli_age>0 and heli_age<=60 and 1 or 0)
 key('P1 Up',heli_age>60 and 1 or 0)
 key('P1 C',n>=900 and n%90<2 and 1 or 0)
 key('P1 A',n>=900 and n%8<4 and 1 or 0)
 key('P1 B',n>=900 and heli_age==0 and n%120<15 and 1 or 0)
 if extended and targets[cx] and n>900 then
  camera_ages[cx]=(camera_ages[cx] or 0)+1
  local age=camera_ages[cx]
  if age==1 or age==60 or age==180 or age==360 then
   screen:snapshot(out..string.format('/matched-%06d.png',n))
   local f=assert(io.open(out..'/reference-captures.csv','a'));f:write(string.format('%d,%d,%d,%d\n',n,cx,memory:read_u16(0x106f54),age));f:close()
  end
 end
 if n%60==0 then
  log:write(string.format('%d,%d,%d\n',n,memory:read_u16(0x106f50),memory:read_u16(0x106f54)));log:flush()
 end
 if (extended and (n%300==0 or (cx>=3776 and n%150==0))) or n==300 or n==600 or n==900 or n==1080 or n==1200 or n==1800 then screen:snapshot(out..string.format('/neogeo-%06d.png',n)) end
 if n>=limit then log:close();manager.machine:exit() end
end,'frame')
