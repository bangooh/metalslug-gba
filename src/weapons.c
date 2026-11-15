#include "weapons.h"
#include "player_rules.h"
#include "bosses.h"

enum Life {
 LIFE_NEW,LIFE_BULLET,LIFE_BULLET_GROUND,LIFE_BULLET_AIR,LIFE_DELETE,
 LIFE_SHOTGUN,LIFE_FLAME,LIFE_FLAME_IMPACT_INIT,LIFE_FLAME_IMPACT,
 LIFE_ROCKET,LIFE_ROCKET_GROUND,LIFE_ROCKET_AIR_INIT,LIFE_ROCKET_AIR,
 LIFE_GRENADE_HEAVY,LIFE_GRENADE_DOWN,LIFE_GRENADE_ARC,
 LIFE_GRENADE_BOUNCE_TIMER,LIFE_GRENADE_BOUNCE_BODY,
 LIFE_GRENADE_BLAST_HIT,LIFE_GRENADE_BLAST_DRAW,LIFE_EXPLOSION,
 LIFE_SURFACE_FOLLOW,LIFE_SURFACE_START,LIFE_SURFACE_DRAW,
 LIFE_DRAW,LIFE_ROCKET_TRAIL,LIFE_FLAME_TRAIL,LIFE_ROCKET_SEEK,
 LIFE_DROP,LIFE_DROP_BOUNCE_INIT,LIFE_DROP_BOUNCE,LIFE_DROP_IMPACT
};
struct AnimationBridge {struct WeaponWorld *world;struct NativeEntity *entity;};
static void projectile_tick(struct EntityPool*,struct NativeEntity*,void*);
static void effect_tick(struct EntityPool*,struct NativeEntity*,void*);
static void auxiliary_tick(struct EntityPool*,struct NativeEntity*,void*);
static void dropped_tick(struct EntityPool*,struct NativeEntity*,void*);
static void player_effect_tick(struct EntityPool*,struct NativeEntity*,void*);
static int fail(struct WeaponWorld*w,unsigned reason){if(!w->fault)w->fault=(uint8_t)reason;return 0;}
static int16_t word(int value){return (int16_t)(uint16_t)value;}
static int32_t whole(int32_t value,int delta){return word((value>>8)+delta)*256;}
static void synchronize(struct WeaponActor*a){
 a->hit.x=(int16_t)(a->transform.x>>8);a->hit.y=transform_y_up(&a->transform);
 a->hit.flip=a->transform.facing;
}
static void start(struct WeaponActor*a,unsigned sequence){
 animation_begin(&a->animation,weapon_animation_entries[sequence]);a->movement_flags|=32;
}
static void retire(struct WeaponWorld*w,struct NativeEntity*e,int children){
 e->payload.weapon.active=0;
 if(children)entity_delete_children(w->entities,e);
 entity_delete(w->entities,e);
}
static struct NativeEntity*allocate(struct WeaponWorld*w,struct EntityRef parent,
 enum EntityKind kind,enum EntityInsert order,EntityTick tick){
 if(w->fault||!w->entities){fail(w,1);return 0;}
 struct NativeEntity*e=entity_spawn(w->entities,parent,kind,order,tick);
 if(!e){fail(w,2);return 0;}
 /* $05DC1C/$05DC34: explicitly preserve the nonzero source defaults used by
  * this family. Zeroing a C object alone does not reproduce allocation. */
 e->payload.weapon=(struct WeaponActor){.active=1,.owner=255,.damage_owner=255,.score_owner=255,
  .trail_drawing=65535,.phase=255,.ttl=255,.raw_surface=255,.origin_y_up=-1,
  .ground_slope=-1,.ground_tag=-1,.platform_ids=255,
  .hit={.id=e->index,.hp=-1,.impact_x=-1,.impact_y=-1,.last_attacker=65535,.last_kind=255,.flags=64},
  .animation={.drawing=65535,.cursor=65535,.last_draw_cursor=65535}};
 return e;
}
static int noise(struct WeaponWorld*w,uint16_t command,int stop){
 return w->sound&&w->sound(w->scene,command,stop)?1:fail(w,3);
}
static int attack(struct WeaponWorld*w,struct WeaponActor*a,struct HitActor**selected){
 synchronize(a);if(selected)*selected=0;
 if(!a->attack)return 1;
 if(!w->attack)return fail(w,4);
 for(unsigned i=0;i<a->attack->count;i++){
  struct HitActor*found=0;
  if(!w->attack(w->scene,&a->hit,&a->attack->records[i],&found))return fail(w,4);
  if(selected&&found)*selected=found;
 }
 return 1;
}
static int count(struct WeaponWorld*w,struct WeaponActor*a,int grenade){
 unsigned slot=a->damage_owner==1?0:1;
 if(!w->players[slot])return fail(w,5);
 if(grenade)w->players[slot]->active_grenades++;
 else w->players[slot]->active_shots++;
 return 1;
}
static int random_word(struct WeaponWorld*w,uint16_t*out){
 if(!w->rng)return fail(w,6);*out=source_random_word(w->rng);return 1;
}
static int bridge_attack(void*user,struct HitActor*hit,const struct HitAttack*shape){
 struct AnimationBridge*b=user;struct WeaponWorld*w=b->world;
 return w->attack&&w->attack(w->scene,hit,shape,0)?1:fail(w,4);
}
static int bridge_sound(void*user,uint16_t command,int stop){
 return noise(((struct AnimationBridge*)user)->world,command,stop);
}
static int bridge_effect(void*user,enum WeaponEffect effect,const struct PlayerTransform*t){
 struct AnimationBridge*b=user;return weapon_spawn_effect(b->world,entity_ref(b->entity),effect,t);
}
static int animate(struct WeaponWorld*w,struct NativeEntity*e){
 struct AnimationBridge bridge={w,e};
 struct WeaponAnimationContext c={.actor=&e->payload.weapon,.world=&bridge,
  .attack=bridge_attack,.sound=bridge_sound,.effect=bridge_effect};
 int complete=weapon_animation_step(&c);
 if(c.fault||e->payload.weapon.animation.fault)fail(w,7);
 return complete;
}
static int move(struct WeaponWorld*w,struct WeaponActor*a,enum WeaponMove mode,struct WeaponContact*hit){
 *hit=(struct WeaponContact){0};
 if(!w->move){fail(w,8);return -1;}
 int result=w->move(w->scene,a,mode,hit);
 if(result<0)fail(w,8);synchronize(a);return result;
}
static int move_twice(struct WeaponWorld*w,struct WeaponActor*a,enum WeaponMove mode,struct WeaponContact*hit){
 int result=move(w,a,mode,hit);return result?result:move(w,a,mode,hit);
}
/* $05DD56's screen rectangle, evaluated against source-sized world camera.
 * World-coordinate actors do not need the original per-object camera subtract. */
