.syntax unified
.cpu arm7tdmi
.arm
.text
.global __aeabi_uidivmod
.type __aeabi_uidivmod,%function
__aeabi_uidivmod:
 push {r4,lr}
 sub sp,sp,#8
 mov r2,sp
 add r3,sp,#4
 bl __divmod_u_helper
 ldr r0,[sp]
 ldr r1,[sp,#4]
 add sp,sp,#8
 pop {r4,pc}
.global __aeabi_idivmod
.type __aeabi_idivmod,%function
__aeabi_idivmod:
 push {r4,lr}
 sub sp,sp,#8
 mov r2,sp
 add r3,sp,#4
 bl __divmod_s_helper
 ldr r0,[sp]
 ldr r1,[sp,#4]
 add sp,sp,#8
 pop {r4,pc}
