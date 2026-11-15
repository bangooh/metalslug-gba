#include "interactions.h"
#include "items.h"
#include "../generated/slug-projectile-programs.h"
/* $09A0FC..$09A5D6. Owner 2 follows the original enemy branch verbatim. */
enum State {NEW,IDLE,ACTIVE,OVERLAY_INIT,OVERLAY,SHELL,DESTROY,EXPLOSION};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static int16_t word(int v){return (int16_t)(uint16_t)v;}
static void schedule(struct InteractionActor*q,enum State s){q->actor.state=(uint8_t)s;q->entry=1;}
static uint8_t angle256(int16_t vx,int16_t vy){
 int16_t hx=vx>>1,hy=vy>>1;uint8_t sx=hx<0?255:0,sy=hy<0?255:0;
 uint8_t x=(uint8_t)hx^sx,y=(uint8_t)hy^sy,swap=((uint8_t)(x-y)&128)?255:0;
 if(x>=y){uint8_t t=x;x=y;y=t;}uint8_t angle;
 if(!x)angle=swap&64;else{angle=slug_atan_exp[(uint8_t)(slug_atan_log[x]-slug_atan_log[y])];angle=(uint8_t)((angle^swap)-swap+32);}
 uint8_t sign=sx^sy;return (uint8_t)((angle^sign)-sign+(sx&128));
}
static int target_angle(struct WeaponWorld*w,struct WeaponActor*a,int16_t*angle){
 int slot=-1;int16_t best=32767;
 for(unsigned i=0;i<2;i++){
  struct PlayerBodyState*p=w->players[i];if(!p||p->life_retired||!(p->actor.lifecycle_flags&128))continue;
  int16_t d=word((p->actor.transform.x>>8)-(a->transform.x>>8));if(d<0)d=word(-d);
  if(slot<0||d<best){slot=(int)i;best=d;}
 }
 if(slot<0)return 0;struct PlayerTransform*t=&w->players[slot]->actor.transform;
 *angle=angle256(word((t->x>>8)-(a->transform.x>>8)),word(transform_y_up(t)-transform_y_up(&a->transform)));return 1;
}
static int rank_value(struct WeaponWorld*w,struct WeaponActor*a,unsigned table,uint16_t*value){
 struct InteractionWorld*s=w->interactions;if(!s->rank_valid||s->rank>7)return interaction_fault(w);
 *value=gun_rank_tables[table][interaction_rank(w,a,runtime_owner_gun[table])][a->transform.display_group&3][!!(s->player_count_flag&2)];return 1;
}
static int follow(struct WeaponWorld*w,struct NativeEntity*e){
 struct InteractionActor*q=&e->payload.interaction;struct WeaponActor*a=&q->actor;
 if(!q->gun.attached)return interaction_move(w,a,WMOVE_SYNC)>=0;
 struct WeaponActor*p=entity_weapon_actor(entity_get(w->entities,e->parent));if(!p)return interaction_fault(w);
 transform_integer_offset(&a->transform,word((p->transform.x>>8)+q->gun.offset_x-(a->transform.x>>8)),
  word(transform_y_up(&p->transform)+q->gun.offset_y-transform_y_up(&a->transform)));
 interaction_priority(w,a,a->transform.priority,a->transform.priority&0x1c);return 1;
}
static void draw(struct InteractionActor*q){
 uint16_t d=(uint16_t)(word(q->gun.angle)>>8);d=(uint16_t)((d+4)&0xf8);
 if((uint8_t)d>=0x88)d=(uint8_t)(-(uint8_t)d);
 if(d>=0x40){q->actor.transform.facing|=1;d=(uint8_t)(0x80-d);}else q->actor.transform.facing&=254;
 q->gun.row=(uint8_t)((d&0x78)>>3);uint8_t angle=gun_muzzle_angles[q->gun.row];
 q->gun.muzzle=q->actor.transform.facing&1?(uint8_t)(128-angle):angle;
 if(q->actor.animation.pose)q->actor.animation.pose--;q->gun.pose=q->actor.animation.pose;
 animation_drawing(&q->actor.animation,&interaction_program,gun_drawings[q->gun.row][q->gun.pose][0]);
}
static void register_target(struct WeaponWorld*w,struct InteractionActor*q){
 uint8_t d=(uint8_t)((q->gun.angle>>8)&0xf8);if(d>=0x88)d=(uint8_t)-d;
 int16_t x=word((q->actor.transform.x>>8)+gun_target_offsets[d>>3]);
 target_ring_register(w->interactions->targets,word(x-w->camera_x),x,transform_y_up(&q->actor.transform),q->key,&q->gun.owner);
}
static int fire(struct WeaponWorld*w,struct NativeEntity*e,int shell){
 struct InteractionActor*q=&e->payload.interaction;q->actor.animation.pose=5;
 if(!interaction_sound(w,0x10f4))return 0;unsigned index=q->gun.muzzle>>4;if(index>8)return interaction_fault(w);
 struct PlayerTransform t={0};transform_integer_copy(&t,&q->actor.transform);transform_integer_offset(&t,gun_muzzle_offsets[index][0],gun_muzzle_offsets[index][1]);
 if(!shell)return weapon_emit_gun_bullet(w,&t,q->gun.muzzle);
 struct NativeEntity*n=interaction_allocate(w,w->projectile_group,ENTITY_FIRST_CHILD,&t,tick);if(!n)return 0;
 n->payload.interaction.params[0]=q->gun.muzzle;schedule(&n->payload.interaction,SHELL);return 1;
}
static int enemy_control(struct WeaponWorld*w,struct NativeEntity*e){
 struct InteractionActor*q=&e->payload.interaction;int16_t d;if(!target_angle(w,&q->actor,&d))return 1;
 if(q->gun.burst==3){d=word(word(d*256-q->gun.angle)>>3);q->gun.angle=(uint16_t)(q->gun.angle+d);}
 if(q->gun.cooldown){q->gun.cooldown--;return 1;}
 if(((uint16_t)(d+0x80)&0xff00)!=0)return 1;
 if(!q->gun.burst){q->gun.burst=3;return rank_value(w,&q->actor,0,&q->gun.cooldown);}
 /* $09A41E decrements zero to FFFF before BNE. Preserve this source branch;
  * do not invent a corrected enemy burst or treat player byte as a WORD. */
 if(--q->gun.cooldown)return 1;q->gun.cooldown=2;q->gun.burst--;return fire(w,e,1);
}
static int player_control(struct WeaponWorld*w,struct NativeEntity*e){
 struct InteractionActor*q=&e->payload.interaction;if(q->gun.owner<0||q->gun.owner>1)return interaction_fault(w);
 struct PlayerSourceInput input=w->inputs[(unsigned)q->gun.owner];
 if(input.held&(PB_LEFT|PB_RIGHT)){
  int16_t d=word((input.held&PB_LEFT?0:0x8000)-q->gun.angle)>>2;
  uint16_t magnitude=d<0?(uint16_t)word(-d):(uint16_t)d,limit=(uint16_t)(q->gun.acceleration+0x100);
  if(limit>=magnitude)limit=magnitude;q->gun.acceleration=limit;q->gun.angle=(uint16_t)(q->gun.angle+(d<0?-limit:limit));
 }else q->gun.acceleration=0;
 if(input.actions&PB_FIRE){q->gun.burst=3;q->gun.cooldown=(uint16_t)((q->gun.cooldown&255)|0x100);}
 if(q->gun.burst){
  uint8_t cooldown=(uint8_t)((q->gun.cooldown>>8)-1);q->gun.cooldown=(uint16_t)((q->gun.cooldown&255)|(cooldown<<8));
  if(!cooldown){q->gun.cooldown|=0x200;q->gun.burst--;return fire(w,e,0);}
 }return 1;
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;struct InteractionActor*q=&e->payload.interaction;struct WeaponActor*a=&q->actor;
 if(q->entry){q->entry=0;
  switch(a->state){
  case NEW:{
   a->palette=0xd1;interaction_priority(w,a,0x8000,4);a->animation.pose=0;q->key=(uint8_t)~target_ring_new_key(w->interactions->targets);q->gun.angle=0;
   struct PlayerTransform t={0};struct NativeEntity*n=interaction_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&t,tick);if(!n)return;
   schedule(&n->payload.interaction,OVERLAY_INIT);a->hit.hp=100;q->gun.sprite_template=a->hit_palette=0x76;a->state=IDLE;
   q->gun.burst=0;a->animation.pose=0;q->gun.owner=-1;a->defense=0;a->defense_count=0;break;
  }
  case IDLE:q->gun.burst=0;a->animation.pose=0;q->gun.owner=-1;a->defense=0;a->defense_count=0;break;
  case SHELL:{
   a->palette=0xd1;a->hit.type_flags|=16;a->attack=&gun_shell_attack;hit_begin_attack(&a->hit);a->transform.priority=0xd000;
   uint16_t speed=0;if(!rank_value(w,a,1,&speed))return;weapon_vector((uint8_t)((q->params[0]+8)&0xf0),word(speed),&a->vx,&a->vy);
   uint16_t entry=gun_shell_entries[q->params[0]>>4];if(entry!=65535){animation_begin(&a->animation,entry);a->movement_flags|=32;}break;
  }
  case DESTROY:
   if(!interaction_sound(w,0x1021)||!w->rng){interaction_fault(w);return;}interaction_start(q,INAN_GUN_EXPLOSION);a->palette=4;
   a->transform.facing|=source_random_word(w->rng)&1;q->scale_x=q->scale_y=255;
   if(interaction_outside(w,a,gun_explosion_bounds,1)){entity_delete(p,e);return;}a->defense=0;a->defense_count=0;a->state=EXPLOSION;break;
  default:break;
  }
 }
 enum State state=(enum State)a->state;
 if(state==OVERLAY_INIT||state==OVERLAY){
  struct NativeEntity*parent=entity_get(p,e->parent);if(!parent||parent->kind!=ENTITY_INTERACTION){interaction_fault(w);return;}
  struct InteractionActor*r=&parent->payload.interaction;
  if(state==OVERLAY_INIT){if(r->gun.owner<0)return;if(r->gun.owner>2){interaction_fault(w);return;}a->palette=gun_palettes[(unsigned)r->gun.owner];a->state=OVERLAY;}
  a->transform.facing=r->actor.transform.facing;if(r->gun.owner<0){schedule(q,OVERLAY_INIT);return;}
  animation_drawing(&a->animation,&interaction_program,gun_drawings[r->gun.row][r->gun.pose][1+(unsigned)r->gun.owner]);
  transform_integer_offset(&a->transform,word((r->actor.transform.x>>8)-(a->transform.x>>8)),word(transform_y_up(&r->actor.transform)-transform_y_up(&a->transform)));
  a->transform.priority=r->actor.transform.priority;if(!animation_output_direct(&a->animation,AN_OUTPUT_DIRECT))interaction_fault(w);return;
 }
 if(state==EXPLOSION){
  /* $044048 is camera-only; world coordinates already include this change. */
  if(interaction_animate(w,e)||interaction_outside(w,a,gun_explosion_bounds,1))entity_delete(p,e);return;
 }
 if(state==SHELL){
  a->hit.flags|=64;if(interaction_move(w,a,WMOVE_POINT)<0)return;interaction_animate(w,e);
  a->hit.x=word(a->transform.x>>8);a->hit.y=transform_y_up(&a->transform);a->hit.flip=a->transform.facing;
  if(!w->attack){interaction_fault(w);return;}
  for(unsigned i=0;i<a->attack->count;i++)if(!w->attack(w->scene,&a->hit,&a->attack->records[i],0)){interaction_fault(w);return;}
  static const int16_t bounds[4]={0,1,0,1};if((a->hit.flags&2)||interaction_outside(w,a,bounds,1))entity_delete(p,e);return;
 }
 if(state==ACTIVE){if(q->gun.owner==2){if(!enemy_control(w,e))return;}else if(!player_control(w,e))return;}
 if(!follow(w,e))return;draw(q);if(!animation_output_direct(&a->animation,AN_OUTPUT_DIRECT)){interaction_fault(w);return;}register_target(w,q);
 if(state==IDLE&&q->gun.owner>=0)schedule(q,ACTIVE);else if(state==ACTIVE&&q->gun.owner<0)schedule(q,IDLE);
 if(interaction_outside(w,a,gun_bounds,0)){entity_delete_children(p,e);entity_delete(p,e);}
}
int gun_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,const struct PlayerTransform*t,int attached){
 if(!w->interactions||!w->interactions->targets)return interaction_fault(w);
 struct NativeEntity*e=interaction_allocate(w,parent,order,t,tick);if(!e)return 0;
 struct InteractionActor*q=&e->payload.interaction;q->kind=2;q->gun.attached=(uint8_t)!!attached;
 if(attached){q->gun.offset_x=word(t->x>>8);q->gun.offset_y=transform_y_up(t);}return 1;
}
int gun_destroy(struct WeaponWorld*w,struct EntityRef ref){
 struct NativeEntity*e=entity_get(w->entities,ref);if(!e||e->kind!=ENTITY_INTERACTION||e->payload.interaction.kind!=2)return 0;
 /* Native scene API removes the overlay before handing the same actor to
  * the source explosion lifetime, which has no gun-child retirement code. */
 entity_delete_children(w->entities,e);schedule(&e->payload.interaction,DESTROY);return 1;
}
