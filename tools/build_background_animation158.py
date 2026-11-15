from pathlib import Path
import sys,re,json,argparse
import numpy as np
R=Path(__file__).resolve().parents[1];G=R/'generated';b=G;sys.path.insert(0,str(R/'tools'))
from compile_m1_native_maps import body,numbers
from native_m1_hit_banks import hit_tables
hit_lookup,hit_colors=hit_tables()
parser=argparse.ArgumentParser();parser.add_argument('--base',type=Path,required=True,help='Freshly compiled static generated directory');args=parser.parse_args();base_dir=args.base.resolve()
def data(name):return (base_dir/name).read_bytes()
def arr(name,typ):return np.frombuffer(data(name),typ).copy()
def rgb(c):return np.stack([c&31,(c>>5)&31,(c>>10)&31],-1).astype(int)
s=(G/'scene-programs.c').read_text();rows=[r for r in json.loads((G/'tile-video-source.json').read_text())['records'] if r['file']=='scene-programs.c']
raw=np.fromfile(G/'native-scaled-background-12x12.4bpp',np.uint8).reshape(-1,12,6);pixels=np.empty((len(raw),12,12),np.uint8);pixels[:,:,::2]=raw&15;pixels[:,:,1::2]=raw>>4
pals=np.array(numbers(body((G/'drawing-data.c').read_text(),'native_source_palettes')),np.uint16).reshape(-1,16)
reports=[];header='/* Generated source auto-animation; two samples per original cycle. */\nstruct AutoTile158 {uint32_t key;uint16_t base,phase[2];};\n'
for mission,program in [(1,0),(2,1),(3,3),(4,2),(5,4),(6,5)]:
 D=G/f'm{mission}-cartridge';P=f'm{mission}-cartridge/'
 meta=[numbers(x) for x in re.findall(r'\{([^{}]+)\}',body(data(f'm{mission}-assets.c').decode(),f'm{mission}_images'))]
 logical=list(arr(P+'tiles.bin',np.uint8).reshape(-1,64));ids=list(map(int,arr(P+'ids4.bin','<u2')));attrs=list(map(int,arr(P+'attrs4.bin','<u2')))
 physical=list(arr(P+'tiles4.bin',np.uint8).reshape(-1,32));palette4=arr(P+'palette4.bin','<u2').reshape(12,16);palette=arr(P+'palette.bin','<u2');sem=arr(P+'semantic.bin','<u2') if (base_dir/P/'semantic.bin').exists() else np.zeros(192,np.uint16)
 sem4=arr(P+'semantic4.bin','<u2').reshape(12,16) if mission!=2 else np.zeros((12,16),int)
 if mission==1:
  length=len(logical) if sem.any() else json.loads(data(P+'animation155.json'))['base_tiles'];logical=logical[:length];ids=ids[:length];attrs=attrs[:length]
  used=sorted(set(ids));remap={v:i for i,v in enumerate(used)};physical=[physical[v] for v in used];ids=[remap[v] for v in ids]
 baselen=len(logical);oldphysical=len(physical);physical_lookup={bytes(t):i for i,t in enumerate(physical)};logical_lookup={}
 for i,(p,a) in enumerate(zip(ids,attrs)):logical_lookup[(p,a,bytes(logical[i]))]=i
 bind=np.full(256,-1,int)
 for slot,pal,flags in np.array(numbers(body((G/'session-data.c').read_text(),'session_palettes'))).reshape(-1,3):bind[slot]=pal
 for slot,pal in np.array(numbers(body(s,'scene_palette_pairs'))).reshape(-1,2)[program*102:(program+1)*102]:bind[slot&255]=-1 if pal==65535 else 1024+pal
 maps=arr(P+('maps-packed.bin' if mission>=3 else 'maps.bin'),'<u2');maprows=arr(P+'map-rows.bin','<u4') if mission>=3 else None
 if mission==1:sourceids=list(range(15));chains=[[0,3,10,12],[1,4,5,7,8,11,13],[6,9],[2,14]];clear=set();repeat=0
 elif mission==2:sourceids=list(range(15,21))+[52];chains=[[0,4,6],[1,3],[2],[5]];clear=set();repeat=0
 else:
  binding=json.loads(data(P+'bindings.json'));sourceids=[v[0] for v in binding['images']];chains=[[i for i,v in enumerate(binding['images']) if v[1]==p] for p in range(4)];clear=set(binding['clear_before']);repeat=binding['repeat_x_mask'] if mission in(3,6) else 0
 colorcache={};records=[];maxerr=0;pixelcount=0;errsum=0;animated_images=[]
 for chain in chains:
  active={}
  for image in chain:
   if image in clear:active={}
   off,w,h,ox,oy=meta[image];row=rows[sourceids[image]];cells=np.array(numbers(body(s,row['cells']))).reshape(row['width'],row['height'],2);rw=row['width']*12;rh=row['height']*12
   repeats=2 if repeat&(1<<image) else 3 if mission==2 and image==6 else 1
   active={k:v for k,v in active.items() if not(ox<=k[0]<ox+rw*repeats and oy<=k[1]<oy+rh)}
   had=False
   scores=[0]*8;maximum=0
   for ct,ca in cells.reshape(-1,2):
    ct,ca=int(ct),int(ca);cm=7 if ca&8 else 3 if ca&4 else 0
    if not cm:continue
    maximum=max(maximum,cm);start=(ct&~cm)+((ca>>4)&15)*65536
    for ph in range(cm+1):scores[ph]+=int(np.count_nonzero(pixels[start]!=pixels[start+ph]))
   alternate=(maximum+1)//2 if maximum else 0
   if maximum and not scores[alternate]:alternate=int(np.argmax(scores))
   for x in range(row['width']):
    for y in range(row['height']):
     t,a=map(int,cells[x,y]);mask=7 if a&8 else 3 if a&4 else 0
     if not mask:continue
     had=True;slot=a>>8;bank=bind[slot];assert bank>=0
     frames=[]
     for ph in [0,alternate&mask]:
      p=pixels[((t&~mask)|ph)+((a>>4)&15)*65536,::-1 if a&2 else 1,::-1 if a&1 else 1]
      tokens=np.where(p, np.where(np.any((sem!=0)&(sem//16==slot)),32768+slot*16+p.astype(np.int32),pals[bank,p]&32767),-1)
      if mission==1 and slot*16+1 in hit_lookup:
       tokens=np.array([-1]+[hit_lookup[slot*16+i] for i in range(1,16)])[p]
      frames.append(tokens)
     for rep in range(repeats):
      for py in range(12):
       for px in range(12):active[(ox+x*12+px+rep*rw,oy+y*12+py)]=(int(frames[0][py,px]),int(frames[1][py,px]))
   if had:animated_images.append(sourceids[image])
   bytile={}
   for (x,y),tokens in active.items():
    if not(0<=x<w*8 and 0<=y<h*8):continue
    tx,ty=x//8,y//8;coord=ty*w+tx
    if coord not in bytile:
     at=int(maprows[off+ty])+tx if mission>=3 else off+coord;ident=int(maps[at]);attr=attrs[ident];bank=attr>>12
     p=physical[ids[ident]];v=np.empty(64,np.uint8);v[::2]=p&15;v[1::2]=p>>4;v=v.reshape(8,8)
     if attr&0x400:v=v[:,::-1]
     if attr&0x800:v=v[::-1]
     bytile[coord]=[ident,bank,np.tile(v,(2,1,1)),np.tile(logical[ident].reshape(8,8),(2,1,1))]
    ident,bank,v,l=bytile[coord]
    for ph,token in enumerate(tokens):
     if (bank,token) in colorcache:ix,li=colorcache[(bank,token)]
     elif token<0:ix=li=0
     elif token>=32768:
      tag=token-32768;choices=np.where(sem4[bank]==tag)[0];ls=np.where(sem==tag)[0]
      if not len(choices):
       choices=np.where(((sem4[bank]!=0) if mission==1 else (sem4[bank]//16==tag//16))&(sem4[bank]!=0))[0]
       assert len(choices),(mission,image,bank,tag,'missing semantic family')
       color=int(pals[bind[tag//16],tag%16])&32767;delta=rgb(palette4[bank,choices])-rgb(np.array(color));choices=np.array([choices[np.argmin((delta*delta).sum(1))]])
      ix=int(choices[0]);ls=np.where(sem==sem4[bank,ix])[0];assert len(ls),(mission,image,bank,tag,int(sem4[bank,ix]),list(sem[sem>0]));li=int(ls[0])
     else:
      # Keep the approved palette bank and every nonanimated pixel exact.
      options=np.where((np.arange(16)>0)&(sem4[bank]==0))[0];assert len(options)
      delta=rgb(palette4[bank,options])-rgb(np.array(token));ix=int(options[np.argmin((delta*delta).sum(1))]);error=int(np.max(abs(rgb(np.array(palette4[bank,ix]))-rgb(np.array(token)))));maxerr=max(maxerr,error);errsum+=error;pixelcount+=1
      choices=np.where((np.arange(len(palette))>0)&(sem==0))[0];delta=rgb(palette[choices])-rgb(np.array(token));li=int(choices[np.argmin((delta*delta).sum(1))])
     colorcache[(bank,token)]=(ix,li)
     v[ph,y%8,x%8]=ix;l[ph,y%8,x%8]=li
   for coord,(base,bank,variants,logicalv) in sorted(bytile.items()):
    result=[]
    for v,l in zip(variants,logicalv):
     flat=v.ravel();packed=(flat[::2]|flat[1::2]<<4).astype(np.uint8);key=packed.tobytes()
     if key not in physical_lookup:physical_lookup[key]=len(physical);physical.append(packed)
     p=physical_lookup[key];attr=bank<<12;lk=(p,attr,l.tobytes())
     if lk not in logical_lookup:logical_lookup[lk]=len(logical);logical.append(l.ravel().copy());ids.append(p);attrs.append(attr)
     result.append(logical_lookup[lk])
    if result[0]!=result[1]:records.append(((image<<24)|coord,base,*result))
 assert max(ids)<18432 and len(logical)<65535,(mission,max(ids),len(logical))
 # Verify static logical tiles retain byte-exact decoded pixel data and bank.
 oldids=arr(P+'ids4.bin','<u2')[:baselen];oldattrs=arr(P+'attrs4.bin','<u2')[:baselen];oldtiles=arr(P+'tiles4.bin',np.uint8).reshape(-1,32)
 for i in range(baselen):assert attrs[i]==oldattrs[i] and np.array_equal(physical[ids[i]],oldtiles[oldids[i]])
 np.array(physical,np.uint8).tofile(D/'tiles4.bin');np.array(ids,'<u2').tofile(D/'ids4.bin');np.array(attrs,'<u2').tofile(D/'attrs4.bin');np.array(logical,np.uint8).tofile(D/'tiles.bin')
 opacity=np.packbits(np.array(logical).reshape(-1,8)>0,axis=1,bitorder='little').reshape(-1,8);opacity.tofile(D/'opacity.bin')
 if mission>=3:
  unique,inverse=np.unique(opacity,axis=0,return_inverse=True);unique.tofile(D/'opacity-packed.bin');inverse.astype('<u2').tofile(D/'opacity-ids.bin')
 hh=G/f'm{mission}-assets.h';hh.write_text(re.sub(fr'M{mission}_TILE_COUNT=\d+',f'M{mission}_TILE_COUNT={len(logical)}',data(f'm{mission}-assets.h').decode()))
 header+=f'static const struct AutoTile158 auto_tiles_m{mission}[]={{\n'+''.join('{%du,%d,{%d,%d}},\n'%t for t in sorted(records))+'};\n'
 reports.append(dict(mission=mission,source_images=animated_images,records=len(records),base_logical=baselen,logical=len(logical),physical=len(physical),added_physical=len(physical)-oldphysical,static_pixels_exact=True,max_new_channel_error=maxerr,mean_new_channel_error=errsum/max(1,pixelcount)))
 print(reports[-1],flush=True)
header+='struct AutoSet158 {const struct AutoTile158*data;unsigned count;};\nstatic const struct AutoSet158 auto_sets158[]={'+','.join(f'{{auto_tiles_m{m},sizeof(auto_tiles_m{m})/sizeof(*auto_tiles_m{m})}}' for m in range(1,7))+'};\n'
(G/'background-animation158.h').write_text(header);(b/'background-animation158.json').write_text(json.dumps(reports,indent=2))
