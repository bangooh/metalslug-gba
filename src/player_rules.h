#ifndef NATIVE_PLAYER_RULES_H
#define NATIVE_PLAYER_RULES_H
#include "player_actions.h"
enum PlayerButtons {PB_UP=1,PB_DOWN=2,PB_LEFT=4,PB_RIGHT=8,PB_FIRE=16,PB_JUMP=32,PB_GRENADE=64};
struct PlayerSourceInput {
 uint8_t held;
 uint8_t actions; /* Original action latch (input table 3), not assumed edges. */
};
enum PlayerIntent {INTENT_NONE,INTENT_FIRE,INTENT_UP,INTENT_BACK,INTENT_DOWN,INTENT_GRENADE,INTENT_MELEE};
enum PlayerPickupResult {PICKUP_IGNORED,PICKUP_AMMO_NOTICE,PICKUP_SCORE_NOTICE};
void player_inventory_init(struct PlayerBodyState*);
int player_set_weapon_ammo(struct PlayerBodyState*,unsigned weapon,unsigned quantity,int change_weapon);
int player_refill_ammo(struct PlayerBodyState*);
enum PlayerPickupResult player_add_ammo_item(struct PlayerBodyState*,uint8_t normal,uint8_t hmg);
void player_add_grenades(struct PlayerBodyState*,uint8_t quantity);
uint16_t player_hud_ammo(const struct PlayerBodyState*);
void player_count_projectile(struct PlayerBodyState players[2],uint8_t damage_owner,int grenade);
int player_decrement_grenade_word(struct PlayerBodyState*);
uint8_t player_read_aim_direction(struct PlayerBodyState*,struct PlayerSourceInput,int vertical_only);
enum PlayerIntent player_select_action(struct PlayerActionContext*,struct PlayerSourceInput,int down_only);
void player_begin_weapon_flash(struct PlayerBodyState*);
int player_step_weapon_flash(struct PlayerBodyState*);
int player_announce_weapon(struct PlayerActionContext*);
#endif
