.section .rodata.title_tiles,"a",%progbits
.balign 4
.global title_tiles
title_tiles:
.incbin "generated/title.tiles.deflate"
.section .rodata.title_maps,"a",%progbits
.balign 4
.global title_maps
title_maps:
.incbin "generated/title.maps.deflate"
.section .rodata.title_palette,"a",%progbits
.balign 4
.global title_palette
title_palette:
.incbin "generated/title.pal"
.section .rodata.hud_font,"a",%progbits
.balign 4
.global hud_font
hud_font:
.incbin "generated/native-ui-font.4bpp"
