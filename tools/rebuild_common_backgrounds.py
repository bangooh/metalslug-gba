#!/usr/bin/env python3
"""Rebuild common background fragments, ending palettes and packed maps."""
import shutil
import subprocess
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def main():
    def run(name):subprocess.run([sys.executable,str(ROOT/'tools'/name)],cwd=ROOT,check=True)
    for name in ['import_tile_video.py','import_bg_opacity.py','prepare_native_background.py']:
        run(name)
    # The legacy pruner expects a complete unpruned baseline. Refresh it from
    # this run, rather than accidentally consuming a previous build backup.
    g=ROOT/'generated';backup=g/'native-background-full';backup.mkdir(exist_ok=True)
    for name in ['maps.u24','tiles.4bpp','tile-banks.u8']:
        shutil.copy2(g/'native-background'/name,backup/name)
    shutil.copy2(g/'native-bg-assets.c',backup/'native-bg-assets.c')
    (ROOT/'build/campaign134').mkdir(parents=True,exist_ok=True)
    for name in ['prune_legacy_campaign_maps.py','prune_ending_prebuilt.py','refine_ending_palette.py','build_ending_all_remap.py']:
        run(name)
    run('import_object_masks.py')

if __name__=='__main__':main()
