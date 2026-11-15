#include <stdint.h>
#include <stddef.h>
#ifdef __arm__
/* Leaf routines in 32-bit IWRAM. Batch aligned RAM/ROM accesses while
 * retaining byte-safe tails, arbitrary alignment, and the AAPCS return value.
 * memcpy retains its standard non-overlapping-buffer contract. */
__asm__(
 ".pushsection .iwram.memory,\"ax\",%progbits\n.arm\n.balign 4\n"
 ".global memset\n.type memset,%function\nmemset:\n"
 "mov ip,r0\n"
 "and r1,r1,#255\n"
 "cmp r2,#0\n"
 "bxeq lr\n"
 "1: tst ip,#3\n"
 "beq 2f\n"
 "strb r1,[ip],#1\n"
 "subs r2,r2,#1\n"
 "bne 1b\n"
 "bx lr\n"
 "2: orr r1,r1,r1,lsl #8\n"
 "orr r1,r1,r1,lsl #16\n"
 "cmp r2,#16\n"
 "blo 4f\n"
 "stmdb sp!,{r4,r5}\n"
 "mov r3,r1\nmov r4,r1\nmov r5,r1\n"
 "3: stmia ip!,{r1,r3-r5}\n"
 "sub r2,r2,#16\n"
 "cmp r2,#16\n"
 "bhs 3b\n"
 "ldmia sp!,{r4,r5}\n"
 "4: cmp r2,#4\n"
 "strhs r1,[ip],#4\n"
 "subhs r2,r2,#4\n"
 "bhs 4b\n"
 "cmp r2,#0\n"
 "bxeq lr\n"
 "5: strb r1,[ip],#1\n"
 "subs r2,r2,#1\n"
 "bne 5b\n"
 "bx lr\n.size memset,.-memset\n"
 ".global memcpy\n.type memcpy,%function\nmemcpy:\n"
 "mov ip,r0\n"
 "eor r3,r0,r1\n"
 "tst r3,#3\n"
 "bne 5f\n"
 "cmp r2,#0\n"
 "bxeq lr\n"
 "1: tst ip,#3\n"
 "beq 2f\n"
 "ldrb r3,[r1],#1\n"
 "strb r3,[ip],#1\n"
 "subs r2,r2,#1\n"
 "bne 1b\n"
 "bx lr\n"
 "2: cmp r2,#16\n"
 "blo 4f\n"
 "stmdb sp!,{r4-r6}\n"
 "3: ldmia r1!,{r3-r6}\n"
 "stmia ip!,{r3-r6}\n"
 "sub r2,r2,#16\n"
 "cmp r2,#16\n"
 "bhs 3b\n"
 "ldmia sp!,{r4-r6}\n"
 "4: cmp r2,#4\n"
 "ldrhs r3,[r1],#4\n"
 "strhs r3,[ip],#4\n"
 "subhs r2,r2,#4\n"
 "bhs 4b\n"
 "5: cmp r2,#0\n"
 "bxeq lr\n"
 "6: ldrb r3,[r1],#1\n"
 "strb r3,[ip],#1\n"
 "subs r2,r2,#1\n"
 "bne 6b\n"
 "bx lr\n.size memcpy,.-memcpy\n.popsection\n");
#else
__attribute__((section(".iwram.memory"))) void*memset(void*dst,int value,size_t size){
 uint8_t*p=dst;while(size&&((uintptr_t)p&3)){*p++=(uint8_t)value;size--;}
 uint32_t v=(uint8_t)value;v*=0x01010101u;while(size>=4){*(uint32_t*)p=v;p+=4;size-=4;}
 while(size--)*p++=(uint8_t)value;return dst;
}
__attribute__((section(".iwram.memory"))) void*memcpy(void*dst,const void*src,size_t size){
 uint8_t*d=dst;const uint8_t*s=src;
 if(!(((uintptr_t)d^(uintptr_t)s)&3)){
  while(size&&((uintptr_t)d&3)){*d++=*s++;size--;}
  while(size>=4){*(uint32_t*)d=*(const uint32_t*)s;d+=4;s+=4;size-=4;}
 }
 while(size--)*d++=*s++;return dst;
}
#endif
