#ifndef NATIVE_FRONTEND_H
#define NATIVE_FRONTEND_H
#include "results.h"
enum NativeFrontendScreen {NFRONT_TITLE,NFRONT_CONTROLS,NFRONT_RANKING,NFRONT_LINK_WAIT,NFRONT_LINK_ERROR,NFRONT_RETURN,NFRONT_ERROR,NFRONT_LINK_SYNC};
/* GBA navigation shell. Original title pixels, live source ranking table;
 * native button/link guidance. Animation/timing policy lives in the app. */
/* START wins if both start buttons arrive on the same input edge. */
static inline unsigned native_title_character(uint16_t edges){return (edges&4)&&!(edges&8);}
void native_frontend_show(enum NativeFrontendScreen,unsigned selection,const struct HighScoreRecord*,unsigned detail);
void native_frontend_link_progress(unsigned);
void native_frontend_title_tick(unsigned age);
#endif
