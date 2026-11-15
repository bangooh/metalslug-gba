#include "gba_interrupts.h"
#define R16(a) (*(volatile uint16_t*)(a))
static struct {NativeIrqCallback call;void*context;} handlers[14];
__attribute__((target("arm"),section(".iwram.irq"))) static void dispatch(void){
 uint16_t pending=R16(0x04000200)&R16(0x04000202);
 /* Read serial first, before any audio mixing work. Each callback owns
  * its IF acknowledgement; do not clear a new same-bit event afterward. */
 const uint8_t order[14]={7,0,6,9,1,2,3,4,5,8,10,11,12,13};
 for(unsigned i=0;i<14;i++){unsigned bit=order[i];if(!(pending&(1u<<bit)))continue;
  if(handlers[bit].call)handlers[bit].call(handlers[bit].context);else R16(0x04000202)=(uint16_t)(1u<<bit);
 }
 /* BIOS IntrWait mirror. Preserve flags already accumulated by BIOS users. */
 *(volatile uint16_t*)0x03007ff8u|=pending;
}
int native_irq_set(unsigned bit,NativeIrqCallback call,void*ctx){
 if(bit>=14)return 0;uint16_t ime=R16(0x04000208);R16(0x04000208)=0;
 handlers[bit].context=ctx;handlers[bit].call=call;__asm__ volatile("":::"memory");R16(0x04000208)=ime;return 1;
}
void native_irq_install(void){
 R16(0x04000208)=0;*(volatile uintptr_t*)0x03007ffcu=(uintptr_t)dispatch;
}
__attribute__((target("arm"))) void native_irq_enable(void){
 __asm__ volatile("mrs r0,cpsr\n\tbic r0,r0,#0x80\n\tmsr cpsr_c,r0":::"r0","memory");R16(0x04000208)=1;
}
