.section .rodata.m3_opacity,"a",%progbits
.balign 4
.global m3_opacity
m3_opacity:
.incbin "generated/m3-cartridge/opacity-packed.bin"
.section .rodata.m3_tiles,"a",%progbits
.balign 4
.global m3_tiles
m3_tiles:
.incbin "generated/m3-cartridge/tiles.bin"
.section .rodata.m3_maps,"a",%progbits
.balign 4
.global m3_maps
m3_maps:
.incbin "generated/m3-cartridge/maps-packed.bin"
.section .rodata.m3_patchmaps,"a",%progbits
.balign 4
.global m3_patchmaps
m3_patchmaps:
.incbin "generated/m3-cartridge/patches.bin"
.section .rodata.m3_transitions,"a",%progbits
.balign 4
.global m3_transitions
m3_transitions:
.incbin "generated/m3-cartridge/transitions.bin"
.section .rodata.m3_palette,"a",%progbits
.balign 4
.global m3_palette
m3_palette:
.incbin "generated/m3-cartridge/palette.bin"
.section .rodata.m3_tiles4,"a",%progbits
.balign 4
.global m3_tiles4
m3_tiles4:
.incbin "generated/m3-cartridge/tiles4.bin"
.section .rodata.m3_ids4,"a",%progbits
.balign 4
.global m3_ids4
m3_ids4:
.incbin "generated/m3-cartridge/ids4.bin"
.section .rodata.m3_attrs4,"a",%progbits
.balign 4
.global m3_attrs4
m3_attrs4:
.incbin "generated/m3-cartridge/attrs4.bin"
.section .rodata.m3_palette4,"a",%progbits
.balign 4
.global m3_palette4
m3_palette4:
.incbin "generated/m3-cartridge/palette4.bin"
.section .rodata.m3_semantic4,"a",%progbits
.balign 4
.global m3_semantic4
m3_semantic4:
.incbin "generated/m3-cartridge/semantic4.bin"
.section .rodata.m3_map_rows,"a",%progbits
.balign 4
.global m3_map_rows
m3_map_rows:
.incbin "generated/m3-cartridge/map-rows.bin"
.section .rodata.m3_opacity_ids,"a",%progbits
.balign 4
.global m3_opacity_ids
m3_opacity_ids:
.incbin "generated/m3-cartridge/opacity-ids.bin"
