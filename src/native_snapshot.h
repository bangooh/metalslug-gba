#ifndef NATIVE_SNAPSHOT_H
#define NATIVE_SNAPSHOT_H
#include <stdint.h>
struct NativeLink;
/* Identical ROM fingerprints guarantee identical fixed addresses and ABI.
 * Destinations are local descriptors, never addresses supplied by the wire. */
struct NativeSnapshotRange {void*data;uint32_t bytes;};
struct NativeSnapshot {
 const struct NativeSnapshotRange*ranges;
 unsigned count;
 uint32_t words,position,hash;
 uint16_t*cache[2];uint32_t cache_base[2],cache_end[2];
};
struct NativeSnapshotEncoder {struct NativeSnapshot state;uint32_t recent[256];};
struct NativeSnapshotStats {uint32_t raw_bytes,encoded_bytes,packets,retries,refreshes;};
extern struct NativeSnapshotStats native_snapshot_stats;
int native_snapshot_init(struct NativeSnapshot*,const struct NativeSnapshotRange*,unsigned);
uint32_t native_snapshot_hash(struct NativeSnapshot*);
void native_snapshot_encoder(struct NativeSnapshotEncoder*,const struct NativeSnapshot*);
unsigned native_snapshot_encode(struct NativeSnapshotEncoder*,uint16_t*,unsigned);
int native_snapshot_decode(struct NativeSnapshot*,const uint16_t*,unsigned);
/* Blocking only while gameplay/audio are frozen. Serial and VBlank IRQs run.
 * Returns after verified transfer (1) or bounded no-progress timeout (0). */
int native_snapshot_transfer(struct NativeLink*,const struct NativeSnapshotRange*,unsigned,volatile uint32_t*clock,void (*progress)(unsigned));
#endif
