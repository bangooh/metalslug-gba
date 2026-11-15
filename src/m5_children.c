#include "m5_internal.h"
static int move(struct WeaponWorld*w,struct WeaponActor*a,enum WeaponMove m){return interaction_move(w,a,m);}
static void outside(struct WeaponWorld*w,struct EnemyActor*q,const int16_t*b,int force,unsigned state){if(interaction_outside(w,&q->actor,b,force))enemy_schedule(q,state);}
static void parent_end(struct WeaponWorld*w,struct NativeEntity*e,int tree){if(entity_parent_deleting(w->entities,e))enemy_schedule(&e->payload.enemy,tree?M5_DELETE_TREE:M5_DELETE);}
static void part_follow(struct EnemyActor*q,const struct EnemyActor*p){m5_follow(q,p,1,1,0,44,0);if(p->actor.hit.events&128)q->actor.hit.events|=1;}
static void crew_follow(struct EnemyActor*q,const struct EnemyActor*p,int grenade){m5_follow(q,p,1,grenade,grenade?-16:-35,(grenade?53:46)+p->m5.offset_y,2);}
static int16_t absolute(int16_t n){return n<0?m5_word(-n):n;}
int m5_emit(struct WeaponWorld*w,struct NativeEntity*e,unsigned id){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*n=0;
 switch(id){
 case M5C_283CA:hit_begin_attack(&a->hit);return 1;
 case M5C_4A110:a->palette=0x15c;return 1;
 case M5C_4A120:a->palette=0xe;return 1;
 case M5C_55BCE:return enemy_spawn_ordnance(w,entity_ref(e),&a->transform,ENEMY_GRENADE,a->variant,q->source_flags);
 case M5C_71038:return q->selector<11?1:enemy_fault(w);
 case M5C_70E3C:case M5C_70E78:
  q->m5.crew=0;n=m5_child(w,e,id==M5C_70E3C?M5_SOLDIER:M5_GRENADIER,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.actor.display_flags|=2;
  if(id==M5C_70E78){n->payload.enemy.m5.gate=q->m5.gate;n->payload.enemy.actor.hit.hp=a->hit.hp;}return 1;
 case M5C_70EBE:
  n=m5_turret_spawn(w,e);if(!n)return 0;a->attached_offset=(int16_t)n->index;a->trail_angle=n->generation;return 1;
 case M5C_70ED6:
  /* $070ED6 releases the earlier attached cannon. It may already have
   * retired after leaving the screen or being destroyed. A generation-
   * checked missing reference is an already completed release, not a
   * fatal event; never write into a recycled slot. */
  n=entity_get(w->entities,(struct EntityRef){(uint16_t)a->attached_offset,a->trail_angle});if(n)m5_turret_release(n);return 1;
 case M5C_70EE0:case M5C_70EF2:return !!m5_child(w,e,id==M5C_70EE0?M5_FX_8770:M5_FX_87AE,ENTITY_FIRST_CHILD);
 case M5C_70F04:n=m5_child(w,e,M5_SMOKE,ENTITY_FIRST_CHILD);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,-44,-56);return 1;
 case M5C_70F22:
  n=m5_child(w,e,M5_BOMB,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.m5.upgraded=q->m5.upgraded;n->payload.enemy.m5.stage=q->m5.stage;n->payload.enemy.m5.side=q->m5.side;n->payload.enemy.m5.choice=q->m5.choice;return 1;
 case M5C_70F4C:case M5C_70F70:case M5C_70F9A:case M5C_70FBE:{
  int second=id==M5C_70F70||id==M5C_70FBE;n=m5_child(w,e,id==M5C_70F4C||id==M5C_70F70?M5_MUZZLE:M5_MUZZLE_END,ENTITY_FIRST_CHILD);if(!n)return 0;
  n->payload.enemy.m5.offset_x=second?-33:-41;n->payload.enemy.m5.offset_y=-15;n->payload.enemy.m5.priority=1;if(second)n->payload.enemy.actor.transform.facing|=1;return 1;
 }
 case M5C_70FE8:return m5_child(w,e,M5_SPARK_BURST,ENTITY_FIRST_CHILD)&&m5_child(w,e,M5_FX_A,ENTITY_FIRST_CHILD)&&m5_child(w,e,M5_FX_B,ENTITY_FIRST_CHILD);
 case M5C_420E4:{
  if(!m5_sound(w,0x1064))return 0;unsigned sector=q->selector>>1;if(sector>=6)sector=5;
  n=m5_child(w,e,M5_SHOT_FLASH,ENTITY_FIRST_CHILD);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,m5_shot_offsets[sector][0],m5_shot_offsets[sector][1]);
  n=m5_child(w,e,M5_SHOT,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.selector=(uint16_t)sector;
  transform_integer_offset(&n->payload.enemy.actor.transform,m5_shot_offsets[sector][0],m5_shot_offsets[sector][1]);return 1;
 }
 case M5C_42366:
  n=m5_child(w,e,M5_EJECT_PIECE,ENTITY_FIRST_CHILD);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,-56,20);n->payload.enemy.actor.vx=a->vx;n->payload.enemy.actor.ay=a->ay;return 1;
 default:return enemy_fault(w);
 }
}
int m5_children_enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned state=a->state;struct EnemyActor*p;
 switch(state){
 case M5_PART:a->palette=0x59;a->hit_palette=0x7f;if(q->m5.stage==255){q->m5.stage=0;q->m5.upgraded=0;}m5_start_entry(q,m5_part[(q->params[0]&3)*4+(q->m5.stage&3)]);break;
 case M5_ROTOR:a->palette=0x59;a->hit_palette=0x7f;q->m5.timer=60;m5_start(q,M5A_ROTOR);break;
 case M5_ROTOR_A:case M5_ROTOR_B:m5_start(q,M5A_ROTOR_FAST);break;
 case M5_BOMB:
  if(!m5_sound(w,0x10a3))return 0;a->hit.type_flags|=16;a->hit.hp=m5_rank_value(w,q,M5R_BOMB_HP);q->m5.segment=(int16_t)((uint16_t)a->hit.hp>>2);q->m5.threshold=m5_word(a->hit.hp-q->m5.segment);
  a->palette=0x5a;a->hit_palette=0x7f;q->m5.offset_x=q->m5.offset_y=0;q->m5.flags=0;m5_start_entry(q,m5_bomb[(q->m5.side&1)+(q->m5.upgraded?2:0)]);break;
 case M5_BOMB_FALL:
  m5_reset(a);q->m5.speed=m5_rank_value(w,q,M5R_BOMB_SPEED);a->vx=q->m5.path.angle>>8?m5_word(-q->m5.speed):q->m5.speed;
  q->m5.path.cursor=m5_paths[(q->m5.choice&3)+((q->m5.path.angle>>8)?4:0)];q->m5.path.distance&=255;m5_start(q,M5A_PROJECTILE);break;
 case M5_BOMB_BOUNCE:m5_reset(a);a->ay=-96;break;
 case M5_MISSILE:break;
 case M5_HOME:a->ay=-128;break;
 case M5_SHELL:q->m5.target_y=(m5_random(w)&32)+16;a->vx=q->m5.path.angle>>8?-128:128;q->selector=q->m5.path.angle>>11;a->vy=-1536;m5_start(q,M5A_PROJECTILE);break;
 case M5_SHELL_FALL:a->vy=a->ay=0;a->vx=m5_word(a->vx*16);break;
 case M5_PROJ_EXPLODE:
  interaction_priority(w,a,0x4000,0);if(!m5_sound(w,0x1021)||!boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMOKE))return 0;entity_delete(w->entities,e);return 2;
 case M5_FX_A:case M5_FX_B:a->palette=4;m5_start(q,M5A_FX_IDLE);break;
 case M5_SPARK_BURST:q->params[1]=m5_random(w)&7;q->m5.timer=0;break;
 case M5_SPARK:{unsigned sector=(q->selector&31)*8;a->vx=m5_word(-boss_trig[1][sector]*4);a->vy=m5_word(-boss_trig[0][sector]*4);transform_integer_offset(&a->transform,0,-8);a->palette=m5_palettes[1][q->params[1]&7];q->m5.timer=8;m5_start(q,M5A_SPARK);break;}
 case M5_SPARK_END:break;
 case M5_MUZZLE:if(!m5_sound(w,0x10a4))return 0;/* FALLTHROUGH */
 case M5_MUZZLE_END:a->palette=4;m5_start(q,state==M5_MUZZLE?M5A_MUZZLE:M5A_MUZZLE_END);break;
 case M5_SOLDIER:q->m5.target=(uint8_t)m5_random_target(w);a->palette=0x15c;a->state=M5_SOLDIER_WALK;/* FALLTHROUGH */
 case M5_SOLDIER_WALK:q->m5.timer=m5_rank_value(w,q,M5R_CREW_WAIT);m5_start(q,M5A_SOLDIER);break;
 case M5_SOLDIER_ACQUIRE:{
  q->m5.target=(uint8_t)m5_random_target(w);int16_t dx=m5_word(m5_sx(w,a)-m5_player_x(w,q->m5.target));
  if((a->transform.facing&1)?dx>0:dx<0){enemy_schedule(q,M5_SOLDIER_TURN);q->entry=0;return m5_children_enter(w,e);}
  a->state=M5_SOLDIER_AIM;
 }/* FALLTHROUGH */
 case M5_SOLDIER_AIM:{int16_t d=absolute(m5_word(m5_player_x(w,q->m5.target)-m5_sx(w,a)));a->variant=d<32?12:d<80?13:d<128?14:15;m5_start(q,M5A_THROW);break;}
 case M5_SOLDIER_TURN:a->transform.facing^=1;m5_start(q,M5A_TURN);break;
 case M5_SOLDIER_WAIT:m5_start(q,M5A_SOLDIER);break;
 case M5_CREW_ENTER:a->palette=0x15c;m5_start(q,M5A_CREW_ENTER);break;
 case M5_CREW_RAISE:m5_start(q,M5A_CREW_RAISE);break;
 case M5_GRENADIER:a->palette=0x98;q->selector=2;a->state=M5_GRENADE_ACQUIRE;/* FALLTHROUGH */
 case M5_GRENADE_ACQUIRE:q->m5.target=(uint8_t)m5_random_target(w);q->m5.timer=m5_rank_value(w,q,M5R_GRENADE_WAIT);a->state=M5_GRENADE_IDLE;/* FALLTHROUGH */
 case M5_GRENADE_IDLE:m5_start(q,M5A_GRENADIER);break;
 case M5_GRENADE_COUNT:q->m5.count=(int8_t)m5_rank_value(w,q,M5R_GRENADE_COUNT);a->state=M5_GRENADE_THROW;/* FALLTHROUGH */
 case M5_GRENADE_THROW:q->m5.target=(uint8_t)m5_random_target(w);q->m5.timer=m5_rank_value(w,q,M5R_GRENADE_REPEAT);if((q->selector>>1)>=6)return enemy_fault(w);m5_start_entry(q,m5_throw[q->selector>>1]);break;
 case M5_GRENADE_HURT:q->m5.timer=m5_word(q->m5.timer-30);m5_start(q,M5A_HURT);break;
 case M5_EJECT:
  q->dead=0;interaction_priority(w,a,0xd000,24);m5_defense(q,65535);a->vx=-2048;a->ax=64;a->vy=512;a->ay=-64;a->palette=0x98;a->display_flags|=64;
  if(!gunship_arc(w,entity_ref(e),&a->transform))return 0;m5_start(q,M5A_EJECT);break;
 case M5_EJECT_BOUNCE:m5_reset(a);a->vy=540;a->ay=-36;m5_start(q,M5A_BOUNCE);break;
 case M5_EJECT_SETTLED:break;
 case M5_EJECT_REST:w->enemies->scripted_player_lock=0;m5_reset(a);break;
 case M5_FIRE:if(!m5_sound(w,0x1022))return 0;/* FALLTHROUGH */
 case M5_PIECE_FIRE:m5_defense(q,65535);interaction_priority(w,a,0x4000,0);boss_fx_transform(w,e,BFX_FIRE);return 2;
 case M5_FX_8770:case M5_FX_87AE:case M5_WIND:
  a->palette=state==M5_WIND?2:4;p=m5_parent(w,e);if(!p)return 0;m5_follow(q,p,1,1,0,state==M5_WIND?0:44,0);
  m5_start(q,state==M5_FX_8770?M5A_FX_8770:state==M5_FX_87AE?M5A_FX_87AE:M5A_WIND);break;
 case M5_SMOKE:case M5_SMOKE_SYNC:a->palette=4;m5_start(q,state==M5_SMOKE?M5A_SMOKE:M5A_SMOKE_SYNC);break;
 case M5_ROCKET:
  a->transform.facing^=1;a->vx=-512;a->hit.hp=30;a->palette=0x173;a->hit.type_flags|=16;interaction_priority(w,a,0xd000,20);m5_start(q,M5A_ROCKET);a->ay=-64;break;
 case M5_ROCKET_JITTER:a->vx=m5_word(-((m5_random(w)&0x700)-0x280));a->vy=m5_word((m5_random(w)&0x700)+0x400);a->ay=-64;break;
 case M5_ROCKET_GROUND:a->vy=a->ay=0;break;
 case M5_SHOT_FLASH:interaction_priority(w,a,0xd000,20);a->palette=0xb3;m5_start(q,M5A_SHOT_FLASH);break;
 case M5_SHOT:
  if(q->selector>=6)return enemy_fault(w);a->hit.type_flags|=16;q->m5.speed=m5_rank_value(w,q,M5R_SHOT_SPEED);weapon_vector((uint8_t)m5_shot_angles[q->selector],q->m5.speed,&a->vx,&a->vy);
  q->attack_set=(struct HitAttackSet){m5_bindings[M5_SHOT_ATTACK].attack,m5_bindings[M5_SHOT_ATTACK].count};a->attack=&q->attack_set;m5_defense(q,M5_SHOT_DEFENSE);a->hit.hp=50;a->palette=0xb4;hit_begin_attack(&a->hit);m5_start_entry(q,m5_shot[q->selector]);interaction_priority(w,a,0xd000,20);break;
 case M5_SHOT_HIT:if(!m5_sound(w,0x1022))return 0;/* FALLTHROUGH */
 case M5_SHOT_SMOKE:m5_defense(q,65535);interaction_priority(w,a,0x4000,0);boss_fx_transform(w,e,BFX_SMOKE);return 2;
 case M5_EJECT_PIECE:a->palette=0x98;m5_start(q,M5A_EJECT_PIECE);break;
 default:return enemy_fault(w);
 }return !w->fault;
}
struct PathContext {struct WeaponWorld*w;struct NativeEntity*e;};
static int path_field(void*ctx,unsigned field,unsigned width,uint32_t v){
 struct PathContext*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;
 if(field==M5PF_89_1&&width==1)q->m5.flags=(uint8_t)v;
 else if(field==M5PF_0_4&&width==4&&v<2)enemy_schedule(q,v?M5_BOMB_BOUNCE:M5_MISSILE);
 else return enemy_fault(c->w);return 1;
}
static void projectile_tail(struct WeaponWorld*w,struct EnemyActor*q){
 struct WeaponActor*a=&q->actor;if(!enemy_attack(w,a))return;if(a->hit.flags&2)enemy_schedule(q,M5_PROJ_EXPLODE);
 if(a->hit.flags&8){a->hit.flags&=247;if((uint16_t)q->m5.threshold>=(uint16_t)a->hit.hp){q->m5.threshold=m5_word(q->m5.threshold-q->m5.segment);q->m5.stage++;a->palette=m5_palettes[0][q->m5.stage&7];}}
 if(a->hit.flags&1)enemy_schedule(q,M5_PROJ_EXPLODE);outside(w,q,m5_bounds[0],1,M5_DELETE);
}
static void soldier_tail(struct WeaponWorld*w,struct NativeEntity*e,struct EnemyActor*p){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(a->hit.flags&8){p->m5.crew=1;interaction_priority(w,a,0x8000,28);p->m5.count=(int8_t)(p->m5.count-1);human_death_hit_tail(w,e);return;}
 if(p->m5.crew==2)enemy_schedule(q,M5_DELETE);parent_end(w,e,0);
}
static void grenadier_tail(struct EnemyActor*q,struct EnemyActor*p,unsigned state){
 if(p->m5.crew==2)enemy_schedule(q,M5_DELETE);
 if(state!=M5_GRENADE_HURT&&(q->actor.hit.flags&8)){q->actor.hit.flags&=254;q->actor.hit.hp=1;if(state==M5_GRENADE_THROW)q->actor.hit.flags&=247;else enemy_schedule(q,M5_GRENADE_HURT);}
 if(p->actor.hit.flags&1)enemy_schedule(q,M5_EJECT);
}
void m5_children_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;int done,r;
 /* These source routines operate in screen space: explicit BSET #6
  * movement, authored flight, or animation without a scrolling move. */
 switch(state){
 case M5_BOMB_FALL:case M5_BOMB_BOUNCE:case M5_HOME:case M5_SHELL:
 case M5_MISSILE:case M5_SHELL_FALL:case M5_EJECT:
 case M5_ROCKET:case M5_ROCKET_JITTER:case M5_ROCKET_GROUND:case M5_SHOT:
 case M5_SPARK_END:case M5_FX_8770:case M5_FX_87AE:case M5_WIND:case M5_SMOKE:
  if(!scene_preserve_screen(w,a))return;break;
 default:break;
 }
 switch(state){
 case M5_PART:
  p=m5_parent(w,e);if(!p)return;part_follow(q,p);m5_animate(w,e);q->m5.upgraded=p->m5.upgraded;if(p->m5.stage!=q->m5.stage){q->m5.stage=p->m5.stage;enemy_schedule(q,M5_PART);}parent_end(w,e,1);return;
 case M5_ROTOR:case M5_ROTOR_A:case M5_ROTOR_B:
  p=m5_parent(w,e);if(!p)return;part_follow(q,p);done=m5_animate(w,e);
  if(done){if(state==M5_ROTOR_A)enemy_schedule(q,M5_ROTOR_A);else m5_start(q,state==M5_ROTOR?M5A_ROTOR:M5A_ROTOR_FAST);}
  if(p->m5.rotor==1&&state!=M5_ROTOR)enemy_schedule(q,M5_ROTOR);
  if(p->m5.rotor==2&&state!=M5_ROTOR_A)enemy_schedule(q,M5_ROTOR_A);if(p->m5.rotor==3&&state!=M5_ROTOR_B)enemy_schedule(q,M5_ROTOR_B);
  q->m5.timer=m5_word(q->m5.timer-(state==M5_ROTOR_A?3:state==M5_ROTOR_B?7:0));
  if(p->m5.rotor){q->m5.timer=m5_word(q->m5.timer-1);if(q->m5.timer<=0){if(!m5_child(w,e,M5_WIND,ENTITY_FIRST_CHILD))return;q->m5.timer=60;}}parent_end(w,e,1);return;
 case M5_BOMB:
  p=m5_parent(w,e);if(!p)return;m5_follow(q,p,0,0,q->m5.offset_x,44+q->m5.offset_y,-2);
  if(m5_animate(w,e)){enemy_schedule(q,q->m5.choice<4?M5_BOMB_FALL:M5_SHELL);interaction_priority(w,a,0x8000,28);}return;
 case M5_BOMB_FALL:{struct PathContext c={w,e};struct FlightOwner o={.context=&c,.field=path_field};
  if(flight_step(&a->transform,&q->m5.path,q->m5.speed,&m5_flight_program,&o)<0){enemy_fault(w);return;}q->selector=q->m5.path.angle>>11;m5_animate(w,e);projectile_tail(w,q);return;}
 case M5_BOMB_BOUNCE:case M5_HOME:case M5_SHELL:
  a->hit.flags|=64;r=move(w,a,WMOVE_FALL);if(r<0)return;if(r)enemy_schedule(q,state==M5_SHELL?M5_SHELL_FALL:M5_PROJ_EXPLODE);
  else if(state==M5_HOME)q->selector=enemy_angle(a->vx>>4,a->vy>>4)>>3;
  m5_animate(w,e);if(m5_sy(w,a)<=336)enemy_schedule(q,state==M5_SHELL?M5_SHELL_FALL:M5_PROJ_EXPLODE);projectile_tail(w,q);return;
 case M5_MISSILE:case M5_SHELL_FALL:
  a->hit.flags|=64;move(w,a,WMOVE_POINT);m5_animate(w,e);
  if(state==M5_MISSILE){int16_t d=m5_word(m5_sx(w,a)-m5_player_x(w,(unsigned)m5_random_target(w)));if(d>=-64&&d<=64)enemy_schedule(q,M5_HOME);}projectile_tail(w,q);return;
 case M5_FX_A:case M5_FX_B:
  p=m5_parent(w,e);if(!p)return;m5_follow(q,p,1,1,0,0,0);q->selector=p->selector;q->m5.flags=p->m5.flags;
  if(m5_animate(w,e)&&(q->m5.flags&(state==M5_FX_A?1:2)))m5_start(q,state==M5_FX_A?M5A_FX_A:M5A_FX_B);parent_end(w,e,1);return;
 case M5_SPARK_BURST:
  p=m5_parent(w,e);if(!p)return;if(!p->actor.phase){q->m5.timer=m5_word(q->m5.timer+1);if(!(q->m5.timer&3)){
   m5_follow(q,p,1,1,0,0,0);q->selector=p->selector;const int16_t*d=m5_spark_offsets[q->selector&31];transform_integer_offset(&a->transform,d[0],m5_word(d[1]+4));
   struct NativeEntity*n=m5_child(w,e,M5_SPARK,ENTITY_LAST_CHILD);if(!n)return;n->payload.enemy.params[1]=q->params[1];n->payload.enemy.selector=q->selector;}}
  parent_end(w,e,0);if(p->actor.state==M5_BOMB_BOUNCE&&p->entry)enemy_schedule(q,M5_DELETE);return;
 case M5_SPARK:move(w,a,WMOVE_POINT);m5_animate(w,e);q->m5.timer=m5_word(q->m5.timer-1);if(q->m5.timer<=0)enemy_schedule(q,M5_SPARK_END);return;
 case M5_SPARK_END:if(m5_animate(w,e))enemy_schedule(q,M5_DELETE_TREE);return;
 case M5_MUZZLE:case M5_MUZZLE_END:
  p=m5_parent(w,e);if(!p)return;m5_follow(q,p,0,0,0,0,q->m5.priority);if(m5_animate(w,e))enemy_schedule(q,M5_DELETE_TREE);return;
 case M5_SOLDIER_WALK:case M5_SOLDIER_TURN:case M5_SOLDIER_AIM:case M5_SOLDIER_WAIT:case M5_CREW_ENTER:case M5_CREW_RAISE:
  p=m5_parent(w,e);if(!p)return;crew_follow(q,p,0);done=m5_animate(w,e);
  if(state==M5_SOLDIER_WALK){q->m5.timer=m5_word(q->m5.timer-1);if(q->m5.timer<=0)enemy_schedule(q,M5_SOLDIER_ACQUIRE);}
  else if(done){if(state==M5_SOLDIER_TURN)enemy_schedule(q,M5_SOLDIER_AIM);else if(state==M5_SOLDIER_AIM){p->m5.count=(int8_t)(p->m5.count-1);enemy_schedule(q,p->m5.count<=0?M5_SOLDIER_WAIT:M5_SOLDIER_WALK);}else if(state==M5_CREW_ENTER)enemy_schedule(q,M5_CREW_RAISE);else if(state==M5_CREW_RAISE)enemy_schedule(q,M5_SOLDIER_ACQUIRE);}
  soldier_tail(w,e,p);return;
 case M5_GRENADE_IDLE:case M5_GRENADE_THROW:case M5_GRENADE_HURT:
  p=m5_parent(w,e);if(!p)return;crew_follow(q,p,1);done=m5_animate(w,e);
  if(state==M5_GRENADE_IDLE){
   if(!q->m5.gate){q->m5.timer=m5_word(q->m5.timer-1);if(q->m5.timer<=0)enemy_schedule(q,q->m5.count>0?M5_GRENADE_COUNT:M5_GRENADE_THROW);}
   int angle=enemy_angle(m5_word(m5_player_x(w,q->m5.target)-m5_sx(w,a)+32),m5_word(m5_player_y(w,q->m5.target)-m5_sy(w,a)));
   if(angle<=112)angle=112;if(angle>=192)angle=192;angle=(angle-104)>>3;if(angle>=10)angle=10;angle&=254;
   if((uint16_t)angle!=q->selector)q->selector=(uint16_t)(q->selector+((uint16_t)angle<q->selector?-1:1));
  }else if(done){
   if(state==M5_GRENADE_HURT){a->hit.flags&=247;enemy_schedule(q,M5_GRENADE_IDLE);}
   else{q->m5.count=(int8_t)(q->m5.count-1);enemy_schedule(q,q->m5.count<=0?M5_GRENADE_ACQUIRE:M5_GRENADE_IDLE);if(q->m5.count<=0)p->m5.count=0;}}
  grenadier_tail(q,p,state);return;
 case M5_EJECT:
  a->hit.flags|=64;if(move(w,a,WMOVE_FALL)>0)enemy_schedule(q,M5_EJECT_BOUNCE);m5_animate(w,e);if(a->vx>=-128)a->ax=0;if(q->dead)enemy_schedule(q,M5_EJECT_REST);return;
 case M5_EJECT_BOUNCE:
  if(move(w,a,WMOVE_FALL)>0){w->enemies->scripted_player_lock=0;m5_start(q,M5A_SETTLED);a->state=M5_EJECT_SETTLED;move(w,a,WMOVE_SYNC);}m5_animate(w,e);if(q->dead)enemy_schedule(q,M5_EJECT_REST);return;
 case M5_EJECT_SETTLED:move(w,a,WMOVE_SYNC);m5_animate(w,e);if(q->dead)enemy_schedule(q,M5_EJECT_REST);return;
 case M5_EJECT_REST:move(w,a,WMOVE_SYNC);if(q->dead!=1)enemy_schedule(q,M5_DELETE);return;
 case M5_FX_8770:case M5_FX_87AE:case M5_WIND:case M5_SMOKE:case M5_SMOKE_SYNC:
  if(state==M5_SMOKE_SYNC)move(w,a,WMOVE_SYNC);if(m5_animate(w,e))enemy_schedule(q,M5_DELETE_TREE);return;
 case M5_ROCKET:case M5_ROCKET_JITTER:case M5_ROCKET_GROUND:
  a->hit.flags|=64;
  if(state==M5_ROCKET_GROUND){r=terrain_weapon_foot_contact(w->interactions->terrain,a);if(r<0){enemy_fault(w);return;}move(w,a,r?WMOVE_ITEM:WMOVE_GROUND);}
  else if(move(w,a,WMOVE_BODY)>0)enemy_schedule(q,state==M5_ROCKET?M5_ROCKET_JITTER:M5_FIRE);
  a->palette=w->frame_phase&2?0x1de:0x173;m5_animate(w,e);if(!enemy_attack(w,a))return;if(a->hit.flags&2)enemy_schedule(q,M5_FIRE);a->hit.flags&=247;if(a->hit.flags&1)enemy_schedule(q,M5_FIRE);outside(w,q,boss_fx_bounds[8],0,M5_DELETE_TREE);return;
 case M5_SHOT_FLASH:move(w,a,WMOVE_SYNC);if(m5_animate(w,e))enemy_schedule(q,M5_DELETE);outside(w,q,boss_fx_bounds[8],1,M5_DELETE);return;
 case M5_SHOT:
  a->hit.flags|=64;if(move(w,a,WMOVE_BODY)>0)enemy_schedule(q,M5_SHOT_HIT);m5_animate(w,e);if(!enemy_attack(w,a))return;if(a->hit.flags&2)enemy_schedule(q,M5_SHOT_SMOKE);a->hit.flags&=247;if(a->hit.flags&1)enemy_schedule(q,M5_SHOT_HIT);outside(w,q,boss_fx_bounds[8],1,M5_DELETE);return;
 case M5_EJECT_PIECE:
  if(move(w,a,WMOVE_FALL)>0)enemy_schedule(q,M5_PIECE_FIRE);if(m5_animate(w,e))enemy_schedule(q,M5_PIECE_FIRE);outside(w,q,boss_fx_bounds[8],1,M5_DELETE);return;
 default:enemy_fault(w);return;
 }
}
