#!/usr/bin/env python3
"""Pack prebuilt GBA tiles without changing a pixel or palette index."""
from pathlib import Path
import struct,json,re,hashlib
import numpy as np
R=Path(__file__).resolve().parents[1];G=R/'generated';P=G/'native-background'
def main():
    tiles=(P/'tiles.4bpp').read_bytes();data=bytearray();offsets=[];references=[]
    source=np.frombuffer(tiles,np.uint8).reshape(-1,8,4)
    for at in range(0,len(source),32768):
        a=source[at:at+32768]
        q=np.stack([a[:,:4,:2],a[:,:4,2:],a[:,4:,:2],a[:,4:,2:]],axis=1).reshape(-1,8)
        blocks,ids=np.unique(q,axis=0,return_inverse=True)
        if len(blocks)>65536:raise ValueError('Native block dictionary width')
        if not np.array_equal(blocks[ids],q):raise ValueError('Native block roundtrip')
        offsets.append(len(data));data+=blocks.tobytes();references.append(ids.astype('<u2').tobytes())
    maps=(P/'maps.u24').read_bytes();pages=[];page_ids={};directory=[]
    for at in range(0,len(maps),24):
        page=maps[at:at+24].ljust(24,b'\0')
        if page not in page_ids:page_ids[page]=len(pages);pages.append(page)
        directory.append(page_ids[page])
    if len(pages)>0xffffffff:raise ValueError('Map page width')
    m=np.fromfile(P/'remap.u8',np.uint8).reshape(256,12,16)
    m=m[:,:,::2]|m[:,:,1::2]<<4
    cost=np.fromfile(P/'bank-cost.u16','<u2').reshape(256,12)
    manifest=json.loads((P/'manifest.json').read_text())
    origins=np.zeros(manifest['source_cells']+1,dtype='<u4')
    for row in manifest['rows']:
        for x in range(row['width']):
            for y in range(row['height']):origins[row['start']+x*row['height']+y]=(x*3)|((y*3)<<16)
    files={'cell-origins.u32':origins.tobytes(),'base-opacity.u16':np.fromfile(G/'native-bg-opacity.u16','<u2').reshape(-1,4)[:,0].tobytes(),'tile-blocks.4bpp':data,'tile-references.u16':b''.join(references),'tile-pages.u32':struct.pack('<'+'I'*len(offsets),*offsets),'map-pages.u32':struct.pack('<'+'I'*len(directory),*directory),'map-data.u24':b''.join(pages),'palette-remap.u8':m.tobytes(),'palette-cost.u16':cost.tobytes()}
    for n,b in files.items():(P/n).write_bytes(b)
    h=G/'native-bg-assets.h';s=h.read_text()
    s=s.replace('extern const uint8_t native_bg_native_tiles[][32],native_bg_native_banks[],native_bg_native_maps[];', 'extern const uint8_t native_bg_native_banks[],native_bg_tile_pages[],native_bg_map_data[];\nextern const uint32_t native_bg_tile_offsets[],native_bg_map_pages[],native_bg_cell_origins[];\nextern const uint16_t native_bg_tile_references[][4];')
    s=s.replace('native_bg_native_remap[256][12][16]', 'native_bg_native_remap[256][12][8]')
    s=s.replace('extern const uint16_t *const *const native_bg_source_addresses[];\n','').replace('#endif','extern const uint16_t *const *const native_bg_source_addresses[];\n#endif');h.write_text(s)
    a=R/'src/native_bg_assets.S';s=a.read_text()
    for sym in ['native_bg_cell_origins','native_bg_native_tiles','native_bg_native_maps','native_bg_native_remap','native_bg_native_cost','native_bg_tile_pages','native_bg_tile_offsets','native_bg_map_pages','native_bg_map_data','native_bg_palette_lookup','native_bg_tile_references']:
        s=re.sub(r'\.section \.rodata\.'+sym+r',.*?(?=\.section|\Z)','',s,flags=re.S)
    names={'cell-origins.u32':'native_bg_cell_origins','tile-blocks.4bpp':'native_bg_tile_pages','tile-references.u16':'native_bg_tile_references','tile-pages.u32':'native_bg_tile_offsets','map-pages.u32':'native_bg_map_pages','map-data.u24':'native_bg_map_data','palette-remap.u8':'native_bg_native_remap','palette-cost.u16':'native_bg_native_cost'}
    for n,sym in names.items():s+=f'.section .rodata.{sym},"a",%progbits\n.balign 4\n.global {sym}\n{sym}:\n.incbin "generated/native-background/{n}"\n'
    a.write_text(s)
    legacy=R/'src/tile_video_assets.S';s=legacy.read_text()
    for sym in ['native_bg_preflipped','native_bg_opacity','native_bg_base_opacity']:
        s=re.sub(r'\.section \.rodata\.'+sym+r',.*?(?=\.section|\Z)','',s,flags=re.S)
    s=s.rstrip();s+='\n.section .rodata.native_bg_base_opacity,"a",%progbits\n.balign 4\n.global native_bg_base_opacity\nnative_bg_base_opacity:\n.incbin "generated/native-background/base-opacity.u16"\n'
    legacy.write_text(s)
    report=dict(tile_pages=len(offsets),map_pages=len(pages),palette_slots=256,roundtrip_tiles=len(tiles)//32,files={n:dict(bytes=len(b),sha256=hashlib.sha256(b).hexdigest()) for n,b in files.items()})
    (P/'packing.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
if __name__=='__main__':main()
