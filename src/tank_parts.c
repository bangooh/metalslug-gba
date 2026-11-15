#include "tank_internal.h"
#include "items.h"
static int score(struct WeaponWorld*w,struct WeaponActor*a){
 if((a->display_flags&2)||(int8_t)a->score_owner<0)return 1;
 return w->items&&item_score_add(w->items,a->score_owner,enemy_hit_scores[a->hit.last_kind&31])?1:enemy_fault(w);
}
static int follow_gun(struct WeaponWorld*w,struct NativeEntity*e,int select){
 if(!tk_follow(w,e,select))return 0;struct EnemyActor*p=tk_parent(w,e);if(!p)return 0;
 struct WeaponActor*a=&e->payload.enemy.actor;transform_integer_offset(&a->transform,-32,(int8_t)p->tank.stage>=4?118:88);a->transform.priority=0xc000;return 1;
}
static int muzzle_follow(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*p=tk_parent(w,e);if(!p)return 0;struct WeaponActor*a=&e->payload.enemy.actor;
 a->transform.x=p->actor.transform.x;a->transform.y=p->actor.transform.y;a->transform.facing=p->actor.transform.facing;a->transform.priority=p->actor.transform.priority;
 transform_integer_offset(&a->transform,-48,-8);return 1;
}
static void polar(struct EnemyActor*q){unsigned angle=(q->selector&31)*8;
 q->actor.vx=tk_word(((int32_t)boss_trig[1][angle]*q->tank.speed)>>8);q->actor.vy=tk_word(((int32_t)boss_trig[0][angle]*q->tank.speed)>>8);
}
static int random_target(struct WeaponWorld*w){
 if(enemy_player_live(w->players[0]))return enemy_player_live(w->players[1])?(tk_random(w)&1):0;return 1;
}
static int path_field(void*ctx,unsigned field,unsigned width,uint32_t v){struct EnemyActor*q=ctx;
 if(field==TKPF_36_2&&width==2){q->tank.speed=(int16_t)v;return 1;}
 if(field==TKPF_0_4&&width==4&&v<2){enemy_schedule(q,v?TK_GRAVITY:TK_HOME);return 1;}return 0;
}
static int rescue(struct WeaponWorld*w,struct NativeEntity*e){
 for(unsigned i=0;i<3;i++){
  static const uint8_t gifts[3][3]={{1,200,0},{10,0,40},{6,10,0}};
  uint8_t params[8]={0,255,0,gifts[i][0],gifts[i][1],gifts[i][2],255,255};struct PlayerTransform t=e->payload.enemy.actor.transform;
  transform_integer_offset(&t,(int16_t)(-40+32*i),32);
  struct NativeEntity*n=pow_spawn_entity(w,entity_ref(e),ENTITY_FIRST_CHILD,POW_BOUND,&t,params);if(!n)return 0;
  /* Source $44 is animation freeze, decremented by the scheduler. $59 is the separate damage timer. */
  n->payload.interaction.actor.animation.freeze=(uint8_t)(4*i);
 }return 1;
}
int tk_spawn_rocket(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy,*p=tk_parent(w,e);if(!p)return 0;
 if(q->tank.pose==5||(int8_t)p->tank.stage<5||p->tank.stage==2||p->tank.stage==3)return 1;
 struct NativeEntity*n=tk_child(w,e,TK_LAUNCH,ENTITY_FIRST_CHILD);if(!n)return 0;
 n->payload.enemy.params[0]=0;n->payload.enemy.tank.serial=q->tank.serial++;return 1;
}
int tk_parts_enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;struct NativeEntity*n;unsigned state=a->state;q->entry=0;
 switch(state){
 case TK_HULL:a->hit_palette=0x8b;q->tank.table=0;a->palette=0xed;a->hit.hp=100;tk_defense(q,TKB_HULL_DEF);tk_start(q,TKA_HULL);break;
 case TK_HULL_DIE:tk_defense(q,65535);transform_integer_offset(&a->transform,0,64);boss_fx_transform(w,e,BFX_SMOKE);return 0;
 case TK_CABIN:
  a->phase=0;q->tank.mode=0;n=tk_child(w,e,TK_CREW,ENTITY_FIRST_CHILD);if(!n)return 0;
  q->tank.child_index=n->index;q->tank.child_generation=n->generation;
  n->payload.enemy.params[0]=1;n->payload.enemy.params[1]=0x20;n->payload.enemy.params[2]=0xe0;n->payload.enemy.params[3]=0x38;
  n->payload.enemy.params[5]=0;n->payload.enemy.params[6]=1;
  a->hit_palette=0x8b;q->tank.table=1;a->palette=0xed;tk_start(q,TKA_CABIN);break;
 case TK_LAUNCHER:
  q->tank.serial=0;q->tank.count=(uint8_t)tk_rank(w,q,TKR_ROCKET_COUNT);q->tank.wait=tk_rank(w,q,TKR_ROCKET_WAIT);
  a->hit_palette=0x8c;q->tank.table=2;a->palette=0xee;tk_start(q,TKA_LAUNCHER);break;
 case TK_TURRET:
  if(!tk_stamp(w,e,0))return 0;a->hit.hp=tk_rank(w,q,TKR_TURRET_HP);q->tank.contact=2;a->palette=0xec;a->hit_palette=0x8b;tk_start(q,TKA_TURRET);break;
 case TK_TURRET_FIRE:
  if(!tk_child(w,e,TK_MUZZLE,ENTITY_FIRST_CHILD))return 0;a->state=TK_AIM;/* FALLTHROUGH */
 case TK_AIM:a->phase=0;q->tank.wait=tk_rank(w,q,TKR_AIM_WAIT);tk_start(q,TKA_AIM);break;
 case TK_RECOIL:q->dead=0;q->tank.wait=tk_rank(w,q,TKR_RECOIL_WAIT);q->tank.mode=0;break;
 case TK_TURRET_DIE:if(!rescue(w,e))return 0;/* FALLTHROUGH */
 case TK_WRECK:
  p=tk_parent(w,e);if(!p)return 0;p->tank.stage=5;q->tank.contact=255;if(!tk_sound(w,0x102c))return 0;tk_defense(q,65535);tk_start(q,TKA_WRECK);
  q->tank.watch=255;enemy_schedule(q,TK_WRECK_WATCH);return 1;
 case TK_WRECK_WATCH:
  q->tank.watch=0;if(!mission_event_spawn(w,entity_ref(e),0xee1da,tk_watch_stop))return 0;a->state=TK_WRECK_IDLE;break;
 case TK_GUN:
  q->tank.wait=tk_rank(w,q,TKR_GUN_WAIT);q->tank.count=(uint8_t)tk_rank(w,q,TKR_GUN_COUNT);q->tank.serial=0;q->params[0]=(uint8_t)tk_rank(w,q,TKR_GUN_CHOICE);
  a->palette=0xed;q->tank.table=3;tk_start(q,TKA_GUN);break;
 case TK_GUN_FIRE:
  p=tk_parent(w,e);if(!p)return 0;q->tank.wait=tk_rank(w,q,(int8_t)p->tank.stage>2?TKR_GUN_LATE:TKR_GUN_EARLY);
  if((p->tank.pose&31)>=7)return enemy_fault(w);tk_start_entry(q,tank_gun_fire[p->tank.pose&31]);break;
 case TK_CASING:a->palette=0xec;a->hit_palette=0x8a;interaction_priority(w,a,0x8000,20);tk_start(q,TKA_CASING);break;
 case TK_GROUND:tk_start(q,TKA_GROUND);break;
 case TK_LAUNCH:a->hit.type_flags|=16;a->palette=0x23;q->selector=16;q->tank.priority=0;tk_start(q,TKA_LAUNCH);break;
 case TK_FLY:{
  q->tank.turn=0;a->hit.hp=20;q->tank.speed=1024;transform_integer_offset(&a->transform,-56,15);
  struct NativeEntity*parent=entity_get(w->entities,e->parent);if(!parent)return enemy_fault(w);p=tk_parent(w,parent);if(!p)return 0;
  unsigned row=!q->params[0]&&p->tank.stage==5;q->tank.path=(struct FlightState){.cursor=tank_paths[tank_path_map[row][q->tank.serial&15]&15]};
  interaction_priority(w,a,0xd000,20);tk_start(q,TKA_ROCKET);break;
 }
 case TK_HOME:q->tank.wait=180;q->selector&=31;polar(q);q->tank.target=(uint8_t)random_target(w);break;
 case TK_GRAVITY:a->hit.hp=20;transform_integer_offset(&a->transform,-56,15);q->tank.wait=(q->tank.serial&3)*4;q->tank.mode=0;a->vx=-1536;interaction_priority(w,a,0xd000,20);tk_start(q,TKA_ROCKET);break;
 case TK_ROCKET_HIT:if(!tk_sound(w,0x1022))return 0;/* FALLTHROUGH */
 case TK_FIRE:interaction_priority(w,a,0x4000,0);boss_fx_transform(w,e,BFX_FIRE);return 0;
 case TK_TRAIL:a->transform.priority=0x2000;a->palette=13;tk_start(q,TKA_TRAIL);break;
 case TK_MISSILE:q->tank.speed=q->params[0]*16;a->vx=tk_word(-q->tank.speed);a->effect_x=q->params[1]*16;a->palette=0x23;a->hit.type_flags|=16;q->tank.mode=0;tk_start(q,TKA_ROCKET);break;
 case TK_MUZZLE:a->palette=0xef;tk_attack(q,TKB_MUZZLE_ATK);tk_start(q,TKA_MUZZLE);break;
 case TK_MUZZLE_B:tk_start(q,TKA_MUZZLE_B);break;
 case TK_MUZZLE_C:
  if(!tk_sound(w,0x10c6))return 0;tk_attack(q,TKB_FLAME_ATK);q->tank.count=(uint8_t)tk_rank(w,q,TKR_MUZZLE_COUNT);tk_start(q,TKA_MUZZLE_C);break;
 case TK_MUZZLE_D:p=tk_parent(w,e);if(!p)return 0;p->dead=255;/* FALLTHROUGH */
 case TK_MUZZLE_END:tk_attack(q,65535);tk_start(q,TKA_MUZZLE_END);break;
 case TK_FRAGMENT:{
  p=tk_parent(w,e);if(!p)return 0;struct NativeEntity*pe=entity_get(w->entities,e->parent);struct EnemyActor*g=tk_parent(w,pe);if(!g)return 0;
  if(p->params[0]>=4)return enemy_fault(w);const int16_t*v=tank_fragment_motion[(int8_t)g->tank.stage>3][p->params[0]][q->tank.serial&7];
  a->vx=v[0];a->ay=v[1];a->vy=256;a->hit.type_flags|=16;a->palette=0x157;q->tank.mode=255;interaction_priority(w,a,0xd000,20);tk_start(q,TKA_FRAGMENT);break;
 }
 case TK_FRAGMENT_SMOKE:a->palette=0x157;tk_start(q,TKA_FRAGMENT_SMOKE);break;
 case TK_SMOKE:a->palette=0x193;tk_start(q,TKA_SMOKE);break;
 case TK_SMOKE_HIT:case TK_SMOKE_HIT_HIGH:
  interaction_priority(w,a,0x4000,state==TK_SMOKE_HIT_HIGH?28:0);tk_defense(q,65535);boss_fx_transform(w,e,BFX_SMOKE);return 0;
 default:return enemy_fault(w);
 }return !w->fault;
}
static int turret_tail(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(a->hit.flags&8){if(!score(w,a))return 0;a->hit.flags&=247;if(!tk_hit_sound(w,a))return 0;}
 if(a->hit.flags&1)enemy_schedule(q,TK_TURRET_DIE);struct EnemyActor*p=tk_parent(w,e);if(!p)return 0;if(p->tank.pose==5)enemy_schedule(q,TK_WRECK);return 1;
}
static int rocket_tail(struct WeaponWorld*w,struct NativeEntity*e,int attack){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(attack){if(!enemy_attack(w,a))return 0;if(a->hit.flags&2)enemy_schedule(q,TK_FIRE);
  if(a->hit.flags&8){if(!tk_hit_sound(w,a))return 0;a->hit.flags&=247;}if(a->hit.flags&1)enemy_schedule(q,TK_ROCKET_HIT);}
 struct NativeEntity*p=entity_get(w->entities,e->parent);if(!p)return enemy_fault(w);
 if(p->kind==ENTITY_ENEMY&&p->payload.enemy.tank.pose==5)enemy_schedule(q,TK_ROCKET_HIT);
 if(interaction_outside(w,a,tank_bounds[0],1))enemy_schedule(q,TK_DELETE_TREE);return 1;
}
static int home(struct WeaponWorld*w,struct EnemyActor*q){
 if(!(w->frame_phase&7)){
  struct PlayerBodyState*p=w->players[q->tank.target&1];
  int16_t px=p?tk_word((p->actor.transform.x>>8)-w->camera_x):-1,py=p?tk_word(transform_y_up(&p->actor.transform)+w->camera_y):-1;
  int16_t dx=tk_word(px-tk_sx(w,&q->actor)),dy=tk_word(py-tk_sy(w,&q->actor));
  unsigned angle=enemy_angle(dx,dy)>>3;
  if(angle!=q->selector){int increment=angle>q->selector?1:-1;if(q->tank.turn)increment=q->tank.turn;q->selector=(q->selector+increment)&31;polar(q);return 1;}
 }
 if(!(tk_random(w)&31))q->tank.turn=tk_word(-q->tank.turn);return !w->fault;
}
int tk_watch_stop(const struct NativeEntity*e){
 return e->kind==ENTITY_ENEMY?e->payload.enemy.tank.watch:-1;
}
int tk_parts_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;int done,r;
 switch(state){
 case TK_HULL:
  if(!tk_follow(w,e,1))return 0;tk_animate(w,e);if(a->hit.flags&8){a->hit.flags&=247;if(!tk_hit_sound(w,a))return 0;}
  p=tk_parent(w,e);if(!p)return 0;if((a->hit.flags&1)||p->tank.stage||p->tank.pose==6)enemy_schedule(q,TK_HULL_DIE);return 1;
 case TK_CABIN:{
  struct NativeEntity*n=entity_get(w->entities,(struct EntityRef){q->tank.child_index,q->tank.child_generation});
  if(!q->tank.mode&&q->tank.pose==5){if(n)n->payload.enemy.actor.hit.flags|=1;q->tank.mode=1;}
  if(!tk_follow(w,e,1))return 0;p=tk_parent(w,e);if(!p)return 0;if(p->actor.hit.events&128)a->hit.events|=1;tk_animate(w,e);
  if(q->tank.pose!=5&&n){n->payload.enemy.actor.hit.invulnerable=2;n->payload.enemy.params[3]=q->params[3];}
  if(entity_parent_deleting(w->entities,e))enemy_schedule(q,TK_DELETE);return 1;
 }
 case TK_LAUNCHER:
  if(!tk_follow(w,e,1))return 0;p=tk_parent(w,e);if(!p)return 0;if(p->actor.hit.events&128)a->hit.events|=1;tk_animate(w,e);
  q->tank.wait=tk_word(q->tank.wait-1);if(q->tank.wait<=0){if(!tk_spawn_rocket(w,e))return 0;q->tank.wait=tk_rank(w,q,TKR_ROCKET_REPEAT);if((int8_t)--q->tank.count<=0){q->tank.count=(uint8_t)tk_rank(w,q,TKR_ROCKET_COUNT);q->tank.wait=tk_rank(w,q,TKR_ROCKET_WAIT);}}
  if(entity_parent_deleting(w->entities,e))enemy_schedule(q,TK_DELETE);return 1;
 case TK_TURRET:case TK_AIM:case TK_RECOIL:
  if(state==TK_RECOIL&&!q->tank.mode){q->tank.wait=tk_word(q->tank.wait-1);if(q->tank.wait<=0){a->phase=1;tk_start(q,TKA_RECOIL);q->tank.mode=1;}}
  if(q->tank.contact<3)interaction_body_contact(w,a,&tank_contacts[q->tank.contact]);if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;done=tk_animate(w,e);
  if(state==TK_TURRET&&done)enemy_schedule(q,TK_TURRET_FIRE);
  if(state==TK_AIM&&--q->tank.wait<=0)enemy_schedule(q,TK_RECOIL);
  if(state==TK_RECOIL){if(q->dead)enemy_schedule(q,TK_AIM);p=tk_parent(w,e);if(!p)return 0;p->tank.signal=1;}
  return turret_tail(w,e);
 case TK_WRECK_WATCH:case TK_WRECK_IDLE:
  if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;tk_animate(w,e);if(entity_parent_deleting(w->entities,e))enemy_schedule(q,TK_DELETE);return 1;
 case TK_GUN:case TK_GUN_FIRE:
  if(!follow_gun(w,e,state==TK_GUN))return 0;p=tk_parent(w,e);if(!p)return 0;
  if(state==TK_GUN){
   unsigned signal=p->tank.signal;p->tank.signal=0;
   if(signal){q->tank.serial=0;q->params[0]=(uint8_t)tk_rank(w,q,TKR_GUN_CHOICE);}
   else if(p->tank.stage!=2&&p->tank.stage!=3){q->tank.wait=tk_word(q->tank.wait-1);if(q->tank.wait<=0)enemy_schedule(q,TK_GUN_FIRE);}
   tk_animate(w,e);
  }else if(tk_animate(w,e)){
   /* Reenter the frame label, preserving burst timers and current sequence. */
   a->state=TK_GUN;q->entry=0;
   if((int8_t)--q->tank.count<=0){q->tank.serial=0;q->params[0]=(uint8_t)tk_rank(w,q,TKR_GUN_CHOICE);q->tank.wait=tk_rank(w,q,TKR_GUN_WAIT);q->tank.count=(uint8_t)tk_rank(w,q,TKR_GUN_COUNT);}
  }
  if(p->tank.stage==6||entity_parent_deleting(w->entities,e))enemy_schedule(q,TK_DELETE);return 1;
 case TK_CASING:case TK_GROUND:
  if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;p=tk_parent(w,e);if(!p)return 0;
  if(state==TK_CASING&&(p->actor.hit.events&128))a->hit.events|=1;tk_animate(w,e);
  if(state==TK_CASING&&p->tank.pose==5)enemy_schedule(q,TK_GROUND);
  if(state==TK_GROUND&&entity_parent_deleting(w->entities,e))enemy_schedule(q,TK_DELETE);return 1;
 case TK_LAUNCH:
  if(!tk_follow(w,e,0))return 0;p=tk_parent(w,e);if(!p)return 0;transform_integer_offset(&a->transform,q->tank.serial&1?-10:2,q->tank.serial&1?p->tank.height_b:p->tank.height_a);
  if(tk_animate(w,e)){if(!tk_sound(w,0x10a4))return 0;enemy_schedule(q,TK_FLY);}return rocket_tail(w,e,0);
 case TK_FLY:{
  struct FlightOwner owner={.context=q,.field=path_field};r=flight_step(&a->transform,&q->tank.path,q->tank.speed,&tank_flight_program,&owner);
  if(r<0)return enemy_fault(w);if(r)enemy_schedule(q,TK_ROCKET_HIT);q->selector=(q->tank.path.angle>>8)>>3;tk_animate(w,e);return rocket_tail(w,e,1);
 }
 case TK_HOME:
  if(!home(w,q))return 0;r=interaction_move(w,a,WMOVE_POINT);if(r<0)return 0;if(r)enemy_schedule(q,TK_ROCKET_HIT);tk_animate(w,e);
  if(--q->tank.wait<=0)enemy_schedule(q,TK_ROCKET_HIT);return rocket_tail(w,e,1);
 case TK_GRAVITY:
  if(!q->tank.mode&&--q->tank.wait<=0){a->ay=-160;q->tank.mode=1;}
  r=interaction_move(w,a,WMOVE_FALL);if(r<0)return 0;if(r)enemy_schedule(q,TK_ROCKET_HIT);tk_animate(w,e);q->selector=enemy_angle(a->vx>>4,a->vy>>4)>>3;return rocket_tail(w,e,1);
 case TK_MISSILE:
  if(!q->tank.mode&&a->effect_x>=tk_sx(w,a)){a->ay=-32;q->tank.mode=1;}
  r=interaction_move(w,a,WMOVE_FALL);if(r<0)return 0;if(r)enemy_schedule(q,TK_ROCKET_HIT);q->selector=enemy_angle(a->vx>>4,a->vy>>4)>>3;tk_animate(w,e);return rocket_tail(w,e,1);
 case TK_TRAIL:case TK_SMOKE:case TK_FRAGMENT_SMOKE:
  if(state==TK_FRAGMENT_SMOKE){p=tk_parent(w,e);if(!p)return 0;a->palette=p->actor.palette;}
  if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;if(tk_animate(w,e))enemy_schedule(q,TK_DELETE_TREE);
  if(state==TK_SMOKE&&interaction_outside(w,a,boss_fx_bounds[8],1))enemy_schedule(q,TK_DELETE_TREE);return 1;
 case TK_MUZZLE:case TK_MUZZLE_B:case TK_MUZZLE_C:case TK_MUZZLE_D:case TK_MUZZLE_END:
  if(!muzzle_follow(w,e))return 0;done=tk_animate(w,e);p=tk_parent(w,e);if(!p)return 0;
  if(state==TK_MUZZLE&&p->actor.phase==1)enemy_schedule(q,TK_MUZZLE_B);
  if(state==TK_MUZZLE_B&&done)enemy_schedule(q,TK_MUZZLE_C);
  if(state==TK_MUZZLE_C&&done){tk_start(q,TKA_MUZZLE_LOOP);if((int8_t)--q->tank.count<=0)enemy_schedule(q,TK_MUZZLE_D);}
  if(state==TK_MUZZLE_D&&done)enemy_schedule(q,TK_MUZZLE);
  if(state==TK_MUZZLE_END){if(done)enemy_schedule(q,TK_DELETE_TREE);return 1;}
  hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;
  if((p->actor.state==TK_WRECK_WATCH&&p->entry)||entity_parent_deleting(w->entities,e))enemy_schedule(q,TK_MUZZLE_END);return 1;
 case TK_FRAGMENT:
  r=interaction_move(w,a,WMOVE_FALL);if(r<0)return 0;if(r)enemy_schedule(q,TK_SMOKE_HIT);tk_animate(w,e);if(!enemy_attack(w,a))return 0;
  if(a->hit.flags&2)enemy_schedule(q,TK_SMOKE_HIT_HIGH);if(interaction_outside(w,a,tank_bounds[1],1))enemy_schedule(q,TK_DELETE_TREE);return 1;
 default:return enemy_fault(w);
 }
}
int tk_emit(struct WeaponWorld*w,struct NativeEntity*e,unsigned callback){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*n;
 switch(callback){
 case TKC_283CA:hit_begin_attack(&a->hit);return 1;
 case TKC_7458A:interaction_priority(w,a,0xd000,28);return 1;
 case TKC_7439A:return tk_child(w,e,TK_CASING,ENTITY_FIRST_CHILD)!=0;
 case TKC_743AC:q->tank.contact=1;n=tk_child(w,e,TK_TURRET,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.tank.priority=0;return 1;
 case TKC_743CA:return boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_FIRE)!=0;
 case TKC_743DE:
  return tk_sound(w,0x1021)&&boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMOKE)&&boss_fx_pattern(w,entity_ref(e),&a->transform,BP_TANK_EXPLOSION);
 case TKC_74408:return boss_fx_pattern(w,entity_ref(e),&a->transform,BP_TANK_DEBRIS)!=0;
 case TKC_74416:
  if(!tk_sound(w,0x10ea)||!tk_child(w,e,TK_SMOKE,ENTITY_FIRST_CHILD))return 0;n=tk_child(w,e,TK_FRAGMENT,ENTITY_FIRST_CHILD);if(!n)return 0;
  transform_integer_offset(&n->payload.enemy.actor.transform,-28,-4);n->payload.enemy.tank.serial=q->tank.serial;q->tank.serial=(q->tank.serial+1)&7;return 1;
 case TKC_6E4CC:
  n=tk_child(w,e,TK_FRAGMENT_SMOKE,ENTITY_LAST_CHILD);if(!n)return 0;n->payload.enemy.params[6]=q->params[6];n->payload.enemy.actor.transform.priority--;
  q->tank.mode=(q->tank.mode+1)&3;if(q->tank.mode>=3)q->tank.mode=0;a->palette=(uint16_t)(0x157+q->tank.mode);return 1;
 case TKC_6B974:
  if(!tk_sound(w,0x1033))return 0;n=boss_fx_pattern(w,entity_ref(e),&a->transform,BP_BAZOOKA_DEATH);if(!n)return 0;n->payload.enemy.actor.transform.priority=0x4000;
  n=boss_fx_pattern(w,entity_ref(e),&a->transform,BP_BAZOOKA_PUFF);if(!n)return 0;n->payload.enemy.actor.transform.priority=0x4000;return 1;
 case TKC_6B9A4:
  if(!tk_sound(w,0x1021))return 0;n=boss_fx_pattern(w,entity_ref(e),&a->transform,BP_BAZOOKA_PUFF);if(!n)return 0;n->payload.enemy.actor.transform.priority=0x4000;
  n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMOKE);if(!n)return 0;n->payload.enemy.actor.transform.priority=0x4000;return 1;
 default:return enemy_fault(w);
 }
}
