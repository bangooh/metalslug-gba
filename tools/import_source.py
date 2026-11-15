#!/usr/bin/env python3
"""Offline conversion from the verified source checkout; never from a play trace."""
import argparse, hashlib, json, re, struct
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
COMMIT = '574a517547f05bc8c1539fab1b7d566035543496'
PROM_SHA = '1b8eee0891b6f8c837348e60ea2923ae229e0fb603d34ae62ae06131332f0d3d'

def rgb555(c):
    return (c[0] >> 3) | ((c[1] >> 3) << 5) | ((c[2] >> 3) << 10)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--source', type=Path, required=True)
    ap.add_argument('--approved-art', type=Path, required=True)
    a = ap.parse_args()
    p = (a.source/'build/mslug_prom.bin').read_bytes()
    assert hashlib.sha256(p).hexdigest() == PROM_SHA
    g = ROOT/'generated'; g.mkdir(exist_ok=True)
    # First complete M1 map descriptor, before later scene replacements.
    meta = json.loads((a.source/'build/assets/levels/levels.json').read_text())
    layer = meta['layers']['00_Mission1'][0]
    assert (layer['tiles'], layer['collision'], layer['anchor'], layer['width'], layer['height']) == ('0A18A8','200000',[0,128],120,18)
    # Keep a world canvas; no sampled camera track is baked into this image.
    width, height = 1920, 416
    rgba = Image.open(a.source/'build/assets/levels'/layer['file']).convert('RGBA')
    image = Image.new('RGB', (width,height), (0,0,0))
    image.paste(rgba, (0,128), rgba)
    scaled = image.resize((width*3//2,height*3//2),Image.Resampling.NEAREST).resize((width*3//4,height*3//4),Image.Resampling.NEAREST)
    # Exact native RGB555 palette. Index 0 is background black, no quantizer.
    values = [rgb555(c) for c in scaled.getdata()]
    palette = [0] + sorted(set(values)-{0})
    assert len(palette) <= 256, 'Palette exceeds exact 8bpp capacity; do not silently quantize'
    ids = {c:i for i,c in enumerate(palette)}
    indexed = bytes(ids[c] for c in values)
    w,h = scaled.size
    tiles = [bytes(64)]; lookup = {tiles[0]:0}; tilemap=[]
    for ty in range(h//8):
        for tx in range(w//8):
            tile=b''.join(indexed[(ty*8+y)*w+tx*8:(ty*8+y)*w+tx*8+8] for y in range(8))
            if tile not in lookup: lookup[tile]=len(tiles); tiles.append(tile)
            tilemap.append(lookup[tile])
    (g/'background.tiles').write_bytes(b''.join(tiles))
    (g/'background.map').write_bytes(struct.pack('<'+'H'*len(tilemap),*tilemap))
    (g/'background.pal').write_bytes(struct.pack('<256H',*(palette+[0]*(256-len(palette)))))
    # $200000: each 16x16 cell contains four 8x8 collision IDs (column pairs).
    cols, rows = width//8, height//8
    cells=bytearray(cols*rows)
    for x in range(240):
        for y in range(36): cells[(y+16)*cols+x]=p[0x100000+(x//2)*72+y*2+(x&1)]
    (g/'terrain.cells').write_bytes(cells)
    shapes=[]
    for i in range(64):
        base=0x178988+i*32; addr=struct.unpack_from('>I',p,base)[0]
        mask=addr-0x100000 if addr>=0x200000 else addr
        shapes.append(p[mask:mask+8]+bytes([p[base+9],p[base+12],p[base+13],0]))
    (g/'terrain.shapes').write_bytes(b''.join(shapes))
    # Only existing approved pixels are reused; no old simulation/renderer.
    art=a.approved_art/'generated'; raw=(art/'players.bin').read_bytes()
    (g/'player.tiles').write_bytes(raw[:35*2048])
    (g/'player.pal').write_bytes((art/'object-palettes.bin').read_bytes()[:32])
    motion=(art/'combat_motion.h').read_text()
    origins=[]
    for axis in ('x','y'):
        vals=list(map(int,re.search(r'player_origin_'+axis+r'\[\]=\{([^}]+)',motion)[1].split(',')))
        origins.append(vals[:35])
    (g/'assets.h').write_text('#ifndef ASSETS_H\n#define ASSETS_H\n#include <stdint.h>\n'
        f'#define MAP_W {w//8}\n#define MAP_H {h//8}\n#define WORLD_W {width}\n#define WORLD_H {height}\n#define CELL_W {cols}\n#define CELL_H {rows}\n#define PLAYER_FRAMES 35\n'
        'struct TerrainShape {uint8_t rows[8],oneway,filter,replacement,pad;};\n'
        'extern const uint8_t bg_tiles[], player_tiles[], terrain_cells[];\nextern const uint16_t bg_map[], bg_palette[], player_palette[];\nextern const struct TerrainShape terrain_shapes[];\n'
        +''.join('static const uint8_t player_origin_'+axis+'[]={'+','.join(map(str,arr))+'};\n' for axis,arr in zip(('x','y'),origins))+'#endif\n')
    scaled.save(ROOT/'build/background-reference.png')
    manifest=dict(upstream='https://github.com/correo415415/mslug-decompiled',commit=COMMIT,prom_sha256=PROM_SHA,
        scope='Mission 1 first map, x=0..1919; static scenery; gameplay events not yet ported',
        source_descriptor=layer,scale='nearest 150% then 50% = approved 75%',palette_exact=True,palette_colors=len(palette),
        background_pixels=[w,h],unique_tiles=len(tiles),collision_source='CollMap_LookupTile_027db2 / CollTileTypes_178988',
        runtime_pixel_composition=False,recorded_route_dependency=False,
        approved_player_sha256=hashlib.sha256(raw[:35*2048]).hexdigest(),
        files={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in g.iterdir() if f.is_file() and f.name!='provenance.json'})
    (g/'provenance.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps({k:manifest[k] for k in ('background_pixels','palette_colors','unique_tiles','palette_exact')}))

if __name__=='__main__': main()
