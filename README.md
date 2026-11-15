# Metal Slug GBA — local ROM builder

Unofficial native Game Boy Advance fan port by **Bangoo**. This repository contains source code, conversion tools, and ROM-relative pixel edit recipes. Supply your own original game and Neo Geo BIOS files to extract, convert, apply the graphics edits, and build the GBA cartridge locally.

[한국어 안내](README.ko.md) · [Build details](REBUILD.md) · [Asset provenance](ASSET_PROVENANCE.md) · [Copyright](NOTICE.md)

No original game ROM, BIOS, complete extracted artwork, music, or compiled GBA ROM is distributed here. Reconstructed editor files and build products remain on your computer. This publication uses a new history; older development snapshots are not part of it.

## Build

Validated on Linux x86-64 with Python 3.12, host GCC/G++, Git, and **GNU Arm Embedded GCC 14.2.Rel1**. Install these tools first. Other compiler versions and native Windows/macOS builds have not been validated; Windows users can use a Linux environment such as WSL.

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install -r requirements.txt
python3 build_rom.py --rom /path/to/mslug.zip --bios /path/to/neogeo.zip --cc /path/to/arm-none-eabi-gcc
```

The builder checks the original input hashes before conversion. Required ZIP member names, sizes, and SHA-256 values are in [assets/input-hashes.json](assets/input-hashes.json). No download source for copyrighted game or BIOS files is provided. Use files you are entitled to use under the rules that apply to you.

The first run downloads the pinned [mslug-decompiled](https://github.com/correo415415/mslug-decompiled) source checkout. Allow several GB of disk space and several minutes for conversion and compilation. Use `--source /path/to/checkout` to supply that exact upstream revision yourself.

Output: `dist/metal-slug-gba-native-v0.5.159-parallax-source-candidate.gba`

## Verification

The asset-reconstructed build reproduced the private v191 checkpoint byte for byte: **33,518,376 bytes**; SHA-256 `635a23adea8e08650393c515080410e890b8d3103bd297f4eb587de03dd1d4d9`.

The internal version remains `0.5.159-parallax-source-candidate` to preserve the existing cartridge. This change concerns distribution and rebuilding; it does not change gameplay. Compilation and byte equality do not constitute a new hardware or gameplay test.

## Rights and credits

Metal Slug and its original game content belong to their respective rights holders, including SNK. Game Boy Advance is a Nintendo trademark. This is an unofficial project with no endorsement or affiliation.

Thanks to the **mslug-decompiled contributors** for the upstream reconstruction and tools. Its MIT notice is retained in [LICENSES/mslug-decompiled-MIT.txt](LICENSES/mslug-decompiled-MIT.txt). See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for scope and dependencies. No blanket open-source license has been selected for Bangoo's separate original contributions. An upstream code license does not license original game assets, and requiring original input files does not establish legal clearance for every part of a fan port.

## Reports and contributions

Report the source revision, ROM hash, device/emulator, flash cartridge, mission, and reproduction steps. Do not attach ROMs, BIOS files, extracted graphics/audio, generated editor projects, or compiled game cartridges to public issues or releases. Keep `art/`, `generated/`, `build/`, and `dist/` local. See [REBUILD.md](REBUILD.md) for checks.