static int outside(struct WeaponWorld*w,struct WeaponActor*a,int left,int right,int top,int bottom){
 a->hit.flags|=128;
 int16_t x=word((a->transform.x>>8)-w->camera_x);
 int16_t y=word(transform_y_up(&a->transform)+w->camera_y);
 return x<left||x>320+right||y<256-bottom||y>496-top;
}
static int grenade_outside(struct WeaponWorld*w,struct WeaponActor*a){
 int16_t y=word(transform_y_up(&a->transform)+w->camera_y);
 uint16_t x=(uint16_t)word((a->transform.x>>8)-w->camera_x);
 return y<256||x>=336;
}
static void bound_count(struct WeaponWorld*w,struct NativeEntity*e,int children){
 if(!count(w,&e->payload.weapon,0))return;
 if(outside(w,&e->payload.weapon,-48,48,-48,48))retire(w,e,children);
}
static struct PlayerActionLayer*player_layer(struct NativeEntity*e){
 if(!e)return 0;
 if(e->kind==ENTITY_PLAYER)return &e->payload.player.actor;
 if(e->kind==ENTITY_PLAYER_ARM)return &e->payload.arm;
 return 0;
}
static const struct PlayerTransform*transform(struct NativeEntity*e){
 struct PlayerActionLayer*p=player_layer(e);if(p)return &p->transform;
 struct WeaponActor*a=entity_weapon_actor(e);if(a)return &a->transform;
 return 0;
}
static void copy_integer_position(struct WeaponActor*a,const struct PlayerTransform*t){
 transform_integer_copy(&a->transform,t);
}

int weapon_spawn_effect(struct WeaponWorld*w,struct EntityRef parent,enum WeaponEffect kind,const struct PlayerTransform*t){
 if((unsigned)kind>WEFFECT_BLAST_C)return fail(w,9);
 /* The four empty-casing programs contain only WAIT/DRAW/END commands:
  * no RNG, sound, attack, child spawn or completion signal. Keep one of
  * four deterministic phases before allocation; bullets are unaffected. */
 if(kind<=WEFFECT_ROCKET_CASING_UP&&((w->frame_phase+parent.index)&3))return 1;
 struct NativeEntity*e=allocate(w,parent,ENTITY_EFFECT,ENTITY_FIRST_CHILD,effect_tick);
 if(!e)return 0;
 e->cosmetic=kind<=WEFFECT_SCENE3_DEBRIS||kind==WEFFECT_BLOOD;
 e->payload.weapon.transform=*t;e->payload.weapon.kind=(uint8_t)kind;return 1;
}
/* $038BE4. Constructor runs at the child's scheduler position, after the
 * parent has changed its inventory, so copy only immutable ownership here. */
int weapon_drop_player_weapon(struct WeaponWorld*w,struct EntityRef parent,const struct PlayerBodyState*p){
 struct NativeEntity*e=allocate(w,parent,ENTITY_EFFECT,ENTITY_FIRST_CHILD,dropped_tick);
 if(!e)return 0;
 struct WeaponActor*a=&e->payload.weapon;a->transform=p->actor.transform;
 transform_integer_copy(&a->transform,&p->actor.transform);
 a->owner=p->character;a->damage_owner=p->actor.damage_owner;return 1;
}
int weapon_spawn_player_effect(struct WeaponWorld*w,struct EntityRef parent,enum PlayerVisualEffect kind){
 struct NativeEntity*owner=entity_get(w->entities,parent);
 if((unsigned)kind>=PVIS_COUNT||!owner||owner->kind!=ENTITY_PLAYER)return fail(w,20);
 struct PlayerBodyState*p=&owner->payload.player;
 if(kind==PVIS_BLOOD)return weapon_spawn_effect(w,parent,WEFFECT_BLOOD,&p->actor.transform);
 struct NativeEntity*e=allocate(w,parent,ENTITY_EFFECT,ENTITY_FIRST_CHILD,player_effect_tick);if(!e)return 0;
 e->cosmetic=kind==PVIS_DEATH_SMOKE||kind==PVIS_DEATH_SMOKE_HIGH||kind==PVIS_SPLASH||kind==PVIS_RIPPLE;
 struct WeaponActor*a=&e->payload.weapon;a->kind=(uint8_t)kind;a->owner=p->character;a->damage_owner=p->actor.damage_owner;
 a->transform=p->actor.transform;transform_integer_copy(&a->transform,&p->actor.transform);
 if(kind==PVIS_DEATH_SMOKE_HIGH){a->transform.priority=0xd000;transform_integer_offset(&a->transform,0,16);}
 return 1;
}
enum PlayerEffectPhase {PF_NEW,PF_DRAW,PF_SPLASH,PF_SPLASH_END,PF_ICON,PF_DELETE,
 PF_CHUTE_OPEN,PF_CHUTE_SWING_ENTER,PF_CHUTE_SWING,PF_CHUTE_RELEASE_ENTER,PF_CHUTE_RELEASE,PF_CHUTE_FALL_ENTER,PF_CHUTE_FALL};
