#!/usr/bin/env python3
"""Full-map offline M1 compiler, with exact screen-local GBA palette reuse.

Camera layers are flattened at their descriptor anchors, foreground last. This
is an explicit fixed-parallax presentation adaptation, not the original camera
program. Destruction patches remain a separately tracked requirement.
"""
import argparse, hashlib, json, struct
from pathlib import Path
import numpy as np
from PIL import Image
from import_source import PROM_SHA

R=Path(__file__).resolve().parents[1]
def scale(im):return im.resize((im.width*3//2,im.height*3//2),Image.Resampling.NEAREST).resize((im.width*3//4,im.height*3//4),Image.Resampling.NEAREST)

def palette_schedule(values):
    """Conflict graph: colours that can share a 256px VRAM window cannot alias.
    The graph includes the ENTIRE map height, not a player's recorded route.
    Palettes are fixed ROM tables; pixels/tiles never change their index.
    """
    height,width=values.shape;cols=width//8
    colours=sorted(int(v) for v in np.unique(values));neighbours={v:set() for v in colours}
    column_colours=[set(map(int,np.unique(values[:,x*8:x*8+8]))) for x in range(cols)]
    windows=[]
    for left in range(cols):
        used=set.union(*column_colours[left:min(left+32,cols)]);used.add(0);windows.append(used)
        for c in used:neighbours[c].update(used-{c})
    assigned={0:0};pending=set(colours)-{0}
    while pending:
        c=max(pending,key=lambda c:(len({assigned[n] for n in neighbours[c] if n in assigned}),len(neighbours[c]),-c))
        forbidden={assigned[n] for n in neighbours[c] if n in assigned};slot=1
        while slot in forbidden:slot+=1
        assert slot<240, 'Exact palette capacity exceeded; conversion needs an explicit new policy'
        assigned[c]=slot;pending.remove(c)
    lut=np.zeros(32768,np.uint8)
    for c,slot in assigned.items():lut[c]=slot
    indices=lut[values];palettes=np.zeros((cols,256),'<u2')
    for left,used in enumerate(windows):
        for c in used:palettes[left,assigned[c]]=c
        # Exhaustive, independent reconstruction; all rows and every column.
        right=min((left+32)*8,width)
        assert np.array_equal(palettes[left][indices[:,left*8:right]],values[:,left*8:right])
    return indices,palettes,dict(world_colours=len(colours),palette_slots=max(assigned.values())+1,
       maximum_window_colours=max(map(len,windows)),palette_window_pixels=256,verified_windows=cols,
       palette_exact=True,quantization=False,runtime_pixel_conversion=False)

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--source',type=Path,required=True);a=ap.parse_args()
    p=(a.source/'build/mslug_prom.bin').read_bytes();assert hashlib.sha256(p).hexdigest()==PROM_SHA
    folder=a.source/'build/assets/levels';meta=json.loads((folder/'levels.json').read_text())
    layers=meta['layers']['00_Mission1'];width,height=4224,416
    im=Image.new('RGBA',(width,height),(0,0,0,255))
    for l in sorted(layers,key=lambda l:-int(l['camera'][-1])):
        assert not l['missing_palette_slots'],l
        im.alpha_composite(Image.open(folder/l['file']).convert('RGBA'),tuple(l['anchor']))
    scaled=scale(im);rgba=np.asarray(scaled).astype(np.uint16)
    values=(rgba[:,:,0]>>3)|((rgba[:,:,1]>>3)<<5)|((rgba[:,:,2]>>3)<<10)
    indices,palettes,report=palette_schedule(values)
    w,h=scaled.size;tiles=[bytes(64)];lookup={tiles[0]:0};tilemap=[]
    for y in range(0,h,8):
        for x in range(0,w,8):
            tile=indices[y:y+8,x:x+8].tobytes()
            if tile not in lookup:lookup[tile]=len(tiles);tiles.append(tile)
            tilemap.append(lookup[tile])
    g=R/'generated';(g/'background.tiles').write_bytes(b''.join(tiles))
    (g/'background.map').write_bytes(struct.pack('<'+'H'*len(tilemap),*tilemap))
    (g/'background.pal').write_bytes(palettes.tobytes())
    # All four immutable cam0 collision descriptors. Explicit empty cells
    # outside descriptors do not fabricate ground over original holes.
    cells=bytearray(width//8*(height//8));collision=[]
    for l in layers:
        if not l['collision']:continue
        assert l['camera']=='cam0'
        address=int(l['collision'],16);off=address-0x100000 if address>=0x200000 else address
        x0,y0=[v//8 for v in l['anchor']];cw,ch=l['width']*2,l['height']*2
        for x in range(cw):
            for y in range(ch):cells[(y0+y)*(width//8)+x0+x]=p[off+(x//2)*ch*2+y*2+(x&1)]
        collision.append(dict(address=l['collision'],anchor=l['anchor'],cells=[cw,ch]))
    (g/'terrain.cells').write_bytes(cells)
    hfile=g/'assets.h';s=hfile.read_text()
    import re
    for key,val in [('MAP_W',w//8),('MAP_H',h//8),('WORLD_W',width),('WORLD_H',height),('CELL_W',width//8),('CELL_H',height//8)]:
        s=re.sub(r'#define '+key+r' \d+',f'#define {key} {val}',s)
    hfile.write_text(s)
    report.update(scope='All 15 M1 layer descriptors at fixed anchors; no dynamic destruction or original camera script yet',
        source_pixels=[width,height],gba_pixels=[w,h],tiles=len(tiles),layers=layers,collision=collision,
        parallax='Flattened offline at source anchors; not source camera equivalence',recorded_route_dependency=False,
        source_prom_sha256=PROM_SHA)
    (g/'m1-world.json').write_text(json.dumps(report,indent=2)+'\n')
    scaled.save(R/'build/background-reference.png')
    values.astype('<u2').tofile(R/'build/background-reference.rgb555')
    print(json.dumps({k:report[k] for k in ('world_colours','palette_slots','maximum_window_colours','verified_windows','tiles')}))
if __name__=='__main__':main()
