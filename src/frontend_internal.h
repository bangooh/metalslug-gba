#ifndef NATIVE_FRONTEND_INTERNAL_H
#define NATIVE_FRONTEND_INTERNAL_H
#include "frontend_actors.h"
#include "tank_internal.h"
#include "result_internal.h"
#include "session.h"
#include "../generated/frontend-data.h"
enum FrontendKind {FR_LOGO,FR_PIECE,FR_CENTER,FR_RANKING,FR_RANK_LOGO,FR_TUTORIAL,FR_DIRECTION,FR_BUTTON,FR_LIT,FR_ANCHOR,FR_DROP};
struct NativeEntity*frontend_allocate(struct WeaponWorld*,struct EntityRef,unsigned,int,int,unsigned,unsigned,unsigned);
void frontend_start(struct EnemyActor*,unsigned);
int frontend_animate(struct WeaponWorld*,struct NativeEntity*);
int frontend_tutorial_init(struct NativePlayWorld*,struct NativeEntity*);
int frontend_tutorial_tick(struct NativePlayWorld*,struct NativeEntity*);
int frontend_finish(struct NativePlayWorld*);
struct PlayerTransform frontend_transform(struct WeaponWorld*,int,int,unsigned);
#endif