static unsigned chute_index(const struct PlayerBodyState*p){
 /* $038E9A uses word arithmetic; preserve its unsigned shift and mask. */
 uint16_t d1=(uint16_t)(p->vx+0x400+0x40),d0=(uint16_t)(0x800+0x40-d1);
 unsigned index=d0>>8;return index<10?index:index&7;
}
static void player_effect_tick(struct EntityPool*pool,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;struct WeaponActor*a=&e->payload.weapon;if(w->fault)return;
 struct NativeEntity*parent=entity_get(pool,e->parent);
 if(!parent||parent->deleting){retire(w,e,0);return;}
 if(parent->kind!=ENTITY_PLAYER){fail(w,20);return;}struct PlayerBodyState*p=&parent->payload.player;
 if(a->state==PF_DELETE){retire(w,e,0);return;}
 if(a->state==PF_NEW){
  switch(a->kind){
   case PVIS_DEATH_SMOKE:case PVIS_DEATH_SMOKE_HIGH:
    a->palette=12;start(a,WANI_SMALL_SMOKE);a->state=PF_DRAW;
    if(outside(w,a,-16,16,0,80)){retire(w,e,0);return;}break;
   case PVIS_DEATH_ALT:a->palette=0x1df;start(a,WANI_PLAYER_DEATH_ALT);a->state=PF_DRAW;break;
   case PVIS_RIPPLE:a->palette=0x185;a->attached_offset=0x1c;start(a,WANI_PLAYER_RIPPLE);a->state=PF_DRAW;break;
   case PVIS_SPLASH:a->palette=0x185;a->attached_offset=0;start(a,WANI_PLAYER_SPLASH);a->state=PF_SPLASH;break;
   case PVIS_ICON:
    a->palette=(uint16_t)(p->character?0xa3:0xa2);a->attached_offset=(int16_t)(p->character?0x38:0x30);
    start(a,p->character?WANI_PLAYER_ICON2:WANI_PLAYER_ICON1);a->display_flags|=64;a->state=PF_ICON;break;
   case PVIS_PARACHUTE:a->palette=0x18d;a->state=PF_CHUTE_OPEN;break;
   default:fail(w,20);return;
  }
 }
 if(a->kind==PVIS_DEATH_SMOKE||a->kind==PVIS_DEATH_SMOKE_HIGH){
  /* $044048: fixed world position already accounts for camera scrolling. */
  if(animate(w,e)||outside(w,a,-16,16,0,80))retire(w,e,0);return;
 }
 if(a->kind!=PVIS_PARACHUTE){
  transform_integer_copy(&a->transform,&p->actor.transform);
  a->transform.priority=p->actor.transform.priority;
  if(a->state==PF_ICON){
   /* Player labels face the viewer, regardless of the parent's facing at spawn. */
   a->transform.facing=0;
   transform_integer_offset(&a->transform,0,a->attached_offset);
   if(animate(w,e)||(p->actor.hit.flags&1))a->state=PF_DELETE;return;
  }
  if(a->state==PF_SPLASH_END){start(a,WANI_PLAYER_SPLASH_END);a->attached_offset=0;a->state=PF_DRAW;}
  a->transform.priority=(uint16_t)((a->transform.priority&0xffe3)|(uint16_t)a->attached_offset);
  int complete=animate(w,e);
  if(a->state==PF_SPLASH){if(complete||!(p->status_flags&2))a->state=PF_SPLASH_END;}
  else if(complete)retire(w,e,0);return;
 }
 unsigned phase=a->state;
 if(phase==PF_CHUTE_OPEN||phase==PF_CHUTE_SWING_ENTER){
  unsigned index=chute_index(p);
  /* Open has ten records, swing has nine. Do not read the following stream
   * as a pointer when malformed scene input supplies an invalid velocity. */
  if(index>=(phase==PF_CHUTE_OPEN?10u:9u)){fail(w,21);return;}
  uint16_t entry=phase==PF_CHUTE_OPEN?weapon_parachute_open_entries[index]:weapon_parachute_swing_entries[index];
  if(entry!=65535){animation_begin(&a->animation,entry);a->movement_flags|=32;}
  if(phase==PF_CHUTE_SWING_ENTER)a->state=PF_CHUTE_SWING;
 }else if(phase==PF_CHUTE_RELEASE_ENTER){
  a->transform.facing=p->actor.transform.facing^1;start(a,WANI_PARACHUTE_RELEASE);a->state=PF_CHUTE_RELEASE;
 }else if(phase==PF_CHUTE_FALL_ENTER){
  a->transform.facing=p->actor.transform.facing;a->parachute_slow=0;start(a,WANI_PARACHUTE_FALL);a->state=PF_CHUTE_FALL;
 }
 phase=a->state;
 if(phase==PF_CHUTE_FALL){
  struct WeaponContact contact;if(move(w,a,WMOVE_SYNC,&contact)<0)return;
  if(!a->parachute_slow||!(w->ticks&1)){if(animate(w,e))retire(w,e,0);}return;
 }
 transform_integer_copy(&a->transform,&p->actor.transform);a->transform.priority=p->actor.transform.priority;
 if(phase==PF_CHUTE_OPEN)a->transform.priority-=0x20;
 else if(phase==PF_CHUTE_SWING)a->transform.priority--;
 int complete=animate(w,e);
 if(phase==PF_CHUTE_RELEASE){if(complete)retire(w,e,0);return;}
 if(phase==PF_CHUTE_OPEN){
  if(a->hit.flags&1)a->state=PF_CHUTE_FALL_ENTER;
  if(p->actor.action_flags&4)a->state=PF_CHUTE_SWING_ENTER;
  if(p->action!=4)a->state=PF_CHUTE_RELEASE_ENTER;
 }else if(phase==PF_CHUTE_SWING){
  if(complete)a->state=PF_CHUTE_OPEN;
  if(a->hit.flags&1)a->state=PF_CHUTE_FALL_ENTER;
  /* $038DDA really tests ==4 before publishing release. Its subsequent
   * ==$26 test cannot succeed on that path. Preserve the source branch. */
  if(p->action==4)a->state=PF_CHUTE_RELEASE_ENTER;
 }else {fail(w,20);return;}
 if(p->actor.hit.flags&1)a->state=PF_CHUTE_FALL_ENTER;
}
static int drop_impact(struct WeaponWorld*w,struct NativeEntity*e){
 /* $077D88/$077E10. This child is appended, consumes one RNG word,
  * and uses its original palette/animation/bounds, not a bullet impact. */
 struct NativeEntity*child=allocate(w,entity_ref(e),ENTITY_EFFECT,ENTITY_LAST_CHILD,dropped_tick);
 if(!child)return 0;
 struct WeaponActor*a=&child->payload.weapon;a->transform=e->payload.weapon.transform;
 transform_integer_copy(&a->transform,&e->payload.weapon.transform);a->program_kind=LIFE_DROP_IMPACT;return 1;
}
static void dropped_tick(struct EntityPool*pool,struct NativeEntity*e,void*world){
 (void)pool;struct WeaponWorld*w=world;struct WeaponActor*a=&e->payload.weapon;
 if(w->fault)return;
 if(a->state==LIFE_NEW){
  if(a->program_kind==LIFE_DROP_IMPACT){
   uint16_t random;if(!random_word(w,&random))return;
   a->transform.facing|=(uint8_t)(random&1);a->palette=4;start(a,WANI_DROP_IMPACT);a->state=LIFE_DROP_IMPACT;
   if(outside(w,a,-8,8,0,16)){retire(w,e,0);return;}
  }else{
   a->palette=(uint16_t)(a->owner?0x177:0x176);
   a->vx=(a->transform.facing&1)?0x110:-0x110;a->vy=0x50c;a->ay=-0x37;a->ax=0;
   a->transform.priority=(uint16_t)((a->transform.priority&0xffe3)|0x18);
   start(a,WANI_DROPPED_WEAPON);a->state=LIFE_DROP;
  }
 }
 if(a->state==LIFE_DELETE){retire(w,e,0);return;}
 if(a->state==LIFE_DROP_IMPACT){
  /* $044048's camera adjustment is already represented by world coords. */
  if(animate(w,e)||outside(w,a,-8,8,0,16))retire(w,e,0);return;
 }
 if(a->state==LIFE_DROP_BOUNCE_INIT){
  if(!drop_impact(w,e))return;
  a->vx=(a->transform.facing&1)?0x99:-0x99;a->vy=0x3fc;a->ay=-0x44;a->ax=0;
  a->hit.invulnerable=a->animation.freeze=0x28;a->state=LIFE_DROP_BOUNCE;
 }
 int bouncing=a->state==LIFE_DROP_BOUNCE;
 if(bouncing){a->display_flags^=8;a->animation.delay&=254;}
 struct WeaponContact hit;int landed=move(w,a,WMOVE_BODY,&hit);if(landed<0)return;
 if(landed)a->state=bouncing?LIFE_DELETE:LIFE_DROP_BOUNCE_INIT;
 (void)animate(w,e);
 if(outside(w,a,0,1,0,1))a->state=LIFE_DELETE;
}
static int spawn_auxiliary(struct WeaponWorld*w,struct NativeEntity*parent,unsigned life){
 struct NativeEntity*e=allocate(w,entity_ref(parent),ENTITY_EFFECT,ENTITY_FIRST_CHILD,auxiliary_tick);
 if(!e)return 0;e->payload.weapon.program_kind=(uint8_t)life;return 1;
}
int weapon_spawn_vehicle_surface(struct WeaponWorld*w,struct EntityRef parent){
 struct NativeEntity*e=allocate(w,parent,ENTITY_EFFECT,ENTITY_FIRST_CHILD,auxiliary_tick);
 if(!e)return 0;e->payload.weapon.program_kind=LIFE_SURFACE_FOLLOW;e->payload.weapon.variant=1;
 struct WeaponActor*owner=entity_weapon_actor(entity_get(w->entities,parent));
 if(!owner)return fail(w,12);e->payload.weapon.transform=owner->transform;e->payload.weapon.transform.display_group=0;
 copy_integer_position(&e->payload.weapon,&owner->transform);return 1;
}
int weapon_emit(struct WeaponWorld*w,struct EntityRef emitter,const struct PlayerEmission*request){
 enum WeaponEffect effect;
 switch(request->kind){
  case EMIT_ROCKET_CASING:effect=WEFFECT_ROCKET_CASING;break;
  case EMIT_ROCKET_CASING_UP:effect=WEFFECT_ROCKET_CASING_UP;break;
  case EMIT_SCENE3_DEBRIS:effect=WEFFECT_SCENE3_DEBRIS;break;
  case EMIT_BODY_FX:effect=WEFFECT_BODY;break;
  default:goto projectile;
 }
 return weapon_spawn_effect(w,emitter,effect,&request->transform);
projectile:
 if((unsigned)request->kind>EMIT_GRENADE_ARC_LOW)return fail(w,10);
 int arc=request->kind==EMIT_GRENADE_ARC_HIGH||request->kind==EMIT_GRENADE_ARC_LOW;
 struct EntityRef parent=(request->kind==EMIT_PISTOL||request->kind==EMIT_HMG||arc)?emitter:w->projectile_group;
 enum EntityInsert order=(request->kind==EMIT_SHOTGUN||request->kind==EMIT_FLAME||arc)?ENTITY_FIRST_CHILD:ENTITY_LAST_CHILD;
 struct NativeEntity*e=allocate(w,parent,ENTITY_PROJECTILE,order,projectile_tick);
 if(!e)return 0;struct WeaponActor*a=&e->payload.weapon;
 a->kind=(uint8_t)request->kind;a->transform=request->transform;
 if(arc)a->transform=(struct PlayerTransform){0};
 a->owner=request->owner;a->damage_owner=request->damage_owner;
 a->variant=request->direction;a->mirror=request->mirror;a->phase=request->phase;a->spread=request->spread;
 return 1;
}

