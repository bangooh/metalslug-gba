#ifndef M1_RUNTIME_H
#define M1_RUNTIME_H
#include <stdint.h>
struct NativeVideo;
struct NativeSnapshotRange;
const uint16_t*m1_runtime_palette(void);
const uint16_t*m1_runtime_palette_bindings(void);
void m1_runtime_attach(struct NativeVideo*);
int m1_runtime_prepare(struct NativeVideo*);
int m1_runtime_commit(struct NativeVideo*);
unsigned m1_runtime_heavy_transfer_bytes(struct NativeVideo*);
void m1_runtime_prestage_background(struct NativeVideo*);
void m1_runtime_prestage_objects(struct NativeVideo*,unsigned background_bytes);
void m1_runtime_load(unsigned native);
/* 1 while the native-mission IWRAM overlay (.m1hot) is resident. */
extern volatile unsigned char m1_overlay_kind;
void m1_runtime_restore(struct NativeVideo*);
unsigned m1_runtime_snapshot(struct NativeVideo*,struct NativeSnapshotRange*);
void m1_runtime_rebuild(struct NativeVideo*);
#endif
