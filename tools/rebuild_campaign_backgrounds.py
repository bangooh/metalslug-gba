#!/usr/bin/env python3
"""Rebuild M1-M6 cartridge backgrounds without historical asset archives.

Consumes original-ROM-scaled pixels and typed scene/patch catalogs. The
animation index base precedes M3 wall repairs; its stable IDs are then retained
while rebuilding the final M3 maps. No legacy layout is shipped as final M3.
"""
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
G = ROOT/'generated'

def main():
    work = ROOT/'build/background-regeneration'
    base, approved = work/'base', work/'m3-approved'
    base.mkdir(parents=True, exist_ok=True)
    def run(name,*args):
        subprocess.run([sys.executable,str(ROOT/'tools'/name),*map(str,args)],cwd=ROOT,check=True)
    for mission in range(1,7):
        args = ['--mission',str(mission)]
        run('build_m1_cartridge_maps.py' if mission<=2 else 'build_campaign_cartridge.py',
            *args, *(['--animation-base'] if mission==3 else []))
        run('build_m1_4bpp.py',*args)
    run('pack_campaign_catalogs.py')
    for mission in range(1,7):
        shutil.copytree(G/f'm{mission}-cartridge',base/f'm{mission}-cartridge',dirs_exist_ok=True)
        for suffix in ('c','h'):
            shutil.copy2(G/f'm{mission}-assets.{suffix}',base/f'm{mission}-assets.{suffix}')
    run('build_background_animation158.py','--base',base)
    shutil.copytree(G/'m3-cartridge',approved,dirs_exist_ok=True)
    run('build_campaign_cartridge.py','--mission',3)
    run('build_m1_4bpp.py','--mission',3,'--preserve-existing','--approved-dir',approved)
    run('preserve_m3_catalog_ids.py','--approved-dir',approved)
    run('pack_campaign_catalogs.py','--mission',3)
    run('build_background_animation_row_index.py')
    print('M1-M6 backgrounds regenerated; historical background ZIP not used.')

if __name__ == '__main__':
    main()