int weapon_emit_slug_bullet(struct WeaponWorld*w,struct EntityRef emitter,const struct PlayerEmission*r){
 struct NativeEntity*e=allocate(w,emitter,ENTITY_PROJECTILE,ENTITY_LAST_CHILD,projectile_tick);
 if(!e)return 0;struct WeaponActor*a=&e->payload.weapon;
 a->kind=EMIT_PISTOL;a->transform=r->transform;transform_integer_copy(&a->transform,&r->transform);
 a->owner=r->owner;a->damage_owner=r->damage_owner;a->variant=r->direction;
 a->phase=r->phase;a->spread=r->spread;a->palette=0x1d6;return 1;
}
static int bullet_init(struct WeaponWorld*w,struct NativeEntity*e){
 struct WeaponActor*a=&e->payload.weapon;unsigned index=(a->variant&0xf8)>>4;
 /* Slug $0308B0 joins $0308D0 and uses the visible $29D4D2 tracer
  * table, like the HMG. The pistol table includes blank flight frames. */
 const uint16_t*entries=(a->kind==EMIT_HMG||a->palette==0x1d6)?weapon_hmg_entries:weapon_pistol_entries;
 if(entries[index]==65535)return fail(w,11);
 animation_begin(&a->animation,entries[index]);a->movement_flags|=32;
 if(a->palette!=0x1d6)a->palette=2;a->transform.priority=0xd000;a->hit.type_flags|=16;
 a->attack=&weapon_attack_sets[WATTACK_BULLET];hit_begin_attack(&a->hit);
 if(!spawn_auxiliary(w,e,LIFE_SURFACE_FOLLOW))return 0;
 uint8_t angle=weapon_bullet_angle(a->variant,a->spread,w->spread_mode,w->special_spread);
 struct NativeEntity*parent=entity_get(w->entities,e->parent);
 const struct PlayerTransform group_origin={0};
 const struct PlayerTransform*t=a->mirror==2?&group_origin:transform(parent);if(!t)return fail(w,12);
 if(a->phase&1){
  weapon_vector(angle,-0x800,&a->vx,&a->vy);
  if(t->facing&1)a->vx=word(-a->vx);
  struct WeaponContact hit;if(move(w,a,WMOVE_POINT,&hit)<0)return 0;
 }
 weapon_vector(angle,0x800,&a->vx,&a->vy);
 struct NativeEntity*grandparent=entity_get(w->entities,parent->parent);
 struct PlayerActionLayer*body=player_layer(grandparent);
 /* $030AF2: only the original player's type bit enables this mirroring. */
 if(body&&(body->hit.type_flags&64)){
  a->transform.facing=t->facing;if(t->facing&1)a->vx=word(-a->vx);
 }
 hit_begin_attack(&a->hit);a->state=LIFE_BULLET;return 1;
}
static void bullet_impact(struct WeaponWorld*w,struct NativeEntity*e,int air){
 struct NativeEntity*child=allocate(w,entity_ref(e),ENTITY_EFFECT,ENTITY_LAST_CHILD,auxiliary_tick);
 if(!child)return;struct WeaponActor*a=&child->payload.weapon,*parent=&e->payload.weapon;
 a->transform=parent->transform;copy_integer_position(a,&parent->transform);
 a->palette=parent->palette==0x1d6?0x1d6:air?2:4;
 a->program_kind=LIFE_DRAW;a->variant=(uint8_t)air;
 e->payload.weapon.state=LIFE_DELETE;
}
static void bullet_tick(struct WeaponWorld*w,struct NativeEntity*e){
 struct WeaponActor*a=&e->payload.weapon;struct WeaponContact hit;if(!scene_preserve_screen(w,a))return;
 int collision=move_twice(w,a,WMOVE_POINT,&hit);if(collision<0)return;
 if(collision){
  a->state=LIFE_BULLET_GROUND;
  if(hit.shape==0x38||((hit.raw&0xc0)!=0&&(hit.raw&0xc0)!=0xc0))a->state=LIFE_DELETE;
 }
 animate(w,e);if(!--a->ttl)a->state=LIFE_DELETE;
 if(!attack(w,a,0))return;
 if(a->hit.flags&2)a->state=LIFE_BULLET_AIR;
 if(outside(w,a,0,1,-48,1))a->state=LIFE_DELETE;
}
static void variant_flip(struct WeaponActor*a,int acceleration){
 if(a->variant==3){a->transform.facing|=2;if(acceleration)a->ay=word(-a->ay);else a->vy=word(-a->vy);}
 else if((a->variant^a->mirror)&2){a->transform.facing|=1;if(acceleration)a->ax=word(-a->ax);else a->vx=word(-a->vx);}
}
static void shotgun_init(struct WeaponWorld*w,struct WeaponActor*a){
 w->shotgun_flash=1;a->palette=0x19b;a->transform.priority=0xd010;
 start(a,(a->variant&1)?WANI_SHOTGUN_VERTICAL:WANI_SHOTGUN_HORIZONTAL);
 a->hit.impact_x=(a->variant&1)?-1:0;a->hit.impact_y=(a->variant&1)?0:-1;
 if(a->variant==3){a->transform.facing|=2;if(!a->hit.impact_y)a->hit.impact_y=1;}
 else if((a->variant^a->mirror)&2){a->transform.facing|=1;if(!a->hit.impact_x)a->hit.impact_x=1;}
 if(a->variant==1)transform_integer_offset(&a->transform,2,62);
 else if(a->variant!=3){
  int offset=(a->transform.facing&1)?-25:25;
  transform_integer_offset(&a->transform,offset,20);a->vx=(int16_t)offset;
 }
 a->state=LIFE_SHOTGUN;
}
static int flame_init(struct WeaponWorld*w,struct NativeEntity*e){
 struct WeaponActor*a=&e->payload.weapon;a->palette=0x184;a->transform.priority=0xd010;
 if(a->variant&1){start(a,WANI_FLAME_VERTICAL);a->vy=0x800;}
 else {start(a,WANI_FLAME_HORIZONTAL);a->vx=0x800;a->vy=-0x200;}
 variant_flip(a,0);a->attack=&weapon_attack_sets[WATTACK_FLAME];hit_begin_attack(&a->hit);
 a->state=LIFE_FLAME;return spawn_auxiliary(w,e,LIFE_FLAME_TRAIL);
}
static int rocket_init(struct WeaponWorld*w,struct NativeEntity*e){
 struct WeaponActor*a=&e->payload.weapon;a->palette=0x17c;a->transform.priority=0xd010;
 if(a->variant&1){start(a,WANI_ROCKET_VERTICAL);a->ay=32;a->attack=&weapon_attack_sets[WATTACK_ROCKET_VERTICAL];}
 else {start(a,a->phase?WANI_ROCKET_ALT:WANI_ROCKET_HORIZONTAL);a->ax=32;a->attack=&weapon_attack_sets[WATTACK_ROCKET_HORIZONTAL];}
 variant_flip(a,1);hit_begin_attack(&a->hit);a->timer=0;a->state=LIFE_ROCKET;
 return spawn_auxiliary(w,e,LIFE_ROCKET_SEEK);
}
static void grenade_blast(struct WeaponWorld*w,struct WeaponActor*a,int big){
 if(!noise(w,0x1027,0))return;
 a->palette=big?0x178:13;start(a,big?WANI_GRENADE_BIG_BLAST:WANI_GRENADE_SMOKE);
 a->attack=&weapon_attack_sets[big?WATTACK_GRENADE_BIG_BLAST:WATTACK_GRENADE_BLAST];
 hit_begin_attack(&a->hit);a->state=LIFE_GRENADE_BLAST_HIT;
}
static int grenade_init(struct WeaponWorld*w,struct NativeEntity*e){
 struct WeaponActor*a=&e->payload.weapon;int height=16;
 a->hit.type_flags|=16;a->transform.priority=0xd000;start(a,WANI_GRENADE_SPIN);
 a->attack=&weapon_attack_sets[WATTACK_GRENADE];hit_begin_attack(&a->hit);
 hit_begin_attack(&a->hit);a->palette=0x7c;
 switch(a->kind){
  case EMIT_GRENADE_HEAVY:a->vx=-0x51e;a->vy=0xccb;a->ay=-0x106;a->palette=0x14c;a->bounces=0;a->state=LIFE_GRENADE_HEAVY;break;
  case EMIT_GRENADE_DOWN:a->vx=-0x400;a->vy=-0x800;height=32;a->state=LIFE_GRENADE_DOWN;break;
  case EMIT_GRENADE_BOUNCE:a->vx=-0x155;a->vy=0x2a6;a->ay=-0x71;a->timer=18;a->state=LIFE_GRENADE_BOUNCE_TIMER;break;
  case EMIT_GRENADE_ARC_HIGH:a->vx=-0x9b2;a->vy=0xb9a;a->ay=-0xb4;height=32;a->state=LIFE_GRENADE_ARC;break;
  case EMIT_GRENADE_ARC_LOW:a->vx=-0x1000;a->vy=0x663;a->ay=-0x147;height=32;a->state=LIFE_GRENADE_ARC;break;
  default:return fail(w,13);
 }
 if(a->transform.facing&1)a->vx=word(-a->vx);
 int arc=a->kind==EMIT_GRENADE_ARC_HIGH||a->kind==EMIT_GRENADE_ARC_LOW;
 if(arc){
  struct NativeEntity*parent=entity_get(w->entities,e->parent);
  const struct PlayerTransform*t=transform(parent);if(!t)return fail(w,12);
  struct NativeEntity*body=parent->kind==ENTITY_PLAYER?parent:entity_get(w->entities,parent->parent);
  a->palette=body&&body->kind==ENTITY_PLAYER&&body->payload.player.character==0?0x7c:0x14c;
  a->transform.facing=t->facing;
  if(a->transform.facing&1)a->vx=word(-a->vx);
 }
 if(e->parent.index!=w->projectile_group.index){
  const struct PlayerTransform*t=transform(entity_get(w->entities,e->parent));if(!t)return fail(w,12);
  copy_integer_position(a,t);a->origin_y_up=word(transform_y_up(&a->transform)+height);
 }
 transform_integer_offset(&a->transform,0,height);return 1;
}
static void grenade_tick(struct WeaponWorld*w,struct NativeEntity*e){
 struct WeaponActor*a=&e->payload.weapon;struct WeaponContact hit;
 int heavy=a->state==LIFE_GRENADE_HEAVY,body=a->state==LIFE_GRENADE_BOUNCE_BODY;
 if(a->state!=LIFE_GRENADE_DOWN&&a->vy<(heavy?-256:0))a->vx=word(a->vx-(a->vx>>4));
 enum WeaponMove policy=(body||(heavy&&w->grenade_body_collision))?WMOVE_BODY:WMOVE_FALL;
 int collision=move(w,a,policy,&hit);if(collision<0)return;
 if(collision){
  if(heavy){
   if(a->bounces>=1){grenade_blast(w,a,1);return;}
   a->vx=(a->transform.facing&1)?0x320:-0x320;a->vy=0x3c0;a->ay=-0x78;a->ax=0;
   transform_integer_offset(&a->transform,0,8);a->bounces++;
  }else if(body){
   a->vx=(a->transform.facing&1)?0xd5:-0xd5;a->vy=0x4fe;a->ay=-0xd5;a->ax=0;
   a->timer=18;a->state=LIFE_GRENADE_BOUNCE_TIMER;
  }else{grenade_blast(w,a,0);return;}
 }
 if(!body&&a->state==LIFE_GRENADE_BOUNCE_TIMER&&!--a->timer)a->state=LIFE_GRENADE_BOUNCE_BODY;
 animate(w,e);
 if(!heavy||a->animation.pose>=1){
  if(!attack(w,a,0))return;
  if(a->hit.flags&2){grenade_blast(w,a,heavy);return;}
 }
 if(a->hit.flags&8){grenade_blast(w,a,0);return;}
 if(grenade_outside(w,a)){retire(w,e,0);return;}
 if(heavy)count(w,a,1);
}

