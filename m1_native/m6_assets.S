.section .rodata.m6_opacity,"a",%progbits
.balign 4
.global m6_opacity
m6_opacity:
.incbin "generated/m6-cartridge/opacity-packed.bin"
.section .rodata.m6_tiles,"a",%progbits
.balign 4
.global m6_tiles
m6_tiles:
.incbin "generated/m6-cartridge/tiles.bin"
.section .rodata.m6_maps,"a",%progbits
.balign 4
.global m6_maps
m6_maps:
.incbin "generated/m6-cartridge/maps-packed.bin"
.section .rodata.m6_patchmaps,"a",%progbits
.balign 4
.global m6_patchmaps
m6_patchmaps:
.incbin "generated/m6-cartridge/patches.bin"
.section .rodata.m6_transitions,"a",%progbits
.balign 4
.global m6_transitions
m6_transitions:
.incbin "generated/m6-cartridge/transitions.bin"
.section .rodata.m6_palette,"a",%progbits
.balign 4
.global m6_palette
m6_palette:
.incbin "generated/m6-cartridge/palette.bin"
.section .rodata.m6_tiles4,"a",%progbits
.balign 4
.global m6_tiles4
m6_tiles4:
.incbin "generated/m6-cartridge/tiles4.bin"
.section .rodata.m6_ids4,"a",%progbits
.balign 4
.global m6_ids4
m6_ids4:
.incbin "generated/m6-cartridge/ids4.bin"
.section .rodata.m6_attrs4,"a",%progbits
.balign 4
.global m6_attrs4
m6_attrs4:
.incbin "generated/m6-cartridge/attrs4.bin"
.section .rodata.m6_palette4,"a",%progbits
.balign 4
.global m6_palette4
m6_palette4:
.incbin "generated/m6-cartridge/palette4.bin"
.section .rodata.m6_semantic4,"a",%progbits
.balign 4
.global m6_semantic4
m6_semantic4:
.incbin "generated/m6-cartridge/semantic4.bin"
.section .rodata.m6_map_rows,"a",%progbits
.balign 4
.global m6_map_rows
m6_map_rows:
.incbin "generated/m6-cartridge/map-rows.bin"
.section .rodata.m6_opacity_ids,"a",%progbits
.balign 4
.global m6_opacity_ids
m6_opacity_ids:
.incbin "generated/m6-cartridge/opacity-ids.bin"
