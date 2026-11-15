#include "tank_internal.h"
#include "../generated/ring-gun-programs.h"
/* 171 automatic gun and193 steerable gun keep their distinct key and
 * ownership protocols. In193 the claim branch explicitly writes owner0. */
enum State {G171_NEW,G171_IDLE,G171_FIRE,G171_WAIT,G171_OVERLAY,G171_BULLET,
 G193_NEW,G193_IDLE,G193_CLAIM,G193_ACTIVE,G193_FIRE,G193_RELOAD,G193_OVERLAY,G193_SHELL};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static void start_entry(struct EnemyActor*q,unsigned n){animation_begin(&q->actor.animation,(uint16_t)n);q->actor.movement_flags|=32;}
static void start(struct EnemyActor*q,unsigned n){start_entry(q,ring_gun_entries[n]);}
static struct NativeEntity*allocate(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,unsigned s){struct NativeEntity*e=enemy_allocate(w,p,o,t,tick);if(e){e->payload.enemy.ring_gun.overlay=65535;e->payload.enemy.ring_gun.owner=255;enemy_schedule(&e->payload.enemy,s);}return e;}
static void attack_binding(struct EnemyActor*q,unsigned n){q->attack_set=(struct HitAttackSet){ring_gun_bindings[n].attack,ring_gun_bindings[n].count};q->actor.attack=&q->attack_set;hit_begin_attack(&q->actor.hit);}
static struct PlayerTransform pending(struct WeaponWorld*w,struct WeaponActor*a){struct PlayerTransform t=a->transform;if(!(a->hit.flags&64)){if(!w->interactions||!w->interactions->terrain){enemy_fault(w);return t;}transform_integer_offset(&t,w->interactions->terrain->delta_x,-w->interactions->terrain->delta_y);}return t;}
struct Context{struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read(void*ctx,unsigned f,unsigned width){struct Context*c=ctx;if(f>RGF_OVERLAY193||width!=4){enemy_fault(c->w);return 0;}return c->e->payload.enemy.ring_gun.overlay;}
static int field(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){struct Context*c=ctx;if(f>RGF_OVERLAY193||width!=4||op!=AN_FIELD_SET||v>65535)return enemy_fault(c->w);c->e->payload.enemy.ring_gun.overlay=(uint16_t)v;return 1;}
static int call(void*ctx,unsigned n,struct Animation*anim){(void)anim;struct Context*c=ctx;if(n)return enemy_fault(c->w);return allocate(c->w,c->w->projectile_group,ENTITY_FIRST_CHILD,&c->e->payload.enemy.actor.transform,G171_BULLET)!=0;}
static int sound(void*ctx,uint16_t n,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,n,stop)?1:enemy_fault(c->w);}
static int animate(struct WeaponWorld*w,struct NativeEntity*e){struct Context c={w,e};struct AnimationOwner o={.context=&c,.read=read,.write=field,.call=call,.sound=sound};int r=animation_step(&e->payload.enemy.actor.animation,&ring_gun_program,&o);if(e->payload.enemy.actor.animation.fault)enemy_fault(w);return r;}
static int sync(struct WeaponWorld*w,struct WeaponActor*a){return interaction_move(w,a,WMOVE_SYNC)>=0;}
static void bounds(struct WeaponWorld*w,struct NativeEntity*e,unsigned n){if(interaction_outside(w,&e->payload.enemy.actor,ring_gun_bounds[n],0)){entity_delete_children(w->entities,e);entity_delete(w->entities,e);}}
static int registered(struct WeaponWorld*w,struct EnemyActor*q,int steerable){if(!w->interactions->targets){enemy_fault(w);return -1;}struct WeaponActor*a=&q->actor;int8_t owner=-1;int16_t x=steerable?tk_word(q->ring_gun.anchor_x+w->camera_x):tk_word(a->transform.x>>8);target_ring_register(w->interactions->targets,tk_sx(w,a),x,transform_y_up(&a->transform),q->ring_gun.key,&owner);return owner;}
static uint16_t palette(unsigned owner){return owner==0?0x134:owner==1?0x152:14;}
static int select_pose(struct WeaponWorld*w,struct EnemyActor*q,const uint16_t*table,int before_sync){
 unsigned nibble=(uint16_t)q->ring_gun.angle>>12,i=nibble&7;if(i>=5)i=8-i;struct WeaponActor*a=&q->actor;
 if(((nibble+3)&15)<7)a->transform.facing|=1;else a->transform.facing&=254;q->ring_gun.pose=(uint8_t)i;start_entry(q,table[i]);
 struct WeaponActor query={.transform=before_sync?pending(w,a):a->transform};int x=tk_sx(w,&query),y=tk_sy(w,&query),direction=a->transform.facing&1?-1:1;
 q->ring_gun.anchor_x=tk_word(x+direction*ring_gun_anchor[i]);q->ring_gun.muzzle_x=tk_word(x+direction*ring_gun_muzzle[i][0]);q->ring_gun.muzzle_y=tk_word(y+ring_gun_muzzle[i][1]);return !w->fault;
}
static int aim(struct WeaponWorld*w,struct EnemyActor*q){struct WeaponActor query={.transform=pending(w,&q->actor)};int n=enemy_nearest_x(w,&query,0);if(n<0)return 0;return enemy_angle(tk_word((w->players[n]->actor.transform.x>>8)-(query.transform.x>>8)),tk_word(transform_y_up(&w->players[n]->actor.transform)-transform_y_up(&query.transform)));}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned s=a->state;
 switch(s){
 case G171_NEW:case G193_NEW:
  if(!w->interactions->targets)return enemy_fault(w);
  if(s==G171_NEW)interaction_priority(w,a,0x8000,4);else a->transform.priority=RING_GUN193_PRIORITY;
  q->ring_gun.key=target_ring_new_key(w->interactions->targets);if(s==G193_NEW){q->ring_gun.key=(uint8_t)~q->ring_gun.key;q->ring_gun.angle=(int16_t)0x8000;}
  if(!allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,s==G171_NEW?G171_OVERLAY:G193_OVERLAY))return 0;enemy_schedule(q,s==G171_NEW?G171_IDLE:G193_IDLE);return 0;
 case G171_IDLE:start(q,RGA_IDLE);break;
 case G171_FIRE:start(q,RGA_FIRE);q->ring_gun.clock=3;break;
 case G171_WAIT:start(q,RGA_WAIT);q->ring_gun.clock=120;break;
 case G171_OVERLAY:case G193_OVERLAY:a->palette=0x56;if(s==G171_OVERLAY)a->transform.facing=0;return 0;
 case G171_BULLET:if(!tk_sound(w,0x108c))return 0;a->palette=0x57;a->transform.facing=0;a->transform.priority=0xd000;transform_integer_offset(&a->transform,32,20);a->vx=1024;start(q,RGA_BULLET);attack_binding(q,RGB_BULLET);hit_begin_attack(&a->hit);break;
 case G193_IDLE:break;
 case G193_CLAIM:a->palette=palette(q->ring_gun.owner);/* FALLTHROUGH */
 case G193_RELOAD:q->ring_gun.clock=120;a->state=G193_ACTIVE;break;
 case G193_FIRE:if(!select_pose(w,q,ring_gun_fire,1))return 0;break;
 case G193_SHELL:
  if(q->params[0]==2){a->defense=ring_gun_bindings[RGB_SHELL_DEFENSE].defense;a->defense_count=ring_gun_bindings[RGB_SHELL_DEFENSE].count;attack_binding(q,RGB_ENEMY_SHELL);q->scale=512;}
  else{a->defense=0;a->defense_count=0;attack_binding(q,RGB_PLAYER_SHELL);q->scale=1024;}
  if(!tk_sound(w,0x1064))return 0;a->palette=0x57;a->hit.type_flags|=16;a->transform.priority=0xd000;weapon_vector((uint8_t)(((uint16_t)q->ring_gun.angle>>8)&240),(int16_t)q->scale,&a->vx,&a->vy);
  if(q->ring_gun.pose>=5)return enemy_fault(w);start_entry(q,ring_gun_shell[q->ring_gun.pose]);break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(q->entry){q->entry=0;if(!enter(w,e))return;}unsigned s=a->state;
 if(s==G171_OVERLAY||s==G193_OVERLAY){struct EnemyActor*parent=tk_parent(w,e);if(!parent)return;transform_integer_offset(&a->transform,tk_word((parent->actor.transform.x>>8)-(a->transform.x>>8)),tk_word(transform_y_up(&parent->actor.transform)-transform_y_up(&a->transform)));a->transform.priority=parent->actor.transform.priority;if(s==G193_OVERLAY)a->transform.facing=parent->actor.transform.facing;animation_drawing(&a->animation,&ring_gun_program,parent->ring_gun.overlay);if(!animation_output_direct(&a->animation,AN_OUTPUT_DIRECT))enemy_fault(w);return;}
 if(s==G171_IDLE||s==G171_FIRE||s==G171_WAIT){
  if(!sync(w,a))return;int done=animate(w,e);
  if(s==G171_FIRE&&done){q->ring_gun.clock=(uint8_t)(q->ring_gun.clock-1);if(q->ring_gun.clock)start(q,RGA_FIRE);else enemy_schedule(q,G171_WAIT);}
  if(s==G171_WAIT){q->ring_gun.clock=(uint8_t)(q->ring_gun.clock-1);if(!q->ring_gun.clock)enemy_schedule(q,G171_FIRE);}
  if(tk_sx(w,a)>32){int owner=registered(w,q,0);if(s==G171_IDLE&&owner>=0){a->palette=palette((unsigned)owner);enemy_schedule(q,G171_FIRE);}else if(s!=G171_IDLE&&owner<0)enemy_schedule(q,G171_IDLE);}else if(s!=G171_IDLE)enemy_schedule(q,G171_IDLE);
  bounds(w,e,0);return;
 }
 if(s==G171_BULLET){if(interaction_move(w,a,WMOVE_POINT)<0)return;if((uint16_t)tk_word(tk_sx(w,a)+16)>=352){entity_delete(p,e);return;}animate(w,e);if(!enemy_attack(w,a))return;if(a->hit.flags&2)entity_delete(p,e);return;}
 if(s==G193_IDLE){if(!select_pose(w,q,ring_gun_idle,1)||!sync(w,a))return;animate(w,e);if(registered(w,q,1)>=0){q->ring_gun.owner=0;enemy_schedule(q,G193_CLAIM);}bounds(w,e,1);return;}
 if(s==G193_ACTIVE){
  if(q->ring_gun.owner==2){int16_t goal=tk_word(aim(w,q)<<8),delta=tk_word(goal-q->ring_gun.angle);q->ring_gun.angle=tk_word(q->ring_gun.angle+(delta>>4));if(enemy_player_live(w->players[0])||enemy_player_live(w->players[1])){q->ring_gun.clock--;if(!q->ring_gun.clock)enemy_schedule(q,G193_FIRE);}}
  else{const struct PlayerSourceInput*input=&w->inputs[q->ring_gun.owner?1:0];uint16_t goal=ring_gun_input_angles[input->held&15];if(goal!=65535){int16_t delta=tk_word(goal-q->ring_gun.angle);q->ring_gun.angle=tk_word(q->ring_gun.angle+(delta>>4));}if(input->actions&16)enemy_schedule(q,G193_FIRE);}
  if(!sync(w,a)||!select_pose(w,q,ring_gun_active,0))return;animate(w,e);
 }else if(s==G193_FIRE){if(!sync(w,a))return;if(animate(w,e)){
   struct PlayerTransform t=a->transform;transform_integer_offset(&t,tk_word(q->ring_gun.muzzle_x-tk_sx(w,a)),tk_word(q->ring_gun.muzzle_y-tk_sy(w,a)));
   struct NativeEntity*n=allocate(w,w->projectile_group,ENTITY_FIRST_CHILD,&t,G193_SHELL);if(!n)return;n->payload.enemy.params[0]=q->ring_gun.owner;n->payload.enemy.ring_gun.pose=q->ring_gun.pose;n->payload.enemy.ring_gun.angle=q->ring_gun.angle>0?tk_word(-q->ring_gun.angle):q->ring_gun.angle;enemy_schedule(q,G193_RELOAD);
  }
 }else if(s==G193_SHELL){if(interaction_move(w,a,WMOVE_POINT)<0)return;animate(w,e);if(!enemy_attack(w,a))return;if(a->hit.flags&10){if(!tk_sound(w,0x1027))return;boss_fx_schedule(w,e,BFX_FIRE);return;}if((uint16_t)tk_word(tk_sx(w,a)+16)>=352||(uint16_t)tk_word(tk_sy(w,a)-240)>=288)entity_delete(p,e);return;}
 else{enemy_fault(w);return;}
 if(registered(w,q,1)<0)enemy_schedule(q,G193_IDLE);bounds(w,e,1);
}
int ring_gun_spawn(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){if(id!=171&&id!=193)return enemy_fault(w);struct NativeEntity*e=allocate(w,p,o,t,id==171?G171_NEW:G193_NEW);if(!e)return 0;if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];return 1;}