static void projectile_tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 (void)p;struct WeaponWorld*w=world;struct WeaponActor*a=&e->payload.weapon;
 if(w->fault)return;
 if(a->state==LIFE_NEW){
  switch(a->kind){
   case EMIT_PISTOL:case EMIT_HMG:if(!bullet_init(w,e))return;break;
   case EMIT_SHOTGUN:shotgun_init(w,a);break;
   case EMIT_FLAME:if(!flame_init(w,e))return;break;
   case EMIT_ROCKET:if(!rocket_init(w,e))return;break;
   default:if(!grenade_init(w,e))return;break;
  }
 }
 if(a->state==LIFE_FLAME_IMPACT_INIT){
  if(a->variant&1){a->vy=0;start(a,WANI_FLAME_IMPACT_VERTICAL);}
  else {a->vx=0;start(a,WANI_FLAME_IMPACT_HORIZONTAL);}a->state=LIFE_FLAME_IMPACT;
 }else if(a->state==LIFE_ROCKET_AIR_INIT){
  if(!noise(w,0x1025,0))return;a->palette=4;
  start(a,(a->variant&1)?WANI_ROCKET_IMPACT_VERTICAL:WANI_ROCKET_IMPACT_HORIZONTAL);a->state=LIFE_ROCKET_AIR;
 }else if(a->state==LIFE_ROCKET_GROUND){
  uint16_t random;if(!random_word(w,&random))return;
  a->transform.facing|=(uint8_t)(random&1);a->palette=4;start(a,WANI_BLAST_A);
  if(outside(w,a,-16,16,-8,24)){retire(w,e,0);return;}a->state=LIFE_EXPLOSION;
 }
 struct WeaponContact hit;int collision;
 switch(a->state){
  case LIFE_BULLET:bullet_tick(w,e);break;
  case LIFE_BULLET_GROUND:bullet_impact(w,e,0);break;
  case LIFE_BULLET_AIR:bullet_impact(w,e,1);break;
  case LIFE_DELETE:retire(w,e,0);break;
  case LIFE_SHOTGUN:
   if(animate(w,e)){retire(w,e,0);return;}
   if(!attack(w,a,0))return;if((a->hit.events&2)&&!noise(w,0x1086,0))return;count(w,a,0);break;
  case LIFE_FLAME:
   weapon_flame_velocity(a,a->variant);hit_begin_attack(&a->hit);
   collision=move_twice(w,a,WMOVE_POINT,&hit);if(collision<0)return;
   if(collision)a->state=LIFE_FLAME_IMPACT_INIT;
   if(animate(w,e)){retire(w,e,1);return;}
   if(!attack(w,a,0))return;if(a->hit.flags&2)a->state=LIFE_FLAME_IMPACT_INIT;bound_count(w,e,1);break;
  case LIFE_FLAME_IMPACT:
   if(move(w,a,WMOVE_POINT,&hit)<0)return;
   if(animate(w,e)){retire(w,e,1);return;}bound_count(w,e,1);break;
  case LIFE_ROCKET:
   collision=move_twice(w,a,WMOVE_FALL,&hit);if(collision<0)return;
   if(collision)a->state=LIFE_ROCKET_GROUND;
   if(!a->timer--){a->timer=2;if(!spawn_auxiliary(w,e,LIFE_ROCKET_TRAIL))return;}
   animate(w,e);if(!attack(w,a,0))return;
   if(a->hit.flags&2)a->state=LIFE_ROCKET_AIR_INIT;bound_count(w,e,0);break;
  case LIFE_ROCKET_AIR:
   hit_begin_attack(&a->hit);if(animate(w,e)){retire(w,e,0);return;}bound_count(w,e,0);break;
  case LIFE_GRENADE_HEAVY:case LIFE_GRENADE_DOWN:case LIFE_GRENADE_ARC:
  case LIFE_GRENADE_BOUNCE_TIMER:case LIFE_GRENADE_BOUNCE_BODY:grenade_tick(w,e);break;
  case LIFE_GRENADE_BLAST_HIT:
   hit_begin_attack(&a->hit);if(!attack(w,a,0))return;a->state=LIFE_GRENADE_BLAST_DRAW;animate(w,e);break;
  case LIFE_GRENADE_BLAST_DRAW:
   a->attack=0;hit_begin_attack(&a->hit);if(animate(w,e))retire(w,e,0);break;
  case LIFE_EXPLOSION:
   if(animate(w,e)||outside(w,a,-16,16,-8,24))retire(w,e,0);break;
  default:fail(w,14);break;
 }
}

