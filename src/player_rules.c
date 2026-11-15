#include "player_rules.h"
#include "../generated/player-rules-data.h"
#include "../generated/player-body-data.h"

void player_inventory_init(struct PlayerBodyState*p){
 /* $032A02/$0517AA. World placement, life count and child allocation belong
  * to the parent spawn state and are deliberately not reset here. */
 p->body_contact=1;p->grenades=10;p->ammo=0;p->burst_remaining=1;p->actor.local_weapon=WEAPON_PISTOL;
 p->actor.action_flags=p->palette_signal=p->arm_busy=p->action=0;
 p->actor.aim=0;p->aim_direction=1;p->arm_offset_x=p->arm_offset_y_up=0;
 p->actor.owner=p->character;p->actor.damage_owner=(uint8_t)(p->character+1);
 p->actor.shot_phase=p->actor.melee_selector=255;
 p->active_grenades=p->active_shots=255;
 p->actor.transform.priority=0x8010;p->actor.hit.type_flags|=64;p->actor.hit.flags&=(uint8_t)~8u;
}
int player_set_weapon_ammo(struct PlayerBodyState*p,unsigned weapon,unsigned quantity,int change_weapon){
 /* $032B58/$032B6A. The asymmetry between same/new weapons is present in
  * the ROM: do not replace it with a conventional refill policy. */
 weapon&=255;quantity&=255;
 if(weapon>=WEAPON_COUNT||p->actor.local_weapon>=WEAPON_COUNT)return 0;
 if(weapon==p->actor.local_weapon){
  if(quantity)quantity=player_default_ammo[weapon];
  p->ammo=(uint16_t)(p->ammo+quantity);
 }else{
  if(!quantity)quantity=player_default_ammo[weapon];
  p->ammo=(uint16_t)quantity;
 }
 if(change_weapon)p->actor.local_weapon=(uint8_t)weapon;return 1;
}
static uint16_t add_ammo(uint16_t current,uint16_t quantity){
 uint16_t value=(uint16_t)(current+quantity);
 /* Source ADD.W then signed BLE, including overflow/wrap of the stored word. */
 return (int16_t)value>999?999:value;
}
int player_refill_ammo(struct PlayerBodyState*p){
 if(p->actor.local_weapon>=WEAPON_COUNT)return 0;
 if(p->actor.local_weapon!=WEAPON_PISTOL)p->ammo=add_ammo(p->ammo,player_default_ammo[p->actor.local_weapon]);
 return 1;
}
enum PlayerPickupResult player_add_ammo_item(struct PlayerBodyState*p,uint8_t normal,uint8_t hmg){
 unsigned quantity=p->actor.local_weapon==WEAPON_HMG?hmg:normal;
 if(quantity==255)return PICKUP_IGNORED;
 if(p->actor.local_weapon==WEAPON_PISTOL)return PICKUP_SCORE_NOTICE;
 p->ammo=add_ammo(p->ammo,(uint16_t)quantity);return PICKUP_AMMO_NOTICE;
}
void player_add_grenades(struct PlayerBodyState*p,uint8_t quantity){
 if(quantity==255)return;
 unsigned value=(uint8_t)(p->grenades+quantity);
 p->grenades=(uint8_t)(value>99?99:value);
}
uint16_t player_hud_ammo(const struct PlayerBodyState*p){return p->actor.local_weapon==WEAPON_PISTOL?65535:p->ammo;}
void player_count_projectile(struct PlayerBodyState players[2],uint8_t damage_owner,int grenade){
 /* Original $032AFA/$032B36 sends every owner other than 1 to slot 2. */
 struct PlayerBodyState*p=&players[damage_owner==1?0:1];
 if(grenade)p->active_grenades++;else p->active_shots++;
}
int player_decrement_grenade_word(struct PlayerBodyState*p){
 /* $032FD4 really uses SUB.W at the grenade byte, including the adjacent
  * live-grenade counter. Preserve that behavior for its actual callers. */
 if(!p->grenades)return 0;
 uint16_t pair=(uint16_t)(((unsigned)p->grenades<<8)|p->active_grenades);
 pair--;p->grenades=(uint8_t)(pair>>8);p->active_grenades=(uint8_t)pair;return 1;
}
uint8_t player_read_aim_direction(struct PlayerBodyState*p,struct PlayerSourceInput input,int vertical_only){
 unsigned previous=p->aim_direction;p->aim_direction=1;
 if(!vertical_only&&(input.held&PB_DOWN))p->aim_direction=8;
 if(input.held&PB_UP)p->aim_direction=4;
 return (uint8_t)((previous<<4)|p->aim_direction);
}
static int consume_grenade_count(struct PlayerBodyState*p){
 unsigned count=p->active_grenades;p->active_grenades=0;return count<=1;
}
static int consume_shot_count(struct PlayerBodyState*p){
 unsigned count=p->active_shots;p->active_shots=0;
 return p->actor.local_weapon<WEAPON_COUNT&&count<=player_shot_limits[p->actor.local_weapon];
}
static enum PlayerIntent fire_intent(struct PlayerActionContext*c,int down){
 struct PlayerBodyState*p=c->body;
 p->actor.hit.x=(int16_t)(p->actor.transform.x>>8);p->actor.hit.y=transform_y_up(&p->actor.transform);
 p->actor.hit.flip=p->actor.transform.facing;
 p->actor.attack=&player_body_attacks[PATT_ACTION_PROBE];
 hit_begin_attack(&p->actor.hit);
 if(!c->attack||!c->attack(c->world,&p->actor.hit,p->actor.attack)){c->fault=13;return INTENT_NONE;}
 if(p->actor.hit.flags&2)return INTENT_MELEE;
 if(!p->ammo&&p->actor.local_weapon!=WEAPON_PISTOL)return INTENT_NONE;
 if(down)return INTENT_DOWN;
 switch(p->aim_direction){
  case 4:return INTENT_UP;
  case 8:return INTENT_DOWN;
  case 2:return INTENT_BACK;
  default:return INTENT_FIRE;
 }
}
enum PlayerIntent player_select_action(struct PlayerActionContext*c,struct PlayerSourceInput input,int down_only){
 /* $0330D0/$0331AE. Probe before ammo rejection permits melee with an empty
  * special weapon. Crouching reverses grenade/fire priority. Both count
  * consumers are destructive reads even when their button is not active. */
 struct PlayerBodyState*p=c->body;
 if(p->actor.action_flags&4)return INTENT_NONE;
 if(input.held&PB_DOWN){
  if(consume_grenade_count(p)&&(input.actions&PB_GRENADE))return INTENT_GRENADE;
  if(consume_shot_count(p)&&(input.actions&PB_FIRE))return fire_intent(c,1);
  return INTENT_NONE;
 }
 if(down_only)return INTENT_NONE;
 if(consume_shot_count(p)&&(input.actions&PB_FIRE)){
  /* Empty-special branch returns before consuming the grenade count. */
  return fire_intent(c,0);
 }
 if(consume_grenade_count(p)&&(input.actions&PB_GRENADE))return INTENT_GRENADE;
 return INTENT_NONE;
}
void player_begin_weapon_flash(struct PlayerBodyState*p){p->palette_signal=5;p->actor.action_flags|=16;}
int player_step_weapon_flash(struct PlayerBodyState*p){
 if(p->palette_signal)p->palette_signal--;
 switch(p->palette_signal&15){
  case 0:case 3:p->actor.palette=p->actor.palette_base;break;
  case 1:case 2:p->actor.palette=p->actor.palette_impact;break;
  case 4:p->actor.palette=p->actor.palette_alternate;break;
  default:return 0;
 }
 return 1;
}
int player_announce_weapon(struct PlayerActionContext*c){
 /* Return 1 if the pending switch was consumed, 0 if absent, -1 on failure. */
 struct PlayerBodyState*p=c->body;
 if(!(p->actor.action_flags&16))return 0;
 p->actor.action_flags&=(uint8_t)~16u;
 if(p->actor.local_weapon>=WEAPON_COUNT){c->fault=14;return -1;}
 unsigned command=player_weapon_announcements[p->actor.local_weapon];
 if(command!=65535&&(!c->sound||!c->sound(c->world,(uint16_t)command,0))){c->fault=1;return -1;}
 return 1;
}
