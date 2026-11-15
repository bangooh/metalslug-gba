# Rebuilding from original input files

Use the command in README.md. The supported entry point is `build_rom.py`; historical editing and diagnostic tools are also retained, but they are not alternative clean-build entry points.

## Inputs and dependencies

- Game and BIOS ZIP files matching `assets/input-hashes.json`. Exact required members are checked for size and SHA-256; ambiguous duplicate members are rejected. BIOS members must be at the ZIP root.
- Python dependencies in `requirements.txt`.
- Git and host C/C++ compilers for source utilities and audio rendering.
- GNU Arm Embedded GCC 14.2.Rel1 (tested). Pass its `arm-none-eabi-gcc` executable via `--cc`, or put it in PATH.
- Upstream `correo415415/mslug-decompiled` revision `574a517547f05bc8c1539fab1b7d566035543496`. The wrapper fetches it into `.work/upstream`, or checks the revision of the supplied `--source` checkout. An existing checkout at another revision is rejected.

## Pipeline

1. Validate original ZIP members before creating asset outputs.
2. Import source catalogs, original tiles, FIX graphics, maps, and palettes from the supplied game ROM.
3. Reconstruct the local editor inputs from `assets/index.json`. Every image checks the ROM-derived base hash, applies the indexed changed pixels, and checks the resulting hash. JSON editor files also check their exact reconstructed byte hash.
4. Apply the existing font, character, HUD, background, and presentation conversion stages. Audio is rendered from the supplied original files locally.
5. Compile and link the native GBA program. Read `generated/native-build.json` for the resulting size and hash.

`art/`, `generated/`, `build/`, `dist/`, validation images, and `.work/` are local build products and are ignored by Git. Keep ROM/BIOS inputs outside the repository. Ignoring files is only a guardrail: do not use `git add -f` on these outputs.

## Checks without original inputs

```sh
python3 -m unittest discover -s tests -v
```

These synthetic tests check patch application and rejection of incorrect source/result hashes and invalid edit positions. They do not require or include original artwork. A full cartridge build requires the original inputs and compiler.

## Reproducibility

The verified **0.9.0** build produces a 33,518,376-byte ROM. SHA-256: `51459b145f87e263ea4b77af3daf9e1bc41aaad7d77c6fd349fb554c98da4f9c`.

Output: `dist/metal-slug-gba-native-v0.9.0.gba`.

Build and asset reconstruction checks passed with the tested inputs and compiler. The embedded link compatibility fingerprint includes the version; both linked consoles must run the same 0.9.0 build.

These checks do not establish full game correctness, performance, compatibility with other inputs/toolchains, or legal clearance. No new hardware gameplay test was performed for this release. No automatic CI workflow uploads ROMs or assets.
