"""GBA ELF extraction and cartridge header fixup, independent of asset conversion."""
import struct

ROM_BASE = 0x08000000


LOGO = bytes.fromhex(
    "24ffae51699aa2213d84820a84e409ad11248b98c0817f21a352be199309ce20"
    "10464a4af82731ec58c7e83382e3cebf85f4df94ce4b09c194568ac01372a7fc"
    "9f844d73a3ca9a615897a327fc039876231dc7610304ae56bf38840040a70efd"
    "ff52fe036f9530f197fbc08560d68025a963be03014e38e2f9a234ffbb3e0344"
    "780090cb88113a9465c07c6387f03cafd625e48b380aac7221d4f807"
)


def elf_to_rom(data: bytes) -> bytearray:
    """Extract file-backed ELF32 ARM segments by physical load address."""
    if data[:7] != b"\x7fELF\x01\x01\x01":
        raise ValueError("Expected ELF32 little-endian output")
    if struct.unpack_from("<H", data, 18)[0] != 40:
        raise ValueError("ELF is not ARM")
    phoff = struct.unpack_from("<I", data, 28)[0]
    entsize, count = struct.unpack_from("<HH", data, 42)
    chunks = []
    for i in range(count):
        ptype, offset, vaddr, paddr, filesz, memsz, flags, align = struct.unpack_from("<8I", data, phoff + i * entsize)
        if ptype == 1 and filesz and ROM_BASE <= paddr < ROM_BASE + 32*1024*1024:
            if paddr + filesz > ROM_BASE + 32*1024*1024:
                raise ValueError("ROM segment exceeds 32 MiB")
            if offset + filesz > len(data):
                raise ValueError("Truncated ELF segment")
            chunks.append((paddr - ROM_BASE, data[offset:offset + filesz]))
    if not chunks or min(offset for offset, _ in chunks) != 0:
        raise ValueError("ELF has no GBA ROM entry segment")
    out = bytearray(b"\xff" * max(offset + len(chunk) for offset, chunk in chunks))
    for offset, chunk in chunks:
        out[offset:offset + len(chunk)] = chunk
    return out


def fix_header(rom: bytearray) -> None:
    if len(LOGO) != 156 or len(rom) < 0xC0:
        raise ValueError("Invalid header length")
    branch = struct.unpack_from("<I", rom)[0]
    if branch & 0xFF000000 != 0xEA000000:
        raise ValueError("Expected ARM branch at ROM entry")
    displacement = branch & 0xFFFFFF
    if displacement & 0x800000: displacement -= 0x1000000
    target = 8 + displacement * 4
    if target < 0xC0 or target >= len(rom):
        raise ValueError("ROM entry branch leaves executable region")
    rom[4:0xA0] = LOGO
    rom[0xA0:0xAC] = b"MSLUG PORT01"
    rom[0xAC:0xB0] = b"MSP1"
    rom[0xB0:0xB2] = b"00"
    rom[0xB2:0xC0] = bytes(14)
    rom[0xB2] = 0x96
    rom[0xBC] = 1
    rom[0xBD] = (-sum(rom[0xA0:0xBD]) - 0x19) & 255
    if (sum(rom[0xA0:0xBE]) + 0x19) & 255:
        raise ValueError("Header checksum failed")

