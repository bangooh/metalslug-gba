#include "enemies.h"
#include "bosses.h"
/* $055BCE..$056A38. Shared attacks keep the source group masks, and enemy
 * projectiles remain descendants of the original global projectile group. */
enum State {NEW,GRENADE,BOUNCE,REST,REST2,FIZZLE,BOUNCE_BLAST,MORTAR,ROLLER,
 GRENADE_GROUND_BLAST,GRENADE_HIT_BLAST,GRENADE_SILENT_BLAST,SHELL_BLAST,BLAST,DELETE,DELETE_TREE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static int16_t word(int n){return (int16_t)(uint16_t)n;}
static int outside(struct WeaponWorld*w,struct WeaponActor*a){return interaction_outside(w,a,enemy_projectile_bounds,1);}
static int point(struct WeaponWorld*w,struct WeaponActor*a,int16_t x,int16_t y,uint8_t*shape){
 return w->point_shape&&w->point_shape(w->scene,a,x,y,shape)?1:enemy_fault(w);
}
static int grenade_init(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(a->variant>=24)return enemy_fault(w);const int16_t*t=enemy_grenade_trajectories[a->variant];
 a->hit.type_flags|=16;a->transform.priority=0xd000;
 a->vx=a->transform.facing&1?word(-t[0]):t[0];a->ax=0;a->ay=t[1];a->vy=t[2];a->timer=(uint16_t)t[3];
 unsigned mode=w->enemies->grenade_mode;a->trail_angle=65535;
 if(mode==2){
  static const struct HitRect filter={-224,224,-256,256};a->transform.facing=0;
  int target=enemy_nearest_x(w,a,&filter);if(target<0){entity_delete_children(w->entities,e);entity_delete(w->entities,e);return 1;}
  struct PlayerTransform*p=&w->players[target]->actor.transform;
  uint8_t angle=enemy_angle(word((p->x>>8)-(a->transform.x>>8)),word(transform_y_up(p)+28-transform_y_up(&a->transform)));
  int16_t speed=word(t[0]+(t[0]>>2));if(speed<0)speed=word(-speed);
  unsigned rounded=(angle+((angle&16)<<1))&224;
  if((rounded&96)==32)rounded=(rounded+8)&248;else if((rounded&96)==96)rounded=(rounded-8)&248;
  a->trail_angle=(uint16_t)rounded;weapon_vector((uint8_t)rounded,speed,&a->vx,&a->vy);a->ax=a->ay=0;
 }
 a->palette=0x15d;
 if(mode==1||mode==2||mode==3){if(mode!=2)a->transform.facing=0;a->trail_angle=(a->trail_angle>>3)&31;enemy_start(q,EAN_GRENADE_ANGLE);}
 else enemy_start(q,EAN_GRENADE);
 enemy_attack_binding(q,ENB_GRENADE);hit_begin_attack(&a->hit);
 q->start_y=word(transform_y_up(&a->transform)+w->camera_y);a->state=GRENADE;return 1;
}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned state=a->state;q->entry=0;
 if(state==NEW){
  switch(q->family){
  case ENEMY_GRENADE:return grenade_init(w,e);
  case ENEMY_BOUNCE:
   /* $5B bit 2 is the source lifecycle flag, not collision byte $59. */
   q->lifecycle_flags|=4;a->hit.type_flags|=16;interaction_priority(w,a,0xd000,16);enemy_start(q,EAN_BOUNCE);a->palette=0x12;
   transform_integer_offset(&a->transform,(a->transform.facing&1)?24:-24,24);a->vx=(a->transform.facing&1)?0x180:-0x180;a->ax=a->ay=0;
   hit_begin_attack(&a->hit);a->variant=0;a->state=BOUNCE;return 1;
  case ENEMY_MORTAR:
   if(!w->interactions->rank_valid)return enemy_fault(w);
   a->palette=14;enemy_attack_binding(q,ENB_MORTAR);hit_begin_attack(&a->hit);hit_begin_attack(&a->hit);a->hit.flags|=32;
   a->hit.hp=(int16_t)enemy_mortar_hp[interaction_rank(w,&q->actor,runtime_owner_enemy_ordnance[0])][q->source_flags&3][(w->interactions->player_count_flag&2)>>1];
   interaction_priority(w,a,0xd000,24);a->vx=a->transform.facing&1?512:-512;a->vy=0x195;a->ay=-81;a->ax=0;
   enemy_start(q,EAN_MORTAR);a->state=MORTAR;return 1;
  case ENEMY_ROLLER:
   a->hit.type_flags|=16;interaction_priority(w,a,0xd000,16);a->palette=14;enemy_start(q,EAN_ROLLER);
   a->vy=128;a->ay=-4;a->vx=0;transform_integer_offset(&a->transform,0,40);a->hit.hp=100;
   enemy_defense(q,ENB_ROLLER_DEFENSE);enemy_attack_binding(q,ENB_ROLLER);hit_begin_attack(&a->hit);hit_begin_attack(&a->hit);q->selector=0;a->state=ROLLER;return 1;
  default:return enemy_fault(w);
  }
 }
 switch(state){
 case REST:enemy_start(q,EAN_BOUNCE_REST);a->vx=a->vy=0;return 1;
 case REST2:enemy_start(q,EAN_BOUNCE_REST2);return 1;
 case FIZZLE:enemy_start(q,EAN_BOUNCE_FIZZLE);return 1;
 case BOUNCE_BLAST:
  if(!interaction_sound(w,0x1022))return 0;enemy_attack_binding(q,ENB_BOUNCE);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;
  interaction_priority(w,a,0xc000,16);a->palette=0x16b;enemy_start(q,EAN_BOUNCE_BLAST);return 1;
 case GRENADE_GROUND_BLAST:case GRENADE_HIT_BLAST:case GRENADE_SILENT_BLAST:
  interaction_priority(w,a,state==GRENADE_GROUND_BLAST?0x2000:0x8000,state==GRENADE_HIT_BLAST?4:16);
  if(!interaction_sound(w,state==GRENADE_SILENT_BLAST?65535:0x1027))return 0;
  enemy_defense(q,65535);a->hit.flags&=(uint8_t)~11u;a->palette=0x16c;enemy_start(q,EAN_GRENADE_BLAST);q->sequence=0;break;
 case SHELL_BLAST:
  if(!interaction_sound(w,0x1022)||!w->rng)return enemy_fault(w);
  interaction_priority(w,a,0x2000,16);a->palette=4;enemy_start(q,EAN_SHELL_BLAST);a->transform.facing|=source_random_word(w->rng)&1;q->sequence=1;break;
 case DELETE:case DELETE_TREE:case BLAST:return 1;
 default:return enemy_fault(w);
 }
 q->scale_x=q->scale_y=255;
 if(interaction_outside(w,a,enemy_blast_bounds[q->sequence],1)){entity_delete(w->entities,e);return 1;}
 enemy_defense(q,65535);a->state=BLAST;return 1;
}
static int grenade_frame(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;int r;
 if(a->timer==65534){if(!scene_preserve_screen(w,a))return 0;r=interaction_move(w,a,WMOVE_BODY);}
 else r=interaction_move(w,a,a->timer?WMOVE_FALL:WMOVE_BODY);
 if(r<0)return 0;if(r)enemy_schedule(q,GRENADE_GROUND_BLAST);
 uint8_t shape;if(!point(w,a,word(a->transform.x>>8),transform_y_up(&a->transform),&shape))return 0;
 if(shape==1){enemy_schedule(q,GRENADE_GROUND_BLAST);if(!enter(w,e))return 0;return 2;}
 if(w->enemies->grenade_mode!=2&&(uint16_t)word(transform_y_up(&a->transform)+w->camera_y)<(uint16_t)q->start_y){
  a->vy=word(a->vy-(a->vy>>5));if(a->vx){int16_t d=a->vx>>5;if(!d)d=1;a->vx=word(a->vx-d);}
 }
 a->palette=enemy_grenade_palettes[w->frame_phase&3];
 if(w->enemies->grenade_mode==1||w->enemies->grenade_mode==3)a->trail_angle=enemy_angle(a->vx>>4,a->vy>>4)>>3;
 enemy_animate(w,e);if(!enemy_attack(w,a))return 0;
 if(a->hit.flags&2)enemy_schedule(q,GRENADE_SILENT_BLAST);
 if(a->hit.flags&8)enemy_schedule(q,GRENADE_HIT_BLAST);
 if(w->enemies->clear_ordnance)enemy_schedule(q,GRENADE_SILENT_BLAST);
 if(outside(w,a))enemy_schedule(q,DELETE_TREE);return 1;
}
static void mortar_drag(struct WeaponActor*a){
 if(a->vx){int16_t d=a->vx>>8;if(!d)d=1;a->vx=word(a->vx-d);}
 unsigned slope=a->ground_slope==0x400?3:a->ground_slope==-0x400?4:a->ground_slope==0x1000?1:a->ground_slope==-0x1000?2:0;
 if(slope){int d=slope<3?3:11;if(!(slope&1))d=-d;a->vx=word(a->vx-d);}
}
static int roller_frame(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 a->vx=word(a->vx+((a->transform.facing&1)?16:-16));int r=interaction_move(w,a,WMOVE_BODY);if(r<0)return 0;
 if(r){transform_integer_offset(&a->transform,0,1);a->vy=0;a->ay=16;}
 else{
  a->ground_tag=a->vx<0?a->vx:word(-a->vx);int16_t x=word((a->transform.x>>8)+((a->transform.facing&1)?7:-7)),y=word(transform_y_up(&a->transform)-8);uint8_t shape;
  if(!point(w,a,x,y,&shape))return 0;
  if(shape){if(a->vy<0)a->vy=0;a->ground_tag=word(-a->ground_tag);}
  else{if(!point(w,a,x,word(y+8),&shape))return 0;if(shape)a->ground_tag=0;}
  a->ay=word(a->ground_tag-a->vy)>>5;
 }
 uint8_t v=(uint8_t)(((uint8_t)(a->vy>>8)+1)&252);q->selector=!v?0:(int8_t)v<0?2:1;
 enemy_animate(w,e);if(outside(w,a)){entity_delete(w->entities,e);return 1;}
 if(!enemy_attack(w,a))return 0;
 if((a->hit.flags&3)||(a->hit.events&32)){enemy_schedule(q,SHELL_BLAST);if(!enter(w,e))return 0;return 2;}
 if(a->hit.flags&8){if(!interaction_sound(w,0x108d))return 0;a->hit.flags&=247;a->vy=word(a->vy+0x300);}return 1;
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(q->entry&&!enter(w,e))return;if(!e->active||e->deleting)return;
 unsigned state=a->state;int r;
 switch(state){
 case DELETE_TREE:entity_delete_children(p,e);entity_delete(p,e);return;
 case DELETE:entity_delete(p,e);return;
 case GRENADE:r=grenade_frame(w,e);if(r!=2||!e->active||e->deleting)return;break;
 case BOUNCE:case REST:case REST2:
  r=interaction_move(w,a,WMOVE_ITEM);if(r<0)return;if(state==BOUNCE&&r)enemy_schedule(q,REST);
  if(enemy_animate(w,e)&&state!=BOUNCE)enemy_schedule(q,state==REST?REST2:FIZZLE);
  if(a->hit.flags&8)enemy_schedule(q,FIZZLE);
  if(!(--a->ttl&3)){a->variant^=1;a->palette=a->variant?0x13:0x12;}
  if(outside(w,a)){entity_delete(p,e);return;}enemy_publish_death(w,a);return;
 case FIZZLE:
  if(interaction_move(w,a,WMOVE_ITEM)<0)return;if(enemy_animate(w,e))enemy_schedule(q,BOUNCE_BLAST);return;
 case BOUNCE_BLAST:
  if(interaction_move(w,a,WMOVE_GROUND)<0)return;if(enemy_animate(w,e)){entity_delete_children(p,e);entity_delete(p,e);}return;
 case MORTAR:
  if(interaction_move(w,a,WMOVE_SYNC)<0)return;r=terrain_weapon_foot_contact(w->interactions->terrain,a);if(r<0){enemy_fault(w);return;}
  if(enemy_swapped_move(w,q,r?WMOVE_GROUND:WMOVE_ITEM)<0)return;if(r)mortar_drag(a);
  if((a->transform.facing&1)?a->vx<=0:a->vx>=0)enemy_schedule(q,SHELL_BLAST);
  if(a->hit.events&32)enemy_schedule(q,SHELL_BLAST);enemy_animate(w,e);if(!enemy_attack(w,a))return;
  if(a->hit.flags&2)enemy_schedule(q,SHELL_BLAST);a->hit.flags&=247;if(a->hit.flags&1)enemy_schedule(q,SHELL_BLAST);
  if(outside(w,a))enemy_schedule(q,DELETE);return;
 case ROLLER:r=roller_frame(w,e);if(r!=2||!e->active||e->deleting)return;break;
 case BLAST:break;
 default:enemy_fault(w);return;
 }
 /* $044048 only adjusts camera words; world-space explosion positions
  * do not inherit moving-platform carry here. */
 if(enemy_animate(w,e)||interaction_outside(w,a,enemy_blast_bounds[q->sequence],1))entity_delete(p,e);
}
int enemy_spawn_ordnance(struct WeaponWorld*w,struct EntityRef parent,const struct PlayerTransform*t,enum EnemyOrdnance kind,uint8_t trajectory,uint8_t flags){
 (void)parent;
 if((unsigned)kind>ENEMY_ROLLER||!t)return enemy_fault(w);
 struct PlayerTransform spawn={.x=t->x,.y=t->y,.facing=t->facing,.display_group=kind==ENEMY_MORTAR?flags:255};
 if(kind==ENEMY_GRENADE)transform_integer_offset(&spawn,0,32);else if(kind==ENEMY_MORTAR)transform_integer_offset(&spawn,0,20);
 struct NativeEntity*e=enemy_allocate(w,w->projectile_group,ENTITY_FIRST_CHILD,&spawn,tick);if(!e)return 0;
 struct EnemyActor*q=&e->payload.enemy;q->family=(uint8_t)kind;q->actor.variant=trajectory;return 1;
}

/* $056204 can adopt a projectile whose first flight is owned by a boss. */
int enemy_bounce_explode(struct WeaponWorld*w,struct NativeEntity*e){
 if(e->kind!=ENTITY_ENEMY)return enemy_fault(w);e->tick=tick;enemy_schedule(&e->payload.enemy,BOUNCE_BLAST);tick(w->entities,e,w);return !w->fault;
}
