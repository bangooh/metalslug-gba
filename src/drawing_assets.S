.section .rodata.native_sprite_pixels,"a",%progbits
.balign 4
.global native_sprite_pixels
native_sprite_pixels:
.incbin "generated/native-sprites.4bpp"
.section .rodata.native_fixed_pixels,"a",%progbits
.balign 4
.global native_fixed_pixels
native_fixed_pixels:
.incbin "generated/native-fixed.4bpp"
