#ifndef NATIVE_INFLATE_H
#define NATIVE_INFLATE_H
#include <stddef.h>
/* Raw DEFLATE for trusted generated cartridge assets. Bounds output; VRAM
 * writes use halfwords because GBA does not support byte stores to VRAM. */
int native_inflate(const unsigned char *source,void *output,size_t bytes,int vram);
#endif
