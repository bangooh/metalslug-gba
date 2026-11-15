#!/usr/bin/env python3
"""Build cartridge-native BG tiles, region maps and fixed regional palettes.

Inputs are the complete authored source catalog, never gameplay recordings.
The separate dynamic-boundary format retains every imported source fragment;
its finite palette mappings are compiled here too (no runtime RGB search).
"""
import hashlib, json, re, struct
from pathlib import Path
import numpy as np

R=Path(__file__).resolve().parents[1]; G=R/'generated'; OUT=G/'native-background'
def body(s,n): return re.search(r'\b'+n+r'\[[^;=]*\]\s*=\s*\{(.*?)\};',s,re.S)[1]
def nums(s): return [int(x.strip(),0) for x in s.split(',') if x.strip()]
def rgb(c): return np.stack([c&31,(c>>5)&31,(c>>10)&31],axis=-1).astype(np.float32)
def colors(c):
    c=np.clip(np.rint(c),0,31).astype(np.uint16)
    return c[...,0]|c[...,1]<<5|c[...,2]<<10
def centers(points, weights, k, rounds=15):
    # Deterministic weighted farthest-point initialization, then Lloyd steps.
    if not len(points):return np.zeros((k,3),np.float32)
    cs=[points[np.argmax(weights)]];dist=np.full(len(points),1e9)
    for _ in range(1,k):
        dist=np.minimum(dist,np.sum((points-cs[-1])**2,axis=1))
        cs.append(points[np.argmax(dist*np.sqrt(weights))])
    cs=np.array(cs)
    for _ in range(rounds):
        ids=np.argmin(np.sum((points[:,None,:]-cs[None,:,:])**2,axis=2),axis=1)
        new=cs.copy()
        for j in range(k):
            m=ids==j
            if np.any(m):new[j]=np.average(points[m],axis=0,weights=weights[m])
        if np.max(abs(new-cs))<.02:break
        cs=new
    return cs

