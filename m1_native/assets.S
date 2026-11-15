.section .rodata.m1_opacity,"a",%progbits
.balign 4
.global m1_opacity
m1_opacity:
.incbin "generated/m1-cartridge/opacity.bin"
.section .rodata.m1_tiles,"a",%progbits
.balign 4
.global m1_tiles
m1_tiles:
.incbin "generated/m1-cartridge/tiles.bin"
.section .rodata.m1_maps,"a",%progbits
.balign 4
.global m1_maps
m1_maps:
.incbin "generated/m1-cartridge/maps.bin"
.section .rodata.m1_patchmaps,"a",%progbits
.balign 4
.global m1_patchmaps
m1_patchmaps:
.incbin "generated/m1-cartridge/patches.bin"
.section .rodata.m1_transitions,"a",%progbits
.balign 4
.global m1_transitions
m1_transitions:
.incbin "generated/m1-cartridge/transitions.bin"
.section .rodata.m1_palette,"a",%progbits
.balign 4
.global m1_palette
m1_palette:
.incbin "generated/m1-cartridge/palette.bin"
.section .rodata.m1_tiles4,"a",%progbits
.balign 4
.global m1_tiles4
m1_tiles4:
.incbin "generated/m1-cartridge/tiles4.bin"
.section .rodata.m1_ids4,"a",%progbits
.balign 4
.global m1_ids4
m1_ids4:
.incbin "generated/m1-cartridge/ids4.bin"
.section .rodata.m1_attrs4,"a",%progbits
.balign 4
.global m1_attrs4
m1_attrs4:
.incbin "generated/m1-cartridge/attrs4.bin"
.section .rodata.m1_palette4,"a",%progbits
.balign 4
.global m1_palette4
m1_palette4:
.incbin "generated/m1-cartridge/palette4.bin"
.section .rodata.m1_semantic4,"a",%progbits
.balign 4
.global m1_semantic4
m1_semantic4:
.incbin "generated/m1-cartridge/semantic4.bin"
