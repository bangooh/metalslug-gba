#ifndef NATIVE_DISPLAY_SCALE_H
#define NATIVE_DISPLAY_SCALE_H
#include <stdint.h>
/* Presentation only. Physics, camera, attacks and collision retain source units.
 * 320x224 -> 240x168. Centre the image in the 240x160 GBA viewport. */
enum {NATIVE_SOURCE_WIDTH=320,NATIVE_SOURCE_HEIGHT=224,
 NATIVE_DISPLAY_WIDTH=240,NATIVE_DISPLAY_HEIGHT=160,
 NATIVE_SCALED_HEIGHT=168,NATIVE_CROP_TOP=4,NATIVE_CROP_BOTTOM=4};
static inline int32_t native_scale75(int32_t pixels){
 /* Defined floor for negative coordinates, without implementation-defined
  * signed shifts. Source positions fit signed16; intermediates fit signed32. */
 int32_t n=pixels*3;return n>=0?n/4:-((-n+3)/4);
}
static inline int32_t native_screen_x(int32_t x){return native_scale75(x);}
static inline int32_t native_screen_y(int32_t y){return native_scale75(y)-NATIVE_CROP_TOP;}
#endif
