#include "bosses.h"
/* $077C7E emitters, common explosion/debris lifetimes, $0628EA's metal
 * fragments and $07C996's debris. No allocations are replaced by draw-only FX. */
enum State {NEW,EFFECT,DEBRIS,METAL,M2_DEBRIS,PATTERN_INIT,PATTERN,DAMAGE_SMOKE1,DAMAGE_SMOKE2,DAMAGE_SMOKE3,PUFF,SPARK,DELETE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static int cosmetic(enum BossFxKind kind){
 /* These are visual explosion children; damage, score, terrain destruction
  * and child lifetimes still run. Keep every draw normally, permitting
  * omission only on the final hardware-capacity retry. */
 if(kind==BFX_SMALL||kind==BFX_FIRE||kind==BFX_BLAST||kind==BFX_BLAST_B||kind==BFX_FIRE_BLUE||kind==BFX_FLASH)
  return ENTITY_COSMETIC_PRESSURE;
 return kind==BFX_SMOKE||kind==BFX_M2_DEBRIS||(kind>=BFX_DEBRIS0&&kind<=BFX_METAL)||
  (kind>=BFX_SURFACE&&kind<=BFX_WRECK2)||kind==BFX_PUFF_SMOKE||kind==BFX_METAL_HEAVY||
  kind==BFX_CRASH_FRAGMENT||kind==BFX_SMOKE_BLUE||(kind>=BFX_DEBRIS_AD_A&&kind<=BFX_CRASH_FRAGMENT_B)||kind==BFX_DEBRIS28;
}
static int16_t word(int n){return (int16_t)(uint16_t)n;}
static uint16_t random_word(struct WeaponWorld*w){if(!w->rng){enemy_fault(w);return 0;}return source_random_word(w->rng);}
static uint16_t bounded(struct WeaponWorld*w,uint16_t n){if(!w->rng){enemy_fault(w);return 0;}return source_random(w->rng,n);}
static int outside(struct WeaponWorld*w,struct EnemyActor*q){return interaction_outside(w,&q->actor,boss_fx_bounds[q->fx.bounds],1);}
static void metal_velocity(struct WeaponWorld*w,struct EnemyActor*q,unsigned base){
 struct WeaponActor*a=&q->actor;uint16_t r=random_word(w);unsigned angle=(r&62)+base;
 int16_t y=word(boss_trig[0][angle]*((r&7)+1));y>>=2;
 a->vy=word(((int32_t)y*q->scale)>>8);a->vx=word(((int32_t)boss_trig[1][angle]*q->scale)>>8);
 if((int8_t)random_word(w)<=0){a->vx=word(a->vx*((random_word(w)&3)+2))>>1;a->vy=word(a->vy*((random_word(w)&3)+2))>>1;}
 q->fx.launch_y=a->vy;a->ay=-48;
}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;q->entry=0;
 if(a->state==PATTERN_INIT){
  if(q->fx.pattern>=BP_COUNT)return enemy_fault(w);const struct BossPatternData*t=&boss_patterns[q->fx.pattern];
  q->fx.delay=t->period;q->fx.remaining=t->life;a->state=PATTERN;return 1;
 }
 if(a->state==DAMAGE_SMOKE2){a->hit.hp=(int16_t)((uint16_t)a->hit.hp>>1);return 1;}
 if(a->state!=NEW)return 1;
 enum BossFxKind kind=(enum BossFxKind)q->family;
 if(kind==BFX_RANDOM_DEBRIS){enemy_fault(w);return 0;} /* resolved by emitter before allocation */
 if((kind>=BFX_DEBRIS0&&kind<=BFX_DEBRIS10)||(kind>=BFX_DEBRIS_AD_A&&kind<=BFX_DEBRIS77_B)||kind==BFX_DEBRIS28){
  unsigned index=kind>=BFX_DEBRIS_AD_A?(kind==BFX_DEBRIS_AD_A||kind==BFX_DEBRIS77_A?8:10):kind-BFX_DEBRIS0;
  if(kind==BFX_DEBRIS_AD_A||kind==BFX_DEBRIS_AD_B)boss_start(q,kind==BFX_DEBRIS_AD_A?BA_FX_SIEGE_DEBRIS_A:BA_FX_SIEGE_DEBRIS_B);
  else boss_start_entry(q,boss_fx_debris_entries[index]);
  a->palette=kind==BFX_DEBRIS28?0x28:kind==BFX_DEBRIS_AD_A||kind==BFX_DEBRIS_AD_B?0xad:kind>=BFX_DEBRIS77_A?0x77:index==6||index==7?11:10;
  q->scale_x=q->scale_y=255;a->transform.facing|=random_word(w)&1;
  a->vy=2040;a->ay=-68;a->ax=0;int16_t vx=(int16_t)bounded(w,1023);if(vx&1)vx=word(-vx);a->vx=vx;
  uint16_t r=bounded(w,2047);a->vy=word(a->vy-r);a->transform.facing|=r&1;
  q->fx.bounds=(index==4||index==7||index==8)?5:4;a->state=DEBRIS;boss_defense(q,65535);
 }else if(kind==BFX_SPARK_DRIFT||kind==BFX_SPARK_SCATTER){
  if(!w->rng)return enemy_fault(w);a->palette=11;a->vx=a->vy=a->ax=a->ay=0;transform_integer_copy(&a->transform,&a->transform);a->ay=-16;
  if(kind==BFX_SPARK_SCATTER)a->vy=(int16_t)source_random_mask(w->rng,511);
  uint16_t v=source_random_mask(w->rng,kind==BFX_SPARK_SCATTER?255:15);a->vx=v&1?word(-v):(int16_t)v;
  q->fx.remaining=40;q->fx.bounds=10;boss_start(q,BA_FX_SPARK_DRIFT);a->state=SPARK;return !w->fault;
 }else if(kind==BFX_METAL||kind==BFX_METAL_HEAVY||(kind>=BFX_WRECK0&&kind<=BFX_WRECK2)){
  if(kind==BFX_METAL||kind==BFX_METAL_HEAVY){
   a->transform.priority=0xd000;uint16_t r=random_word(w);q->scale=(r&0x700)+(kind==BFX_METAL?0x100:0x200);
   boss_start(q,r&1?BA_FX_METAL_A:BA_FX_METAL_B);a->palette=r&1?0x77:0x78;
  }else{static const enum BossAnimation roots[]={BA_GS_WRECK0,BA_GS_WRECK1,BA_GS_WRECK2};boss_start(q,roots[kind-BFX_WRECK0]);a->palette=0x3b;q->scale=512;}
  interaction_priority(w,a,0x8000,20);q->fx.bounces=q->fx.initial_bounces=1;q->fx.blink=255;metal_velocity(w,q,kind==BFX_METAL?80:32);
  q->fx.bounds=6;a->state=METAL;return !w->fault;
 }else if(kind==BFX_CRASH_FRAGMENT||kind==BFX_CRASH_FRAGMENT_B){
  boss_start_entry(q,(kind==BFX_CRASH_FRAGMENT?boss_crash:boss_crash_b)[random_word(w)&15]);interaction_priority(w,a,0x4000,0);
  q->scale=(random_word(w)&0x700)+0x200;q->fx.bounces=q->fx.initial_bounces=3;q->fx.blink=255;
  a->palette=boss_crash_palettes[q->params[3]&3];q->fx.alternate_palette=1;
  metal_velocity(w,q,32);q->fx.bounds=6;a->state=METAL;return !w->fault;
 }else if(kind==BFX_M2_DEBRIS){
  boss_start_entry(q,boss_m2_debris[random_word(w)&7]);a->palette=10;
  uint16_t r=random_word(w);unsigned angle=((r&62)-32)&255;
  a->vy=word(boss_trig[0][angle]*((r&7)+1));a->vx=word(boss_trig[1][angle]*4);
  a->vx=word(a->vx*((random_word(w)&3)+2))>>1;a->vy=word(a->vy*((random_word(w)&3)+2))>>1;
  a->state=M2_DEBRIS;return !w->fault;
 }else if(kind==BFX_DAMAGE_SMOKE||kind==BFX_DAMAGE_SMOKE_BODY){
  struct NativeEntity*p=entity_get(w->entities,e->parent);struct WeaponActor*owner=entity_weapon_actor(p);if(!owner)return enemy_fault(w);
  a->hit.hp=(int16_t)((uint16_t)owner->hit.hp>>1);q->fx.delay=255;q->fx.blink=kind==BFX_DAMAGE_SMOKE;
  boss_start(q,BA_GS_SMOKE_MARK);boss_animate(w,e,0);a->state=DAMAGE_SMOKE1;return !w->fault;
 }else if(kind==BFX_PUFF_SMOKE||kind==BFX_PUFF_FIRE){
  boss_start(q,kind==BFX_PUFF_FIRE?BA_GS_FIRE:BA_GS_SMOKE);a->palette=13;a->transform.priority++;
  a->vx=a->vy=a->ax=a->ay=0;transform_integer_copy(&a->transform,&a->transform);
  a->vy=64+(random_word(w)&255);a->ay=-(a->vy>>6);q->fx.bounces=0;if(q->fx.remaining<10)q->fx.remaining=10;
  a->state=PUFF;return !w->fault;
 }else if(kind>=BFX_SURFACE&&kind<=BFX_SURFACE40){
  /* $078864 falls through $078890: five sprites, including this owner. */
  static const int8_t offsets[]={32,-32,16,-16};unsigned first=kind==BFX_SURFACE40?0:2;
  if(kind!=BFX_SURFACE)for(unsigned i=first;i<4;i++){
   struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SURFACE);if(!n)return 0;
   transform_integer_offset(&n->payload.enemy.actor.transform,offsets[i],0);
  }
  a->palette=0x139;boss_start(q,BA_FX_SURFACE);q->fx.bounds=9;q->scale_x=q->scale_y=255;
  a->transform.facing|=random_word(w)&1;a->display_flags|=64;a->state=EFFECT;boss_defense(q,65535);
 }else if(kind==BFX_TRAIL77){
  boss_start(q,BA_FX_TRAIL77);a->palette=0x145;q->fx.bounds=1;q->scale_x=q->scale_y=255;a->transform.facing|=random_word(w)&1;a->state=EFFECT;boss_defense(q,65535);
 }else if(kind==BFX_FLASH){
  boss_start(q,BA_FX_FLASH);a->palette=4;q->fx.bounds=3;q->scale_x=q->scale_y=255;a->transform.facing|=random_word(w)&1;a->state=EFFECT;boss_defense(q,65535);
 }else if(kind==BFX_BLAST_B||kind==BFX_FIRE_BLUE||kind==BFX_SMOKE_BLUE){
  boss_start(q,kind==BFX_BLAST_B?BA_FX_BLAST:kind==BFX_FIRE_BLUE?BA_FX_FIRE:BA_FX_SMOKE);a->palette=kind==BFX_BLAST_B?0x12d:0x82;q->fx.bounds=kind==BFX_BLAST_B?1:kind==BFX_FIRE_BLUE?2:3;q->scale_x=q->scale_y=255;
  a->transform.facing|=random_word(w)&1;a->state=EFFECT;boss_defense(q,65535);
 }else{
  static const enum BossAnimation anims[]={BA_FX_SMALL,BA_FX_FIRE,BA_FX_SMOKE,BA_FX_BLAST};
  static const uint8_t bounds[]={0,2,3,1};if(kind>BFX_BLAST)return enemy_fault(w);
  boss_start(q,anims[kind]);a->palette=4;q->fx.bounds=bounds[kind];q->scale_x=q->scale_y=255;
  a->transform.facing|=random_word(w)&1;a->state=EFFECT;boss_defense(q,65535);
 }
 if(outside(w,q))entity_delete(w->entities,e);return !w->fault;
}
static int pattern(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 struct NativeEntity*parent=entity_get(w->entities,e->parent);struct WeaponActor*owner=entity_weapon_actor(parent);
 if(!parent||!owner)return enemy_fault(w);
 if(parent->kind==ENTITY_ENEMY){q->scale_x=parent->payload.enemy.scale_x;q->scale_y=parent->payload.enemy.scale_y;}
 const struct BossPatternData*t=&boss_patterns[q->fx.pattern];
 int emit=q->fx.delay==255;if(!emit){q->fx.delay--;if(!q->fx.delay){q->fx.delay=t->period;emit=1;}}
 if(emit){
  if(t->follow){
   int16_t dx=owner->transform.facing&1?word(-t->dx):t->dx,dy=owner->transform.facing&1?word(-t->dy):t->dy;
   transform_integer_offset(&a->transform,word((owner->transform.x>>8)+dx-(a->transform.x>>8)),word(transform_y_up(&owner->transform)+dy-transform_y_up(&a->transform)));
  }
  for(unsigned budget=0;budget<ENTITY_CAPACITY;budget++){
   unsigned kind=t->kind;if(t->random){const uint8_t*choices=(q->fx.pattern==BP_TANK_DEBRIS||q->fx.pattern==BP_TANK_EXPLOSION)?boss_tank_fx_choices:q->fx.pattern==BP_WALKER_IMPACT?boss_walker_fx_choices:q->fx.pattern==BP_ROCKET_VEHICLE_DEBRIS?boss_rocket_vehicle_fx_choices:q->fx.pattern==BP_BAZOOKA_PUFF?boss_bazooka_fx_choices:q->fx.pattern==BP_GS_DEATH?boss_gunship_fx_choices:q->fx.pattern==BP_TURRET_CANNON?boss_turret_fx_choices:boss_fx_choices;kind=(q->fx.pattern>=BP_SCRIPT151_0&&q->fx.pattern<=BP_SCRIPT151_10)?boss_script151_choices[bounded(w,16)]:(q->fx.pattern==BP_TURRET142_HIT||q->fx.pattern==BP_TURRET142_DEATH)?boss_turret142_choices[q->fx.pattern==BP_TURRET142_DEATH][bounded(w,16)]:BFX_DEBRIS0+choices[bounded(w,16)];}
   struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_LAST_CHILD,&a->transform,(enum BossFxKind)kind);if(!n)return 0;
   int16_t dx=(int16_t)bounded(w,t->spread_x),dy=(int16_t)bounded(w,t->spread_y);
   if(a->transform.facing&1)dx=word(-dx);if(a->transform.facing&2)dy=word(-dy);
   transform_integer_offset(&n->payload.enemy.actor.transform,dx,dy);
   n->payload.enemy.actor.display_flags|=a->display_flags&64;n->payload.enemy.scale_x=q->scale_x;n->payload.enemy.scale_y=q->scale_y;
   if(q->fx.delay!=255)break;
   if(!--q->fx.remaining){entity_delete(w->entities,e);return 1;}
   if(budget==ENTITY_CAPACITY-1)return enemy_fault(w);
  }
 }
 if(!--q->fx.remaining)entity_delete(w->entities,e);return !w->fault;
}
static int smoke_puff(struct WeaponWorld*w,struct NativeEntity*e,int fire){
 struct EnemyActor*q=&e->payload.enemy;struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&q->actor.transform,fire?BFX_PUFF_FIRE:BFX_PUFF_SMOKE);if(!n)return 0;
 n->payload.enemy.fx.remaining=30;int16_t dx=(random_word(w)&63)-32,dy=(random_word(w)&31)+8;
 if(q->scale_x!=255)dx=word(((int32_t)dx*(q->scale_x+1))>>8);if(q->scale_y!=255)dy=word(((int32_t)dy*(q->scale_y+1))>>8);
 transform_integer_offset(&n->payload.enemy.actor.transform,dx,dy);n->payload.enemy.scale_x=q->scale_x;n->payload.enemy.scale_y=q->scale_y;return !w->fault;
}
static void damage_smoke(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned state=a->state;
 struct NativeEntity*p=entity_get(w->entities,e->parent);struct WeaponActor*owner=entity_weapon_actor(p);if(!owner){enemy_fault(w);return;}
 if(p->kind==ENTITY_ENEMY){q->scale_x=p->payload.enemy.scale_x;q->scale_y=p->payload.enemy.scale_y;}
 transform_integer_offset(&a->transform,word((owner->transform.x>>8)-(a->transform.x>>8)),word(transform_y_up(&owner->transform)+a->attached_offset-transform_y_up(&a->transform)));
 a->transform.priority=owner->transform.priority&0xffe3;a->hit.flags=owner->hit.flags;if(q->fx.blink)a->hit.flags=(a->hit.flags&254)|(p->deleting?1:0);
 if(state!=DAMAGE_SMOKE3&&owner->hit.hp<=a->hit.hp)enemy_schedule(q,state==DAMAGE_SMOKE1?DAMAGE_SMOKE2:DAMAGE_SMOKE3);
 if(state==DAMAGE_SMOKE2){if(!(++q->fx.delay&7)&&!smoke_puff(w,e,0))return;}
 if(state==DAMAGE_SMOKE3){q->fx.delay++;if(!(q->fx.delay&15)){if(!smoke_puff(w,e,1))return;}else if(!(q->fx.delay&3)&&!smoke_puff(w,e,0))return;}
 if(p->deleting||(a->hit.flags&1))enemy_schedule(q,DELETE);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(q->entry&&!enter(w,e))return;if(!e->active||e->deleting)return;int done=0;
 switch(a->state){
 case DELETE:entity_delete(p,e);return;
 case PATTERN:pattern(w,e);return;
 case DAMAGE_SMOKE1:case DAMAGE_SMOKE2:case DAMAGE_SMOKE3:damage_smoke(w,e);return;
 case PUFF:{
  int16_t r=(int16_t)random_word(w),delta=(uint16_t)r&1023;if(r<=0)delta=word(-delta);
  q->fx.bounces=word(q->fx.bounces+delta);a->vx=word(boss_trig[0][(uint16_t)q->fx.bounces>>8]*2);
  if(interaction_move(w,a,WMOVE_POINT)<0)return;if(boss_animate(w,e,0))enemy_schedule(q,DELETE);
  if((int16_t)--q->fx.remaining<=0||interaction_outside(w,a,boss_fx_bounds[8],1))enemy_schedule(q,DELETE);return;
 }
 case SPARK:
  if(interaction_move(w,a,WMOVE_POINT)<0)return;if(q->fx.remaining==10)a->damage_timer=20;boss_animate(w,e,0);
  if(outside(w,q)||!--q->fx.remaining)entity_delete(p,e);return;
 case EFFECT:
  /* $044048 only reconstructs camera-relative coordinates. */
  done=boss_animate(w,e,0);break;
 case DEBRIS:
  if(a->vy<=-1280)a->vy=-1279;if(interaction_move(w,a,WMOVE_POINT)<0)return;
  if(boss_sprite_admitted(w))done=boss_animate(w,e,0);break;
 case METAL:{
  int collision=interaction_move(w,a,WMOVE_FALL);if(collision<0)return;
  if(collision){a->ay=-48;a->vx>>=2;q->fx.launch_y>>=1;a->vy=q->fx.launch_y;if(--q->fx.bounces<=0)enemy_schedule(q,DELETE);}
  int animate=q->fx.initial_bounces==q->fx.bounces;
  if(!animate)animate=++q->fx.blink&1;
  if(animate&&boss_sprite_admitted(w))boss_animate(w,e,0);
  if(outside(w,q))enemy_schedule(q,DELETE);return;
 }
 case M2_DEBRIS:{
  if(interaction_move(w,a,WMOVE_FALL)<0)return;boss_animate(w,e,0);
  if(interaction_outside(w,a,boss_fx_bounds[8],1))enemy_schedule(q,DELETE);return;
 }
 default:enemy_fault(w);return;
 }if(!w->fault&&(done||outside(w,q)))entity_delete(p,e);
}
struct NativeEntity*boss_fx_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,const struct PlayerTransform*t,enum BossFxKind kind){
 if((unsigned)kind>BFX_DEBRIS28){enemy_fault(w);return 0;}
 struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;
 e->cosmetic=cosmetic(kind);e->processing|=ENTITY_PROCESS_FX;
 e->payload.enemy.family=(uint8_t)kind;e->payload.enemy.fx=(__typeof__(e->payload.enemy.fx)){0};enemy_schedule(&e->payload.enemy,NEW);return e;
}
struct NativeEntity*boss_fx_pattern(struct WeaponWorld*w,struct EntityRef parent,const struct PlayerTransform*t,enum BossPattern pattern_id){
 if((unsigned)pattern_id>=BP_COUNT){enemy_fault(w);return 0;}
 struct NativeEntity*e=enemy_allocate(w,parent,ENTITY_LAST_CHILD,t,tick);if(!e)return 0;
 e->processing|=ENTITY_PROCESS_FX;
 e->payload.enemy.fx=(__typeof__(e->payload.enemy.fx)){.pattern=(uint16_t)pattern_id};enemy_schedule(&e->payload.enemy,PATTERN_INIT);return e;
}
int boss_fx_schedule(struct WeaponWorld*w,struct NativeEntity*e,enum BossFxKind kind){
 if(e->kind!=ENTITY_ENEMY||(unsigned)kind>BFX_DEBRIS28)return enemy_fault(w);
 e->cosmetic=cosmetic(kind);e->processing|=ENTITY_PROCESS_FX;
 e->tick=tick;e->payload.enemy.family=(uint8_t)kind;e->payload.enemy.fx=(__typeof__(e->payload.enemy.fx)){0};enemy_schedule(&e->payload.enemy,NEW);return !w->fault;
}
int boss_fx_transform(struct WeaponWorld*w,struct NativeEntity*e,enum BossFxKind kind){if(!boss_fx_schedule(w,e,kind))return 0;tick(w->entities,e,w);return !w->fault;}

