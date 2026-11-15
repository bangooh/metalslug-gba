Offline regeneration (not needed for normal GNU cartridge builds)

Dependencies: Python3, numpy, Pillow, unicorn. Original source/assets supplied
separately, upstream commit 574a517547f05bc8c1539fab1b7d566035543496.
The tooling intentionally uses the following temporary workspace layout:
  src/metal-slug-native/  this project (can be a symlink)
  upstream/              original decompilation checkout
  upstream/build/mslug_prom.bin  normalized P-ROM
  upstream/rom/201-c1.bin ... 201-c4.bin
  opening-work/          copies of the Python files in this directory
Run from that workspace root:
  python3 opening-work/oracle.py
  python3 opening-work/pack.py
pack.py runs build.py before compressing tables and title art.
The included trace.json records the trace used for this build. You may copy it
into opening-work and run pack.py directly to avoid rerunning Unicorn.

oracle.py hooks platform services, task scheduling, palette interpolation,
render submission and sound. It executes the original actor and animation
instructions. Unhandled services abort. It is a focused extraction harness,
not a complete Neo Geo emulator. See main status README for scope/limitations.
