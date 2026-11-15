/* Existing converted mission data, linked for dependency compilation.
 * These are source event data and skip boundaries, not CPU instructions. */
.section .rodata
.balign 4
.global mission_data
mission_data:
.incbin "generated/mission.bin"
.balign 4
.global mission_next
mission_next:
.incbin "generated/mission.skip"
