#ifndef NATIVE_SCENE_TEXT_H
#define NATIVE_SCENE_TEXT_H
#include "weapons.h"
enum SceneTextEntry {SCENE_TEXT_ENTRY_WATCHER,SCENE_TEXT_ENTRY_CONTINUED};
/* Gameplay GO watcher and continue notice. */
int scene_text_spawn(struct WeaponWorld*,struct EntityRef,enum SceneTextEntry);
/* Source46A96 (deep bottom border) and46AC6 (two-row bottom border). */
int scene_text_clear_border(struct SceneVideoWorld*,int deep);
int scene_text_hex(struct SceneVideoWorld*,unsigned column,unsigned row,uint16_t);
#endif
