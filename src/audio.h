#ifndef GBA_AUDIO_H
#define GBA_AUDIO_H
/* Audio is presentation state, never part of the deterministic Game snapshot. */
#if !defined(__arm__) && !defined(__thumb__) && !defined(NATIVE_AUDIO_CAPTURE)
static inline void audio_command(unsigned command){(void)command;}
static inline void audio_init(void){}
static inline void audio_scene(unsigned scene){(void)scene;}
#else
void audio_command(unsigned command);
void audio_init(void);
void audio_scene(unsigned scene);
#endif
#endif
