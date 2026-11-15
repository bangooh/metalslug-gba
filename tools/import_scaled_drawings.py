#!/usr/bin/env python3
"""Offline75% source map composition and legal GBA OBJ packing.

Consumes the complete fingerprinted drawing import, never a recorded route.
No colour quantization or gameplay execution. Original/approved assets are
retained separately; approved hand repairs are not silently overwritten.
"""
import hashlib,json,re
from pathlib import Path
import numpy as np
from PIL import Image
from approved_font_overrides import ApprovedFonts
from reviewed_assets import ReviewedAssets
R=Path(__file__).resolve().parents[1]
G=R/'generated'
# width,height,hardware shape,size; all legal4bpp1D OBJ shapes.
SHAPES=[(8,8,0,0),(16,16,0,1),(32,32,0,2),(64,64,0,3),
 (16,8,1,0),(32,8,1,1),(32,16,1,2),(64,32,1,3),
 (8,16,2,0),(8,32,2,1),(16,32,2,2),(32,64,2,3)]
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(s,name):
 text=re.search(r'const struct \w+ '+name+r'\[\]=\{(.*?)\};',s,re.S)
 if not text:raise ValueError('Missing typed input '+name)
 return [tuple(map(int,row.split(','))) for row in re.findall(r'\{([^{}]+)\}',text[1])]
def unpack_tiles(raw):
 a=np.frombuffer(raw,dtype=np.uint8).reshape(-1,8,4)
 out=np.empty((len(a),8,8),np.uint8);out[:,:,::2]=a&15;out[:,:,1::2]=a>>4
 return out
