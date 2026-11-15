"""Exhaustive storage roundtrip checks, independent of gameplay validation."""
import json,re
import numpy as np
from compile_m1_native_maps import ROOT as R,G,body,numbers

reports=[]
for m in range(3,7):
    p=G/f'm{m}-cartridge'
    old=[numbers(x) for x in re.findall(r'\{([^{}]+)\}',body((p/'assets-unpacked.c').read_text(),f'm{m}_images'))]
    new=[numbers(x) for x in re.findall(r'\{([^{}]+)\}',body((G/f'm{m}-assets.c').read_text(),f'm{m}_images'))]
    source=np.fromfile(p/'maps.bin','<u2');packed=np.fromfile(p/'maps-packed.bin','<u2');rows=np.fromfile(p/'map-rows.bin','<u4')
    checked=0
    for a,b in zip(old,new,strict=True):
        off,w,h,x,y=a;start,ww,hh,xx,yy=b
        assert (w,h,x,y)==(ww,hh,xx,yy)
        expected=source[off:off+w*h].reshape(h,w)
        actual=np.stack([packed[r:r+w] for r in rows[start:start+h]])
        assert np.array_equal(expected,actual),(m,off)
        checked+=w*h
    opacity=np.fromfile(p/'opacity.bin',np.uint8).reshape(-1,8)
    masks=np.fromfile(p/'opacity-packed.bin',np.uint8).reshape(-1,8)
    ids=np.fromfile(p/'opacity-ids.bin','<u2')
    assert np.array_equal(opacity,masks[ids])
    logical=np.fromfile(p/'tiles.bin',np.uint8).reshape(-1,64)
    tile_ids=np.fromfile(p/'ids4.bin','<u2');attrs=np.fromfile(p/'attrs4.bin','<u2')
    physical=np.fromfile(p/'tiles4.bin',np.uint8).reshape(-1,32)
    assert len(logical)==len(tile_ids)==len(attrs)==len(opacity)
    assert tile_ids.max()<len(physical) and (attrs>>12).max()<12
    semantic=np.fromfile(p/'semantic.bin','<u2');bank_semantic=np.fromfile(p/'semantic4.bin','<u2').reshape(12,16)
    # Quantization may change RGB; transparent coverage and palette ownership stay exact.
    for i,(ident,attr) in enumerate(zip(tile_ids,attrs)):
        raw=physical[ident];v=np.empty(64,np.uint8);v[::2]=raw&15;v[1::2]=raw>>4;v=v.reshape(8,8)
        if attr&0x400:v=v[:,::-1]
        if attr&0x800:v=v[::-1]
        assert np.array_equal(v.ravel()!=0,logical[i]!=0),(m,i)
        mask=logical[i]!=0
        assert np.array_equal(semantic[logical[i][mask]]//16,bank_semantic[attr>>12,v.ravel()[mask]]//16),(m,i,"animated palette identity")
    reports.append(dict(mission=m,images=len(old),map_cells_exact=checked,opacity_masks_exact=len(opacity),transparent_tiles_exact=len(logical),animated_palette_identity_exact=True,rgb_quantization=json.loads((p/'report4.json').read_text())))
out=R/'build/campaign134/storage-validation.json';out.write_text(json.dumps(reports,indent=2)+'\n')
print(json.dumps([{k:v for k,v in r.items() if k!='rgb_quantization'} for r in reports],indent=2))

# Retained legacy frontend/ending maps must decode to their original tile bytes
# and palette-bank choices, despite global tile-ID renumbering.
p=G/'native-background';q=G/'native-background-full'
def maps24(path):
    a=np.fromfile(path,np.uint8).reshape(-1,3).astype(np.uint32)
    return a[:,0]|(a[:,1]<<8)|(a[:,2]<<16)
oldmap,newmap=maps24(q/'maps.u24'),maps24(p/'maps.u24')
oldtiles=np.fromfile(q/'tiles.4bpp',np.uint8).reshape(-1,32)
newtiles=np.fromfile(p/'tiles.4bpp',np.uint8).reshape(-1,32)
oldbanks=np.fromfile(q/'tile-banks.u8',np.uint8);newbanks=np.fromfile(p/'tile-banks.u8',np.uint8)
oldtext=(q/'native-bg-assets.c').read_text();newtext=(G/'native-bg-assets.c').read_text()
oldvar=[numbers(x) for x in re.findall(r'\{([^{}]+)\}',body(oldtext,'native_bg_variants'))]
newvar=[numbers(x) for x in re.findall(r'\{([^{}]+)\}',body(newtext,'native_bg_variants'))]
regions=[x.split(',') for x in re.findall(r'\{([^{}]+)\}',body(newtext,'native_bg_regions'))]
rows=json.loads((p/'manifest.json').read_text())['rows'];cells=variants=0
for region,record in zip(regions,rows,strict=True):
    first,count=int(region[1]),int(region[5]);length=record['map_width']*record['map_height']
    for v in range(first,first+count):
        a=oldmap[oldvar[v][0]:oldvar[v][0]+length]
        b=newmap[newvar[v][0]:newvar[v][0]+length]
        assert np.array_equal(oldtiles[a],newtiles[b])
        assert np.array_equal(oldbanks[a],newbanks[b])
        cells+=length;variants+=1
legacy=dict(retained_variants=variants,retained_cells_exact=cells,tile_bytes_and_palette_banks_exact=True)
(R/'build/campaign134/legacy-storage-validation.json').write_text(json.dumps(legacy,indent=2)+'\n');print(legacy)
