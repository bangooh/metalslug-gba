#ifndef NATIVE_LINK_H
#define NATIVE_LINK_H
#include "session.h"
enum {NLINK_WORDS=64,NLINK_PROTOCOL=2,NLINK_TIMEOUT=900};
enum NativeLinkState {NLINK_HELLO,NLINK_READY,NLINK_RUNNING,NLINK_LEFT,NLINK_LOST,NLINK_MISMATCH,NLINK_DESYNC,NLINK_FAULT};
struct NativeLinkDiagnostic {uint32_t expected,actual;uint16_t sio_initial,sio_last,words,code;uint8_t local_slot,peer_slot;};
extern volatile struct NativeLinkDiagnostic native_link_diagnostic;
struct NativeLinkConfig {
 uint32_t build[4],epoch;
 struct SourceRng rng;
 struct NativeSessionConfig session;
};
struct NativeLinkInput {uint16_t keys;uint8_t credits,audio_reply;uint32_t checksum;};
struct NativeLinkPeerFrame {struct NativeLinkInput input;uint32_t number;uint8_t valid;};
struct NativeLink {
 struct NativeLinkConfig config;
 struct NativeLinkInput local;
 struct NativeLinkPeerFrame peer[2];
 uint32_t config_hash,frame,peer_ack,received_frame,age;
 volatile uint32_t bad_packets;
 volatile uint8_t state;
 uint8_t slot,prepared,ack_valid,received_valid,leave,agreed;
 uint8_t control,peer_control,peer_running;
};
enum NativeLinkControl {NLCTRL_IDLE,NLCTRL_NEW,NLCTRL_JOIN,NLCTRL_ACCEPT,NLCTRL_STREAM,NLCTRL_COMMIT,NLCTRL_DONE};
enum {NLINK_BULK_DATA=0x5344,NLINK_BULK_ACK=0x5341};
uint16_t native_link_crc(const uint16_t*,unsigned);
/* Host supplies the initial game config and ALL33 RNG words. Guest adopts
 * them after matching the128-bit build fingerprint, before session_begin.
 * There is no inferred/repeated remote input and no automatic solo fallback. */
int native_link_init(struct NativeLink*,unsigned slot,const struct NativeLinkConfig*);
int native_link_config(struct NativeLinkConfig*,const struct NativeSessionConfig*,const struct SourceRng*,uint32_t epoch);
/* Master only, while idle. Call with serial IRQs excluded. */
int native_link_select_character(struct NativeLink*,unsigned character);
int native_link_start(struct NativeLink*);
unsigned native_link_packet(const struct NativeLink*,uint16_t out[NLINK_WORDS]);
int native_link_receive(struct NativeLink*,const uint16_t*,unsigned words);
void native_link_vblank(struct NativeLink*);
void native_link_leave(struct NativeLink*);
int native_link_offer(struct NativeLink*,const struct NativeLinkInput*);
/*1: committed pair;0: barrier waiting;-1: stop/error. Both sides must ACK
 * the same input opportunity before either simulation may advance. */
int native_link_take(struct NativeLink*,struct NativeLinkInput out[2]);
uint32_t native_link_frame_checksum(const struct NativeLink*,const struct NativePlayWorld*);
uint32_t native_link_world_checksum(const struct NativePlayWorld*);
/* Actual16-bit wired multiplayer, two cartridges. Hardware slot is selected
 * by cable parent/first-child, not user input. Timer2 is the pacing clock. */
int native_link_gba_init(struct NativeLink*,const struct NativeLinkConfig*);
int native_link_gba_poll(void);
int native_link_gba_leave_complete(void);
void native_link_gba_irq(void);
void native_link_gba_stop(void);
/* Frozen-world transfer uses the existing framed, paced serial transport. */
void native_link_gba_bulk(void (*receive)(void*,const uint16_t*,unsigned),void*);
void native_link_gba_send(const uint16_t*,unsigned);
#endif