/* $06DCE0: the caller has selected drawing, palette and scale already. */
static int scatter_entry(struct WeaponWorld*w,struct NativeEntity*e,uint16_t entry,uint16_t palette,uint16_t scale){
 if(e->kind!=ENTITY_ENEMY)return enemy_fault(w);
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;e->tick=tick;q->entry=0;e->processing|=ENTITY_PROCESS_FX;
 q->fx=(__typeof__(q->fx)){.bounces=1,.initial_bounces=1,.blink=255,.bounds=6};q->scale=(int16_t)scale>0?scale:512;a->palette=palette;
 boss_start_entry(q,entry);interaction_priority(w,a,0x8000,20);metal_velocity(w,q,32);a->state=METAL;
 tick(w->entities,e,w);return !w->fault;
}
int boss_fx_scatter_selected(struct WeaponWorld*w,struct NativeEntity*e,unsigned drawing,uint16_t palette,uint16_t scale){if(drawing>=11)return enemy_fault(w);return scatter_entry(w,e,boss_fx_debris_entries[drawing],palette,scale);}
int boss_fx_scatter_graph(struct WeaponWorld*w,struct NativeEntity*e,enum BossAnimation animation,uint16_t palette,uint16_t scale){if((unsigned)animation>=BA_COUNT)return enemy_fault(w);return scatter_entry(w,e,boss_entries[animation],palette,scale);}
