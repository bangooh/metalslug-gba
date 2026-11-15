import sys,json,csv,hashlib,zipfile
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
from approved_font_overrides import ApprovedFonts
from import_scaled_drawings import unpack_tiles
R=Path(__file__).resolve().parents[1];D=R/'docs/hud-0.5.133';D.mkdir(exist_ok=True);B=R/'build/hud133/final'
F=ApprovedFonts();pix=unpack_tiles((R/'generated/native-scaled-fixed-6x6.4bpp').read_bytes())
assert all(np.array_equal(pix[k,:6,:6],v) for k,v in F.fixed.items())
base=zipfile.ZipFile('/workspace/scratch/779e6ec69435/metal-slug-gba-v0.5.123-hud-source.zip');unchanged=[]
for name in ['native-scaled-background-12x12.4bpp','native-bg-blocks.4bpp','native-bg-fragments.u32','native-bg-lookup.u16','approved-player-source.json']:
 key=next(k for k in base.namelist() if k.endswith('/generated/'+name));assert base.read(key)==(R/'generated'/name).read_bytes();unchanged.append(name)
mem=(B/'ewram.bin').read_bytes();state={}
for l in (R/'build/hud133/probe.txt').read_text().splitlines():
 a,n,k=l.split();a=int(a,16);n=int(n)
 if n and 0x2000000<=a<a+n<=0x2040000 and ('fault' in k or k=='scene.scene'):state[k]=int.from_bytes(mem[a-0x2000000:a-0x2000000+n],'little')
assert all(v==0 for k,v in state.items() if 'fault' in k),state
assert state['scene.scene']==1,state
exe=json.loads((B/'execution.json').read_text());assert exe['exit_code']==0 and exe['all_white_game_frames']==0
commits=list(csv.DictReader((B/'commits.csv').open()));assert max(int(x['palette_spills']) for x in commits)==0
build=json.loads((R/'generated/native-build.json').read_text());rom=Path(build['rom']);raw=rom.read_bytes();assert len(raw)<32*1024*1024;assert raw[0xbd]==(-sum(raw[0xa0:0xbd])-0x19)&255
result={'version':'0.5.133-hud','build':build,'execution':exe,'final_state':state,'fixed_cells_match':len(F.fixed),'unchanged_vs123':unchanged,'palette_error_note':'Palette merge color-distance telemetry; maximum17 also present in v123, not a runtime fault','max_palette_error':max(int(x['palette_error']) for x in commits),'max_late':max(int(x['late']) for x in commits),'source_comparison':'docs/ui133-all,250 entries against native Neo Geo assets, equal integer zoom; not a full behavior equivalence test','scope':'M1 input-only through boss/results to M2 initialization. P2 and angled GO asset checks only. No physical hardware/link/full campaign validation. No new performance claim.'}
(D/'validation.json').write_text(json.dumps(result,indent=2)+'\n')
frames=[2700,6000,12600,13500];im=Image.new('RGB',(480,360),'#283038');draw=ImageDraw.Draw(im)
for i,f in enumerate(frames):
 x=i%2*240;y=i//2*180;draw.text((x+4,y+3),f'GBA v0.5.133 / LCD {f}',fill='white');im.paste(Image.open(B/f'frame-{f:06d}.ppm'),(x,y+20))
im.save(D/'gameplay.png');print(json.dumps({'execution':exe,'state':state,'fixed':len(F.fixed),'rom_bytes':len(raw)}))