def main():
    OUT.mkdir(exist_ok=True)
    ledger=json.loads((G/'tile-video-source.json').read_text()); arrays={}
    for f,h in ledger['source_hashes'].items():
        p=G/f
        original=p.read_text().split('\n/* Native BG region addresses */')[0]
        if hashlib.sha256(original.encode()).hexdigest()!=h:raise ValueError('Changed source '+f)
        arrays[f]={m[1]:np.array(nums(m[2]),np.uint32) for m in re.finditer(r'(?:static\s+)?const uint16_t (\w+)\[[^]]*\]\s*=\s*\{([^{}]*)\};',p.read_text())}
    srcpal=np.array(nums(body((G/'drawing-data.c').read_text(),'native_source_palettes').replace('{','').replace('}','')),np.uint16).reshape(-1,16)
    source=(G/'scene-programs.c').read_text()
    pairs=np.array(nums(body(source,'scene_palette_pairs')),np.uint16).reshape(-1,2)
    commands=[(nums(m[1]),int(m[2]),int(m[3]),int(m[4])) for m in re.finditer(r'\{\{([^{}]*)\},(\d+),(\d+),(\d+)\}',body(source,'scene_commands'))]
    descriptors=[nums(x) for x in re.findall(r'\{([^{}]*)\}',body(source,'scene_descriptors'))]
    profiles=[]; image_profiles={}; program_profiles=[]
    for d in descriptors:
        state=np.full(256,65535,np.uint16)
        session=nums(body((G/'session-data.c').read_text(),'session_palettes').replace('{','').replace('}',''))
        for slot,pal,flags in np.array(session,np.uint16).reshape(-1,3):state[slot]=pal
        seen=set();at=d[0];used=[];profile=-1
        while at not in seen and at<len(commands):
            seen.add(at);v,nxt,op,plane=commands[at]
            if op==10:
                for slot,pal in pairs[v[0]:v[0]+v[1]]:
                    if pal!=65535:pal=1024+int(pal)
                    state[int(slot)&255]=pal
                # Slots0..15 are FIX. They do not select the BG profile.
                key=state[16:196].tobytes()
                profile=next((i for i,p in enumerate(profiles) if p[16:196].tobytes()==key),len(profiles))
                if profile==len(profiles):profiles.append(state.copy())
                if profile not in used:used.append(profile)
            if op==0:image_profiles.setdefault(v[0],set()).add(profile)
            at=nxt
        program_profiles.append(used)
    # M5 relink uses the same mission palette and is not a BIND command.
    image_profiles[52]={program_profiles[4][0]}
    image_rows=[r for r in ledger['records'] if r['file']=='scene-programs.c']
    if len(image_rows)!=53:raise ValueError('Image catalog')
    raw=np.fromfile(G/'native-scaled-background-12x12.4bpp',np.uint8).reshape(-1,12,6)
    pixels=np.empty((len(raw),12,12),np.uint16);pixels[:,:,::2]=raw&15;pixels[:,:,1::2]=raw>>4
    rows=[];all_images=[];cell_start=1
    for rec in ledger['records']:
        w,h=rec['width'],rec['height'];a=arrays[rec['file']][rec['cells']].reshape(w,h,2)
        im=np.zeros((h*12,w*12),np.uint16);tids=(((a[:,:,1]>>4)&15)<<16)|a[:,:,0]
        for x in range(w):
            for y in range(h):
                attr=int(a[x,y,1]);v=pixels[tids[x,y],::-1 if attr&2 else 1,::-1 if attr&1 else 1]
                im[y*12:y*12+12,x*12:x*12+12]=np.where(v,(attr>>8)*16+v,0)
        if rec['file']=='scene-programs.c':ps=sorted(image_profiles[image_rows.index(rec)])
        else:
            # Patch code is typed by mission. Generic boss patch data is
            # compiled for all six mission profiles, with no route inference.
            # Actor families are reused across missions (e.g. village objects
            # in urban scenes and fort structures in the airship). Compile
            # every authored patch for all six mission palette profiles.
            ps=sorted(set(program_profiles[p][0] for p in range(6)))
        row=dict(**rec,start=cell_start,profiles=ps,map_width=w*3-1,map_height=h*3-1)
        cell_start+=w*h;rows.append(row);all_images.append(im)
    if cell_start>65536:raise ValueError('Cell provenance width')
    # One indexed atlas for all authored regions. Palette vectors contain
    # all14 scene bindings; clustering them jointly keeps each tile's index
    # pattern invariant while regional RGB palettes remain ROM-resident.
    profile_rgb=[]
    for state in profiles:
        table=np.zeros(4096,np.uint16)
        for slot,pal in enumerate(state):
            if pal!=65535:table[slot*16:slot*16+16]=srcpal[pal]
        table[np.arange(256)*16]=0;profile_rgb.append(table)
    vectors=np.stack([rgb(table) for table in profile_rgb],axis=1).reshape(4096,-1)
    hist=np.zeros(4096,np.float64)
    for im in all_images:hist+=np.bincount(im[im!=0],minlength=4096)
    ids=np.flatnonzero(hist);weights=hist[ids];coarse=centers(vectors[ids],weights,12)
    distance=np.sum((vectors[ids,None,:]-coarse[None,:,:])**2,axis=2)/len(profiles)
    palvectors=np.zeros((12,15,vectors.shape[1]),np.float32)
    for bank in range(12):palvectors[bank]=centers(vectors[ids],weights/(1+distance[:,bank]/80)**2,15)
    for iteration in range(3):
        palvectors=np.clip(np.rint(palvectors),0,31)
        # Stable brightness ordering helps the4x4 block dictionary share
        # repeated shading patterns across banks without changing RGB.
        for bank in range(12):
            light=palvectors[bank].reshape(15,len(profiles),3)@np.array([3,6,1])
            palvectors[bank]=palvectors[bank,np.argsort(light.sum(axis=1),kind='stable')]
        d=np.sum((vectors[:,None,None,:]-palvectors[None,:,:,:])**2,axis=3)/len(profiles)
        near=np.argmin(d,axis=2).astype(np.uint8)+1;err=np.min(d,axis=2)
        if iteration==2:break
        bankhist=np.zeros((12,4096),np.float64)
        for im in all_images:
            windows=np.lib.stride_tricks.sliding_window_view(im,(8,8))[::4,::4].reshape(-1,64)
            for off in range(0,len(windows),2048):
                q=windows[off:off+2048];valid=q!=0;best=np.argmin((err[q]*valid[:,:,None]).sum(axis=1),axis=1)
                for bank in range(12):
                    z=q[(best==bank)[:,None]&valid]
                    if len(z):bankhist[bank]+=np.bincount(z,minlength=4096)
        for bank in range(12):
            selected=np.flatnonzero(bankhist[bank])
            if len(selected):palvectors[bank]=centers(vectors[selected],bankhist[bank,selected],15)
        print('joint palette refinement',iteration,flush=True)
    palettes=np.zeros((len(profiles),12,16),np.uint16)
    for p in range(len(profiles)):palettes[p,:,1:]=colors(palvectors.reshape(12,15,len(profiles),3)[:,:,p,:])
    tile_ids={bytes(33):0};tiles=[bytes(32)];banks=[0];maps=bytearray();variants=[];metrics=[]
    for rid,(row,im) in enumerate(zip(rows,all_images)):
        windows=np.lib.stride_tricks.sliding_window_view(im,(8,8))[::4,::4].reshape(-1,64)
        row['variant_first']=len(variants);start=len(maps)//3;errors_by_profile={p:0. for p in row['profiles']};opaque=0
        for off in range(0,len(windows),2048):
            q=windows[off:off+2048];valid=q!=0;e=(err[q]*valid[:,:,None]).sum(axis=1);best=np.argmin(e,axis=1)
            indexes=near[q,best[:,None]];indexes[~valid]=0
            assert np.array_equal(indexes!=0,valid)
            packed=indexes[:,::2]|indexes[:,1::2]<<4
            for pix,bank in zip(packed,best):
                key=bytes([int(bank)])+pix.tobytes()
                if not pix.any():key=bytes(33)
                if key not in tile_ids:tile_ids[key]=len(tiles);tiles.append(key[1:]);banks.append(key[0])
                maps+=int(tile_ids[key]).to_bytes(3,'little')
            for p in row['profiles']:
                actual=palettes[p,best[:,None],indexes];expected=profile_rgb[p][q]
                errors_by_profile[p]+=float((np.sum((rgb(actual)-rgb(expected))**2,axis=2)*valid).sum())
            opaque+=int(valid.sum())
        #65535 is a format-declared universal map, not missing-data fallback.
        variants.append([65535,start]);row['variants']=1
        for p,error in errors_by_profile.items():metrics.append(dict(region=rid,profile=p,opaque=opaque,squared_rgb555_error=error))
    remap=near.reshape(256,16,12).transpose(0,2,1).copy();remap[:,:,0]=0
    bankcost=np.rint(err.reshape(256,16,12)[:,1:,:].mean(axis=1)).astype('<u2')
    files={'tiles.4bpp':b''.join(tiles),'tile-banks.u8':bytes(banks),'maps.u24':maps,
        'palettes.rgb555':palettes.astype('<u2').tobytes(),'remap.u8':remap.tobytes(),
        'bank-cost.u16':bankcost.tobytes(),'profiles.u16':np.stack(profiles).astype('<u2').tobytes()}
    cell_regions=np.zeros(cell_start,np.uint16)
    for rid,row in enumerate(rows):cell_regions[row['start']:row['start']+row['width']*row['height']]=rid
    files['cell-regions.u16']=cell_regions.astype('<u2').tobytes()
    for n,data in files.items():(OUT/n).write_bytes(data)
    header='''#ifndef NATIVE_BG_ASSETS_H
#define NATIVE_BG_ASSETS_H
#include <stdint.h>
struct NativeBgRegion {const uint16_t*cells;uint32_t map_first;uint16_t start,width,height;uint8_t variants,pad;};
struct NativeBgVariant {uint32_t offset;uint16_t profile,pad;};
'''
    header+=f'enum {{NATIVE_BG_REGION_COUNT={len(rows)},NATIVE_BG_PROFILE_COUNT={len(profiles)},NATIVE_BG_NATIVE_TILES={len(tiles)},NATIVE_BG_SOURCE_CELLS={cell_start}}};\n'
    header+='extern const struct NativeBgRegion native_bg_regions[NATIVE_BG_REGION_COUNT];\nextern const struct NativeBgVariant native_bg_variants[];\n'
    header+='extern const uint8_t native_bg_native_tiles[][32],native_bg_native_banks[],native_bg_native_maps[];\nextern const uint16_t native_bg_native_palettes[][12][16],native_bg_profiles[][256],native_bg_cell_regions[];\nextern const uint8_t native_bg_native_remap[256][12][16];\nextern const uint16_t native_bg_native_cost[256][12];\n#endif\n'
    (G/'native-bg-assets.h').write_text(header)
    # Refer to existing exported typed records. Source arrays stay private
    # and every source-table hash remains unchanged.
    c='#include "native-bg-assets.h"\n'
    for filename in ledger['source_hashes']:c+='#include "'+filename.replace('.c','.h')+'"\n'
    occurrences={};addresses=[]
    for row in rows:
        key=(row['file'],row['table']);index=occurrences.get(key,0);occurrences[key]=index+1
        source=(G/row['file']).read_text()
        match=re.search(r'const struct \w+ '+row['table']+r'(\[[^]]*\])?\s*=',source)
        if not match:raise ValueError('Missing public region '+str(key))
        target=row['table']+(f'[{index}]' if match[1] else '')
        addresses.append('(const uint16_t*const*)&'+target)
    c+='const uint16_t *const *const native_bg_source_addresses[]={'+','.join(addresses)+'};\n'
    c+='const struct NativeBgRegion native_bg_regions[]={\n'+''.join('{0,%d,%d,%d,%d,%d,0},\n'%(r['variant_first'],r['start'],r['width'],r['height'],r['variants']) for r in rows)+'};\n'
    c+='const struct NativeBgVariant native_bg_variants[]={'+','.join('{%d,%d,0}'%(off,p) for p,off in variants)+'};\n'
    (G/'native-bg-assets.c').write_text(c)
    asm=''
    names={'tiles.4bpp':'native_bg_native_tiles','tile-banks.u8':'native_bg_native_banks','maps.u24':'native_bg_native_maps','palettes.rgb555':'native_bg_native_palettes','remap.u8':'native_bg_native_remap','bank-cost.u16':'native_bg_native_cost','profiles.u16':'native_bg_profiles','cell-regions.u16':'native_bg_cell_regions'}
    for n,sym in names.items():asm+=f'.section .rodata.{sym},"a",%progbits\n.balign 4\n.global {sym}\n{sym}:\n.incbin "generated/native-background/{n}"\n'
    (R/'src/native_bg_assets.S').write_text(asm)
    report=dict(regions=len(rows),source_cells=cell_start-1,profiles=len(profiles),native_tiles=len(tiles),map_entries=len(maps)//3,program_profiles=program_profiles,rows=rows,metrics=metrics,
      files={n:dict(bytes=len(b),sha256=hashlib.sha256(b).hexdigest()) for n,b in files.items()},source_hashes=ledger['source_hashes'],format='joint-index-regional-palettes',quantized=True,alpha_preserved=True,scale='75%, 4+4 crop unchanged',runtime_rgb_search=False)
    (OUT/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ('rows','metrics','source_hashes')}))
if __name__=='__main__':main()
