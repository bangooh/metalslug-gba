#!/usr/bin/env python3
"""Rebuild v191 source catalogs, graphics, presentation and audio from ROMs.
Editor inputs are reconstructed locally from ROM-relative pixel edit recipes.
"""
import argparse,subprocess,sys,json,shutil
from pathlib import Path
R=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--rom',type=Path,required=True);p.add_argument('--bios',type=Path,required=True);p.add_argument('--cc',required=True);a=p.parse_args()
 a.source=a.source.resolve();a.rom=a.rom.resolve();a.bios=a.bios.resolve()
 compiler=shutil.which(a.cc)
 if compiler is None:p.error('ARM compiler was not found: '+a.cc)
 a.cc=str(Path(compiler).resolve())
 if not a.source.is_dir() or not a.rom.is_file() or not a.bios.is_file():p.error('Source checkout, game ROM and BIOS must exist')
 def run(name,*args):
  print('Rebuild stage: '+name,flush=True)
  subprocess.run([sys.executable,str(R/'tools'/name),*map(str,args)],cwd=R,check=True)
 run('check_original_inputs.py','--rom',a.rom,'--bios',a.bios)
 (R/'art').mkdir(exist_ok=True)
 shutil.copy2(R/'assets/ranking-defaults.json',R/'art/ranking-defaults.json')
 # Drawing import validates unextended original source ledgers.
 run('rebuild_source_catalogs.py','--source',a.source,'--rom',a.rom)
 run('import_drawing_data.py','--source',a.source,'--rom',a.rom)
 run('restore_edit_inputs.py','--source',a.source,'--bios',a.bios)
 run('rebuild_authored_seeds.py')
 run('compile_strip_plans.py')
 run('import_scaled_drawings.py')
 import build_ending_credits as ec
 masters=ec.load(R/'art/ending-credits/fonts.json');cards=ec.load(R/'art/ending-credits/cards.json');ec.apply(masters['masters'],cards['cards']);ec.register(cards['cards'],originals=R/'generated')
 run('build_player_crouch_fixes.py');run('apply_final_obj_fonts.py');run('apply_final_fonts.py')
 # Release program deltas are content guarded and independent of packing.
 run('apply_program_overrides.py',R)
 run('repair_affine_cloud_padding.py')
 for name in ['bridge-horizontal.inc','blast-packed.inc']:(R/'generated'/name).unlink(missing_ok=True)
 run('build_bridge_horizontal_chunks.py');run('build_blast_packed.py')
 run('import_full_audio.py','--upstream',a.source,'--rom',a.rom,'--workers','4')
 run('import_audio_tempo.py','--rom',a.rom)
 run('build_audio_ima_fast.py')
 run('import_audio_pcm_cache.py');run('pack_audio_pcm_lossless.py')
 run('rebuild_common_backgrounds.py')
 run('rebuild_presentation.py','--source',a.source,'--bios',a.bios)
 run('rebuild_campaign_backgrounds.py')
 run('build_native.py','--cc',a.cc,'--incremental')
if __name__=='__main__':main()
