#ifndef NATIVE_RESULTS_H
#define NATIVE_RESULTS_H
#include "weapons.h"
struct HighScoreRecord {uint32_t packed_score;uint16_t name[3];uint8_t owner,rank;};
struct ResultWorld {
 struct HighScoreRecord table[10];
 /*10E3B8/BA/BC snapshots, captured by the outer mission-end controller. */
 /* Reserved byte preserves the existing layout; no regional state remains. */
 uint8_t active[2],continues[2],prisoners[2],reserved_layout,summary_valid;
 /* Raw edge10E203/209 and repeat10E204/20A, before control remapping. */
 uint8_t edges[2],repeat[2],input_valid;
 /* Live BIOS player states and106ECE/CF, distinct from ending snapshots. */
 uint8_t live_active[2],mission,level,progress,progress_snapshot,mission_valid;
 /* Raw10E214/21A, distinct from the name editor's inputs. */
 uint8_t start[2],start_valid,vehicle_award;
 uint8_t ready;
};
void results_reset(struct ResultWorld*);
enum ResultEntry {RESULT_ENDING,RESULT_PANEL_P1,RESULT_PANEL_P2,RESULT_MISSION_TEXT,RESULT_WIPE,RESULT_PEACE};
int results_spawn(struct WeaponWorld*,struct EntityRef,enum ResultEntry);
/*97A60/97A72; creates the real input child or clears the parent's ack. */
int results_highscore_enter(struct WeaponWorld*,struct NativeEntity*,unsigned slot,uint16_t cell);
/*97CC4 standalone table editor; caller initializes its phase child count. */
int results_highscore_edit(struct WeaponWorld*,struct EntityRef,unsigned slot);
int mission_results_spawn(struct WeaponWorld*,struct EntityRef);
int mission_banner_spawn(struct WeaponWorld*,struct EntityRef,int complete);
void results_snapshot(struct ResultWorld*);
void results_continue(struct ResultWorld*,unsigned slot);
void results_clear_prisoners(struct WeaponWorld*);
#endif
