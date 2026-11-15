.syntax unified
.cpu arm7tdmi
.arm
.section .gba_header,"ax",%progbits
.global _rom_start
_rom_start:
    b _start
    .space 188,0
.section .text.startup,"ax",%progbits
_start:
    mov r0,#0xD2
    msr cpsr_c,r0
    ldr sp,=0x03007FA0
    mov r0,#0xDF
    msr cpsr_c,r0
    ldr sp,=0x03007700
    ldr r0,=__iwram_load
    ldr r1,=__iwram_start
    ldr r2,=__iwram_end
1:  cmp r1,r2
    ldrlo r3,[r0],#4
    strlo r3,[r1],#4
    blo 1b
    ldr r0,=__data_load
    ldr r1,=__data_start
    ldr r2,=__data_end
2:  cmp r1,r2
    ldrlo r3,[r0],#4
    strlo r3,[r1],#4
    blo 2b
    ldr r1,=__bss_start
    ldr r2,=__bss_end
    mov r3,#0
3:  cmp r1,r2
    strlo r3,[r1],#4
    blo 3b
    ldr r1,=0x03006700
    ldr r2,=0x03007FA0
    ldr r3,=0xA55A3CC3
5:  cmp r1,r2
    strlo r3,[r1],#4
    blo 5b
    bl main
4:  b 4b
.ltorg
