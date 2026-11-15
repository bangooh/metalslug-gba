.section .rodata.native_bg_native_banks,"a",%progbits
.balign 4
.global native_bg_native_banks
native_bg_native_banks:
.incbin "generated/native-background/tile-banks.u8"
.section .rodata.native_bg_native_palettes,"a",%progbits
.balign 4
.global native_bg_native_palettes
native_bg_native_palettes:
.incbin "generated/native-background/palettes.rgb555"
.section .rodata.native_bg_profiles,"a",%progbits
.balign 4
.global native_bg_profiles
native_bg_profiles:
.incbin "generated/native-background/profiles.u16"
.section .rodata.native_bg_cell_regions,"a",%progbits
.balign 4
.global native_bg_cell_regions
native_bg_cell_regions:
.incbin "generated/native-background/cell-regions.u16"
.section .rodata.native_bg_cell_origins,"a",%progbits
.balign 4
.global native_bg_cell_origins
native_bg_cell_origins:
.incbin "generated/native-background/cell-origins.u32"
.section .rodata.native_bg_tile_pages,"a",%progbits
.balign 4
.global native_bg_tile_pages
native_bg_tile_pages:
.incbin "generated/native-background/tile-blocks.4bpp"
.section .rodata.native_bg_tile_references,"a",%progbits
.balign 4
.global native_bg_tile_references
native_bg_tile_references:
.incbin "generated/native-background/tile-references.u16"
.section .rodata.native_bg_tile_offsets,"a",%progbits
.balign 4
.global native_bg_tile_offsets
native_bg_tile_offsets:
.incbin "generated/native-background/tile-pages.u32"
.section .rodata.native_bg_map_pages,"a",%progbits
.balign 4
.global native_bg_map_pages
native_bg_map_pages:
.incbin "generated/native-background/map-pages.u32"
.section .rodata.native_bg_map_data,"a",%progbits
.balign 4
.global native_bg_map_data
native_bg_map_data:
.incbin "generated/native-background/map-data.u24"
.section .rodata.native_bg_native_remap,"a",%progbits
.balign 4
.global native_bg_native_remap
native_bg_native_remap:
.incbin "generated/native-background/palette-remap.u8"
.section .rodata.native_bg_native_cost,"a",%progbits
.balign 4
.global native_bg_native_cost
native_bg_native_cost:
.incbin "generated/native-background/palette-cost.u16"