def scale(im):
 h,w=im.shape
 if w%4 or h%4:raise ValueError('Scale phase alignment')
 return np.array(Image.fromarray(im).resize((w*3//2,h*3//2),Image.Resampling.NEAREST)
                 .resize((w*3//4,h*3//4),Image.Resampling.NEAREST))
def pack(im):
 h,w=im.shape
 a=im.reshape(h//8,8,w//8,8).transpose(0,2,1,3).reshape(-1,8,8)
 return (a[:,:,::2]|(a[:,:,1::2]<<4)).tobytes()
def tiny_axis(im,origin,factor,axis):
 # GBA signed8.8 inverse matrices cannot encode1/256 and1/128 shrink.
 # Bake only these extreme axes offline, using map-origin-aligned nearest
 # samples. Other dynamic ratios use hardware affine sampling at runtime.
 if not factor:return im,origin
 extent=im.shape[axis];left=origin*factor//256;right=-(-(origin+extent)*factor//256)
 coords=np.arange(left,right,dtype=np.int64)*256//factor-origin
 valid=(coords>=0)&(coords<extent);sample=np.clip(coords,0,extent-1)
 out=np.take(im,sample,axis=axis).copy()
 if axis==0:out[~valid,:]=0
 else:out[:,~valid]=0
 return out,left
def greedy_chunks(im,weight=3):
 h,w=im.shape;canvas=np.zeros(((h+7)//8*8,(w+7)//8*8),np.uint8);canvas[:h,:w]=im
 th,tw=canvas.shape[0]//8,canvas.shape[1]//8
 occupied=canvas.reshape(th,8,tw,8).any(axis=(1,3));result=[]
 while occupied.any():
  # Always cover the first remaining nontransparent cell. Bounding shapes
  # may pad past the map, but never contain pixels already emitted elsewhere.
  ty,tx=np.argwhere(occupied)[0];best=None
  for w,h,shape,size in SHAPES:
   nw,nh=w//8,h//8;count=int(occupied[ty:ty+nh,tx:tx+nw].sum())
   score=count*weight-nw*nh
   key=(score,count,-nw*nh)
   if best is None or key>best[0]:best=(key,w,h,shape,size)
  _,w,h,shape,size=best;nw,nh=w//8,h//8
  tile=np.zeros((h,w),np.uint8)
  for yy in range(ty,min(ty+nh,th)):
   for xx in range(tx,min(tx+nw,tw)):
    if occupied[yy,xx]:
     tile[(yy-ty)*8:(yy-ty+1)*8,(xx-tx)*8:(xx-tx+1)*8]=canvas[yy*8:yy*8+8,xx*8:xx*8+8]
     occupied[yy,xx]=False
  result.append((tx*8,ty*8,w,h,shape,size,tile))
 return result
def chunks(im):
 # Compare complete legal partitions offline. Prefer fewer VRAM tiles only
 # when the original OAM count is not increased; all source pixels survive.
 baseline=greedy_chunks(im)
 if len(baseline)<2:return baseline
 area=lambda parts:sum(p[2]*p[3] for p in parts)
 best=baseline
 for weight in (2,2.5,4):
  candidate=greedy_chunks(im,weight)
  if len(candidate)<=len(baseline) and area(candidate)<area(best):best=candidate
 if 5<=len(baseline)<=16 and area(best)>=80*64:
  best=compact_chunks(im,len(baseline),best)
 return best
def compact_chunks(im,limit,baseline):
 # Bounded offline search over legal GBA shapes. Keep several alternatives
 # for each remaining occupied-cell count, so small dense blocks do not
 # displace every partition able to finish within the same OAM budget.
 h,w=im.shape;W=(w+7)//8;H=(h+7)//8
 canvas=np.zeros((H*8,W*8),np.uint8);canvas[:h,:w]=im
 occupied=canvas.reshape(H,8,W,8).any(axis=(1,3)).flatten()
 mask=sum(1<<int(i) for i in np.flatnonzero(occupied));options={}
 for i in np.flatnonzero(occupied):
  i=int(i);ty,tx=divmod(i,W);options[i]=[]
  for w,h,shape,size in SHAPES:
   nw,nh=w//8,h//8
   bits=sum(((1<<min(nw,W-tx))-1)<<(y*W+tx) for y in range(ty,min(ty+nh,H)))
   options[i].append((bits,nw*nh,(tx*8,ty*8,w,h,shape,size)))
 bound=sum(p[2]*p[3]//64 for p in baseline);states={mask:(0,())};best=None
 for depth in range(limit):
  candidates={}
  for remaining,(area,path) in states.items():
   i=(remaining&-remaining).bit_length()-1
   for bits,cost,shape in options[i]:
    rest=remaining&~bits;total=area+cost
    if total+rest.bit_count()>=bound:continue
    if not rest:bound=total;best=path+(shape,);continue
    if depth+1>=limit or rest.bit_count()>(limit-depth-1)*64:continue
    if rest not in candidates or total<candidates[rest][0]:candidates[rest]=(total,path+(shape,))
  groups={}
  for rest,v in candidates.items():groups.setdefault(rest.bit_count(),[]).append((v[0],rest,v[1]))
  states={}
  for group in groups.values():
   for area,rest,path in sorted(group,key=lambda a:a[0])[:32]:states[rest]=(area,path)
 if best is None:return baseline
 result=[];remaining=occupied.reshape(H,W).copy()
 for x,y,w,h,shape,size in best:
  tx,ty=x//8,y//8;tile=np.zeros((h,w),np.uint8)
  for yy in range(ty,min(ty+h//8,H)):
   for xx in range(tx,min(tx+w//8,W)):
    if remaining[yy,xx]:
     tile[(yy-ty)*8:(yy-ty+1)*8,(xx-tx)*8:(xx-tx+1)*8]=canvas[yy*8:yy*8+8,xx*8:xx*8+8]
     remaining[yy,xx]=False
  result.append((x,y,w,h,shape,size,tile))
 assert not remaining.any()
 return result
def main():
 fonts=ApprovedFonts()
 local_path=R/"art/character-local-fixes/patch.json"
 local_maps={a["target"]["source"]:a for a in json.loads(local_path.read_text())["assets"] if a["target"]["kind"]=="scaled-map"} if local_path.exists() else {}
 # Marco/Tarma repairs, applied last (after review-v18 and
 # character-local-fixes) and in this order; each stage records the pixels
 # it expects, so a stage can never land on a different picture.
 #  1 tools/build_player_animation_fixes.py  tears, twins, approved recomposition
 #  2 tools/build_player_solid_fixes.py      pinholes and stray near-black dots
 #  3 tools/build_player_twin_fixes.py       near-identical frames share one picture
 #  4 tools/build_player_face_fixes.py       Marco eye 1x2 + headband, Tarma sunglasses
 player_paths=[R/"art/player-animation-fixes/patch.json",R/"art/player-solid-fixes/patch.json",R/"art/player-twin-fixes/patch.json",R/"art/player-face-fixes/patch.json"]
 player_stages=[json.loads(q.read_text())["assets"] if q.exists() else [] for q in player_paths]
 player_maps=[{a["target"]["source"]:a for a in st if a["target"]["kind"]=="scaled-map"} for st in player_stages]
 player_frames=[[a for a in st if a["target"]["kind"]=="approved-player"] for st in player_stages]
 hexpixels=lambda rows:np.array([[int(c,16) for c in row] for row in rows],np.uint8)
 # reviewed.prepare() only recognises its own approved frames: unwind ours first.
 tiles_path=G/'player.tiles';tiles=bytearray(tiles_path.read_bytes())
 for frames in reversed(player_frames):
  for a in frames:
   start=a["target"]["frame"]*2048;old,new=pack(hexpixels(a["original"])),pack(hexpixels(a["rows"]))
   assert tiles[start:start+2048] in (old,new),('Approved frame baseline mismatch',a["id"])
   tiles[start:start+2048]=old
 tiles_path.write_bytes(tiles)
 reviewed=ReviewedAssets(fonts);reviewed.prepare(pack)
 tiles=bytearray(tiles_path.read_bytes())
 for frames in player_frames:
  for a in frames:
   start=a["target"]["frame"]*2048;old,new=pack(hexpixels(a["original"])),pack(hexpixels(a["rows"]))
   assert tiles[start:start+2048]==old,('Approved frame baseline mismatch',a["id"])
   tiles[start:start+2048]=new
 if any(player_frames):
  tiles_path.write_bytes(tiles);approved_path=G/'approved-player-source.json';approved=json.loads(approved_path.read_text())
  approved['approved_sha256']=hashlib.sha256(tiles).hexdigest();approved_path.write_text(json.dumps(approved,indent=2)+'\n')
 ledger=json.loads((G/'drawing-source.json').read_text());s=(G/'drawing-data.c').read_text()
 maps=rows(s,'native_draw_maps');pieces=rows(s,'native_draw_pieces')
 tile_text=re.search(r'const uint16_t native_draw_tiles\[\]=\{([^}]+)\};',s)[1]
 tiles=list(map(int,tile_text.split(',')))
 if len(maps)!=len(ledger['maps']):raise ValueError('Map ledger mismatch')
 # Verify each imported binary against its generating fingerprint ledger.
 for name in ['native-sprites.4bpp','native-fixed.4bpp']:
  expected=ledger['assets'][name]
  if isinstance(expected,dict):expected=expected['sha256']
  if digest(G/name)!=expected:raise ValueError('Changed source asset '+name)
 raw=unpack_tiles((G/'native-sprites.4bpp').read_bytes())
 if len(raw)!=131072*4:raise ValueError('Source sprite count')
 sprites=raw.reshape(-1,2,2,8,8).transpose(0,1,3,2,4).reshape(-1,16,16)
 output=bytearray();lookup={};outmaps=[];outchunks=[];audit=[];densemaps=[];dense_count=0
 for mid,(first,count,attributes) in enumerate(maps):
  pp=pieces[first:first+count]
  if len(pp)!=count or not count:raise ValueError('Piece span')
  left=min(p[1] for p in pp)//4*4;top=min(-p[2] for p in pp)//4*4
  right=(max(p[1]+16*p[3] for p in pp)+3)//4*4
  bottom=(max(-p[2]+16*p[4] for p in pp)+3)//4*4
  canvas=np.zeros((bottom-top,right-left),np.uint8)
  for start,x,y,w,h in pp:
   for col in range(w):
    for row in range(h):
     tid=((attributes>>4)<<16)|tiles[start+col*h+row]
     src=sprites[tid];px=x-left+16*col;py=-y-top+16*row
     dst=canvas[py:py+16,px:px+16];np.copyto(dst,src,where=src!=0)
  reduced=reviewed.drawing(ledger['maps'][mid]['source'],fonts.drawing(ledger['maps'][mid]['source'],scale(canvas)),[left*3//4,top*3//4],mid)
  if mid==5505 and (R/'art/presentation-refined/nazca-5505.npy').exists():
   refined=np.load(R/'art/presentation-refined/nazca-5505.npy')
   assert reduced.shape==(72,240) and refined.shape==(72,248) and not refined[:,240:].any()
   reduced=refined[:,:240].copy()
  local_asset=local_maps.get(ledger['maps'][mid]['source'])
  if local_asset is not None:
   original=np.array([[int(c,16) for c in row] for row in local_asset['original']],np.uint8)
   assert np.array_equal(reduced,original), ('Local artwork baseline mismatch',mid)
   reduced=np.array([[int(c,16) for c in row] for row in local_asset['rows']],np.uint8)
  for stage in player_maps:
   player_asset=stage.get(ledger['maps'][mid]['source'])
   if player_asset is None:continue
   assert player_asset['target']['map_id']==mid and player_asset['target']['origin']==[left*3//4,top*3//4]
   assert np.array_equal(reduced,hexpixels(player_asset['original'])),('Player fix baseline mismatch',mid)
   reduced=hexpixels(player_asset['rows']);assert reduced.shape==(canvas.shape[0]*3//4,canvas.shape[1]*3//4)
  base_chunks=0;base_layout=None
  for vy in range(3):
   for vx in range(3):
    target,lx=tiny_axis(reduced,left*3//4,vx,1)
    target,ty=tiny_axis(target,top*3//4,vy,0)
    firstout=len(outchunks);rebuilt=np.zeros_like(target)
    layout=chunks(target)
    if not vx and not vy:base_layout=layout
    for x,y,w,h,shape,size,pixels in layout:
     data=pack(pixels)
     if data not in lookup:lookup[data]=len(output);output.extend(data)
     outchunks.append((lookup[data],lx+x,ty+y,w,h,shape,size))
     # Asset conversion integrity: packing cannot add/drop/recolour a pixel.
     decoded=unpack_tiles(data).reshape(h//8,w//8,8,8).transpose(0,2,1,3).reshape(h,w)
     ch=min(h,target.shape[0]-y);cw=min(w,target.shape[1]-x)
     dst=rebuilt[y:y+ch,x:x+cw];src=decoded[:ch,:cw];np.copyto(dst,src,where=src!=0)
    if not np.array_equal(rebuilt,target):raise ValueError('OBJ packing map '+str(mid))
    countout=len(outchunks)-firstout;outmaps.append((firstout,countout))
    if not vx and not vy:base_chunks=countout
  # A second immutable layout trades spare OAM entries for less OBJ VRAM.
  # It is selected only after a real tile-capacity failure, at full zoom.
  dense=None;area=lambda pp:sum(q[2]*q[3]//64 for q in pp)
  if len(base_layout)>=5 and area(base_layout)>=80:
   for weight in (1.1,1.25,1.5,2):
    candidate=greedy_chunks(reduced,weight);extra=max(0,len(candidate)-len(base_layout))
    if len(candidate)>min(40,len(base_layout)+24) or area(base_layout)-area(candidate)<max(8,extra):continue
    if dense is None or area(candidate)<area(dense):dense=candidate
  firstdense=len(outchunks)
  if dense is not None:
   rebuilt=np.zeros_like(reduced)
   for x,y,w,h,shape,size,pixels in dense:
    data=pack(pixels)
    if data not in lookup:lookup[data]=len(output);output.extend(data)
    outchunks.append((lookup[data],left*3//4+x,top*3//4+y,w,h,shape,size))
    decoded=unpack_tiles(data).reshape(h//8,w//8,8,8).transpose(0,2,1,3).reshape(h,w)
    ch=min(h,reduced.shape[0]-y);cw=min(w,reduced.shape[1]-x)
    dst=rebuilt[y:y+ch,x:x+cw];np.copyto(dst,decoded[:ch,:cw],where=decoded[:ch,:cw]!=0)
   if not np.array_equal(rebuilt,reduced):raise ValueError('Dense OBJ packing map '+str(mid))
   dense_count+=1
  densemaps.append((firstdense,len(outchunks)-firstdense))
  audit.append(dict(id=mid,source=ledger['maps'][mid]['source'],source_bounds=[left,top,right,bottom],
   scaled_bounds=[left*3//4,top*3//4,right*3//4,bottom*3//4],chunks=base_chunks,
   pixel_sha256=hashlib.sha256(reduced.tobytes()).hexdigest()))
 # Complete source tile intermediate for the background compiler. Each12x12
 # tile has72 bytes of packed rows. No runtime pixel composition is enabled.
 with (G/'native-scaled-background-12x12.4bpp').open('wb') as f:
  for tile in sprites:
   a=scale(tile);f.write((a[:,::2]|(a[:,1::2]<<4)).tobytes())
 fixed=unpack_tiles((G/'native-fixed.4bpp').read_bytes());fixedout=bytearray()
 for code,tile in enumerate(fixed):
  a=np.zeros((8,8),np.uint8);a[:6,:6]=reviewed.fixed(code,fonts.fixed[code] if code in fonts.fixed else scale(tile));fixedout.extend(pack(a))
 (G/'native-scaled-fixed-6x6.4bpp').write_bytes(fixedout)
 (G/'native-scaled-objects.4bpp').write_bytes(output)
 def cr(v):return ',\n'.join('{'+','.join(map(str,row))+'}' for row in v)
 assert len(outchunks)<=65535, 'compact map chunk index overflow'
 (G/'scaled-drawing-data.c').write_text('#include "scaled-drawing-data.h"\nconst struct NativeScaledMap native_scaled_maps[]={'+cr(outmaps)+'};\nconst struct NativeScaledMap native_dense_maps[]={'+cr(densemaps)+'};\nconst struct NativeScaledChunk native_scaled_chunks[]={'+cr(outchunks)+'};\n')
 (G/'scaled-drawing-data.h').write_text('#ifndef NATIVE_SCALED_DRAWING_DATA_H\n#define NATIVE_SCALED_DRAWING_DATA_H\n#include "../src/scaled_drawing.h"\n'
  +f'enum {{NATIVE_SCALED_SOURCE_MAP_COUNT={len(maps)},NATIVE_SCALED_VARIANTS=9,NATIVE_SCALED_MAP_COUNT={len(outmaps)},NATIVE_SCALED_CHUNK_COUNT={len(outchunks)},NATIVE_SCALED_PIXEL_BYTES={len(output)}}};\n'
  +'extern const struct NativeScaledMap native_scaled_maps[NATIVE_SCALED_MAP_COUNT];\nextern const struct NativeScaledMap native_dense_maps[NATIVE_SCALED_SOURCE_MAP_COUNT];\nextern const struct NativeScaledChunk native_scaled_chunks[NATIVE_SCALED_CHUNK_COUNT];\nextern const uint8_t native_scaled_sprite_pixels[NATIVE_SCALED_PIXEL_BYTES];\n#endif\n')
 names=['native-scaled-objects.4bpp','native-scaled-background-12x12.4bpp','native-scaled-fixed-6x6.4bpp','scaled-drawing-data.c','scaled-drawing-data.h']
 report=dict(scale='nearest150percent_then50percent',approved_font_overrides=fonts.report(),reviewed_assets=reviewed.report(),source_canvas=[320,224],scaled_canvas=[240,168],viewport=[240,160],crop=dict(top=4,bottom=4,left=0,right=0),
  source_ledger_sha256=digest(G/'drawing-source.json'),source_data_sha256=digest(G/'drawing-data.c'),
  programs=len(ledger['programs']),maps=len(maps),map_variants=len(outmaps),dense_maps=dense_count,tiny_axis_factors=[1,2],chunks=len(outchunks),unique_blocks=len(lookup),object_bytes=len(output),
  source_sprite_tiles=len(sprites),fixed_tiles=len(fixed),quantized=False,conversion_roundtrip=True,
  approved_player_sha256=digest(G/'player.tiles'),approved_player_override_binding='35 exact Marco body/arm graph pairs; v18 reviewed pixels; see approved-player-source.json',
  limitations=['Raw12x12 BG and6x6 FIX intermediates feed tile-video assets; full native compositor is described in port-status.json',
   'Dynamic zoom uses GBA affine sampling and tiny-axis offline nearest variants, not exact NeoGeo shrink masks',
   'Exact original48slot palette and overflow-strip pixel selection remain; source sorting/reflection/native main are connected'],
  player_fix_stages=[dict(path=str(q.relative_to(R)),maps=len(m),approved_frames=len(f),sha256=digest(q) if q.exists() else None) for q,m,f in zip(player_paths,player_maps,player_frames)],
  files={n:dict(bytes=(G/n).stat().st_size,sha256=digest(G/n)) for n in names},map_audit=audit,gameplay_executed=False)
 (G/'scaled-drawing-source.json').write_text(json.dumps(report,indent=2)+'\n')
 # Masks consume BG fragments derived from the new scaled pixels. Build
 # them first instead of reading fragments left by a previous conversion.
 from import_tile_video import main as generate_tile_video
 generate_tile_video()
 from import_object_masks import generate
 generate()
 from build_result_affine import generate as generate_result_affine
 generate_result_affine()
 # Selected editor font/HUD edits are the final presentation override.
 if (R/"art/hud-font-edit/patch.json").exists():
  from apply_final_obj_fonts import apply as apply_hud_font_edit
  apply_hud_font_edit()
 if (R/"art/final-fonts/manifest.json").exists():
  from apply_final_fonts import apply
  apply()
 print(json.dumps({k:v for k,v in report.items() if k not in ['map_audit','files']}))
if __name__=='__main__':main()
