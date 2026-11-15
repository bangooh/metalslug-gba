ENTRY(_rom_start)
MEMORY {
 ROM (rx) : ORIGIN = 0x08000000, LENGTH = 32M
 EWRAM (rwx) : ORIGIN = 0x02000000, LENGTH = 256K
 IWRAM (rwx) : ORIGIN = 0x03000000, LENGTH = 30K
}
SECTIONS {
 .text : { KEEP(*(.gba_header)) *(.text.startup) *(.text*) *(.iwram.native_assets) *(.rodata*) . = ALIGN(4); } > ROM
 OVERLAY 0x03000000 : AT(LOADADDR(.text)+SIZEOF(.text)) {
  .bglegacy { *(.bglegacy*) }
  .m1hot { *(.m1hot*) }
  .openinghot { *(.openinghot*) . = ALIGN(4); }
 } > IWRAM
 __bg_overlay_start = ADDR(.bglegacy);
 __bg_legacy_load = LOADADDR(.bglegacy); __bg_legacy_size = SIZEOF(.bglegacy);
 __bg_m1_load = LOADADDR(.m1hot); __bg_m1_size = SIZEOF(.m1hot);
 __opening_load = LOADADDR(.openinghot); __opening_size = SIZEOF(.openinghot);
 .iwram : AT(LOADADDR(.openinghot)+SIZEOF(.openinghot)) { __iwram_start = .; *(.iwram*) . = ALIGN(4); __iwram_end = .; } > IWRAM
 __iwram_load = LOADADDR(.iwram);
 /* Opening-only scratch occupies unused bytes in the overlay window. */
 __opening_lookup = ADDR(.openinghot)+SIZEOF(.openinghot);
 ASSERT(__opening_lookup+1024 <= __iwram_start, "Opening lookup exceeds overlay window")
 .data : AT(LOADADDR(.iwram)+SIZEOF(.iwram)) { __data_start = .; *(.data*) . = ALIGN(4); __data_end = .; } > EWRAM
 __data_load = LOADADDR(.data);
 .bss (NOLOAD) : { __bss_start = .; *(.bss*) *(COMMON) . = ALIGN(4); __bss_end = .; } > EWRAM
 /* Reserve4KiB below the main SP and2208B below the IRQ SP. Startup fills
  * both regions with a sentinel for measured stack high-water checks. */
 ASSERT(__iwram_end <= 0x03006700, "IWRAM overlaps4KiB main/2208B IRQ stacks")
 /DISCARD/ : { *(.comment) *(.note*) *(.ARM.exidx*) *(.ARM.extab*) }
}
