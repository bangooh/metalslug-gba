.section .rodata.m4_opacity,"a",%progbits
.balign 4
.global m4_opacity
m4_opacity:
.incbin "generated/m4-cartridge/opacity-packed.bin"
.section .rodata.m4_tiles,"a",%progbits
.balign 4
.global m4_tiles
m4_tiles:
.incbin "generated/m4-cartridge/tiles.bin"
.section .rodata.m4_maps,"a",%progbits
.balign 4
.global m4_maps
m4_maps:
.incbin "generated/m4-cartridge/maps-packed.bin"
.section .rodata.m4_patchmaps,"a",%progbits
.balign 4
.global m4_patchmaps
m4_patchmaps:
.incbin "generated/m4-cartridge/patches.bin"
.section .rodata.m4_transitions,"a",%progbits
.balign 4
.global m4_transitions
m4_transitions:
.incbin "generated/m4-cartridge/transitions.bin"
.section .rodata.cart_m4_palette,"a",%progbits
.balign 4
.global cart_m4_palette
cart_m4_palette:
.incbin "generated/m4-cartridge/palette.bin"
.section .rodata.m4_tiles4,"a",%progbits
.balign 4
.global m4_tiles4
m4_tiles4:
.incbin "generated/m4-cartridge/tiles4.bin"
.section .rodata.m4_ids4,"a",%progbits
.balign 4
.global m4_ids4
m4_ids4:
.incbin "generated/m4-cartridge/ids4.bin"
.section .rodata.m4_attrs4,"a",%progbits
.balign 4
.global m4_attrs4
m4_attrs4:
.incbin "generated/m4-cartridge/attrs4.bin"
.section .rodata.cart_m4_palette4,"a",%progbits
.balign 4
.global cart_m4_palette4
cart_m4_palette4:
.incbin "generated/m4-cartridge/palette4.bin"
.section .rodata.m4_semantic4,"a",%progbits
.balign 4
.global m4_semantic4
m4_semantic4:
.incbin "generated/m4-cartridge/semantic4.bin"
.section .rodata.m4_map_rows,"a",%progbits
.balign 4
.global m4_map_rows
m4_map_rows:
.incbin "generated/m4-cartridge/map-rows.bin"
.section .rodata.m4_opacity_ids,"a",%progbits
.balign 4
.global m4_opacity_ids
m4_opacity_ids:
.incbin "generated/m4-cartridge/opacity-ids.bin"
