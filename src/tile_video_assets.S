.section .rodata.native_bg_lookup,"a",%progbits
.balign 4
.global native_bg_lookup
native_bg_lookup:
.incbin "generated/native-bg-lookup.u16"
.section .rodata.native_bg_fragments,"a",%progbits
.balign 4
.global native_bg_fragments
native_bg_fragments:
.incbin "generated/native-bg-fragments.u32"
.section .rodata.native_bg_blocks,"a",%progbits
.balign 4
.global native_bg_blocks
native_bg_blocks:
.incbin "generated/native-bg-blocks.4bpp"
.section .rodata.native_fix_fragments,"a",%progbits
.balign 4
.global native_fix_fragments
native_fix_fragments:
.incbin "generated/native-fix-fragments.u16"
.section .rodata.native_bg_base_opacity,"a",%progbits
.balign 4
.global native_bg_base_opacity
native_bg_base_opacity:
.incbin "generated/native-background/base-opacity.u16"
