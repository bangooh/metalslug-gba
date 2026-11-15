#include "slug_body.h"
#include "slug_aux.h"
#include "slug_projectile.h"
#include "slug_death_fx.h"
#include "slug_children.h"
#include "slug_surface.h"
#include "../generated/slug-body-data.h"
#include "../generated/slug-aux-programs.h"
#include "../generated/slug-support-data.h"
/* Body handlers, translated by dependency family from $02B38C..$02FFE0.
 * Source addresses/aliases live in the ledger.
 * schedule() models a handler write for NEXT tick. Only entry aliases below
 * fall through to another entry/frame in THIS tick. Later checks may replace
 * an earlier schedule; do not turn these sequences into else-if priorities. */
static struct SlugSharedState*body(struct SlugBodyContext*c){return c->animation.slug;}
static int fail(struct SlugSharedState*s,unsigned code){if(!s->fault)s->fault=(uint8_t)code;return 0;}
static int16_t word(int v){return (int16_t)(uint16_t)v;}
static int16_t clamp(int16_t v,int limit){return v>limit?(int16_t)limit:v<-limit?(int16_t)-limit:v;}
static int held(struct SlugBodyContext*c,unsigned key){return !!(c->input.held&key);}
void slug_body_schedule_argument(struct SlugSharedState*s,enum SlugBodyState state,uint8_t argument){
 s->body_state=(uint8_t)state;s->body_enter=1;s->body_argument=argument;
}
void slug_body_schedule(struct SlugSharedState*s,enum SlugBodyState state){slug_body_schedule_argument(s,state,0);}
void slug_body_schedule_speed(struct SlugSharedState*s,int16_t velocity){
 slug_body_schedule_argument(s,SB_SET_SPEED,1);s->body_parameter=velocity;
}
int slug_body_implemented(enum SlugBodyState state){return (unsigned)state<SB_STATE_COUNT;}
static void schedule(struct SlugBodyContext*c,enum SlugBodyState state){slug_body_schedule(body(c),state);}
static int noise(struct SlugBodyContext*c,int16_t command){
 if(command==-1)return 1;struct WeaponWorld*w=c->animation.weapons;
 return w&&w->sound&&w->sound(w->scene,(uint16_t)command,0)?1:fail(body(c),5);
}
static int attack(struct SlugBodyContext*c){
 struct SlugSharedState*s=body(c);struct WeaponActor*a=&s->actor;struct WeaponWorld*w=c->animation.weapons;
 if(s->fault)return 0;if(!a->attack)return 1;
 if(!w||!w->attack)return fail(s,8);
 a->hit.x=word(a->transform.x>>8);a->hit.y=transform_y_up(&a->transform);a->hit.flip=a->transform.facing;
 for(unsigned i=0;i<a->attack->count;i++)if(!w->attack(w->scene,&a->hit,&a->attack->records[i],0))return fail(s,8);
 return 1;
}
static int update(struct SlugBodyContext*c,int preserve){return slug_update_chassis(body(c),preserve,0);}
static int move(struct SlugBodyContext*c,enum SlugMotionPolicy policy){
 return slug_motion_step(body(c),c->animation.terrain,c->input,policy);
}
static int animate(struct SlugBodyContext*c){int complete=slug_animation_step(&c->animation);return body(c)->fault?-1:complete;}
static void display(struct SlugBodyContext*c){slug_motion_display(body(c),c->input);}
static int contact(struct SlugBodyContext*c,unsigned*count){return slug_ground_contact(body(c),c->animation.terrain,count);}
static int select_slope(struct SlugSharedState*s,enum SlugAnimationTableId table,unsigned base,int fixed,int defense){
 unsigned i=slug_slope_index(s); /* selector itself does not write $94 */
 if(!slug_animation_select(s,table,i,(uint8_t)(base+(fixed?0:i))))return 0;
 return defense<0?1:slug_set_defense_table(s,(enum SlugDefenseTableId)defense,i);
}
static int select_angle(struct SlugSharedState*s,enum SlugAnimationTableId table,unsigned base,int fixed){
 uint8_t frame;int i=slug_angle_sprite(s,&frame);if(i<0)return fail(s,4);
 return slug_animation_select(s,table,(unsigned)i,(uint8_t)(base+(fixed?0:frame)));
}
static void store_angle(struct SlugSharedState*s){(void)slug_slope_product(s,&s->angle);}
static int angle_changed(struct SlugSharedState*s){int16_t a;(void)slug_slope_product(s,&a);if(a==s->angle)return 0;s->angle=a;return 1;}
static int flat(struct SlugSharedState*s){
 if(s->angle<-64||s->angle>64)return fail(s,4);
 return slug_flat_angles[s->angle+64]!=0; /* $02A506 reads stored $80 */
}
static void stop(struct SlugSharedState*s){
 s->actor.ax=s->actor.vx=s->actor.vy=0;transform_integer_copy(&s->actor.transform,&s->actor.transform);
}
static void ground_y_max(struct SlugBodyContext*c){
 struct SlugSharedState*s=body(c);int16_t camera=c->animation.terrain->camera_y;
 /* CMP.W compares original screen-space words, not unwrapped world Y. */
 int16_t shown=word(transform_y_up(&s->actor.transform)+camera);
 if(shown>word(s->ground_y_up+camera))s->ground_y_up=word(shown-camera);
}
static void route(struct SlugBodyContext*c,const uint8_t*table,unsigned count,unsigned choice){
 if(choice>=count){fail(body(c),7);return;}
 if(table[choice]!=SB_STATE_COUNT)schedule(c,(enum SlugBodyState)table[choice]);
}
static void down(struct SlugBodyContext*c,int landing){
 if(held(c,PB_DOWN))route(c,landing?slug_body_landing_down:slug_body_down,5,slug_slope_index(body(c)));
}
static void jump(struct SlugBodyContext*c,int alternate){route(c,slug_body_jump,5,slug_jump_input(c->input,c->layout,alternate));}
static void fire(struct SlugBodyContext*c,enum SlugBodyState state){if(slug_cannon_ready(body(c),c->input,c->layout))schedule(c,state);}
static void absent(struct SlugBodyContext*c){if(slug_rider_absent(body(c)))schedule(c,SB_IDLE_ENTER_B);}
static void combo(struct SlugBodyContext*c,enum SlugBodyState state){if(slug_exit_combo(c->input))schedule(c,state);}
static int fall_if_air(struct SlugBodyContext*c,enum SlugBodyState state){
 int found=contact(c,0);if(found<0)return 0;if(!found)schedule(c,state);return 1;
}
static int damage(struct SlugBodyContext*c,int air){
 struct SlugSharedState*s=body(c);
 /* $02A690 returns zero immediately after time-up; impact must not replace it. */
 if(!(s->status_flags&32)&&!c->campaign_timer){s->actor.hit.flags|=9;schedule(c,SB_DEATH_START);return 1;}
 if(s->actor.hit.invulnerable)s->input_masks[(~c->frame_phase)&1]=0xf0; /* $08F6DA */
 if(c->debug_flags&8){s->actor.hit.hp=0x30;return 1;}
 if(c->debug_flags&32)s->actor.hit.hp=0x300;
 unsigned side=slug_impact_side(s,c->animation.terrain->camera_x);
 if(side&&!noise(c,0x1089))return 0;
 route(c,air?slug_body_damage_air:slug_body_damage_ground,3,side);return !s->fault;
}
static int dead(struct SlugBodyContext*c){
 struct SlugSharedState*s=body(c);
 if(!(s->status_flags&32)&&!c->campaign_timer){s->actor.hit.flags|=1;schedule(c,SB_DEATH_START);}
 return !!(s->actor.hit.flags&1); /* $02A68A -> $028758 */
}
static int release_palettes(struct SlugSharedState*s){
 if(s->palette_count>4)return fail(s,4);
 while(s->palette_count)s->palette_slots[--s->palette_count]=0;
 return 1;
}
static void palette_request(struct SlugSharedState*s,uint16_t palette){
 /* Native IDs replace Neo Geo palette-pool handles. Preserve the actual
  * $00236E three-slot limit and lookup-before-limit order. The fourth
  * death request ($019A) is NOT installed as a fictitious fourth slot. */
 for(unsigned i=s->palette_count;i;i--)if(s->palette_slots[i-1]==palette){s->actor.palette=palette;return;}
 if(s->palette_count>=3)return;
 s->palette_slots[s->palette_count++]=palette;s->actor.palette=palette;
}
static int kill_init(struct SlugSharedState*s){
 s->actor.damage_timer=0;s->actor.hit.invulnerable=254;s->actor.hit.hp=0;s->actor.hit.flags|=1;
 if(!release_palettes(s))return 0;
 palette_request(s,0x19c);palette_request(s,0x19d);palette_request(s,0x19e);palette_request(s,0x19a);
 slug_damage_reset(s);return 1;
}
static int retire_slot(struct SlugBodyContext*c){
 struct SlugSharedState*s=body(c);struct WeaponWorld*w=c->animation.weapons;
 if(!w->entities)return fail(s,6);
 struct NativeEntity*e=entity_get(w->entities,c->animation.entity);
 if(!e||e->kind!=ENTITY_SLUG||e->payload.slug!=s)return fail(s,6);
 entity_delete_children(w->entities,e);s->lifecycle_flags&=(uint8_t)~128u;
 if(!release_palettes(s))return 0;
 schedule(c,SB_RETIRED_RESET);(void)slug_rider_absent(s);return !s->fault;
}
static int reset_slot(struct SlugBodyContext*c){
 struct SlugSharedState*s=body(c);if(!c->animation.weapons->entities)return fail(s,6);
 struct NativeEntity*e=entity_get(c->animation.weapons->entities,c->animation.entity);
 if(!e||e->kind!=ENTITY_SLUG||e->payload.slug!=s)return fail(s,6);
 /* $02DCAA/$0005FE keep the fixed scheduler slot and its identity. There
  * is no free-list delete or recursive destruction in this next-tick reset. */
 slug_body_reset(s,e->index);return 1;
}
void slug_body_reset(struct SlugSharedState*s,uint16_t entity_id){
 *s=(struct SlugSharedState){.body_state=SB_INACTIVE,.previous_owner=255,.angle=-1,.frame_index=255,.ammo=255,.rider_parameter=0xffffffffu,.palette_slots={65535,65535,65535,65535},
  .actor={.owner=255,.damage_owner=255,.score_owner=255,.platform_ids=255,.ground_slope=-1,.ground_tag=-1,.display_flags=1,
   .phase=255,.ttl=255,.raw_surface=255,.origin_y_up=-1,.trail_drawing=65535,
   .hit={.id=entity_id,.hp=-1,.impact_x=-1,.impact_y=-1,.last_attacker=65535,.last_kind=255,.flags=64},
   .animation={.drawing=65535,.cursor=65535,.last_draw_cursor=65535}}};
}
static void bounds_table(struct SlugBodyContext*c,const int16_t*b){
 struct SlugSharedState*s=body(c);
 int16_t x=word((s->actor.transform.x>>8)-c->animation.terrain->camera_x);
 int16_t y=word(transform_y_up(&s->actor.transform)+c->animation.terrain->camera_y);
 s->actor.hit.flags|=128;
 if(x<b[0]||x>word(320+b[1])||y<word(256-b[3])||y>word(496-b[2])){
  (void)retire_slot(c); /* source tail-jumps immediately */
 }
}
static void bounds(struct SlugBodyContext*c){bounds_table(c,slug_body_bounds);}
static void unoccupied_bounds(struct SlugBodyContext*c){bounds_table(c,c->narrow_bounds?slug_body_bounds:slug_body_wide_bounds);}
static int mount_candidate(struct SlugBodyContext*c){
 if(!c->mount_enabled||!c->player_query_enabled)return 0;
 struct SlugSharedState*s=body(c);struct WeaponWorld*w=c->animation.weapons;
 for(unsigned i=0;i<2;i++){
  struct PlayerBodyState*p=w->players[i];
  if(!p||p->life_retired||!(p->actor.lifecycle_flags&128)||(p->actor.hit.flags&9))continue;
  uint16_t dx=(uint16_t)((p->actor.transform.x>>8)-(s->actor.transform.x>>8)+slug_mount_rect[0]);
  uint16_t dy=(uint16_t)(transform_y_up(&p->actor.transform)-transform_y_up(&s->actor.transform)+slug_mount_rect[2]);
  if(dx>=slug_mount_rect[1]||dy>=slug_mount_rect[3])continue;
  if(slug_mount_candidate(s,p))return 1;
 }
 return 0;
}
static void reenter(struct SlugBodyContext*c,int crouch){
 schedule(c,crouch?SB_CROUCH_BRAKE:SB_IDLE_FLAT);
 if(held(c,PB_RIGHT))schedule(c,crouch?SB_CROUCH_CRUISE_RIGHT:SB_CRUISE_RIGHT);
 if(held(c,PB_LEFT))schedule(c,crouch?SB_CROUCH_CRUISE_LEFT:SB_CRUISE_LEFT);
}
static int ground_tail(struct SlugBodyContext*c){
 down(c,0);if(!fall_if_air(c,SB_FALL))return 0;jump(c,0);fire(c,SB_FIRE_FLAT);
 absent(c);combo(c,SB_DESTROYED_SLIDE);return damage(c,0);
}
static int crouch_tail(struct SlugBodyContext*c){
 if(!fall_if_air(c,SB_FALL))return 0;absent(c);combo(c,SB_KNOCKED);return damage(c,0);
}
static void steer(struct SlugBodyContext*c,int friction){
 struct WeaponActor*a=&body(c)->actor;a->ax=slug_body_steer[(c->input.held>>2)&3];
 if(friction&&!a->ax)a->ax=word(-a->vx)>>3;a->vx=clamp(a->vx,0x2a0);
}
static void scale_slope_speed(struct SlugBodyContext*c){
 struct SlugSharedState*s=body(c);if(c->layout!=3)return;
 int16_t a;(void)slug_slope_product(s,&a);
 if(!((uint16_t)(word(a-s->angle)^s->actor.vx)&0x8000))s->actor.vx=word(s->actor.vx+(s->actor.vx>>1));
}
static void ramp(struct SlugSharedState*s,int target){
 /* The source switches permanently to a second frame when the byte reaches
  * zero. Later animation writes to $91 must not restart the countdown. */
 if(!s->body_phase&&!--s->accel_timer){s->actor.vx=(int16_t)target;s->actor.ax=0;s->body_phase=1;}
}
static int fire_children(struct SlugBodyContext*c,int idle,int drop_b,int air){
 struct SlugSharedState*s=body(c);enum SlugCannonKind kind;
 int drop=!!(s->input_masks[c->frame_phase&1]&4);
 if(drop)kind=drop_b?SCANNON_DROP_B:SCANNON_DROP_A;
 else kind=s->turn_timer==255?SCANNON_LONG:idle?SCANNON_IDLE:SCANNON_NORMAL;
 if(!slug_spawn_cannon(&c->animation,kind,c->input,c->layout))return fail(s,6);
 if(!drop&&!slug_spawn_exhaust(&c->animation,air))return fail(s,6);
 slug_ammo_consume(s);s->cooldown=30;return 1;
}
static int impact_left(struct SlugBodyContext*c){
 struct SlugSharedState*s=body(c);if(!s->actor.defense)return 0;
 const struct HitRect*r=&s->actor.defense[0].rect;
 int16_t x=word((s->actor.transform.x>>8)-c->animation.terrain->camera_x);
 int16_t centre=word(word(x+r->left)+word(x+r->right));centre=(int16_t)(centre>>1);
 return (uint16_t)word(s->actor.hit.impact_x-c->animation.terrain->camera_x)<(uint16_t)centre;
}
/* $02A0F8/$02AEF8..$02AFAC. Three embedded contacts share a native
 * scheduling group so their B/A/C order remains between turret and surface. */
