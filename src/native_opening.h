#ifndef NATIVE_OPENING_H
#define NATIVE_OPENING_H
#include <stddef.h>
#include "source_audio.h"
/* Standalone cinematic borrows gameplay arenas only while gameplay is stopped. */
int native_opening_begin(void *tiles,size_t tile_bytes,void *pixels,size_t pixel_bytes);
int native_opening_frame(unsigned elapsed,struct SourceAudioQueue *audio);
#endif
