#ifndef NATIVE_GBA_INTERRUPTS_H
#define NATIVE_GBA_INTERRUPTS_H
#include <stdint.h>
typedef void(*NativeIrqCallback)(void*);
/* Hardware integration uses one vector; audio must not replace serial or
 * VBlank. Registration masks only IME and restores its previous state. */
int native_irq_set(unsigned bit,NativeIrqCallback,void*);
void native_irq_install(void);
void native_irq_enable(void);
#endif
