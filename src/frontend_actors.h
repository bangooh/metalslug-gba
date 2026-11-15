#ifndef NATIVE_FRONTEND_ACTORS_H
#define NATIVE_FRONTEND_ACTORS_H
#include "play_world.h"
enum NativePresentation {NPRESENT_NONE,NPRESENT_LOGO,NPRESENT_RANKING,NPRESENT_TUTORIAL};
int frontend_actors_begin(struct NativePlayWorld*,enum NativePresentation);
int frontend_actors_done(const struct NativePlayWorld*);
#endif
