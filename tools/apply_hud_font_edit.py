"""Apply only the selected reviewed font/HUD/item edits, preserving unrelated packed assets."""
from pathlib import Path
import json,re,hashlib,struct
import numpy as np
from import_scaled_drawings import rows,pack,unpack_tiles,tiny_axis,chunks as partition
from approved_font_overrides import ApprovedFonts
R=Path(__file__).resolve().parents[1];G=R/'generated'
def pix(v):return np.array([[int(c,16) for c in row]for row in v],np.uint8)
def decode(blob,cs,entry,l,t,shape):
 out=np.zeros(shape,np.uint8);first,n=entry
 for off,x,y,w,h,*_ in cs[first:first+n]:
  tile=unpack_tiles(blob[off:off+w*h//2]).reshape(h//8,w//8,8,8).transpose(0,2,1,3).reshape(h,w);x-=l;y-=t;hh=min(h,shape[0]-y);ww=min(w,shape[1]-x)
  np.copyto(out[y:y+hh,x:x+ww],tile[:hh,:ww],where=tile[:hh,:ww]!=0)
 return out
def generate(extra_assets=None,only_ids=None,final_assets=False):
 assets=json.loads((R/'art/hud-font-edit/patch.json').read_text())['assets']+(extra_assets or [])
 if final_assets:assets=extra_assets or []
 if (R/'art/final-fonts/manifest.json').exists():assets=[a for a in assets if a['target']['kind']!='fixed']
 if only_ids is not None:assets=[a for a in assets if a['id'] in only_ids]
 s=(G/'scaled-drawing-data.c').read_text();maps=[list(v)for v in rows(s,'native_scaled_maps')];dense=[list(v)for v in rows(s,'native_dense_maps')];cs=[list(v)for v in rows(s,'native_scaled_chunks')]
 blob=bytearray((G/'native-scaled-objects.4bpp').read_bytes());initial=len(blob);report=json.loads((G/'scaled-drawing-source.json').read_text());audit={m['source']:m for m in report['map_audit']};fonts=ApprovedFonts();missions={ch:src for src,ch in fonts.maps.items()};fixed=bytearray((G/'native-scaled-fixed-6x6.4bpp').read_bytes());frags=bytearray((G/'native-fix-fragments.u16').read_bytes());changed=[]
 lookup={bytes(blob[o:o+w*h//2]):o for o,x,y,w,h,*_ in cs}
 for a in assets:
  k=a['target']['kind'];new=pix(a['after_padded']);old=pix(a['before_current'])
  if k=='fixed':
   code=a['target']['code'];actual=unpack_tiles(fixed[code*32:code*32+32])[0,:6,:6];assert np.array_equal(actual,old)or np.array_equal(actual,new),a['id']
   pad=np.zeros((8,8),np.uint8);pad[:6,:6]=new;fixed[code*32:code*32+32]=pack(pad)
   words=[sum(int(c)<<(4*i)for i,c in enumerate(new[y*2:y*2+2,x*2:x*2+2].flatten()))for y in range(3)for x in range(3)];frags[code*18:code*18+18]=struct.pack('<9H',*words)
  else:
   src=missions[a['target']['character']] if k=='mission' else fonts.ending['source'] if k=='fin' else a['target']['source'];m=audit[src];mid=m['id']
   if final_assets:m['scaled_bounds']=a['bounds']
   l,t,r,b=m['scaled_bounds'];actual=np.zeros_like(new) if final_assets else decode(blob,cs,maps[mid*9],l,t,new.shape)
   if final_assets or not np.array_equal(actual,new):
    assert final_assets or np.array_equal(actual,old) or np.array_equal(actual,pix(a["original"])),a['id']
    for vy in range(3):
     for vx in range(3):
      im,lx=tiny_axis(new,l,vx,1);im,ty=tiny_axis(im,t,vy,0);first=len(cs)
      for x,y,w,h,shape,size,tile in partition(im):
       raw=pack(tile)
       if raw not in lookup:lookup[raw]=len(blob);blob.extend(raw)
       cs.append([lookup[raw],lx+x,ty+y,w,h,shape,size])
      maps[mid*9+vy*3+vx]=[first,len(cs)-first]
      assert np.array_equal(decode(blob,cs,maps[mid*9+vy*3+vx],lx,ty,im.shape),im)
    dense[mid]=[0,0];m['chunks']=maps[mid*9][1];m['pixel_sha256']=hashlib.sha256(new.tobytes()).hexdigest()
  changed.append({'id':a['id'],'pixels_changed':int(np.count_nonzero(old!=new))})
 # Reclaim orphaned layouts from prior private edits; deduplicate live blocks.
 # This changes storage offsets only, preserving every referenced chunk byte.
 compact=bytearray();live=[];indices={};blocks={}
 for table in (maps,dense):
  for slot,entry in enumerate(table):
   first,count=entry;start=len(live)
   for old_index in range(first,first+count):
    off,x,y,w,h,shape,size=cs[old_index];raw=bytes(blob[off:off+w*h//2])
    if raw not in blocks:blocks[raw]=len(compact);compact.extend(raw)
    live.append([blocks[raw],x,y,w,h,shape,size])
   table[slot]=[start,count]
 blob=compact;cs=live
 for name,values in [('native_scaled_maps',maps),('native_dense_maps',dense),('native_scaled_chunks',cs)]:
  m=re.search(r'(const struct \w+ '+name+r'\[\]=\{)(.*?)(\};)',s,re.S);s=s[:m.start(2)]+',\n'.join('{'+','.join(map(str,v))+'}'for v in values)+s[m.end(2):]
 (G/'scaled-drawing-data.c').write_text(s);(G/'native-scaled-objects.4bpp').write_bytes(blob);(G/'native-scaled-fixed-6x6.4bpp').write_bytes(fixed);(G/'native-fix-fragments.u16').write_bytes(frags)
 p=G/'scaled-drawing-data.h';p.write_text(re.sub(r'NATIVE_SCALED_PIXEL_BYTES=\d+',f'NATIVE_SCALED_PIXEL_BYTES={len(blob)}',re.sub(r'NATIVE_SCALED_CHUNK_COUNT=\d+',f'NATIVE_SCALED_CHUNK_COUNT={len(cs)}',p.read_text())))
 words=struct.unpack('<36864H',frags);mask=[sum(bool(any(words[i*9:(i+1)*9]))<<(i%32)for i in range(b*32,b*32+32))for b in range(128)];(G/'native-fix-visible.inc').write_text(','.join(hex(x)+'u'for x in mask)+'\n')
 report['object_bytes']=len(blob);report['chunks']=len(cs);report['hud_font_edit']=changed
 for name in report['files']:
  p=G/name
  if p.exists():report['files'][name]={'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
 (G/'scaled-drawing-source.json').write_text(json.dumps(report,indent=2)+'\n')
 from import_object_masks import generate as masks
 masks()
 from build_mission_spacing import generate as spacing
 spacing()
 print(json.dumps({'assets':len(changed),'changed_pixels':sum(a['pixels_changed']for a in changed),'appended_bytes':len(blob)-initial}))
if __name__=='__main__':
 import argparse
 p=argparse.ArgumentParser();p.add_argument('--extra',type=Path);args=p.parse_args()
 generate(json.loads(args.extra.read_text())['assets'] if args.extra else None)
