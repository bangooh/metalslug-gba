#!/usr/bin/env python3
"""Read-only GBA capture; direct source-pixel alpha/HUD and DMA checks.

Background RGB is intentionally quantized. Geometry/occlusion is checked
against independent whole12x12 source sampling, not the new asset decoder.
"""
import argparse,json,os,re,struct,subprocess
from pathlib import Path
import numpy as np
from PIL import Image
R=Path(__file__).resolve().parents[1]
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--cc',required=True);ap.add_argument('--mgba-prefix',type=Path,required=True);ap.add_argument('--run',type=Path,required=True);ap.add_argument('--frame',type=int,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
    run=a.run.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);lib=a.mgba_prefix/'lib/x86_64-linux-gnu'
    subprocess.run(['cc','-O2','-I'+str(a.mgba_prefix/'include'),'tests/native_snapshot.c','-L'+str(lib),'-Wl,-rpath-link,'+str(lib),'-lmgba','-o',str(out/'capture')],cwd=R,check=True)
    with (out/'capture.log').open('w') as f:subprocess.run([str(out/'capture'),str(run/'rom.gba'),str(run/'probe.txt'),str(run/'inputs.csv'),str(a.frame),str(out)],stdout=f,stderr=f,env=dict(os.environ,LD_LIBRARY_PATH=str(lib)),check=True)
    specs={
      'planes':('video','NativeVideo','tiles.planes'),'columns':('video','NativeVideo','tiles.columns'),
      'fixed':('video','NativeVideo','tiles.fixed'),'slots':('video','NativeVideo','palettes.slots'),
      'bg':('video','NativeVideo','backgrounds'),'colors':('video','NativeVideo','colors.colors'),
      'hud':('video','NativeVideo','fixed.pixels'),'views':('world','NativePlayWorld','video.views'),
      'pools':('world','NativePlayWorld','video.pools')}
    metadata={k:f'offsetof(struct {t},{field})' for k,(symbol,t,field) in specs.items()}
    metadata.update({
      'view_size':'sizeof(struct SceneTileView)','view_first':'offsetof(struct SceneTileView,first_column)','view_valid':'offsetof(struct SceneTileView,valid)',
      'column_size':'sizeof(struct NativeTileColumn)','plane_size':'sizeof(struct NativeTilePlane)',
      'palette_size':'sizeof(struct NativePaletteState)','palette_colors':'offsetof(struct NativePaletteState,colors)',
      'bg_count':'offsetof(struct NativeBackgroundFrame,count)','bg_maps':'offsetof(struct NativeBackgroundFrame,map_count)',
      'bg_enable':'offsetof(struct NativeBackgroundFrame,enable)','bg_control':'offsetof(struct NativeBackgroundFrame,control)',
      'bg_scroll_x':'offsetof(struct NativeBackgroundFrame,scroll_x)','bg_scroll_y':'offsetof(struct NativeBackgroundFrame,scroll_y)',
      'bg_profile':'offsetof(struct NativeBackgroundFrame,native_profile)',
      'bg4_maps':'offsetof(struct NativeBackground4,maps)','bg4_colors':'offsetof(struct NativeBackground4,physical_colors)',
      'hud_map':'offsetof(struct NativeHudFrame,map)','hud_pixels':'offsetof(struct NativeHudFrame,pixels)',
      'hud_occupied':'offsetof(struct NativeHudFrame,occupied)','hud_palettes':'offsetof(struct NativeHudFrame,palettes)'})
    code='#include "native_video.h"\n#include "play_world.h"\n#include "hud_bg.h"\n#include <stddef.h>\nconst unsigned offsets[]={'+','.join(metadata.values())+'};\n'
    (out/'layout.c').write_text(code)
    subprocess.run([a.cc,'-mcpu=arm7tdmi','-marm','-Isrc','-c',str(out/'layout.c'),'-o',str(out/'layout.o')],cwd=R,check=True)
    subprocess.run([a.cc.replace('gcc','objcopy'),'-O','binary','-j','.rodata',str(out/'layout.o'),str(out/'layout.bin')],check=True)
    offsets=dict(zip(metadata,struct.unpack('<'+'I'*len(metadata),(out/'layout.bin').read_bytes())))
    nm=subprocess.check_output([a.cc.replace('gcc','nm'),'-n',str(run/'native.elf')],text=True);symbols={line.split()[-1]:int(line.split()[0],16) for line in nm.splitlines() if len(line.split())>=3}
    ram=(out/'ewram.bin').read_bytes();vram=(out/'vram.bin').read_bytes();pal=np.fromfile(out/'pal.bin','<u2')
    addr={k:symbols[symbol]-0x02000000+offsets[k] for k,(symbol,_,_) in specs.items()}
    def u16(at):return struct.unpack_from('<H',ram,at)[0]
    bg=addr['bg'];count=u16(bg+offsets['bg_count']);layers=ram[bg+offsets['bg_maps']];enable=u16(bg+offsets['bg_enable'])
    bad_tiles=sum(ram[bg+i*32:bg+(i+1)*32]!=vram[i*32:(i+1)*32] for i in range(count))
    bad_maps=0;actual_priority=np.full((160,240),4,np.uint8);actual_rgb=np.full((160,240),pal[0],np.uint16)
    sy,sx=np.indices((160,240));actual_layer=[]
    for layer in range(layers):
        control=u16(bg+offsets['bg_control']+layer*2);priority=control&3;map_address=((control>>8)&31)*2048
        staged=bg+32768+offsets['bg4_maps']+layer*2048
        # Only displayed entries are required; off-screen cache IDs can be
        # recycled before the next scroll exposes/rebuilds their cells.
        x=(sx+u16(bg+offsets['bg_scroll_x']+layer*2))&255;y=(sy+u16(bg+offsets['bg_scroll_y']+layer*2))&255
        entries=np.frombuffer(vram,dtype='<u2',count=1024,offset=map_address).reshape(32,32)[y//8,x//8]
        staged_entries=np.frombuffer(ram,dtype='<u2',count=1024,offset=staged).reshape(32,32)[y//8,x//8]
        bad_maps+=int(np.sum(entries!=staged_entries))
        px=np.where(entries&1024,7-x%8,x%8);py=np.where(entries&2048,7-y%8,y%8)
        address=(entries&1023)*32+py*4+px//2
        idx=(np.frombuffer(vram,np.uint8)[address]>>((px&1)*4))&15
        actual_layer.append(idx!=0)
        show=(idx!=0)&(priority<actual_priority)&bool(enable&(256<<layer))
        actual_rgb[show]=pal[((entries>>12)*16+idx)[show]];actual_priority[show]=priority
    expected_priority=np.full((160,240),4,np.uint8);expected_rgb=np.full((160,240),pal[0],np.uint16)
    raw=np.fromfile(R/'generated/native-scaled-background-12x12.4bpp',np.uint8).reshape(-1,12,6)
    source=np.empty((len(raw),12,12),np.uint8);source[:,:,::2]=raw&15;source[:,:,1::2]=raw>>4
    text=(R/'generated/drawing-data.c').read_text();m=re.search(r'native_source_palettes\[[^;=]*\]\s*=\s*\{(.*?)\};',text,re.S)
    source_pal=np.array([int(n.strip(),0) for n in m[1].replace('{','').replace('}','').split(',') if n.strip()],np.uint16).reshape(-1,16)
    profiles=np.fromfile(R/'generated/native-background/profiles.u16','<u2').reshape(-1,256);profile=ram[bg+offsets['bg_profile']]
    views=[]
    for plane in range(4):
        at=addr['views']+plane*offsets['view_size'];first=u16(at+offsets['view_first'])
        if not ram[at+offsets['view_valid']]:continue
        columns=[struct.unpack_from('<HHB',ram,addr['columns']+(first+c)*offsets['column_size']) for c in range(32)]
        if any(not hidden and ((x>>7)&511)<320 or not hidden and ((x>>7)&511)>=496 for x,y,hidden in columns):views.append((first,plane,columns))
    views.sort(reverse=True);upper=u16(addr['pools']+2)
    for first,plane,columns in views[:layers]:
        pri=1 if first>=upper else 3;alpha=np.zeros((160,240),bool);image=np.zeros((160,240),np.uint16)
        cells=np.ndarray((32,32,2),dtype='<u2',buffer=ram,offset=addr['planes']+plane*offsets['plane_size'])
        for col,(cx,cy,hidden) in enumerate(columns):
            if hidden:continue
            x=(cx>>7)&511
            if x>=496:x-=512
            elif x>=320:continue
            left=x*3//4;top=(496-((cy>>7)&511))*3//4-4;height=(cy&63) or 32
            for xx in range(max(left,0),min(left+12,240)):
                dy=(np.arange(160)-top)%384;valid=dy<height*12;rows=dy//12
                tile=cells[col,rows,0].astype(np.uint32);attr=cells[col,rows,1].astype(np.uint32)
                tid=tile+(((attr>>4)&15)<<16);px=np.where(attr&1,11-(xx-left),xx-left);py=np.where(attr&2,11-dy%12,dy%12)
                value=source[tid,py,px];valid&=value!=0;alpha[:,xx]=valid
                binding=profiles[profile,attr>>8];binding=np.where(binding==65535,0,binding)
                image[:,xx]=source_pal[binding,value]
        show=alpha&(pri<expected_priority);expected_priority[show]=pri;expected_rgb[show]=image[show]
    geometry_bad=int(np.sum(actual_priority!=expected_priority))
    h=addr['hud'];fixed=np.frombuffer(ram,dtype='<u2',count=1280,offset=addr['fixed']).reshape(40,32)
    glyph=np.fromfile(R/'generated/native-scaled-fixed-6x6.4bpp',np.uint8).reshape(4096,8,4)
    words=fixed[sx//6,(sy+16)//6];gx=sx%6;gy=(sy+16)%6
    value=(glyph[words&4095,gy,gx//2]>>((gx&1)*4))&15
    slots=np.array([[u16(addr['slots']+i*offsets['palette_size']+offsets['palette_colors']+j*2) for j in range(16)] for i in range(16)],np.uint16)
    expected_hud=np.where(value!=0,slots[words>>12,value].astype(np.uint32)|0x8000,0)
    entries=np.frombuffer(vram,'<u2',1024,0xf800).reshape(32,32)[sy//8,sx//8]
    indexes=(np.frombuffer(vram,np.uint8)[0xc000+(entries&1023)*32+(sy%8)*4+(sx%8)//2]>>((sx&1)*4))&15
    actual_hud=np.where(indexes!=0,pal[(entries>>12)*16+indexes].astype(np.uint32)|0x8000,0)
    hud_bad=int(np.sum(actual_hud!=expected_hud))
    bgpal=bg+32768+offsets['bg4_colors'];palette_bad=sum(x!=y for x,y in zip(ram[bgpal:bgpal+384],(out/'pal.bin').read_bytes()[:384]))
    def image(rgb,path):
        values=np.stack([(rgb&31),((rgb>>5)&31),((rgb>>10)&31)],axis=-1).astype(np.uint16)*255//31
        Image.fromarray(values.astype(np.uint8)).resize((720,480),Image.Resampling.NEAREST).save(path)
    image(expected_rgb,out/'source-background.png');image(actual_rgb,out/'native-background.png')
    report=dict(frame_requested=a.frame,profile=profile,layers=layers,tiles=count,background_geometry_priority_mismatches=geometry_bad,background_tile_mismatches=bad_tiles,visible_map_mismatches=bad_maps,background_palette_byte_mismatches=palette_bad,hud_rgb_mismatches=hud_bad,background_rgb='intentionally quantized; source RGB image is unfaded regional reference')
    (out/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
    assert not (geometry_bad or bad_tiles or bad_maps or palette_bad or hud_bad)
if __name__=='__main__':main()
