#include "player_actions.h"
#include "../generated/player-programs.h"

static int fail(struct PlayerActionContext*c,unsigned code){c->fault=(uint8_t)code;return 0;}
static int sound(struct PlayerActionContext*c,uint16_t command,int stop){
 return c->sound&&c->sound(c->world,command,stop)?1:fail(c,1);
}
static int emit(struct PlayerActionContext*c,const struct PlayerEmission*e){
 return c->emit&&c->emit(c->world,e)?1:fail(c,2);
}
/* Constructors copy the source's integer coordinate words, not fractions. */
static int32_t coordinate(int32_t v,int offset){return (int16_t)((v>>8)+offset)*256;}
static struct PlayerEmission emission(struct PlayerActionContext*c,enum PlayerEmissionKind kind,int body){
 const struct PlayerTransform*t=body?&c->layer->transform:&c->body->actor.transform;
 struct PlayerEmission e={.transform=*t,.kind=kind,.owner=c->layer->owner,.damage_owner=c->layer->damage_owner};
 transform_integer_copy(&e.transform,t);
 return e;
}
static int casing(struct PlayerActionContext*c,unsigned variant,int low){
 struct PlayerEmission e=emission(c,variant==1?EMIT_ROCKET_CASING_UP:EMIT_ROCKET_CASING,1);
 if(variant==2)e.transform.facing^=1;
 e.transform.y=coordinate(e.transform.y,low?12:0);
 return emit(c,&e);
}
/* $03C710/$03C872/$03C9C4/$03CB16/$03CCD2: every callback uses the
 * same inventory and ownership. An empty special weapon is a successful
 * no-shot callback; it never changes to a pistol inside its animation. */
