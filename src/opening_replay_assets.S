.section .rodata.opening_tiles,"a",%progbits
.balign 4
.global opening_tiles_packed
opening_tiles_packed:
.incbin "generated/opening-tiles.deflate"
.section .rodata.opening_cells,"a",%progbits
.balign 4
.global opening_cells_packed
opening_cells_packed:
.incbin "generated/opening-cells.deflate"
.section .rodata.opening_maps,"a",%progbits
.balign 4
.global opening_maps_packed
opening_maps_packed:
.incbin "generated/opening-maps.deflate"
.section .rodata.opening_palettes,"a",%progbits
.balign 4
.global opening_palettes_packed
opening_palettes_packed:
.incbin "generated/opening-palettes.deflate"
.section .rodata.opening_frames,"a",%progbits
.balign 4
.global opening_frames_packed
opening_frames_packed:
.incbin "generated/opening-frames.deflate"
.section .rodata.opening_palrefs,"a",%progbits
.balign 4
.global opening_palrefs_packed
opening_palrefs_packed:
.incbin "generated/opening-palrefs.deflate"
.section .rodata.opening_draws,"a",%progbits
.balign 4
.global opening_draws_packed
opening_draws_packed:
.incbin "generated/opening-draws.deflate"
.section .rodata.neogeo_tiles,"a",%progbits
.balign 4
.global neogeo_tiles_packed
neogeo_tiles_packed:
.incbin "generated/neogeo-tiles.deflate"
.section .rodata.neogeo_changes,"a",%progbits
.balign 4
.global neogeo_changes_packed
neogeo_changes_packed:
.incbin "generated/neogeo-changes.deflate"
.section .rodata.neogeo_palettes,"a",%progbits
.balign 4
.global neogeo_palettes_packed
neogeo_palettes_packed:
.incbin "generated/neogeo-palettes.deflate"
.section .rodata.opening_background,"a",%progbits
.balign 4
.global opening_background_packed
opening_background_packed:
.incbin "generated/opening-background.deflate"
.section .rodata.opening_visibility,"a",%progbits
.balign 4
.global opening_visibility_packed
opening_visibility_packed:
.incbin "generated/opening-visibility.deflate"
