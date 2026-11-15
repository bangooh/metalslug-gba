#!/usr/bin/env python3
"""Compile every authored BG image/patch into exact 8x8 source-colour plans.

These are OFFLINE plans, not a claim that the ROM loads them. Source colour
identities survive fades. Cross-patch seams, independently moving columns,
and automatic animation require runtime fallback or additional region plans.
The active renderer freezes automatic BG animation, so authored tile values
are used. No play trace, quantizer or captured viewport supplies assets.
"""
import argparse, hashlib, json, re, struct, zlib
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1]; G=R/'generated'; EMPTY=0x0fffffff

def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def compile_regions(out):
    out.mkdir(parents=True,exist_ok=True)
    ledger=json.loads((G/'tile-video-source.json').read_text())
    arrays={}
    for filename,expected in ledger['source_hashes'].items():
        path=G/filename
        if digest(path)!=expected: raise ValueError('Changed source table '+filename)
        arrays[filename]={m[1]:np.array([int(x.strip(),0) for x in m[2].split(',') if x.strip()],dtype=np.uint32)
            for m in re.finditer(r'(?:static\s+)?const uint16_t (\w+)\[[^]]*\]\s*=\s*\{([^{}]*)\};',path.read_text())}
    lookup=np.fromfile(G/'native-bg-lookup.u16',dtype='<u2')
    fragments=np.fromfile(G/'native-bg-fragments.u32',dtype='<u4').reshape(-1,3,3)
    source=np.fromfile(G/'native-scaled-background-12x12.4bpp',dtype=np.uint8).reshape(-1,12,6)
    pixels=np.empty((len(source),12,12),np.uint16);pixels[:,:,::2]=source&15;pixels[:,:,1::2]=source>>4
    raw=np.fromfile(G/'native-bg-blocks.4bpp',dtype=np.uint8).reshape(-1,4,2)
    blocks=np.empty((len(raw),4,4),np.uint16);blocks[:,:,::2]=raw&15;blocks[:,:,1::2]=raw>>4
    opacity=np.any(blocks,axis=(1,2))
    rows=[];allkeys=[];allimages=[];verified=0;offset=0
    for rec in ledger['records']:
        w,h=rec['width'],rec['height'];cells=arrays[rec['file']][rec['cells']].reshape(w,h,2)
        attr=cells[:,:,1];tid=(((attr>>4)&15)<<16)|cells[:,:,0];index=lookup[tid]
        if np.any(index>=len(fragments)):raise ValueError('Unimported tile')
        grid=np.empty((h*3,w*3),np.uint32)
        oracle=np.empty((h*12,w*12),np.uint16)
        for bx in range(3):
            for by in range(3):
                ix=np.where(attr&1,2-bx,bx);iy=np.where(attr&2,2-by,by)
                block=fragments[index,iy,ix]
                grid[by::3,bx::3]=np.where(opacity[block],block<<10|((attr>>8)<<2)|(attr&3),EMPTY).T
        for x in range(w):
            for y in range(h):
                a=int(attr[x,y]);part=pixels[tid[x,y],::-1 if a&2 else 1,::-1 if a&1 else 1]
                oracle[y*12:y*12+12,x*12:x*12+12]=np.where(part,(a>>8)*16+part,0)
        keys=np.stack([grid[:-1,:-1],grid[:-1,1:],grid[1:,:-1],grid[1:,1:]],axis=-1).reshape(-1,4)
        images=np.lib.stride_tricks.sliding_window_view(oracle,(8,8))[::4,::4].reshape(-1,64).copy()
        # Independent whole12x12 source decode must agree with fragment keys.
        rebuilt=np.zeros((len(keys),8,8),np.uint16)
        for q in range(4):
            valid=keys[:,q]!=EMPTY;k=keys[valid,q];v=blocks[k>>10].copy()
            for flip in (1,2,3):
                at=(k&3)==flip;v[at]=v[at,::-1 if flip&2 else 1,::-1 if flip&1 else 1]
            v=np.where(v,((k>>2)&255)[:,None,None]*16+v,0)
            rebuilt[valid,(q//2)*4:(q//2+1)*4,(q%2)*4:(q%2+1)*4]=v
        if not np.array_equal(rebuilt.reshape(-1,64),images):raise ValueError('Source/fragment mismatch '+rec['cells'])
        verified+=images.size
        rows.append(dict(**rec,window_width=w*3-1,window_height=h*3-1,map_offset=offset,windows=len(keys)))
        offset+=len(keys);allkeys.append(keys);allimages.append(images)
    keys=np.concatenate(allkeys);images=np.concatenate(allimages)
    unique_keys,first,key_ids=np.unique(keys,axis=0,return_index=True,return_inverse=True)
    # Different source keys can have identical pixels, including transparency.
    unique_pixels,pixel_ids=np.unique(images[first],axis=0,return_inverse=True)
    plans=[];counts=[];payload=bytearray();starts=[0]
    for im in unique_pixels:
        palette=np.unique(im);palette=palette[palette!=0]
        ids=np.where(im,np.searchsorted(palette,im)+1,0).astype(np.uint8)
        count=len(palette);plans.append(frozenset(map(int,palette)));depth=4 if count<=15 else 8
        packed=(ids[::2]|ids[1::2]<<4).tobytes() if depth==4 else ids.tobytes()
        decoded=np.empty(64,np.uint8)
        if depth==4:
            b=np.frombuffer(packed,np.uint8);decoded[::2]=b&15;decoded[1::2]=b>>4
        else:decoded[:]=np.frombuffer(packed,np.uint8)
        restored=np.concatenate([np.array([0],np.uint16),palette])[decoded]
        if not np.array_equal(restored,im):raise ValueError('Local palette round trip')
        payload+=struct.pack('<BB',depth,count)+palette.astype('<u2').tobytes()+packed
        if len(payload)&1:payload+=b'\0'
        starts.append(len(payload));counts.append(count)
    counts=np.array(counts);map_ids=pixel_ids[key_ids].astype('<u4')
    palette_plans=[]
    for rec in rows:
        ids=map_ids[rec['map_offset']:rec['map_offset']+rec['windows']];c=counts[ids]
        unique=sorted(map(int,np.unique(ids)),key=lambda i:(-int(counts[i]),i))
        banks=[];bindings=[]
        for tile in unique:
            if counts[tile]>15:bindings.append([tile,65535]);continue
            colors=plans[tile];best=-1;score=16
            for b,bank in enumerate(banks):
                missing=len(colors-bank)
                if len(bank)+missing<=15 and missing<score:
                    best=b;score=missing
                    if not missing:break
            if best<0:best=len(banks);banks.append(set())
            banks[best].update(colors);bindings.append([tile,best])
        for tile,b in bindings:
            if b!=65535:assert plans[tile]<=banks[b] and len(banks[b])<=15
        rec.update(unique_tiles=len(unique),source_identity_over15=int(np.sum(c>15)),max_source_colors=int(c.max()),
                   greedy_source_palette_banks=len(banks),standalone_fits12=len(banks)<=12 and not np.any(c>15))
        palette_plans.append(dict(file=rec['file'],cells=rec['cells'],banks=[sorted(x) for x in banks],tile_banks=bindings))
    data=bytearray(struct.pack('<8sIIII',b'BGPLAN1\0',len(unique_keys),len(unique_pixels),len(map_ids),len(payload)))
    data+=unique_keys.astype('<u4').tobytes()+pixel_ids.astype('<u4').tobytes()+np.array(starts,dtype='<u4').tobytes()+map_ids.tobytes()+payload
    compressed=zlib.compress(data,9);planfile=out/'native-bg-regions.plan.z';planfile.write_bytes(compressed)
    assert zlib.decompress(compressed)==data
    palette_bytes=json.dumps(palette_plans,separators=(',',':')).encode()
    palette_file=out/'native-bg-region-palettes.json.z';palette_file.write_bytes(zlib.compress(palette_bytes,9))
    report=dict(standalone_regions_fitting12=sum(r['standalone_fits12'] for r in rows),
        palette_plan_compressed_bytes=palette_file.stat().st_size,palette_plan_sha256=digest(palette_file),
        format='BGPLAN1 offline only; little endian header, key[4], key_to_tile, tile_offsets, region_maps, variable depth/count/source-palette/pixels',
        inventory=ledger['inventory'],regions=len(rows),windows=len(keys),unique_keys=len(unique_keys),unique_pixel_tiles=len(unique_pixels),
        source_identity_4bpp_tiles=int(np.sum(counts<=15)),source_identity_over15_tiles=int(np.sum(counts>15)),max_source_colors=int(counts.max()),
        original_pixels_verified=verified,roundtrip_tiles=len(unique_pixels),uncompressed_bytes=len(data),compressed_bytes=len(compressed),
        compressed_sha256=digest(planfile),rom_linked=False,quantized=False,source_hashes=ledger['source_hashes'],
        input_hashes={n:digest(G/n) for n in ['native-bg-lookup.u16','native-bg-fragments.u32','native-bg-blocks.4bpp','native-scaled-background-12x12.4bpp']},
        limitations=['Source-identity count is conservative: equal RGB from different slots may share now but diverge on a fade.',
        'Greedy bank count is an upper bound, not a proof of infeasibility above12. A standalone region fit does not establish simultaneous layers or transitions fit12 banks.',
        'All authored source tables are compiled; arbitrary inter-patch seams, moving-column junctions and additional animation phases are not baked.',
        'zlib is an offline container, not a GBA loading/decompression implementation. Runtime integration and palette placement remain pending.'],records=rows)
    (out/'native-bg-regions.json').write_text(json.dumps(report,indent=2)+'\n')
    return {k:v for k,v in report.items() if k not in ('records','source_hashes','input_hashes')}
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=G/'background-regions');a=p.parse_args();print(json.dumps(compile_regions(a.output)))