static int shot(struct PlayerActionContext*c,enum PlayerEmissionKind kind,unsigned direction,int spread,int low,int lateral){
 struct PlayerBodyState*p=c->body;
 struct PlayerActionLayer*l=c->layer;
 if(kind==EMIT_HMG)l->shot_phase=0;
 if(kind!=EMIT_PISTOL&&!p->ammo)return 1;
 if(kind==EMIT_PISTOL)l->shot_phase++;
 if(p->ammo)p->ammo--;
 if(kind==EMIT_SHOTGUN&&p->burst_remaining)p->burst_remaining--;
 static const uint16_t commands[]={0x10f7,0x10f6,0x10f3,0x10f5,0x10f4};
 if(!sound(c,commands[kind],0))return 0;
 struct PlayerEmission e=emission(c,kind,0);
 /* $517FE copies ownership only. Variant-based projectiles initialize their
  * own transform flags/priority; pistol/HMG copy the parent's priority. */
 e.transform.facing=0;e.transform.display_group=0;
 if(kind!=EMIT_PISTOL&&kind!=EMIT_HMG)e.transform.priority=0;
 e.direction=(uint8_t)direction;
 e.mirror=direction==1?0:(l->transform.facing&1)?255:0;
 /* $05DC1C fills payload with $FF. Rocket $9A is untouched by its fire
  * callback and selects the alternate horizontal stream in $09BF22. */
 e.phase=(kind==EMIT_PISTOL||kind==EMIT_HMG)?l->shot_phase:255;
 e.spread=(int8_t)spread;
 int height=kind==EMIT_SHOTGUN?0:kind==EMIT_HMG?20:28;
 e.transform.y=coordinate(e.transform.y,low-height);
 e.transform.x=coordinate(e.transform.x,(l->transform.facing&1)?-lateral:lateral);
 return emit(c,&e);
}
static int rocket(struct PlayerActionContext*c,unsigned variant,int low,int make_casing){
 /* The source creates a casing even when ammunition is already zero. */
 if(make_casing&&!casing(c,variant,low))return 0;
 int lateral=low||variant==1||variant==3?0:variant==2?-16:16;
 return shot(c,EMIT_ROCKET,variant,0,low?12:0,lateral);
}
static int hmg_pair(struct PlayerActionContext*c,unsigned first){
 return shot(c,EMIT_HMG,first<<3,0,0,0)&&shot(c,EMIT_HMG,(first+2)<<3,0,0,0);
}
static int grenade(struct PlayerActionContext*c,int back){
 if(!c->body->grenades)return 1;
 c->body->grenades--;
 struct PlayerEmission e=emission(c,EMIT_GRENADE_HEAVY,1);
 if(back)e.transform.facing^=1;
 /* These are absolute X offsets in the actual two callbacks. */
 e.transform.x=coordinate(e.transform.x,back?16:-16);
 if(!emit(c,&e))return 0;
 c->layer->action_flags|=4;return 1;
}
static int debris(struct PlayerActionContext*c,int dx,int down,unsigned priority){
 if(c->scene!=3)return 1;
 struct PlayerEmission e=emission(c,EMIT_SCENE3_DEBRIS,1);
 e.transform.priority=(uint16_t)((e.transform.priority&0xffe3)|priority);
 e.transform.x=coordinate(e.transform.x,(e.transform.facing&1)?-dx:dx);
 e.transform.y=coordinate(e.transform.y,down);return emit(c,&e);
}
int player_action_callback(struct PlayerActionContext*c,unsigned callback,struct Animation*a){
 if(c->fault)return 0;
 switch(callback){
  case PAC_RANDOM_LOOP:
   if(!c->rng)return fail(c,3);
   a->counter=(uint8_t)(source_random_word(c->rng)&10);return 1;
  case PAC_GRENADE_BACK:return grenade(c,1);
  case PAC_GRENADE_FORWARD:return grenade(c,0);
  case PAC_RETURN:return 1; /* Verified original $0332BA is exactly RTS. */
  case PAC_PISTOL_FORWARD:return shot(c,EMIT_PISTOL,0,0,0,0);
  case PAC_PISTOL_BACK:return shot(c,EMIT_PISTOL,128,0,0,0);
  case PAC_PISTOL_UP:return shot(c,EMIT_PISTOL,64,0,0,0);
  case PAC_PISTOL_DOWN:return shot(c,EMIT_PISTOL,192,0,0,0);
  case PAC_PISTOL_FORWARD_LOW:return shot(c,EMIT_PISTOL,0,0,12,0);
  case PAC_SHOTGUN_FORWARD:case PAC_SHOTGUN_FORWARD_B:return shot(c,EMIT_SHOTGUN,0,0,0,0);
  case PAC_SHOTGUN_UP:return shot(c,EMIT_SHOTGUN,1,0,0,0);
  case PAC_SHOTGUN_BACK:return shot(c,EMIT_SHOTGUN,2,0,0,0);
  case PAC_SHOTGUN_DOWN_LOW:return shot(c,EMIT_SHOTGUN,3,0,12,0);
  case PAC_FLAME_FORWARD:return shot(c,EMIT_FLAME,0,0,0,0);
  case PAC_FLAME_UP:return shot(c,EMIT_FLAME,1,0,0,0);
  case PAC_FLAME_BACK:return shot(c,EMIT_FLAME,2,0,0,0);
  case PAC_FLAME_DOWN_LOW:return shot(c,EMIT_FLAME,3,0,12,0);
  case PAC_FLAME_FORWARD_LOW:return shot(c,EMIT_FLAME,0,0,12,0);
  case PAC_ROCKET_FORWARD:return rocket(c,0,0,1);
  case PAC_ROCKET_UP:return rocket(c,1,0,1);
  case PAC_ROCKET_BACK:return rocket(c,2,0,1);
  case PAC_ROCKET_FORWARD_LOW:return rocket(c,0,1,1);
  case PAC_ROCKET_DOWN_LOW:return rocket(c,3,1,0);
  case PAC_HMG_FORWARD_NEG:return shot(c,EMIT_HMG,0,-2,0,0);
  case PAC_HMG_FORWARD:return shot(c,EMIT_HMG,0,0,0,0);
  case PAC_HMG_FORWARD_POS:return shot(c,EMIT_HMG,0,2,0,0);
  case PAC_HMG_BACK_NEG:return shot(c,EMIT_HMG,128,-2,0,0);
  case PAC_HMG_BACK:return shot(c,EMIT_HMG,128,0,0,0);
  case PAC_HMG_BACK_POS:return shot(c,EMIT_HMG,128,2,0,0);
  case PAC_HMG_UP_NEG:return shot(c,EMIT_HMG,64,-2,0,0);
  case PAC_HMG_UP:return shot(c,EMIT_HMG,64,0,0,0);
  case PAC_HMG_UP_POS:return shot(c,EMIT_HMG,64,2,0,0);
  case PAC_HMG_DOWN_NEG:return shot(c,EMIT_HMG,192,-2,0,0);
  case PAC_HMG_DOWN:return shot(c,EMIT_HMG,192,0,0,0);
  case PAC_HMG_DOWN_POS:return shot(c,EMIT_HMG,192,2,0,0);
  case PAC_HMG_SPREAD_1_3:return hmg_pair(c,1);
  case PAC_HMG_SPREAD_5_7:return hmg_pair(c,5);
  case PAC_HMG_SPREAD_9_11:return hmg_pair(c,9);
  case PAC_HMG_SPREAD_13_15:return hmg_pair(c,13);
  case PAC_HMG_SPREAD_17_19:return hmg_pair(c,17);
  case PAC_HMG_SPREAD_21_23:return hmg_pair(c,21);
  case PAC_HMG_SPREAD_25_27:return hmg_pair(c,25);
  case PAC_HMG_SPREAD_29_31:return hmg_pair(c,29);
  case PAC_MELEE:
   hit_begin_attack(&c->layer->hit);
   if(!c->attack||!c->layer->attack||!c->attack(c->world,&c->layer->hit,c->layer->attack))return fail(c,4);
   return !(c->layer->hit.events&4)||sound(c,0x10fb,0);
  case PAC_BODY_FX:{
   /* The ASM file omits $033382..$033388; the matched ROM contains
    * JSR $5DD02; RTS, so this effect DOES inherit the body transform. */
   struct PlayerEmission e=emission(c,EMIT_BODY_FX,1);return emit(c,&e);
  }
  case PAC_SCENE3_BASE:return debris(c,0,0,16);
  case PAC_SCENE3_LOW8:return debris(c,0,-8,16);
  case PAC_SCENE3_UP_D:return debris(c,3,13,16);
  case PAC_SCENE3_UP_C:return debris(c,0,12,16);
  case PAC_SCENE3_UP_B:return debris(c,-4,11,16);
  case PAC_SCENE3_PRIORITY4:return debris(c,-3,6,4);
  default:return fail(c,5);
 }
}
static uint32_t read_field(void*user,unsigned field,unsigned width){
 struct PlayerActionContext*c=user;struct PlayerActionLayer*l=c->layer;
 if(width!=1){fail(c,6);return 0;}
 switch(field){
  case PAF_ACTION_FLAGS:return l->action_flags;
  case PAF_POSE:return l->animation.pose;
  case PAF_PRIORITY_HIGH:return l->transform.priority>>8;
  case PAF_LOCAL_WEAPON:return l->local_weapon;
  default:fail(c,6);return 0;
 }
}
static int write_field(void*user,unsigned field,unsigned width,enum AnimationFieldOperation op,uint32_t value){
 struct PlayerActionContext*c=user;struct PlayerActionLayer*l=c->layer;
 uint32_t previous=read_field(user,field,width);if(c->fault)return 0;
 switch(op){
  case AN_FIELD_SET:break;
  case AN_FIELD_AND:value&=previous;break;
  case AN_FIELD_OR:value|=previous;break;
  case AN_FIELD_XOR:value^=previous;break;
  default:return fail(c,7);
 }
 switch(field){
  case PAF_ACTION_FLAGS:l->action_flags=(uint8_t)value;break;
  case PAF_POSE:l->animation.pose=(uint8_t)value;break;
  case PAF_PRIORITY_HIGH:l->transform.priority=(uint16_t)((l->transform.priority&255)|((value&255)<<8));break;
  case PAF_LOCAL_WEAPON:l->local_weapon=(uint8_t)value;break;
  default:return fail(c,6);
 }
 return 1;
}
static int callback(void*user,unsigned id,struct Animation*a){return player_action_callback(user,id,a);}
static int play_sound(void*user,uint16_t command,int stop){return sound(user,command,stop);}
static int bind(void*user,unsigned kind,uint32_t value){
 struct PlayerActionContext*c=user;
 if(kind!=AN_DEFENSE)return fail(c,15);
 if(value==0xffffffffu)c->layer->defense=0;
 else if(value<PLAYER_ANIMATION_BINDINGS)c->layer->defense=&player_animation_defenses[value];
 else return fail(c,15);
 return 1;
}
int player_action_begin(struct PlayerActionContext*c,unsigned table){
 if(table>=PLAYER_PROGRAM_TABLES||c->body->character>=2||c->body->actor.local_weapon>=WEAPON_COUNT)return fail(c,8);
 animation_begin(&c->layer->animation,player_animation_entries[table][c->body->character][c->body->actor.local_weapon]);return 1;
}
int player_action_step(struct PlayerActionContext*c){
 const struct AnimationOwner owner={.context=c,.read=read_field,.write=write_field,.call=callback,.sound=play_sound,.bind=bind};
 if(c->fault)return 0;
 return animation_step(&c->layer->animation,&player_animation_program,&owner);
}
void player_action_timers(struct PlayerActionLayer*l){
 if(l->animation.freeze)l->animation.freeze--;
 if(l->damage_timer)l->damage_timer--;
 if(l->hit.invulnerable)l->hit.invulnerable--;
}
