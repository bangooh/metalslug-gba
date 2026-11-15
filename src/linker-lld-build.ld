ENTRY(_rom_start)
MEMORY { ROM (rx) : ORIGIN = 0x08000000, LENGTH = 32M
 EWRAM (rwx) : ORIGIN = 0x02000000, LENGTH = 256K
 IWRAM (rwx) : ORIGIN = 0x03000000, LENGTH = 30K }
SECTIONS {
 .text : { KEEP(*(.gba_header)) *(.text.startup) *(.text*)
   *(.iwram.native_assets) *(.iwram.hud) *(.iwram.obj_index_palette) *(.iwram.render_hot)
   *(.rodata*) . = ALIGN(4); } > ROM
 .bglegacy 0x03000000 : AT(LOADADDR(.text)+SIZEOF(.text)) { *(.bglegacy*) } > IWRAM
 .m1hot 0x03000000 : AT(LOADADDR(.bglegacy)+SIZEOF(.bglegacy)) { *(.m1hot*) } > IWRAM
 __bg_overlay_start = ADDR(.bglegacy);
 __bg_legacy_load = LOADADDR(.bglegacy); __bg_legacy_size = SIZEOF(.bglegacy);
 __bg_m1_load = LOADADDR(.m1hot); __bg_m1_size = SIZEOF(.m1hot);
 .iwram 0x03003B60 : AT(LOADADDR(.m1hot)+SIZEOF(.m1hot)) { __iwram_start = .; *(.iwram*) . = ALIGN(4); __iwram_end = .; } > IWRAM
 __iwram_load = LOADADDR(.iwram);
 .data : AT(LOADADDR(.iwram)+SIZEOF(.iwram)) { __data_start = .; *(.data*) . = ALIGN(4); __data_end = .; } > EWRAM
 __data_load = LOADADDR(.data);
 .bss (NOLOAD) : { __bss_start = .; *(.bss*) *(COMMON) . = ALIGN(4); __bss_end = .; } > EWRAM
 ASSERT(__iwram_end <= 0x03006700, "IWRAM stack protection")
 ASSERT(__bss_end <= 0x02040000, "EWRAM overflow")
 /DISCARD/ : { *(.comment) *(.note*) *(.ARM.exidx*) *(.ARM.extab*) }
}
