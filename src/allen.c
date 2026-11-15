#include "enemies.h"
/* $05029C..$0515F8. Actual template 328, not an inferred mission-boss ID.
 * Source labels reverse the level/diagonal MG animations. Names below follow
 * the callbacks and projectile vectors. State assignments are next-tick,
 * except the source's explicit branches into another entry/frame. */
enum State {INIT,SPAWN,INTRO,MAIN,TURN,REVERSE,TAUNT,FIRE_LEVEL,FIRE_DIAGONAL,
 IDLE,LAND,JUMP_DECIDE,JUMP_TOWARD,JUMP_HIGH,AIR,KNIFE,DEATH,DEATH_GROUND,
 DEATH_AIR,DEATH_LAND,STAGGER,FADE,SENSOR,FLASH,GRENADE,BULLET,
 BLAST_GROUND,BLAST_HIT,BLAST_SILENT,BLAST,IMPACT,DELETE_TREE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static int16_t word(int n){return (int16_t)(uint16_t)n;}
static int16_t sx(struct WeaponWorld*w,const struct WeaponActor*a){return word((a->transform.x>>8)-w->camera_x);}
static int16_t sy(struct WeaponWorld*w,const struct WeaponActor*a){return word(transform_y_up(&a->transform)+w->camera_y);}
static uint16_t random_word(struct WeaponWorld*w){if(!w->rng){enemy_fault(w);return 0;}return source_random_word(w->rng);}
static uint16_t rank(struct WeaponWorld*w,struct EnemyActor*q,unsigned table){
 if(!w->interactions->rank_valid){enemy_fault(w);return 0;}
 return allen_rank[table][interaction_rank(w,&q->actor,runtime_owner_allen[table])][q->source_flags&3][(w->interactions->player_count_flag&2)>>1];
}
/* $04400E uses the secondary view origin, not the primary camera. */
static int16_t secondary_y(struct WeaponWorld*w,struct WeaponActor*a){
 if(!w->secondary_view_valid){enemy_fault(w);return 0;}
 return word(512-sy(w,a)+w->secondary_y_offset);
}
static const struct PlayerTransform*target(struct WeaponWorld*w,struct EnemyActor*q){
 return q->allen.target>=0&&q->allen.target<2&&w->players[(unsigned)q->allen.target]?
  &w->players[(unsigned)q->allen.target]->actor.transform:0;
}
static void acquire(struct WeaponWorld*w,struct EnemyActor*q){
 if(q->allen.cooldown)q->allen.cooldown--;
 int old=q->allen.target;
 if(old>=0&&(old>1||!enemy_player_live(w->players[old])))q->allen.target=-1;
 const int16_t*b=allen_target_bounds;struct HitRect filter={b[0],b[1],b[2],b[3]};
 int found=enemy_nearest_x(w,&q->actor,&filter);q->allen.visible=found>=0?255:0;
 if(found>=0)q->allen.target=(int8_t)found;
 else if(q->allen.target<0){found=enemy_nearest_x(w,&q->actor,0);if(found>=0)q->allen.target=(int8_t)found;}
 const struct PlayerTransform*t=target(w,q);q->allen.in_front=255;
 if(t)q->allen.in_front=((word((t->x>>8)-(q->actor.transform.x>>8))<0)^!!(q->actor.transform.facing&1))?255:0;
}
/* $05029C/$0502A6. Preserve the unusual AND of the remaining WORDs:
 * the source does not keep substepping once either axis becomes zero. */
static int step(struct WeaponWorld*w,struct WeaponActor*a,enum WeaponMove mode){
 int16_t vx=a->vx,vy=a->vy,ax=a->ax,ay=a->ay,rx=vx,ry=vy;
 a->ax=a->ay=0;int16_t dx=vx<0?-2048:2048,dy=vy<0?-2048:2048;int collision=0;
 for(unsigned budget=0;budget<64;budget++){
  int16_t nx=word(rx-dx),ny=word(ry-dy);
  if((rx<0)!=(nx<0)){dx=word(dx+nx);nx=0;}
  if((ry<0)!=(ny<0)){dy=word(dy+ny);ny=0;}
  rx=nx;ry=ny;a->vx=dx;a->vy=dy;collision=interaction_move(w,a,mode);
  if(collision<0)break;
  if(collision||!((uint16_t)rx&(uint16_t)ry))break;
  if(budget==63){enemy_fault(w);collision=-1;}
 }
 a->ax=ax;a->ay=ay;a->vx=word(vx+ax);a->vy=word(vy+ay);return collision;
}
static int shape(struct WeaponWorld*w,struct WeaponActor*a,int16_t y,uint8_t*out){
 return w->point_shape&&w->point_shape(w->scene,a,word(a->transform.x>>8),y,out)?1:enemy_fault(w);
}
static int physics(struct WeaponWorld*w,struct EnemyActor*q){
 struct WeaponActor*a=&q->actor;int r;
 if(q->allen.airborne){
  if(secondary_y(w,a)>=239)q->allen.ground_seen=255;
  if(w->fault)return 0;
  if(q->allen.ground_seen)r=step(w,a,WMOVE_BODY);
  else{
   uint8_t here,below;if(!shape(w,a,transform_y_up(&a->transform),&here))return 0;
   if(here){if(!shape(w,a,word(transform_y_up(&a->transform)-8),&below))return 0;if(!below)q->allen.ground_seen=255;}
   r=interaction_move(w,a,WMOVE_FALL);
  }
  if(r<0)return 0;q->allen.airborne=r?0:255;
 }else{
  if(interaction_move(w,a,WMOVE_GROUND)<0)return 0;
  r=terrain_weapon_foot_contact(w->interactions->terrain,a);if(r<0)return enemy_fault(w);
  if(!r){a->ay=-64;q->allen.airborne=255;}
 }return 1;
}
static int knife_probe(struct WeaponWorld*w,struct EnemyActor*q){
 struct WeaponActor*a=&q->actor;int x=sx(w,a);
 if(a->hit.invulnerable||x<32||x>=768)return 0;
 enemy_attack_binding(q,ENB_ALLEN_KNIFE_PROBE);hit_begin_attack(&a->hit);hit_begin_attack(&a->hit);
 if(!enemy_attack(w,a))return 0;return !!(a->hit.flags&2);
}
static int above(struct WeaponWorld*w,struct EnemyActor*q){
 struct WeaponActor*a=&q->actor;const struct PlayerTransform*t=target(w,q);
 if((uint16_t)word(sx(w,a)-32)>=256||q->allen.cooldown||!t)return 0;
 int16_t dx=word((t->x>>8)-(a->transform.x>>8));
 if(!((dx<0)^!!(a->transform.facing&1)))return 0;
 if(dx<0)dx=word(-dx);int16_t dy=word(transform_y_up(t)-transform_y_up(&a->transform));
 return dy>=0&&(uint16_t)word(dy-dx-16)<32;
}
static int front(struct WeaponWorld*w,struct EnemyActor*q){
 return (uint16_t)word(sx(w,&q->actor)-32)<256&&!q->allen.cooldown&&q->allen.in_front&&q->allen.visible;
}
static int far_below(struct WeaponWorld*w,struct EnemyActor*q){
 if(q->allen.voice_timer)q->allen.voice_timer--;
 const struct PlayerTransform*t=target(w,q);if(!t||!q->allen.in_front)return 0;
 int16_t dy=word(transform_y_up(&q->actor.transform)-transform_y_up(t));if(dy<=0)return 0;
 int16_t dx=word((q->actor.transform.x>>8)-(t->x>>8));if(dx<0)dx=word(-dx);
 return (uint16_t)word(dx+dx)>=(uint16_t)dy;
}
static int jump_check(struct WeaponWorld*w,struct EnemyActor*q,int high){
 if(!target(w,q))return 0;
 int y=secondary_y(w,&q->actor);if(w->fault)return 0;
 if(high?(y>=239||q->allen.sensors[3]):(y<=143||q->allen.sensors[2]))return 0;
 if(q->allen.visible&&(q->allen.sensors[0]|q->allen.sensors[1]))return 1;
 return (random_word(w)&15)<q->allen.jump_chance;
}
static int tail(struct WeaponWorld*w,struct NativeEntity*e,int damage){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(damage){
  if(q->allen.blood_timer)q->allen.blood_timer--;
  if(a->hit.flags&8){
   a->hit.flags&=247;
   if(!q->allen.blood_timer){if(!human_spawn_blood(w,e,1))return 0;q->allen.blood_timer=12;}
   int16_t floor=word(q->allen.previous_hp-50);if(a->hit.hp<floor)a->hit.hp=floor;
   if((uint16_t)a->hit.hp<(uint16_t)q->allen.threshold){
    q->allen.threshold=word(q->allen.threshold-q->allen.segment);
    if(q->allen.stage>4)return enemy_fault(w);
    /* $002C30 changes this actor's private palette contents, not a HUD bar. */
    a->palette=allen_stage_palettes[q->allen.stage];if(q->allen.stage<4)q->allen.stage++;
   }
   if(a->hit.hp>0)a->hit.flags&=254;
   /* $05E766/$05E770 is the hit SOUND table; there is no flash timer here. */
   if(!(a->hit.flags&1)&&(a->hit.events&1)&&a->hit.last_kind==1&&!interaction_sound(w,0x108d))return 0;
  }
  if(a->hit.flags&1)enemy_schedule(q,DEATH);
 }
 q->allen.previous_hp=a->hit.hp;
 if(interaction_outside(w,a,allen_bounds,1))entity_delete(w->entities,e);return !w->fault;
}
static struct NativeEntity*child(struct WeaponWorld*w,struct EntityRef parent,const struct PlayerTransform*t,unsigned state){
 struct NativeEntity*e=enemy_allocate(w,parent,ENTITY_FIRST_CHILD,t,tick);if(e)enemy_schedule(&e->payload.enemy,state);return e;
}
int allen_emit(struct WeaponWorld*w,struct NativeEntity*e,enum AllenEmission kind){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(kind==ALLEN_FLASH_LEVEL||kind==ALLEN_FLASH_DIAGONAL){
  struct PlayerTransform t={0};struct NativeEntity*n=child(w,entity_ref(e),&t,FLASH);if(!n)return 0;
  n->payload.enemy.sequence=kind==ALLEN_FLASH_DIAGONAL;return 1;
 }
 struct PlayerTransform t={.x=a->transform.x,.y=a->transform.y,.display_group=255};
 if(kind==ALLEN_GRENADE){
  const struct PlayerTransform*p=target(w,q);if(!p)return 1;
  transform_integer_offset(&t,0,16);struct NativeEntity*n=child(w,w->projectile_group,&t,GRENADE);if(!n)return 0;
  int16_t dx=word((p->x>>8)+(random_word(w)&31)-16-(t.x>>8)),dy=word(transform_y_up(p)-transform_y_up(&t));
  n->payload.enemy.selector=enemy_angle(dx,dy);return !w->fault;
 }
 if(kind!=ALLEN_BULLET_LEVEL&&kind!=ALLEN_BULLET_DIAGONAL)return enemy_fault(w);
 if(!interaction_sound(w,0x10f4))return 0;
 int diagonal=kind==ALLEN_BULLET_DIAGONAL;t.facing=a->transform.facing;
 transform_integer_offset(&t,(t.facing&1)?(diagonal?40:56):-(diagonal?40:56),diagonal?64:24);
 struct NativeEntity*n=child(w,w->projectile_group,&t,BULLET);if(!n)return 0;
 n->payload.enemy.selector=(t.facing&1)?(diagonal?32:0):(diagonal?96:128);return 1;
}
/* 1 = execute frame now; 2 = entry returned without a frame; 0 = fault. */
static int enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 for(unsigned budget=0;budget<8;budget++){
  unsigned s=a->state;q->entry=0;
  switch(s){
  case INIT:{
   a->hit.hp=(int16_t)rank(w,q,0);if(w->fault||!a->hit.hp)return enemy_fault(w);
   q->allen.previous_hp=a->hit.hp;q->allen.segment=(int16_t)((uint16_t)a->hit.hp/5);
   q->allen.threshold=word(a->hit.hp-q->allen.segment);a->palette=0x18e;q->allen.stage=0;
   for(unsigned i=0;i<4;i++){
    struct PlayerTransform t={0};struct NativeEntity*n=child(w,entity_ref(e),&t,SENSOR);if(!n)return 0;n->payload.enemy.selector=(uint16_t)i;
   }
   q->allen.cooldown=q->allen.jump_chance=q->allen.voice_timer=q->allen.blood_timer=0;
   if(sx(w,a)<160){a->transform.facing|=1;q->allen.retreat=0;}
   acquire(w,q);enemy_schedule(q,SPAWN);return 2;
  }
  case SPAWN:enemy_defense(q,65535);enemy_start(q,EAN_ALLEN_WALK);a->vx=q->allen.retreat?-1536:1536;return 1;
  case INTRO:enemy_start(q,EAN_ALLEN_IDLE);a->timer=90;a->vx=0;return 1;
  case REVERSE:q->allen.jump_chance++;q->allen.retreat=(uint8_t)~q->allen.retreat;enemy_schedule(q,MAIN);continue;
  case MAIN:
   a->vx=q->allen.retreat?-1536:1536;
   enemy_start(q,((q->allen.retreat^a->transform.facing)&1)?EAN_ALLEN_WALK:EAN_ALLEN_BACK);return 1;
  case TURN:q->allen.voice_timer=0;enemy_start(q,EAN_ALLEN_TURN);a->vx=0;return 1;
  case TAUNT:
   if(!q->allen.voice_timer){q->allen.voice_timer=30;if(!interaction_sound(w,allen_voices[(random_word(w)&6)>>1]))return 0;}
   enemy_start(q,EAN_ALLEN_TAUNT);a->vx=0;return 1;
  case FIRE_LEVEL:case FIRE_DIAGONAL:
   enemy_start(q,s==FIRE_LEVEL?EAN_ALLEN_FIRE_LEVEL:EAN_ALLEN_FIRE_DIAGONAL);q->allen.voice_timer=0;a->vx=0;return 1;
  case IDLE:enemy_start(q,EAN_ALLEN_IDLE);a->vx=0;return 1;
  case LAND:q->allen.voice_timer=q->allen.jump_chance=0;enemy_start(q,EAN_ALLEN_LAND);a->vx=0;return 1;
  case JUMP_DECIDE:{
   int y=secondary_y(w,a);const struct PlayerTransform*t=target(w,q);if(w->fault)return 0;
   if(y>143&&t){if((uint16_t)word(transform_y_up(t)+w->camera_y)<(uint16_t)sy(w,a)){enemy_schedule(q,JUMP_TOWARD);continue;}}
   else if(!q->allen.visible){
    enemy_start(q,EAN_ALLEN_AIR);a->vy=0x886;a->ay=-291;a->vx=q->allen.retreat?-1092:1092;
    q->allen.airborne=255;a->state=AIR;return 1;
   }
   enemy_start(q,EAN_ALLEN_AIR);a->vx=a->vy=0;a->ay=-64;a->state=AIR;return 1;
  }
  case JUMP_TOWARD:{
   enemy_start(q,EAN_ALLEN_AIR);
   uint32_t speed=0x1111,accel=((0x180u*speed)>>8);accel=(uint16_t)accel*speed;
   a->ay=word(-(int)(accel>>16));a->vy=word((0xf0u*speed)>>8);
   int y=secondary_y(w,a);if(w->fault)return 0;int x=sx(w,a),dest=y>191?48:96;
   if(x>=160)dest=320-dest;a->vx=word(word(dest-x)*17);q->allen.airborne=255;a->state=AIR;return 1;
  }
  case JUMP_HIGH:
   enemy_start(q,EAN_ALLEN_AIR);a->vx=a->ax=0;a->vy=0x922;a->ay=-668;
   q->allen.airborne=255;q->allen.ground_seen=0;a->state=AIR;return 1;
  case AIR:return 1;
  case KNIFE:q->allen.voice_timer=0;enemy_start(q,EAN_ALLEN_KNIFE);hit_begin_attack(&a->hit);return 1;
  case DEATH:
   if(!human_spawn_blood(w,e,0)||!interaction_sound(w,0x1140))return 0;a->palette=0x18e;
   if(q->allen.airborne){
    enemy_start(q,EAN_ALLEN_DEATH_AIR);a->vx=a->ax=0;a->vy=1024;a->ay=-256;a->state=DEATH_AIR;return 1;
   }
   enemy_start(q,EAN_ALLEN_DEATH_GROUND);a->vx=0;a->state=DEATH_GROUND;return 1;
  case DEATH_LAND:enemy_start(q,EAN_ALLEN_DEATH_LAND);a->vx=0;a->state=DEATH_GROUND;return 1;
  case DEATH_GROUND:case DEATH_AIR:return 1;
  case STAGGER:enemy_start(q,EAN_ALLEN_STAGGER);a->vx=0;return 1;
  case FADE:a->damage_timer=30;return 1;
  case SENSOR:{
   static const unsigned bindings[]={ENB_ALLEN_SENSOR0,ENB_ALLEN_SENSOR1,ENB_ALLEN_SENSOR2,ENB_ALLEN_SENSOR3};
   if(q->selector>=4)return enemy_fault(w);enemy_defense(q,bindings[q->selector]);return 1;
  }
  case FLASH:enemy_start(q,q->sequence?EAN_ALLEN_FLASH_DIAGONAL:EAN_ALLEN_FLASH_LEVEL);a->palette=0x18f;return 1;
  case GRENADE:
   a->hit.type_flags|=16;a->transform.priority=0xd000;
   weapon_vector((uint8_t)q->selector,(int16_t)rank(w,q,2),&a->vx,&a->vy);a->ax=a->ay=0;
   a->palette=0x15d;enemy_start(q,EAN_ALLEN_GRENADE);enemy_attack_binding(q,ENB_ALLEN_GRENADE);hit_begin_attack(&a->hit);return !w->fault;
  case BULLET:
   a->palette=0x18f;enemy_start(q,EAN_ALLEN_BULLET);
   weapon_vector((uint8_t)q->selector,(int16_t)rank(w,q,3),&a->vx,&a->vy);
   enemy_attack_binding(q,ENB_ALLEN_BULLET);hit_begin_attack(&a->hit);return !w->fault;
  case BLAST_GROUND:case BLAST_HIT:case BLAST_SILENT:
   interaction_priority(w,a,s==BLAST_GROUND?0x2000:0x8000,s==BLAST_HIT?4:16);
   if(!interaction_sound(w,s==BLAST_SILENT?65535:0x1027))return 0;
   enemy_defense(q,65535);a->hit.flags&=(uint8_t)~11u;a->palette=0x1e0;enemy_start(q,EAN_ALLEN_BLAST);
   enemy_attack_binding(q,ENB_ALLEN_BLAST);hit_begin_attack(&a->hit);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;
   a->state=BLAST;return 2;
  case BLAST:case DELETE_TREE:return 1;
  case IMPACT:
   /* $0515B0 is retained as a source entry. $0283D8's final FF sentinel
    * CMP clears carry, so the preceding BCS does not select this state. */
   a->palette=a->hit.contact_resistance;enemy_start(q,a->vy?EAN_ALLEN_IMPACT_DIAGONAL:EAN_ALLEN_IMPACT_LEVEL);
   a->vx=a->vy=0;return 1;
  default:return enemy_fault(w);
  }
 }return enemy_fault(w);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(q->entry&&enter(w,e)!=1)return;unsigned s=a->state;int done;
 if(s==SENSOR||s==FLASH){
  struct NativeEntity*parent=entity_get(p,e->parent);if(!parent||parent->deleting){entity_delete(p,e);return;}
  if(parent->kind!=ENTITY_ENEMY){enemy_fault(w);return;}struct EnemyActor*r=&parent->payload.enemy;
  transform_integer_copy(&a->transform,&r->actor.transform);
  if(s==SENSOR){r->allen.sensors[q->selector]=!r->allen.blood_timer&&(a->hit.flags&8)?255:0;a->hit.flags&=247;}
  else{a->transform.facing=r->actor.transform.facing;a->transform.priority=r->actor.transform.priority;if(enemy_animate(w,e))entity_delete(p,e);}return;
 }
 if(s==DELETE_TREE){entity_delete_children(p,e);entity_delete(p,e);return;}
 if(s==GRENADE){
  int r=step(w,a,WMOVE_FALL);if(r<0)return;if(r)enemy_schedule(q,BLAST_GROUND);
  a->palette=enemy_grenade_palettes[w->frame_phase&3];enemy_animate(w,e);
  if(a->hit.flags&2)enemy_schedule(q,BLAST_SILENT);
  if(a->hit.flags&8)enemy_schedule(q,BLAST_HIT);
  if(interaction_outside(w,a,enemy_projectile_bounds,1))enemy_schedule(q,DELETE_TREE);return;
 }
 if(s==BLAST){
  if(interaction_move(w,a,WMOVE_SYNC)<0)return;
  if(enemy_animate(w,e)){entity_delete_children(p,e);entity_delete(p,e);return;}
  enemy_attack_binding(q,65535);hit_begin_attack(&a->hit);return;
 }
 if(s==BULLET||s==IMPACT){
  if(s==BULLET){if(step(w,a,WMOVE_FALL)<0)return;enemy_animate(w,e);if(!enemy_attack(w,a))return;}
  else if(enemy_animate(w,e)){entity_delete(p,e);return;}
  if((uint16_t)word(sx(w,a)+16)>=352||(uint16_t)word(sy(w,a)-240)>=288)entity_delete(p,e);return;
 }
 if(s==DEATH_GROUND||s==DEATH_AIR||s==STAGGER||s==FADE){
  if(s!=FADE)a->hit.invulnerable=2;
  if(!physics(w,q))return;done=enemy_animate(w,e);
  if(s==DEATH_GROUND&&done)enemy_schedule(q,FADE);
  if(s==DEATH_AIR&&!q->allen.airborne)enemy_schedule(q,DEATH_LAND);
  if(s==STAGGER&&done){a->damage_timer=a->hit.invulnerable=15;a->hit.flags&=247;enemy_schedule(q,MAIN);}
  if(s==FADE&&!a->damage_timer){entity_delete(p,e);return;}tail(w,e,0);return;
 }
 if(s==SPAWN||s==INTRO)a->hit.invulnerable=30;
 acquire(w,q);if(!physics(w,q))return;done=enemy_animate(w,e);if(w->fault)return;
 switch(s){
 case SPAWN:if((uint16_t)word(sx(w,a)-48)<224)enemy_schedule(q,INTRO);break;
 case INTRO:if(!--a->timer){enemy_defense(q,ENB_ALLEN_BODY);enemy_schedule(q,MAIN);}break;
 case MAIN:
  if(!target(w,q))enemy_schedule(q,IDLE);
  if(!q->allen.in_front)enemy_schedule(q,TURN);
  if(above(w,q))enemy_schedule(q,FIRE_DIAGONAL);
  if(front(w,q))enemy_schedule(q,FIRE_LEVEL);
  if(knife_probe(w,q))enemy_schedule(q,KNIFE);
  if(far_below(w,q))enemy_schedule(q,TAUNT);
  if(a->vx<0?sx(w,a)<48:sx(w,a)>=272)enemy_schedule(q,REVERSE);
  if(jump_check(w,q,0))enemy_schedule(q,JUMP_TOWARD);
  if(jump_check(w,q,1))enemy_schedule(q,JUMP_HIGH);
  if(!a->hit.invulnerable&&q->allen.sensors[q->allen.retreat?0:1])enemy_schedule(q,REVERSE);
  if(q->allen.airborne)enemy_schedule(q,JUMP_DECIDE);break;
 case TURN:if(done){a->transform.facing^=1;enemy_schedule(q,MAIN);}break;
 case FIRE_LEVEL:case FIRE_DIAGONAL:if(done){q->allen.cooldown=(uint8_t)rank(w,q,1);enemy_schedule(q,MAIN);}break;
 case IDLE:if(target(w,q))enemy_schedule(q,MAIN);break;
 case AIR:if(!q->allen.airborne)enemy_schedule(q,LAND);if(knife_probe(w,q))enemy_schedule(q,KNIFE);break;
 case KNIFE:if(done)enemy_schedule(q,MAIN);if(!q->allen.airborne)a->vx=0;break;
 case TAUNT:case LAND:if(done)enemy_schedule(q,MAIN);break;
 default:enemy_fault(w);return;
 }if(!w->fault)tail(w,e,1);
}
int allen_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,const struct PlayerTransform*t,const uint8_t params[8]){
 struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;struct EnemyActor*q=&e->payload.enemy;
 q->allen=(__typeof__(q->allen)){.target=-1,.airborne=255,.ground_seen=255,.retreat=255,.sensors={255,255,255,255}};
 if(params)for(unsigned i=0;i<8;i++)q->params[i]=params[i];enemy_schedule(q,INIT);return 1;
}
