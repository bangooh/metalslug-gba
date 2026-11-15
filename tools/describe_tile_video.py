#!/usr/bin/env python3
"""Static source/pool/viewport inventory only; never execute game code."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[1]
COMMIT='574a517547f05bc8c1539fab1b7d566035543496'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(s,name):
 body=re.search(r'const struct \w+ '+name+r'\[\]=\{(.*?)\};',s,re.S)[1]
 return [list(map(int,row.split(','))) for row in re.findall(r'\{([^{}]+)\}',body)]
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--hardware-reference',type=Path);a=p.parse_args()
 commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=a.source,text=True).strip()
 if commit!=COMMIT:raise ValueError('Source commit')
 s=(R/'generated/scene-programs.c').read_text();desc=rows(s,'scene_descriptors');initial=rows(s,'scene_initial_planes');pools=[]
 for i,(command,first,count,reserved) in enumerate(desc):
  used=initial[first:first+count];low=sum(32 for row in used if row[1]);high=count*32-low;front=380-reserved;middle=front-high
  cursors=[middle,0];planes=[]
  for plane,pool,x,y,width,height in used:
   planes.append(dict(plane=plane,source_pool=pool,first_physical_column=cursors[pool]+1,last_physical_column=cursors[pool]+32))
   cursors[pool]+=32
  pools.append(dict(scene_program=i,command=command,lower_background=[0,low],ordinary_objects=[low,middle],upper_background=[middle,front],forced_objects=[front,380],planes=planes))
 source_names=['asm/mission/scene_loader_0434ea.s','asm/video/sprite_allocator_01390e.s','asm/video/sprite_queue_render_05aa96.s','asm/video/cellmap_fix_overlay_palfade_051aa4.s','asm/video/fix_blit_rect_05da56.s']
 report=dict(version=(R/'VERSION').read_text().strip(),source_commit=commit,source_files={n:sha(a.source/n) for n in source_names},
  source_entries={'043568':'scene two-pass reservation and plane initialization','01390E':'380-strip low/high/forced bounds','05B232':'ordinary and forced sorted queues in separate strip ranges','051E74':'tile-only clear retains attributes','052050':'original bottom credits row29','05223A':'original LEVEL row29'},
  descriptor_pools=pools,ranges='Half-open source strip indices; callback physical column adds1',
  priority_plan={'FIX':0,'forced_objects':1,'upper_background':1,'ordinary_objects':2,'lower_background':3},
  priority_caveat='GBA OBJ wins a same-priority BG tie. Source BGs are mapped in physical strip order by background_video.c; this script inventories source descriptors.',
  fix=dict(source_cells=[40,32],glyph_pixels=[6,6],source_visible_y=[16,239],gba_tile_surface=[30,20],screen=[240,160],screen_x='6*column',screen_y='6*row-16',
   ordinary_top_row=dict(row=2,y=-4,visible_height=2),ordinary_bottom_row=dict(row=29,y=158,visible_height=2),
   native_bottom_hud=dict(source_row=29,output_row=28,y=152,height=6,includes=['credits','LEVEL'],unchanged=['pause row28','ordinary FIX art','gameplay coordinates']),
   renderer='offline2x2 fragments -> exact4bppOBJ composition -> shared actor/FIX OAM, tiles and palettes -> DMA3',
   shared_maximum_tiles=1024,shared_maximum_oam=128,shared_maximum_palettes=16,
   transfer_caveat='Shared physical limits; busy-scene capacity and VBlank transfer timing are not verified'),
  implementation=dict(tile_world_callbacks=True,whole_background_compositor=True,fix_gba_component=True,main_connected=True,source_free_strip_producer=True,source_auto_clock_producer=True),
  game_execution=False,emulator_execution=False,performance_measured=False,original_residual_exceptions_resolved=0,native_argument_corrections=3)
 if a.hardware_reference:
  report['hardware_reference']=dict(url='https://raw.githubusercontent.com/mamedev/mame/master/src/mame/snk/neogeo_spr.cpp',sha256=sha(a.hardware_reference),scope='Source inspection: tile code high nibble, auto bits3/2 precedence and disable, FIX column-major addressing. No emulator execution.')
 (R/'generated/tile-video-connection.json').write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({'scenes':len(pools),'planes':len(initial),'source_commit':commit,'game_execution':False}))
if __name__=='__main__':main()
