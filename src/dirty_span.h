#ifndef NATIVE_DIRTY_SPAN_H
#define NATIVE_DIRTY_SPAN_H
#include <stdint.h>
/* Return each maximal dirty interval once. Full words advance in one step;
 * the VBlank path must not test every tile in a large scene reload. */
static inline unsigned native_dirty_span(const uint32_t*bits,unsigned limit,unsigned*cursor,unsigned*first){
 unsigned at=*cursor;
 while(at<limit){
  uint32_t mask=bits[at>>5]&(0xffffffffu<<(at&31));
  if(!mask){at=(at|31)+1;continue;}
  unsigned begin=(at&~31u)+__builtin_ctz(mask);
  if(begin>=limit)break;
  at=begin;
  do{
   mask=bits[at>>5]>>(at&31);
   unsigned length=mask==0xffffffffu?32:__builtin_ctz(~mask);
   at+=length;
   if(at>=limit){at=limit;break;}
   if(!length||(at&31))break;
  }while(1);
  *cursor=at;*first=begin;return at-begin;
 }
 *cursor=limit;return 0;
}
#endif
