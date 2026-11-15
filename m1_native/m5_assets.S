.section .rodata.m5_opacity,"a",%progbits
.balign 4
.global m5_opacity
m5_opacity:
.incbin "generated/m5-cartridge/opacity-packed.bin"
.section .rodata.m5_tiles,"a",%progbits
.balign 4
.global m5_tiles
m5_tiles:
.incbin "generated/m5-cartridge/tiles.bin"
.section .rodata.m5_maps,"a",%progbits
.balign 4
.global m5_maps
m5_maps:
.incbin "generated/m5-cartridge/maps-packed.bin"
.section .rodata.m5_patchmaps,"a",%progbits
.balign 4
.global m5_patchmaps
m5_patchmaps:
.incbin "generated/m5-cartridge/patches.bin"
.section .rodata.m5_transitions,"a",%progbits
.balign 4
.global m5_transitions
m5_transitions:
.incbin "generated/m5-cartridge/transitions.bin"
.section .rodata.m5_palette,"a",%progbits
.balign 4
.global m5_palette
m5_palette:
.incbin "generated/m5-cartridge/palette.bin"
.section .rodata.m5_tiles4,"a",%progbits
.balign 4
.global m5_tiles4
m5_tiles4:
.incbin "generated/m5-cartridge/tiles4.bin"
.section .rodata.m5_ids4,"a",%progbits
.balign 4
.global m5_ids4
m5_ids4:
.incbin "generated/m5-cartridge/ids4.bin"
.section .rodata.m5_attrs4,"a",%progbits
.balign 4
.global m5_attrs4
m5_attrs4:
.incbin "generated/m5-cartridge/attrs4.bin"
.section .rodata.m5_palette4,"a",%progbits
.balign 4
.global m5_palette4
m5_palette4:
.incbin "generated/m5-cartridge/palette4.bin"
.section .rodata.m5_semantic4,"a",%progbits
.balign 4
.global m5_semantic4
m5_semantic4:
.incbin "generated/m5-cartridge/semantic4.bin"
.section .rodata.m5_map_rows,"a",%progbits
.balign 4
.global m5_map_rows
m5_map_rows:
.incbin "generated/m5-cartridge/map-rows.bin"
.section .rodata.m5_opacity_ids,"a",%progbits
.balign 4
.global m5_opacity_ids
m5_opacity_ids:
.incbin "generated/m5-cartridge/opacity-ids.bin"