static void effect_tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 (void)p;struct WeaponWorld*w=world;struct WeaponActor*a=&e->payload.weapon;
 if(w->fault)return;
 if(a->state==LIFE_NEW){
  struct PlayerTransform t=a->transform;enum WeaponEffect kind=(enum WeaponEffect)a->kind;
  struct WeaponAnimationContext c={.actor=a};
  if(!weapon_effect_init(&c,kind,&t)){fail(w,15);return;}a->hit.id=e->index;a->hit.flags|=64;a->state=LIFE_DRAW;
 }
 if(animate(w,e)){retire(w,e,0);return;}
 if(a->kind>=WEFFECT_BLAST_A&&outside(w,a,0,1,0,1))retire(w,e,0);
}
static int surface_follow(struct WeaponWorld*w,struct NativeEntity*e,struct NativeEntity*parent){
 struct WeaponActor*a=&e->payload.weapon,*owner=entity_weapon_actor(parent);
 if(!owner)return fail(w,12);
 copy_integer_position(a,&owner->transform);
 struct WeaponContact hit={0};
 if(!w->probe||!w->probe(w->scene,owner,&hit))return fail(w,16);
 if(hit.solid)a->raw_surface=hit.raw;a->surface_kind=hit.raw&0xc0;
 if(a->surface_kind==0x40||a->surface_kind==0x80){
  if(a->surface_kind==0x40){
   /* $077C28/$0440D0: source fixed water surface in scenes 0/9. */
   int world_y=(w->scene_number==6||w->scene_number==9)?0x1ef:0x167;
   a->transform.y=world_y*256;
  }
  a->state=LIFE_SURFACE_START;
  if(a->surface_kind==0x80)a->display_flags&=(uint8_t)~64;else a->display_flags|=64;
 }else if(hit.solid)a->state=LIFE_DELETE;
 if(outside(w,a,0,1,0,1)||parent->deleting)a->state=LIFE_DELETE;
 return 1;
}
static void auxiliary_tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;struct WeaponActor*a=&e->payload.weapon;
 if(w->fault)return;
 struct NativeEntity*parent=entity_get(p,e->parent);
 if(a->state==LIFE_NEW){
  if(!parent){fail(w,12);return;}
  struct WeaponActor*owner=entity_weapon_actor(parent);if(!owner){fail(w,12);return;}
  switch(a->program_kind){
   case LIFE_SURFACE_FOLLOW:
    if(w->scene_number!=0&&w->scene_number!=9){retire(w,e,0);return;}
    a->palette=0x10;a->transform.priority=0xe000;
    a->state=LIFE_SURFACE_FOLLOW;break;
   case LIFE_FLAME_TRAIL:
    a->palette=0x184;copy_integer_position(a,&owner->transform);a->state=LIFE_FLAME_TRAIL;break;
   case LIFE_ROCKET_SEEK:
    a->variant=owner->variant;
    a->attack=&weapon_attack_sets[(a->variant&1)?WATTACK_ROCKET_SEEK_VERTICAL:WATTACK_ROCKET_SEEK_HORIZONTAL];
    hit_begin_attack(&a->hit);a->state=LIFE_ROCKET_SEEK;break;
   case LIFE_ROCKET_TRAIL:weapon_rocket_trail_init(a,owner);a->state=LIFE_ROCKET_TRAIL;break;
   case LIFE_DRAW:{
    uint16_t random;if(!random_word(w,&random))return;
    int dx=(random&(a->variant?31:15))-8;a->transform.x=whole(a->transform.x,dx);
    if(a->variant){
     if(!random_word(w,&random))return;transform_integer_offset(&a->transform,0,(random&31)-8);start(a,WANI_AIR_IMPACT);
    }else {animation_begin(&a->animation,weapon_ground_impact_entries[(unsigned)dx&1]);a->movement_flags|=32;}
    a->state=LIFE_DRAW;break;
   }
   default:fail(w,17);return;
  }
 }
 if(a->state==LIFE_SURFACE_START){
  uint16_t random;
  if(!a->variant&&a->surface_kind!=0x80){if(!random_word(w,&random))return;a->transform.x=whole(a->transform.x,(random&15)-8);}
  unsigned which=(a->raw_surface==0x82||a->raw_surface==0x8c)?1:0;
  animation_begin(&a->animation,(a->variant?weapon_surface_vehicle_entries:weapon_surface_bullet_entries)[which]);a->movement_flags|=32;
  a->palette=w->scene_number==5?0x138:0x10;a->transform.priority++;a->state=LIFE_SURFACE_DRAW;
 }
 switch(a->state){
  case LIFE_DELETE:retire(w,e,0);break;
  case LIFE_SURFACE_FOLLOW:
   if(!parent){retire(w,e,0);return;}surface_follow(w,e,parent);break;
  case LIFE_SURFACE_DRAW:
   if(animate(w,e)||outside(w,a,0,1,0,1))a->state=LIFE_DELETE;break;
  case LIFE_DRAW:if(animate(w,e))retire(w,e,1);break;
  case LIFE_FLAME_TRAIL:
   if(!parent){fail(w,12);return;}weapon_trail_follow(a,&parent->payload.weapon,0);break;
  case LIFE_ROCKET_TRAIL:
   weapon_integrate_free(a);a->vx=word(a->vx-(a->vx>>4));a->vy=word(a->vy-(a->vy>>4));
   if(animate(w,e))retire(w,e,0);break;
  case LIFE_ROCKET_SEEK:{
   if(!parent||parent->deleting){retire(w,e,0);return;}
   struct WeaponActor*owner=&parent->payload.weapon;copy_integer_position(a,&owner->transform);
   a->transform.facing=owner->transform.facing;hit_begin_attack(&a->hit);
   struct HitActor*selected=0;if(!attack(w,a,&selected))return;
   if(a->hit.flags&2){if(!selected){fail(w,18);return;}weapon_rocket_seek(owner,selected,a->variant&1);}break;
  }
  default:fail(w,19);break;
 }
}

int weapon_emit_gun_bullet_at(struct WeaponWorld*w,struct EntityRef parent,const struct PlayerTransform*t,uint8_t angle){
 struct NativeEntity*e=allocate(w,parent,ENTITY_PROJECTILE,ENTITY_FIRST_CHILD,projectile_tick);if(!e)return 0;
 struct WeaponActor*a=&e->payload.weapon;a->kind=EMIT_PISTOL;transform_integer_copy(&a->transform,t);
 a->variant=angle;a->phase=255;a->spread=-1;a->mirror=2;return 1;
}

int weapon_emit_gun_bullet(struct WeaponWorld*w,const struct PlayerTransform*t,uint8_t angle){return weapon_emit_gun_bullet_at(w,w->projectile_group,t,angle);}
