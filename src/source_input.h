#ifndef NATIVE_SOURCE_INPUT_H
#define NATIVE_SOURCE_INPUT_H
#include <stdint.h>
/*25066/5CC1A: raw six-byte records and separate controller snapshots. */
struct SourceInputRecord {uint8_t reserved,previous,held,edges,repeat,timer;};
enum SourceInputMode {SOURCE_INPUT_PHYSICAL,SOURCE_INPUT_REPLAY,SOURCE_INPUT_DONE};
struct SourceInput {
 struct SourceInputRecord raw[2],active[2];
 const uint8_t *replay;
 uint16_t block,cursor;
 uint8_t elapsed,replay_held[2],mode,repeat_delay,repeat_interval;
 uint8_t system,system_edges,fault;
};
void source_input_reset(struct SourceInput*);
/*24FEC: indices0/1 select the two original512-byte streams; other indices
 * restore physical input. The extracted513th byte preserves source lookahead. */
void source_input_mode(struct SourceInput*,unsigned);
int source_input_frame(struct SourceInput*,const uint8_t held[2],uint8_t system,
 const uint8_t live_active[2]);
#endif