static int construct(struct SlugBodyContext*c,uint16_t main_palette,int boss){
 struct SlugSharedState*s=body(c);struct WeaponActor*a=&s->actor;struct WeaponWorld*w=c->animation.weapons;
 if(!c->contact_tick)return fail(s,6);
 palette_request(s,main_palette);palette_request(s,1);a->palette=s->palette_slots[0];
 /* $1C aliases the fourth palette record. Init adds $74 to $1E;
  * later three-slot palette release leaves this alias intact. */
 s->palette_slots[3]=s->sprite_template=a->hit_palette=(uint16_t)((boss?3:0x1e)+0x74);s->scale_x_y=65535;a->phase=a->ttl=255;
 a->display_flags|=20;s->lifecycle_flags|=4;a->hit.type_flags|=1;
 a->transform.facing=0;a->transform.priority=0x800c;s->action_flags=0;s->body_contact=SBC_IDLE;
 s->input_masks[0]=s->input_masks[1]=0xf0;a->hit.hp=48;
 if(!boss){slug_damage_reset(s);s->ammo=10;s->rider_parameter=0x10000;}
 s->slope_index=0;s->impulse=0;s->destroyed_timer=0;s->cannon_count=0;a->hit.flags&=(uint8_t)~8u;
 if(!slug_spawn_surface(&c->animation))return fail(s,6);
 slug_chassis_create(s,0,c->animation.terrain->camera_x,c->animation.terrain->camera_y);
 struct NativeEntity*contacts=entity_spawn(w->entities,c->animation.entity,ENTITY_GROUP,ENTITY_FIRST_CHILD,c->contact_tick);
 if(!contacts)return fail(s,6);contacts->payload.slug=s;return 1;
}
static int drop_init(struct SlugBodyContext*c,unsigned variant){
 struct SlugSharedState*s=body(c);struct WeaponActor*a=&s->actor;
 if(variant>=5)return fail(s,6);if(!construct(c,3,0))return 0;
 a->transform.priority|=2;
 if(!slug_spawn_turret(&c->animation,variant)||!slug_spawn_marker(&c->animation)||!slug_spawn_wheel(&c->animation))return fail(s,6);
 animation_begin(&a->animation,SLUG_INITIAL_ENTRY);a->movement_flags|=32;
 a->vx=a->vy=a->ax=a->ay=0;transform_integer_copy(&a->transform,&a->transform);s->cooldown=0;
 hit_begin_attack(&a->hit);return slug_set_attack(s,SATT_NORMAL);
}
static int boss_init(struct SlugBodyContext*c,int arena,unsigned variant){
 struct SlugSharedState*s=body(c);struct WeaponActor*a=&s->actor;struct TerrainWorld*t=c->animation.terrain;
 if(variant>1)return fail(s,6);
 int16_t x=arena||variant?64:104,y=arena?369:variant?480:472;
 transform_integer_offset(&a->transform,word(x+t->camera_x-(a->transform.x>>8)),word(y-t->camera_y-transform_y_up(&a->transform)));
 s->ground_y_up=word(y-t->camera_y);
 /* $02A242 raw footer is JSR $03060A; RTS: ordinary chassis, no turret. */
 if(!construct(c,!arena&&variant?0x4d:3,1))return 0;
 if(arena||!variant)a->transform.priority|=2;
 animation_begin(&a->animation,SLUG_INITIAL_ENTRY);a->movement_flags|=32;
 a->vx=a->vy=a->ax=a->ay=0;transform_integer_copy(&a->transform,&a->transform);s->cooldown=0;
 if(!arena){a->vy=512;a->ay=-512;hit_begin_attack(&a->hit);return slug_set_attack(s,SATT_NORMAL);}return 1;
}
static int boss_drop_frame(struct SlugBodyContext*c,enum SlugBodyState state){
 struct SlugSharedState*s=body(c);struct WeaponActor*a=&s->actor;
 if(state==SB_BOSS_DESCEND_FRAME&&word(transform_y_up(&a->transform)+c->animation.terrain->camera_y)<=384){
  a->vy=0;schedule(c,SB_BOSS_DESCEND_STEADY);
 }
 int descending=state!=SB_BOSS_DROP_FRAME;
 a->hit.flags|=64;a->hit.invulnerable=descending?70:90;if(descending)s->status_flags|=2;
 if(!update(c,1))return 0;if(descending)a->vy=clamp(a->vy,0x400);
 int landed=move(c,SLMOVE_AIR);if(landed<0)return 0;
 if(descending&&landed)schedule(c,SB_IDLE_ENTER_B);
 display(c);a->transform.priority=(uint16_t)((a->transform.priority&0xffe3)|0x9fec);
 if(animate(c)<0||!attack(c))return 0;
 if(!descending){
  if(word(transform_y_up(&a->transform)+c->animation.terrain->camera_y)<=384)schedule(c,SB_BOSS_DESCEND);
  int grounded=contact(c,0);if(grounded<0)return 0;if(!grounded)schedule(c,SB_BOSS_DESCEND);
 }else if(a->vy<=0){
  unsigned count=0;if(contact(c,&count)<0)return 0;if(count)a->vx=a->ax=0;
  if(angle_changed(s)&&!select_angle(s,SANI_ANGLED_NORMAL,20,1))return 0;
 }
 return !s->fault;
}
static int drop_frame(struct SlugBodyContext*c,int descending){
 struct SlugSharedState*s=body(c);struct WeaponActor*a=&s->actor;
 a->hit.invulnerable=descending?70:90;if(descending)s->status_flags|=2;
 if(!update(c,1))return 0;a->vy=descending?clamp(a->vy,0x400):-0x400;
 int landed=move(c,SLMOVE_AIR);if(landed<0)return 0;
 if(descending&&landed)schedule(c,SB_IDLE_ENTER_A);
 display(c);a->transform.priority=(uint16_t)((a->transform.priority&0xffe3)|0x9fec);
 if(animate(c)<0||!attack(c))return 0;
 if(!descending){
  if(word(transform_y_up(&a->transform)+c->animation.terrain->camera_y)<=384)schedule(c,SB_DROP_DESCEND);
  int grounded=contact(c,0);if(grounded<0)return 0;if(!grounded)schedule(c,SB_DROP_DESCEND);
 }else if(a->vy<=0){
  unsigned count=0;if(contact(c,&count)<0)return 0;if(count)a->vx=a->ax=0;
  if(angle_changed(s)&&!select_angle(s,SANI_ANGLED_NORMAL,20,1))return 0;
 }
 return !s->fault;
}
static enum SlugBodyState enter(struct SlugBodyContext*c,enum SlugBodyState state){
 struct SlugSharedState*s=body(c);struct WeaponActor*a=&s->actor;
 unsigned argument=s->body_argument;s->body_phase=0;
 switch(state){
 case SB_BOSS_ARENA:if(!boss_init(c,1,0))break;return state;
 case SB_BOSS_DROP_INIT:if(!boss_init(c,0,argument))break;return SB_BOSS_DROP_FRAME;
 case SB_BOSS_DESCEND:
  animation_begin(&a->animation,SLUG_INITIAL_ENTRY);a->movement_flags|=32;
  s->ground_y_up=word(s->ground_y_up+8);a->vy=512;a->ay=-512;ground_y_max(c);store_angle(s);display(c);
  return SB_BOSS_DESCEND_FRAME;
 case SB_BOSS_DROP_FRAME:case SB_BOSS_DESCEND_FRAME:case SB_BOSS_DESCEND_STEADY:break;
 case SB_DROP_INIT:if(!drop_init(c,argument))break;return SB_DROP_FRAME;
 case SB_DROP_DESCEND:
  animation_begin(&a->animation,SLUG_INITIAL_ENTRY);a->movement_flags|=32;
  s->ground_y_up=word(s->ground_y_up+8);ground_y_max(c);store_angle(s);display(c);a->vy=-0x400;
  return SB_DROP_DESCEND_FRAME;
 case SB_DROP_FRAME:case SB_DROP_DESCEND_FRAME:break;
 case SB_DESTROYED_SLIDE:case SB_DESTROYED_SLIDE_RESUME:
  if(state==SB_DESTROYED_SLIDE){if(!noise(c,0x10e9)||!kill_init(s))break;s->destroyed_timer=48;s->body_contact=SBC_DESTROYED;}
  else a->ax=0;
  a->vx=0x600;select_slope(s,SANI_DESTROYED_SLIDE,25,0,-1);noise(c,0x10b2);
  return state==SB_DESTROYED_SLIDE?state:SB_DESTROYED_SLIDE_LOOP;
 case SB_KNOCKED:case SB_KNOCKED_RESUME:
  if(state==SB_KNOCKED){if(!noise(c,0x10e9)||!kill_init(s))break;s->destroyed_timer=48;}
  a->vx=0x600;select_slope(s,SANI_KNOCKED,150,1,-1);
  noise(c,slug_body_destroyed_sounds[slug_slope_index(s)]);return SB_KNOCKED;
 case SB_DESTROYED:case SB_DESTROYED_RESUME:case SB_DESTROYED_STOP:
  if(state==SB_DESTROYED){if(!noise(c,0x10e9)||!kill_init(s))break;s->destroyed_timer=48;noise(c,0x10af);}
  s->impulse=state==SB_DESTROYED_STOP?0:0x600;s->action_flags&=(uint8_t)~32u;a->ax=0;store_angle(s);
  select_angle(s,SANI_ANGLED_NORMAL,0,0);a->defense=0;a->defense_count=0;s->body_contact=SBC_DESTROYED;return SB_DESTROYED_FRAME;
 case SB_DEATH_EXPLODE:case SB_DEATH_LAUNCH:
  if(state==SB_DEATH_EXPLODE&&!kill_init(s))break;
  s->action_flags&=(uint8_t)~32u;a->hit.flags|=1;
  select_slope(s,impact_left(c)?SANI_HIT_REACT_B:SANI_HIT_REACT_A,150,1,-1);stop(s);a->ay=0;
  if(state==SB_DEATH_LAUNCH){a->vy=0x3fc;a->ay=-85;s->accel_timer=60;}break;
 case SB_DEATH_FADE:
  s->action_flags&=(uint8_t)~32u;s->accel_timer=40;a->defense=0;a->defense_count=0;s->body_contact=SBC_NONE;break;
 case SB_SELF_DESTRUCT:s->action_flags&=(uint8_t)~32u;break;
 case SB_DEATH_START:case SB_DESTROYED_FRAME:case SB_DESTROYED_SLIDE_LOOP:break;
 case SB_FIRE_IDLE:case SB_FIRE_FLAT:
  s->action_flags&=(uint8_t)~32u;select_slope(s,SANI_FIRE_FLAT,state==SB_FIRE_IDLE?20:60,0,SDEFT_NORMAL);
  if(!fire_children(c,state==SB_FIRE_IDLE,0,0))break;
  s->status_flags|=1;s->body_contact=SBC_IDLE;if(!(c->input.held&(PB_RIGHT|PB_LEFT)))a->vx=a->ax=0;
  return SB_FIRE_FLAT;
 case SB_FIRE_FLAT_RESUME:stop(s);a->vx=0;s->body_contact=SBC_IDLE;return SB_FIRE_FLAT;
 case SB_FIRE_RECOIL:s->action_flags&=(uint8_t)~32u;select_slope(s,SANI_FIRE_RECOIL,20,0,-1);break;
 case SB_FIRE_MOVE_RIGHT:case SB_FIRE_MOVE_LEFT:a->vx=state==SB_FIRE_MOVE_RIGHT?0x2a0:-0x2a0;a->ax=0;break;
 case SB_FIRE_SLOPE:
  s->action_flags&=(uint8_t)~32u;if(!fire_children(c,0,1,0))break;
  if(angle_changed(s))select_angle(s,SANI_ANGLED_NORMAL,0,0);s->status_flags|=1;s->body_contact=SBC_IDLE;break;
 case SB_FIRE_JUMP_CROUCH:case SB_FIRE_JUMP_AIR:
  if(state==SB_FIRE_JUMP_CROUCH){s->lifecycle_flags&=(uint8_t)~8u;a->vy=0x4a4;a->ay=-99;}
  s->action_flags&=(uint8_t)~32u;select_slope(s,SANI_JUMP_LAUNCH,45,0,-1);
  if(!fire_children(c,0,1,1))break;s->status_flags|=1;
  return s->status_flags&4?SB_FIRE_AIR_RESUME:SB_FIRE_AIR_LOOP;
 case SB_FIRE_AIR_ANGLE:s->status_flags|=4; /* source alias falls into FireAir */
  /* fall through */
 case SB_FIRE_AIR:
  s->action_flags&=(uint8_t)~32u;if(!fire_children(c,0,1,1))break;
  store_angle(s);select_angle(s,SANI_ANGLED_NORMAL,0,0);s->status_flags|=5;return SB_FIRE_AIR_RESUME;
 case SB_FIRE_AIR_LOOP:case SB_FIRE_AIR_RESUME:break;
 case SB_IDLE_FLAT:
  stop(s);a->vx=0;select_slope(s,SANI_IDLE_FLAT,20,0,SDEFT_NORMAL);s->body_contact=SBC_IDLE;break;
 case SB_IDLE_SLOPE:
  s->impulse=0;s->action_flags&=(uint8_t)~32u;a->ax=0;store_angle(s);
  select_angle(s,SANI_ANGLED_NORMAL,0,0);s->body_contact=SBC_IDLE;break;
 case SB_ACCEL_RIGHT:case SB_ACCEL_LEFT:case SB_CROUCH_ACCEL_RIGHT:case SB_CROUCH_ACCEL_LEFT:{
  int left=state==SB_ACCEL_LEFT||state==SB_CROUCH_ACCEL_LEFT,crouch=state>=SB_CROUCH_ACCEL_RIGHT;
  if(!argument&&!slug_animation_acceleration_dust(&c->animation,left))break;
  a->ax=word((left?-0x2a0:0x2a0)-a->vx)>>3;s->accel_timer=8;
  select_slope(s,crouch?(left?SANI_CROUCH_ACCEL_LEFT:SANI_CROUCH_ACCEL_RIGHT):
   (left?SANI_ACCEL_LEFT:SANI_ACCEL_RIGHT),crouch?150:left?30:25,crouch,-1);
  noise(c,(left?slug_body_accel_left_sounds:slug_body_accel_right_sounds)[slug_slope_index(s)]);break;
 }
 case SB_BRAKE_RIGHT:case SB_BRAKE_LEFT:case SB_DECEL_RIGHT:case SB_DECEL_LEFT:{
  int left=state==SB_BRAKE_LEFT||state==SB_DECEL_LEFT,crouch=state>=SB_DECEL_RIGHT;
  a->ax=word(-a->vx)>>3;s->accel_timer=8;
  select_slope(s,crouch?(left?SANI_DECEL_B:SANI_DECEL_A):(left?SANI_BRAKE_LEFT:SANI_BRAKE_RIGHT),
   crouch?150:left?15:10,crouch,-1);break;
 }
 case SB_CRUISE_RIGHT:case SB_CRUISE_LEFT:case SB_CROUCH_CRUISE_RIGHT:case SB_CROUCH_CRUISE_LEFT:{
  int left=state==SB_CRUISE_LEFT||state==SB_CROUCH_CRUISE_LEFT,crouch=state>=SB_CROUCH_CRUISE_RIGHT;
  a->vx=left?-0x2a0:0x2a0;a->ax=0;
  select_slope(s,crouch?(left?SANI_CROUCH_CRUISE_LEFT:SANI_CROUCH_CRUISE_RIGHT):(left?SANI_CRUISE_LEFT:SANI_CRUISE_RIGHT),
   crouch?150:left?5:0,crouch,crouch?-1:SDEFT_NORMAL);break;
 }
 case SB_SLOPE_IDLE_ENTER:case SB_SLOPE_MOUNT:
  s->action_flags&=(uint8_t)~32u;a->vx=a->ax=0;
  /* $02B706/$02B7EC intentionally select using the stored old angle. */
  select_angle(s,state==SB_SLOPE_MOUNT?SANI_ANGLED_SLOPE_MOUNT:SANI_ANGLED_IDLE_ENTER,
   state==SB_SLOPE_MOUNT?130:65,0);s->sound_state=255;break;
 case SB_TURN_TO_DRIVE:case SB_TURN_TO_DRIVE_ALT:{
  int alternate=state==SB_TURN_TO_DRIVE_ALT;s->action_flags&=(uint8_t)~34u;s->sound_state=0;
  unsigned phase=a->animation.pose;
  if(phase>=8)return enter(c,alternate?SB_DRIVE_ALT:SB_DRIVE);
  slug_animation_turn(s,(unsigned)alternate,slug_slope_index(s),phase,
   (uint8_t)((alternate?110:70)+phase*5+slug_slope_index(s)));
  if(!alternate){slug_set_defense(s,SDEF_BASE);s->body_contact=SBC_CROUCH;}
  a->ax=0;return alternate?SB_DRIVE_ALT:SB_DRIVE;
 }
 case SB_DRIVE:case SB_DRIVE_B:
  s->action_flags&=(uint8_t)~32u;s->action_flags|=2;s->sound_state=3;
  select_slope(s,state==SB_DRIVE_B?SANI_DRIVE_B:SANI_DRIVE,state==SB_DRIVE_B?65:35,0,state==SB_DRIVE_B?-1:SDEFT_CROUCH);
  if(state==SB_DRIVE_B){s->lifecycle_flags|=128;slug_set_defense(s,SDEF_BASE);}
  s->body_contact=SBC_CROUCH;a->ax=0;break;
 case SB_DRIVE_ALT:
  a->hit.type_flags|=1;s->action_flags&=(uint8_t)~34u;
  select_slope(s,SANI_DRIVE_ALT,40,0,SDEFT_CROUCH);s->sound_state=9;a->ax=0;break;
 case SB_CROUCH_BRAKE:
  stop(s);a->vx=a->ax=0;select_slope(s,SANI_BRAKE,150,1,-1);s->body_contact=SBC_CROUCH;break;
 case SB_STALL:
  if(argument)noise(c,argument==1?0x10af:0x10b0);a->ax=0;store_angle(s);
  select_angle(s,SANI_ANGLED_STALL,195,1);break;
 case SB_SET_SPEED:
  if(argument)a->vx=s->body_parameter;
  select_slope(s,SANI_JUMP_CROUCH,150,1,-1);return SB_JUMP_CROUCH;
 case SB_JUMP_NEUTRAL:case SB_JUMP_RIGHT:case SB_JUMP_LEFT:
  a->vx=state==SB_JUMP_RIGHT&&s->slope_index!=3?0x2aa:state==SB_JUMP_LEFT&&s->slope_index!=4?-0x2aa:0;
  return enter(c,SB_JUMP_CROUCH);
 case SB_JUMP_CROUCH:
  s->action_flags&=(uint8_t)~32u;a->ax=0;select_slope(s,SANI_JUMP_CROUCH,45,0,SDEFT_CROUCH_A);
  s->body_contact=SBC_IDLE;break;
 case SB_JUMP_LAUNCH:
  s->lifecycle_flags&=(uint8_t)~8u;s->action_flags&=(uint8_t)~32u;a->vy=0x4a4;a->ay=-99;
  ground_y_max(c);select_slope(s,SANI_JUMP_LAUNCH,50,0,SDEFT_CROUCH_B);store_angle(s);display(c);return SB_JUMP_AIR;
 case SB_JUMP_LAND:
  s->action_flags&=(uint8_t)~32u;s->lifecycle_flags|=128;a->vy=a->ay=0;
  if(a->vx)a->vx=a->vx<0?-128:128;store_angle(s);
  if(!flat(s))return enter(c,SB_LAND_SLOPE);
  s->accel_timer=17;select_slope(s,SANI_JUMP_LAND,55,0,SDEFT_NORMAL);break;
 case SB_LAND_SLOPE:
  s->accel_timer=17;store_angle(s);select_angle(s,SANI_ANGLED_NORMAL,0,0);return SB_LAND_SLOPE_FRAME;
 case SB_FALL_START:case SB_FALL:
  s->action_flags&=(uint8_t)~32u;a->ay=-32;a->ax=0;s->ground_y_up=word(s->ground_y_up+4);ground_y_max(c);
  if(state==SB_FALL){store_angle(s);select_angle(s,SANI_ANGLED_NORMAL,0,0);s->status_flags|=4;return SB_FALL_LOOP;}
  break;
 case SB_HIT_GROUND_A:case SB_HIT_GROUND_B:
  s->action_flags&=(uint8_t)~32u;
  select_slope(s,state==SB_HIT_GROUND_A?SANI_HIT_REACT_A:SANI_HIT_REACT_B,150,1,-1);
  a->ay=0;stop(s);s->body_contact=SBC_IDLE;return SB_HIT_GROUND_FRAME;
 case SB_HIT_AIR_A:case SB_HIT_AIR_B:
  s->action_flags&=(uint8_t)~32u;
  select_slope(s,state==SB_HIT_AIR_A?SANI_HIT_LAUNCH_A:SANI_HIT_LAUNCH_B,150,1,-1);
  a->vy=0x3fc;a->ay=-85;a->ax=0;break;
 case SB_HIT_LAND_A:case SB_HIT_LAND_B:
  s->action_flags&=(uint8_t)~32u;
  select_slope(s,state==SB_HIT_LAND_A?SANI_HIT_LAND_A:SANI_HIT_LAND_B,150,1,-1);break;
 case SB_HIT_GROUND_FRAME:break;
 case SB_IDLE_ENTER_A:case SB_IDLE_ENTER_B:case SB_IDLE_ENTER_C:
  s->action_flags&=(uint8_t)~32u;
  select_slope(s,state==SB_IDLE_ENTER_B?SANI_IDLE_ENTER_B:SANI_IDLE_ENTER_A,65,0,-1);
  a->ax=0;s->body_contact=SBC_NONE;slug_set_defense(s,SDEF_UNOCCUPIED);return SB_UNOCCUPIED_IDLE;
 case SB_UNOCCUPIED_ANGLE:case SB_IDLE_ANGLED_B:
  a->ax=0;store_angle(s);select_angle(s,SANI_ANGLED_UNOCCUPIED,195,1);
  s->body_contact=SBC_NONE;slug_set_defense(s,SDEF_UNOCCUPIED);break;
 case SB_UNOCCUPIED_FALL:case SB_UNOCCUPIED_FALL_KEEP:
  s->action_flags&=(uint8_t)~32u;a->ay=-32;a->vx=a->ax=0;
  s->ground_y_up=word(s->ground_y_up+4);ground_y_max(c);
  if(state==SB_UNOCCUPIED_FALL){store_angle(s);select_angle(s,SANI_ANGLED_UNOCCUPIED,0,0);s->status_flags|=4;}
  s->body_contact=SBC_NONE;slug_set_defense(s,SDEF_UNOCCUPIED);break;
 case SB_HUNKER:
  a->hit.type_flags|=1;s->action_flags&=(uint8_t)~34u;
  select_slope(s,SANI_HUNKER,40,0,SDEFT_CROUCH);
  if(!slug_spawn_mount_overlay(&c->animation))break;
  s->body_contact=SBC_NONE;slug_set_defense(s,SDEF_UNOCCUPIED);a->ax=0;break;
 case SB_PLAYER_MOUNT:
  a->hit.type_flags|=1;if(!slug_spawn_mount_overlay(&c->animation))break;
  s->body_contact=SBC_NONE;s->action_flags&=(uint8_t)~32u;a->vx=a->ax=0;
  select_angle(s,SANI_ANGLED_PLAYER_MOUNT,130,0);s->sound_state=255;
  slug_set_defense(s,SDEF_UNOCCUPIED);break;
 case SB_UNOCCUPIED_IDLE:break;
 case SB_JUMP_AIR:case SB_JUMP_AIR_ANGLE:case SB_LAND_SLOPE_FRAME:case SB_FALL_LOOP:case SB_FALL_ANGLE:break;
 default:fail(s,7);break;
 }
 return state;
}
static int idle_flat(struct SlugBodyContext*c){
 struct SlugSharedState*s=body(c);slug_set_attack(s,SATT_NORMAL);hit_begin_attack(&s->actor.hit);
 if(!update(c,0)||move(c,SLMOVE_GROUND_E)<0)return 0;
 if(slug_update_angle(s))schedule(c,SB_IDLE_SLOPE);
 if(held(c,PB_RIGHT))slug_body_schedule_argument(s,SB_ACCEL_RIGHT,1);
 if(held(c,PB_LEFT))slug_body_schedule_argument(s,SB_ACCEL_LEFT,1);
 down(c,0);if(animate(c)<0||!fall_if_air(c,SB_FALL))return 0;
 jump(c,0);fire(c,SB_FIRE_FLAT);absent(c);combo(c,SB_DESTROYED_SLIDE);
 if(!damage(c,0))return 0;bounds(c);return !s->fault;
}
static int ground_drive(struct SlugBodyContext*c,enum SlugBodyState state){
 struct SlugSharedState*s=body(c);
 int left=state==SB_ACCEL_LEFT||state==SB_BRAKE_LEFT||state==SB_CRUISE_LEFT;
 int accel=state==SB_ACCEL_RIGHT||state==SB_ACCEL_LEFT;
 int brake=state==SB_BRAKE_RIGHT||state==SB_BRAKE_LEFT;
 if(accel||brake)ramp(s,brake?0:left?-0x2a0:0x2a0);
 if(!update(c,0))return 0;int mismatch=move(c,SLMOVE_GROUND_E);if(mismatch<0)return 0;
 if(mismatch)schedule(c,SB_IDLE_SLOPE);if(slug_update_angle(s))schedule(c,SB_IDLE_SLOPE);
 if(!accel&&!brake){
  if(!held(c,left?PB_LEFT:PB_RIGHT))schedule(c,left?SB_BRAKE_LEFT:SB_BRAKE_RIGHT);
  if(held(c,left?PB_RIGHT:PB_LEFT))schedule(c,left?SB_CRUISE_RIGHT:SB_CRUISE_LEFT);
 }
 int complete=animate(c);if(complete<0)return 0;
 if(complete&&(accel||brake))schedule(c,brake?SB_IDLE_FLAT:left?SB_CRUISE_LEFT:SB_CRUISE_RIGHT);
 if(!attack(c))return 0;
 if(accel){
  if(!held(c,left?PB_LEFT:PB_RIGHT))schedule(c,left?SB_BRAKE_LEFT:SB_BRAKE_RIGHT);
  if(held(c,left?PB_RIGHT:PB_LEFT))schedule(c,left?SB_CRUISE_RIGHT:SB_CRUISE_LEFT);
 }else if(brake){
  if(held(c,PB_RIGHT))schedule(c,SB_CRUISE_RIGHT);if(held(c,PB_LEFT))schedule(c,SB_CRUISE_LEFT);
 }
 return ground_tail(c);
}
static int slope_drive(struct SlugBodyContext*c,int crouch){
 struct SlugSharedState*s=body(c);struct WeaponActor*a=&s->actor;
 if(!crouch){slug_set_attack(s,SATT_NORMAL);hit_begin_attack(&a->hit);}s->status_flags|=4;
 if(!update(c,0))return 0;
 a->vx=crouch?0:s->impulse;if(crouch)s->status_flags|=16;
 if(crouch||(!s->destroyed_timer&&!s->impulse)){
  if(held(c,PB_RIGHT))a->vx=0x2a0;else if(held(c,PB_LEFT))a->vx=-0x2a0;
 }
 scale_slope_speed(c);display(c);if(move(c,crouch?SLMOVE_GROUND_C:SLMOVE_GROUND_B)<0)return 0;
 if(angle_changed(s)&&!select_angle(s,crouch?SANI_ANGLED_STALL:SANI_ANGLED_NORMAL,crouch?195:0,crouch))return 0;
 if(animate(c)<0)return 0;if(flat(s))reenter(c,crouch);
 if(!a->animation.pose&&!attack(c))return 0;
 if(crouch){if(!held(c,PB_DOWN))schedule(c,SB_SLOPE_MOUNT);}
 else if(held(c,PB_DOWN))schedule(c,SB_SLOPE_IDLE_ENTER);
 if(!fall_if_air(c,SB_FALL_START))return 0;
 if(!crouch){jump(c,1);fire(c,SB_FIRE_SLOPE);}absent(c);combo(c,SB_DESTROYED);
 if(!damage(c,0))return 0;if(!crouch)bounds(c);return !s->fault;
}
static int slope_transition(struct SlugBodyContext*c,int mounting){
 struct SlugSharedState*s=body(c);s->status_flags|=4;
 if(!update(c,0))return 0;display(c);if(move(c,SLMOVE_GROUND_B)<0)return 0;
 int complete=animate(c);if(complete<0)return 0;if(complete)schedule(c,mounting?SB_IDLE_SLOPE:SB_STALL);
 if(!fall_if_air(c,SB_FALL_START))return 0;jump(c,0);if(mounting)fire(c,SB_FIRE_SLOPE);
 absent(c);combo(c,SB_DESTROYED);return damage(c,0);
}
static int drive_transition(struct SlugBodyContext*c,enum SlugBodyState state){
 struct SlugSharedState*s=body(c);struct WeaponActor*a=&s->actor;int alternate=state==SB_DRIVE_ALT;
 if(!update(c,0))return 0;a->vx=0;if(!alternate)s->status_flags|=16;
 if(held(c,PB_RIGHT))a->vx=0x2a0;if(held(c,PB_LEFT))a->vx=-0x2a0;
 int mismatch=move(c,alternate?SLMOVE_GROUND_E:SLMOVE_GROUND_F);if(mismatch<0)return 0;
 if(mismatch)schedule(c,alternate?SB_IDLE_SLOPE:SB_STALL);
 a->vx=0;display(c);int complete=animate(c);if(complete<0)return 0;
 if(complete){
  if(alternate){slug_set_defense(s,SDEF_CROUCH);s->body_contact=SBC_CROUCH;}
  reenter(c,!alternate);
 }
 if(state!=SB_DRIVE_B){
  if(alternate?held(c,PB_DOWN):!held(c,PB_DOWN))schedule(c,alternate?SB_TURN_TO_DRIVE:SB_TURN_TO_DRIVE_ALT);
 }
 if(!fall_if_air(c,SB_FALL))return 0;
 if(slug_jump_input(c->input,c->layout,state==SB_DRIVE_B))slug_body_schedule_speed(s,0);
 if(alternate)fire(c,SB_FIRE_FLAT);absent(c);combo(c,alternate?SB_DESTROYED_SLIDE:SB_KNOCKED);return damage(c,0);
}
static int crouch_brake(struct SlugBodyContext*c){
 struct SlugSharedState*s=body(c);if(!update(c,0))return 0;s->status_flags|=16;
 int mismatch=move(c,SLMOVE_GROUND_F);if(mismatch<0)return 0;
 if(mismatch)schedule(c,SB_STALL);if(slug_update_angle(s))schedule(c,SB_STALL);
 if(held(c,PB_RIGHT))slug_body_schedule_argument(s,SB_CROUCH_ACCEL_RIGHT,1);
 if(held(c,PB_LEFT))slug_body_schedule_argument(s,SB_CROUCH_ACCEL_LEFT,1);
 if(animate(c)<0)return 0;if(!held(c,PB_DOWN))schedule(c,SB_DRIVE_ALT);return crouch_tail(c);
}
static int crouch_drive(struct SlugBodyContext*c,enum SlugBodyState state){
 struct SlugSharedState*s=body(c);
 int left=state==SB_CROUCH_ACCEL_LEFT||state==SB_DECEL_LEFT||state==SB_CROUCH_CRUISE_LEFT;
 int accel=state==SB_CROUCH_ACCEL_RIGHT||state==SB_CROUCH_ACCEL_LEFT;
 int decel=state==SB_DECEL_RIGHT||state==SB_DECEL_LEFT;
 if(accel||decel)ramp(s,decel?0:left?-0x2a0:0x2a0);
 if(!update(c,0))return 0;s->status_flags|=16;
 int mismatch=move(c,SLMOVE_GROUND_F);if(mismatch<0)return 0;
 if(mismatch)slug_body_schedule_argument(s,SB_STALL,left?2:1);
 if(slug_update_angle(s))slug_body_schedule_argument(s,SB_STALL,left?2:1);
 if(!held(c,PB_DOWN))schedule(c,SB_DRIVE_ALT);
 if(!accel&&!decel&&!held(c,left?PB_LEFT:PB_RIGHT))schedule(c,left?SB_DECEL_LEFT:SB_DECEL_RIGHT);
 int complete=animate(c);if(complete<0)return 0;
 if(complete&&(accel||decel))schedule(c,decel?SB_CROUCH_BRAKE:left?SB_CROUCH_CRUISE_LEFT:SB_CROUCH_CRUISE_RIGHT);
 if(!attack(c))return 0;
 if(accel){
  if(!held(c,left?PB_LEFT:PB_RIGHT))schedule(c,left?SB_DECEL_LEFT:SB_DECEL_RIGHT);
  if(held(c,left?PB_RIGHT:PB_LEFT))schedule(c,left?SB_CROUCH_CRUISE_RIGHT:SB_CROUCH_CRUISE_LEFT);
 }else if(decel){
  if(held(c,PB_RIGHT))schedule(c,SB_CROUCH_CRUISE_RIGHT);if(held(c,PB_LEFT))schedule(c,SB_CROUCH_CRUISE_LEFT);
 }
 return crouch_tail(c);
}
static int jump_crouch(struct SlugBodyContext*c){
 if(!update(c,0)||move(c,SLMOVE_GROUND_RAW)<0)return 0;display(c);
 int complete=animate(c);if(complete<0)return 0;if(complete)schedule(c,SB_JUMP_LAUNCH);
 if(!attack(c))return 0;fire(c,SB_FIRE_JUMP_CROUCH);absent(c);return damage(c,0);
}
static int air_frame(struct SlugBodyContext*c,enum SlugBodyState state){
 struct SlugSharedState*s=body(c);struct WeaponActor*a=&s->actor;
 int falling=state==SB_FALL_START||state==SB_FALL_LOOP||state==SB_FALL_ANGLE;
 if(state==SB_JUMP_AIR_ANGLE||state==SB_FALL_START||state==SB_FALL_ANGLE)s->status_flags|=4;
 if(state==SB_JUMP_AIR)fire(c,SB_FIRE_JUMP_AIR);
 else if(state==SB_JUMP_AIR_ANGLE||state==SB_FALL_ANGLE)fire(c,SB_FIRE_AIR_ANGLE);
 steer(c,0);if(falling||a->vy<=0)a->vy=clamp(a->vy,0x400);s->status_flags|=2;
 if(!update(c,1))return 0;if(falling)fire(c,SB_FIRE_AIR);
 int landed=move(c,SLMOVE_AIR);if(landed<0)return 0;
 if(landed){
  schedule(c,SB_JUMP_LAND);down(c,1);
  if(falling){if(c->layout==2||c->layout==3)jump(c,1);}else fire(c,SB_FIRE_FLAT);
 }
 if(falling)display(c);if(animate(c)<0)return 0;
 if(a->vy<=0){
  unsigned count=0;if(contact(c,&count)<0)return 0;
  if(count){a->vx=a->ax=0;}
  /* Jump checks angle only after at least one contact; fall checks it even
   * with count zero. These two source branches must not be merged. */
  if((falling||count)&&angle_changed(s)){
   if(!select_angle(s,SANI_ANGLED_NORMAL,0,0))return 0;s->status_flags|=4;
   if(s->body_state==(falling?SB_FALL_LOOP:SB_JUMP_AIR))schedule(c,falling?SB_FALL_ANGLE:SB_JUMP_AIR_ANGLE);
  }
 }
 if(!damage(c,!!(s->status_flags&2)))return 0;bounds(c);return !s->fault;
}
static int landing(struct SlugBodyContext*c,int slope){
 struct SlugSharedState*s=body(c);struct WeaponActor*a=&s->actor;
 if(slope){s->status_flags|=4;--s->accel_timer;}
 if(!update(c,0))return 0;
 if(slope){if(held(c,PB_DOWN))schedule(c,SB_SLOPE_IDLE_ENTER);}else down(c,1);
 steer(c,1);if(move(c,SLMOVE_GROUND_E)<0)return 0;display(c);if(!slope)--s->accel_timer;
 int complete=animate(c);if(complete<0)return 0;
 if(slope){
  unsigned count=0;if(contact(c,&count)<0)return 0;
  if(!s->accel_timer||count>=3){a->vx=a->ax=0;schedule(c,SB_IDLE_SLOPE);}
 }else if(complete||!s->accel_timer){a->vx=a->ax=0;schedule(c,SB_IDLE_SLOPE);}
 if(angle_changed(s)){
  if(!select_angle(s,SANI_ANGLED_NORMAL,0,0))return 0;
  if(!slope){s->status_flags|=4;schedule(c,SB_LAND_SLOPE_FRAME);}
 }
 if(!attack(c)||!fall_if_air(c,SB_FALL))return 0;
 if(!slope||c->layout==2||c->layout==3)jump(c,1);
 fire(c,SB_FIRE_FLAT);absent(c);combo(c,slope?SB_DESTROYED:SB_DESTROYED_SLIDE);return damage(c,0);
}
static int hit_ground(struct SlugBodyContext*c){
 struct SlugSharedState*s=body(c);struct WeaponActor*a=&s->actor;
 a->hit.invulnerable=40;a->hit.flags&=(uint8_t)~8u;
 if(!update(c,0)||move(c,SLMOVE_GROUND_E)<0)return 0;
 int complete=animate(c);if(complete<0)return 0;
 if(complete){
  schedule(c,SB_IDLE_FLAT);a->hit.invulnerable=40;a->damage_timer=40; /* $05E722: byte $59 */
  if(!slug_set_defense(s,SDEF_CROUCH))return 0;
 }
 if(!fall_if_air(c,SB_FALL))return 0;absent(c);if(dead(c))schedule(c,SB_DEATH_EXPLODE);return !s->fault;
}
static int hit_air(struct SlugBodyContext*c,enum SlugBodyState state){
 struct SlugSharedState*s=body(c);struct WeaponActor*a=&s->actor;
 int land=state==SB_HIT_LAND_A||state==SB_HIT_LAND_B;
 if(!land)s->status_flags|=2;
 a->hit.invulnerable=40;a->damage_timer=40;a->hit.flags&=(uint8_t)~8u;
 if(!update(c,1))return 0;a->vy=clamp(a->vy,0x400);
 int landed=move(c,land?SLMOVE_GROUND_E:SLMOVE_AIR);if(landed<0)return 0;
 if(!land&&landed){schedule(c,state==SB_HIT_AIR_A?SB_HIT_LAND_A:SB_HIT_LAND_B);a->ay=0;stop(s);}
 int complete=animate(c);if(complete<0)return 0;
 if(land){if(complete)schedule(c,SB_HIT_GROUND_FRAME);absent(c);}
 if(dead(c))schedule(c,land?SB_DEATH_EXPLODE:SB_DEATH_LAUNCH);bounds(c);return !s->fault;
}
static int unoccupied_idle(struct SlugBodyContext*c){
 struct SlugSharedState*s=body(c);if(!update(c,0))return 0;
 s->status_flags|=48;s->actor.vx=0;int mismatch=move(c,SLMOVE_GROUND_F);if(mismatch<0)return 0;
 if(mismatch)schedule(c,SB_UNOCCUPIED_ANGLE);display(c);
 if(animate(c)<0||!fall_if_air(c,SB_UNOCCUPIED_FALL))return 0;
 if(mount_candidate(c))schedule(c,SB_HUNKER);
 if(dead(c))schedule(c,SB_DEATH_EXPLODE);unoccupied_bounds(c);return !s->fault;
}
static int unoccupied_angle(struct SlugBodyContext*c,int alternate){
 struct SlugSharedState*s=body(c);s->status_flags|=52;if(!update(c,0))return 0;
 s->actor.vx=0;display(c);if(move(c,SLMOVE_GROUND_C)<0)return 0;
 if(angle_changed(s)&&!select_angle(s,SANI_ANGLED_UNOCCUPIED,195,1))return 0;
 if(animate(c)<0)return 0;if(flat(s))schedule(c,SB_IDLE_ENTER_C);
 if(alternate){
  if(!s->actor.animation.pose&&!attack(c))return 0;
  if(!held(c,PB_DOWN))schedule(c,SB_HUNKER);
  if(!fall_if_air(c,SB_FALL_START))return 0;return damage(c,0);
 }
 if(!fall_if_air(c,SB_UNOCCUPIED_FALL_KEEP))return 0;
 if(mount_candidate(c))schedule(c,SB_PLAYER_MOUNT);
 if(dead(c))schedule(c,SB_DEATH_EXPLODE);unoccupied_bounds(c);return !s->fault;
}
static int unoccupied_fall(struct SlugBodyContext*c,int keep){
 struct SlugSharedState*s=body(c);struct WeaponActor*a=&s->actor;
 if(keep)s->status_flags|=4;a->vy=clamp(a->vy,0x400);s->status_flags|=34;
 if(!update(c,1))return 0;int landed=move(c,SLMOVE_AIR);if(landed<0)return 0;
 if(landed)schedule(c,SB_IDLE_ENTER_C);display(c);if(animate(c)<0)return 0;
 if(a->vy<=0){
  unsigned count=0;if(contact(c,&count)<0)return 0;if(count)a->vx=a->ax=0;
  if(angle_changed(s)){if(!select_angle(s,SANI_ANGLED_UNOCCUPIED,0,0))return 0;s->status_flags|=4;}
 }
 if(!(s->status_flags&2)&&dead(c))schedule(c,SB_DEATH_EXPLODE);unoccupied_bounds(c);return !s->fault;
}
static int mounting(struct SlugBodyContext*c,int slope){
 struct SlugSharedState*s=body(c);if(slope)s->status_flags|=4;
 if(!update(c,0))return 0;if(!slope)s->actor.vx=0;
 if(slope)display(c);if(move(c,slope?SLMOVE_GROUND_B:SLMOVE_GROUND_E)<0)return 0;
 if(!slope){s->actor.vx=0;display(c);}
 int complete=animate(c);if(complete<0)return 0;
 if(complete){
  if(slope)schedule(c,SB_SLOPE_MOUNT);
  else{if(!slug_set_defense(s,SDEF_CROUCH))return 0;s->body_contact=SBC_CROUCH;schedule(c,SB_DRIVE_ALT);}
 }
 if(!fall_if_air(c,slope?SB_FALL_START:SB_FALL))return 0;
 combo(c,slope?SB_DESTROYED:SB_DESTROYED_SLIDE);return damage(c,0);
}
static int fire_ground(struct SlugBodyContext*c,int recoil){
 struct SlugSharedState*s=body(c);struct WeaponActor*a=&s->actor;
 if(recoil){a->vx=a->ax=0;if(!update(c,0))return 0;
  if(held(c,PB_RIGHT))slug_body_schedule_argument(s,SB_ACCEL_RIGHT,1);
  if(held(c,PB_LEFT))slug_body_schedule_argument(s,SB_ACCEL_LEFT,1);
 }else{
  fire(c,SB_FIRE_FLAT);if(held(c,PB_RIGHT))schedule(c,SB_FIRE_MOVE_RIGHT);if(held(c,PB_LEFT))schedule(c,SB_FIRE_MOVE_LEFT);
  if(!update(c,0))return 0;
 }
 a->vx=clamp(a->vx,0x540);int mismatch=move(c,recoil?SLMOVE_GROUND_E:SLMOVE_GROUND_RAW);if(mismatch<0)return 0;
 if(mismatch)schedule(c,SB_IDLE_SLOPE);display(c);int complete=animate(c);if(complete<0)return 0;
 if(complete)schedule(c,recoil?SB_IDLE_FLAT:SB_FIRE_RECOIL);
 if(recoil)return ground_tail(c);
 if(s->cooldown<16){down(c,0);jump(c,0);}if(!fall_if_air(c,SB_FALL))return 0;
 absent(c);combo(c,SB_DESTROYED_SLIDE);return damage(c,0);
}
static int fire_move(struct SlugBodyContext*c,int left){
 struct SlugSharedState*s=body(c);if(!update(c,0))return 0;
 int mismatch=move(c,SLMOVE_GROUND_E);if(mismatch<0)return 0;
 if(mismatch)schedule(c,SB_IDLE_SLOPE);if(slug_update_angle(s))schedule(c,SB_IDLE_SLOPE);
 if(!held(c,left?PB_LEFT:PB_RIGHT))schedule(c,SB_FIRE_FLAT_RESUME);
 if(held(c,left?PB_RIGHT:PB_LEFT))schedule(c,left?SB_FIRE_MOVE_RIGHT:SB_FIRE_MOVE_LEFT);
 int complete=animate(c);if(complete<0)return 0;if(complete)schedule(c,left?SB_CRUISE_LEFT:SB_CRUISE_RIGHT);
 if(!attack(c))return 0;down(c,0);if(!fall_if_air(c,SB_FALL))return 0;jump(c,0);
 absent(c);combo(c,SB_DESTROYED_SLIDE);return damage(c,0);
}
static int fire_slope(struct SlugBodyContext*c){
 struct SlugSharedState*s=body(c);s->status_flags|=4;if(!update(c,1))return 0;
 s->actor.vy=clamp(s->actor.vy,0x400);if(move(c,SLMOVE_GROUND_RAW)<0)return 0;
 display(c);if(animate(c)<0)return 0;if(angle_changed(s)&&!select_angle(s,SANI_ANGLED_NORMAL,0,0))return 0;
 schedule(c,SB_IDLE_SLOPE);if(!fall_if_air(c,SB_FALL))return 0;jump(c,0);fire(c,SB_FIRE_SLOPE);
 absent(c);return damage(c,0);
}
static int fire_air(struct SlugBodyContext*c,int resume){
 struct SlugSharedState*s=body(c);struct WeaponActor*a=&s->actor;if(resume)s->status_flags|=4;
 steer(c,0);if(a->vy<=0)a->vy=clamp(a->vy,0x400);s->status_flags|=2;
 if(!update(c,1))return 0;int landed=move(c,SLMOVE_AIR);if(landed<0)return 0;
 if(landed){schedule(c,SB_JUMP_LAND);down(c,1);if(c->layout==2||c->layout==3)jump(c,1);}
 display(c);if(animate(c)<0)return 0;
 if(a->vy<=0){unsigned count=0;if(contact(c,&count)<0)return 0;
  if(count){a->vx=a->ax=0;if(angle_changed(s)){
   if(!select_angle(s,SANI_ANGLED_NORMAL,0,0))return 0;s->status_flags|=4;
   if(s->body_state==SB_FIRE_AIR_LOOP)schedule(c,SB_FIRE_AIR_RESUME);
  }}
 }
 if(!damage(c,!!(s->status_flags&2)))return 0;bounds(c);return !s->fault;
}
static int blast_once(struct SlugBodyContext*c,unsigned kind){
 struct SlugSharedState*s=body(c);slug_set_attack(s,kind);hit_begin_attack(&s->actor.hit);hit_begin_attack(&s->actor.hit);
 if(!attack(c))return 0;s->actor.attack=0;hit_begin_attack(&s->actor.hit);return 1;
}
static int explode(struct SlugBodyContext*c){return slug_spawn_death_fx(&c->animation)?1:fail(body(c),6);}
static int death_tail(struct SlugBodyContext*c){
 struct SlugSharedState*s=body(c);bounds(c);
 if(s->body_state!=SB_RETIRED_RESET)(void)slug_rider_absent(s);return !s->fault;
}
static int death_frame(struct SlugBodyContext*c,enum SlugBodyState state){
 struct SlugSharedState*s=body(c);struct WeaponActor*a=&s->actor;
 if(state==SB_DEATH_FADE){
  if(move(c,SLMOVE_SYNC)<0)return 0;if(!--s->accel_timer)return retire_slot(c);
  (void)slug_rider_absent(s);return !s->fault;
 }
 if(state==SB_DEATH_START){
  if(!blast_once(c,SATT_SHORT)||!explode(c))return 0;schedule(c,SB_DEATH_FADE);
 }
 if(!update(c,0))return 0;
 if(state==SB_SELF_DESTRUCT){
  s->status_flags|=16;c->animation.weapons->shotgun_flash=1;
  if(!blast_once(c,SATT_DESTROYED_A)||!explode(c))return 0;
  if(!slug_spawn_self_destruct_blast(&c->animation))return fail(s,6);
  if(move(c,SLMOVE_SYNC)<0||animate(c)<0)return 0;schedule(c,SB_DEATH_FADE);
  (void)slug_rider_absent(s);return !s->fault;
 }
 if(state==SB_DEATH_EXPLODE){
  s->status_flags|=16;int finished;
  if(!slug_damage_display(s,1,&finished))return 0;
  if(finished){schedule(c,SB_DEATH_FADE);if(!blast_once(c,SATT_SHORT)||!explode(c))return 0;}
 }else if(state==SB_DEATH_LAUNCH){
  s->status_flags|=16;a->vy=clamp(a->vy,0x400);int landed=move(c,SLMOVE_AIR);if(landed<0)return 0;
  if(landed)schedule(c,SB_DEATH_EXPLODE);
 }
 if(state!=SB_DEATH_LAUNCH&&move(c,SLMOVE_SYNC)<0)return 0;if(animate(c)<0)return 0;return death_tail(c);
}
static void clamp_destroyed_timer(struct SlugBodyContext*c){
 struct SlugSharedState*s=body(c);struct WeaponActor*a=&s->actor;
 int16_t x=word((a->transform.x>>8)-c->animation.terrain->camera_x);if(x<192)return;
 int16_t y=word(transform_y_up(&a->transform)+c->animation.terrain->camera_y);a->hit.flags|=128;
 if((x>321||y<255||y>496)&&(int8_t)s->destroyed_timer>10)s->destroyed_timer=10;
}
static int destroyed_damage(struct SlugBodyContext*c){
 int complete;if(!slug_damage_display(body(c),2,&complete))return 0;
 if(complete)schedule(c,SB_SELF_DESTRUCT);clamp_destroyed_timer(c);return 1;
}
static int destroyed_slide(struct SlugBodyContext*c,enum SlugBodyState state){
 struct SlugSharedState*s=body(c);struct WeaponActor*a=&s->actor;int knocked=state==SB_KNOCKED;
 if(state!=SB_DESTROYED_SLIDE_LOOP&&!s->body_phase){
  if(!knocked){a->defense=0;a->defense_count=0;}
  if(s->destroyed_timer<=41){a->vx=0x600;a->ax=0;s->body_phase=1;}
 }
 if(!update(c,0))return 0;if(knocked)s->status_flags|=16;
 int mismatch=move(c,SLMOVE_GROUND_E);if(mismatch<0)return 0;if(mismatch)schedule(c,SB_DESTROYED_RESUME);
 if(animate(c)<0)return 0;
 if(!(c->frame_phase&1)){slug_set_attack(s,SATT_DESTROYED_B);hit_begin_attack(&a->hit);hit_begin_attack(&a->hit);}
 if(!attack(c)||!destroyed_damage(c))return 0;
 if(s->destroyed_timer)s->destroyed_timer--;if(!s->destroyed_timer)schedule(c,SB_SELF_DESTRUCT);
 if(word((a->transform.x>>8)-c->animation.terrain->camera_x)>=279)schedule(c,SB_SELF_DESTRUCT);
 a->vy=a->ay=0;(void)slug_rider_absent(s);return !s->fault;
}
static int destroyed_slope(struct SlugBodyContext*c){
 struct SlugSharedState*s=body(c);struct WeaponActor*a=&s->actor;s->status_flags|=4;
 if(!update(c,0))return 0;a->vx=s->impulse;scale_slope_speed(c);display(c);
 if(move(c,SLMOVE_GROUND_B)<0)return 0;
 if(angle_changed(s)){if(!select_angle(s,SANI_ANGLED_NORMAL,0,0))return 0;a->defense=0;a->defense_count=0;}
 if(animate(c)<0)return 0;
 if(!(c->frame_phase&1)){slug_set_attack(s,SATT_DESTROYED_B);hit_begin_attack(&a->hit);hit_begin_attack(&a->hit);if(!attack(c))return 0;}
 if(flat(s)&&s->destroyed_timer&&s->impulse>0)schedule(c,SB_DESTROYED_SLIDE_RESUME);
 if(!a->animation.pose&&!attack(c))return 0;if(!destroyed_damage(c))return 0;
 if(!s->destroyed_timer){s->impulse=0;schedule(c,SB_SELF_DESTRUCT);}else s->destroyed_timer--;
 if(word((a->transform.x>>8)-c->animation.terrain->camera_x)>=279)schedule(c,SB_SELF_DESTRUCT);
 a->vy=a->ay=0;(void)slug_rider_absent(s);return !s->fault;
}
int slug_body_step(struct SlugBodyContext*c){
 if(!c||!c->animation.slug)return 0;struct SlugSharedState*s=body(c);
 if(s->fault)return 0;
 if(s->body_state==SB_INACTIVE)return 1;
 if(!c->animation.terrain||!c->animation.weapons)return fail(s,3);
 if(c->animation.terrain->fault||c->animation.weapons->fault)return fail(s,3);
 enum SlugBodyState state=(enum SlugBodyState)s->body_state;
 if(!slug_body_implemented(state))return fail(s,7);
 if(state==SB_RETIRE)return retire_slot(c);if(state==SB_RETIRED_RESET)return reset_slot(c);
 if(s->body_enter){s->body_enter=0;state=enter(c,state);if(s->fault)return 0;s->body_state=(uint8_t)state;}
 switch(state){
 case SB_BOSS_ARENA:
  s->actor.hit.flags|=64;if(!update(c,1)||move(c,SLMOVE_AIR)<0)return 0;display(c);
  if(animate(c)<0)return 0;schedule(c,SB_FALL);return 1;
 case SB_BOSS_DROP_FRAME:case SB_BOSS_DESCEND_FRAME:case SB_BOSS_DESCEND_STEADY:return boss_drop_frame(c,state);
 case SB_DROP_FRAME:case SB_DROP_DESCEND_FRAME:return drop_frame(c,state==SB_DROP_DESCEND_FRAME);
 case SB_DESTROYED_SLIDE:case SB_DESTROYED_SLIDE_LOOP:case SB_KNOCKED:return destroyed_slide(c,state);
 case SB_DESTROYED_FRAME:return destroyed_slope(c);
 case SB_DEATH_START:case SB_DEATH_EXPLODE:case SB_DEATH_LAUNCH:case SB_DEATH_FADE:case SB_SELF_DESTRUCT:return death_frame(c,state);
 case SB_FIRE_FLAT:case SB_FIRE_RECOIL:return fire_ground(c,state==SB_FIRE_RECOIL);
 case SB_FIRE_MOVE_RIGHT:case SB_FIRE_MOVE_LEFT:return fire_move(c,state==SB_FIRE_MOVE_LEFT);
 case SB_FIRE_SLOPE:return fire_slope(c);
 case SB_FIRE_AIR_LOOP:case SB_FIRE_AIR_RESUME:return fire_air(c,state==SB_FIRE_AIR_RESUME);
 case SB_IDLE_FLAT:return idle_flat(c);
 case SB_IDLE_SLOPE:return slope_drive(c,0);
 case SB_ACCEL_RIGHT:case SB_ACCEL_LEFT:case SB_BRAKE_RIGHT:case SB_BRAKE_LEFT:
 case SB_CRUISE_RIGHT:case SB_CRUISE_LEFT:return ground_drive(c,state);
 case SB_SLOPE_IDLE_ENTER:case SB_SLOPE_MOUNT:return slope_transition(c,state==SB_SLOPE_MOUNT);
 case SB_DRIVE:case SB_DRIVE_B:case SB_DRIVE_ALT:return drive_transition(c,state);
 case SB_CROUCH_BRAKE:return crouch_brake(c);
 case SB_STALL:return slope_drive(c,1);
 case SB_CROUCH_ACCEL_RIGHT:case SB_CROUCH_ACCEL_LEFT:case SB_DECEL_RIGHT:case SB_DECEL_LEFT:
 case SB_CROUCH_CRUISE_RIGHT:case SB_CROUCH_CRUISE_LEFT:return crouch_drive(c,state);
 case SB_JUMP_CROUCH:return jump_crouch(c);
 case SB_JUMP_AIR:case SB_JUMP_AIR_ANGLE:case SB_FALL_START:case SB_FALL_LOOP:case SB_FALL_ANGLE:return air_frame(c,state);
 case SB_JUMP_LAND:case SB_LAND_SLOPE_FRAME:return landing(c,state==SB_LAND_SLOPE_FRAME);
 case SB_HIT_GROUND_FRAME:return hit_ground(c);
 case SB_HIT_AIR_A:case SB_HIT_AIR_B:case SB_HIT_LAND_A:case SB_HIT_LAND_B:return hit_air(c,state);
 case SB_UNOCCUPIED_IDLE:return unoccupied_idle(c);
 case SB_UNOCCUPIED_ANGLE:case SB_IDLE_ANGLED_B:return unoccupied_angle(c,state==SB_IDLE_ANGLED_B);
 case SB_UNOCCUPIED_FALL:case SB_UNOCCUPIED_FALL_KEEP:return unoccupied_fall(c,state==SB_UNOCCUPIED_FALL_KEEP);
 case SB_HUNKER:case SB_PLAYER_MOUNT:return mounting(c,state==SB_PLAYER_MOUNT);
 default:return fail(s,7);
 }
}
