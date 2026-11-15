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

The reconstructed-asset pipeline produced exactly the same 33,518,376-byte ROM as the baseline v191 rebuild using the tested compiler. SHA-256: `635a23adea8e08650393c515080410e890b8d3103bd297f4eb587de03dd1d4d9`.

This establishes preservation of the existing output for the tested inputs. It does not establish full game correctness, performance, compatibility with other inputs/toolchains, or legal clearance. No automatic CI workflow uploads ROMs or assets.
