.section .rodata.native_scaled_graphics,"a",%progbits
.balign 4
.global native_scaled_sprite_pixels
native_scaled_sprite_pixels:
.incbin "generated/native-scaled-objects.4bpp"
