#include "tank_internal.h"
/* Attached crew $06A7DE and weapon $06ACB2, including the parent-phase
 * ejection into the separate rocket-vehicle controller $06BA34. */
static void palette(struct EnemyActor*q){q->actor.palette=tank_crew_palettes[q->bazooka.variant&3];q->actor.timer=7;}
static int crew_follow(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy,*p=tk_parent(w,e);if(!p)return 0;struct WeaponActor*a=&q->actor;
 int16_t dx=(int8_t)q->params[2];if(!(p->actor.transform.facing&1))dx=tk_word(-dx);
 transform_integer_offset(&a->transform,tk_word((p->actor.transform.x>>8)+dx-(a->transform.x>>8)),tk_word(transform_y_up(&p->actor.transform)+(int8_t)q->params[3]-transform_y_up(&a->transform)));
 a->transform.priority=(uint16_t)(p->actor.transform.priority+(int8_t)q->params[4]);return 1;
}
static int weapon_follow(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy,*p=tk_parent(w,e);if(!p)return 0;struct WeaponActor*a=&q->actor;
 a->transform.x=p->actor.transform.x;a->transform.y=p->actor.transform.y;a->transform.facing=p->actor.transform.facing;a->transform.priority=p->actor.transform.priority;
 transform_integer_offset(&a->transform,tk_faced(a,(int8_t)p->bazooka.offset_x),(int8_t)p->bazooka.offset_y);
 unsigned angle=q->selector&15;if(!angle||angle>=8)a->transform.priority--;if(p->actor.hit.events&128)a->hit.events|=1;return 1;
}
static int fire(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(q->selector>=16)return enemy_fault(w);
 if(!tk_sound(w,0x1064))return 0;unsigned angle=a->transform.facing&1?tank_mirror[q->selector]&15:q->selector&15;
 int16_t dx=tk_faced(a,tank_crew_offsets[q->selector][0]),dy=tank_crew_offsets[q->selector][1];
 struct NativeEntity*n=tk_child(w,e,TK_CREW_MUZZLE,ENTITY_FIRST_CHILD);if(!n)return 0;
 n->payload.enemy.selector=q->selector;n->payload.enemy.bazooka.offset_x=(uint8_t)dx;n->payload.enemy.bazooka.offset_y=(uint8_t)dy;
 n=tk_child(w,e,TK_CREW_ROCKET,ENTITY_FIRST_CHILD);if(!n)return 0;n->payload.enemy.selector=(uint16_t)angle;n->payload.enemy.params[0]=q->params[0];transform_integer_offset(&n->payload.enemy.actor.transform,dx,dy);return 1;
}
int tk_crew_enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;struct NativeEntity*n;unsigned state=a->state;q->entry=0;
 switch(state){
 case TK_ALLY:
  q->params[6]=0;q->bazooka.variant=0;palette(q);interaction_priority(w,a,0x8000,8);a->hit.hp=tk_rank(w,q,TKR_CREW_HP);tk_defense(q,q->bazooka.ally_defense?TKB_CREW_DEF:65535);
  n=tk_child(w,e,TK_WEAPON,ENTITY_FIRST_CHILD);if(!n)return 0;
  n->payload.enemy.params[0]=1;n->payload.enemy.params[4]=n->payload.enemy.params[5]=n->payload.enemy.params[6]=n->payload.enemy.params[7]=0;n->payload.enemy.bazooka.variant=0;
  /* $06AA40 overwrites the computed parameter with literal $0110. */
  q->params[0]=1;q->params[1]=16;q->bazooka.direction=q->bazooka.count=0;q->bazooka.wait=tk_rank(w,q,TKR_CREW_WAIT);a->state=TK_ALLY_ACQUIRE;
  /* fall through */
 case TK_ALLY_ACQUIRE:{int i=enemy_nearest_x(w,a,0);q->bazooka.target=(uint8_t)(i<0?1:i);q->bazooka.command=4;q->bazooka.ack=0;tk_start(q,TKA_ALLY_IDLE);break;}
 case TK_ALLY_CHOOSE:q->bazooka.count=(uint8_t)tk_rank(w,q,TKR_CREW_COUNT);a->state=TK_ALLY_FIRE;/* fall through */
 case TK_ALLY_FIRE:q->bazooka.wait=tk_rank(w,q,TKR_CREW_REPEAT);q->bazooka.ack=0;q->bazooka.command=2;tk_start(q,TKA_CREW_FIRE);break;
 case TK_ALLY_HURT:q->bazooka.command=5;tk_start(q,TKA_CREW_HURT);break;
 case TK_ALLY_DIE:q->bazooka.command=6;tk_start(q,TKA_CREW_DIE);break;
 case TK_ALLY_FADE:q->bazooka.wait=60;tk_defense(q,65535);a->display_flags&=253;q->bazooka.command=6;break;
 case TK_CREW:case TK_CREW_ZERO:
  q->bazooka.variant=state==TK_CREW;palette(q);interaction_priority(w,a,0x8000,8);a->hit.hp=tk_rank(w,q,TKR_CREW_HP);tk_defense(q,TKB_CREW_DEF);q->bazooka.direction=q->params[5];
  n=tk_child(w,e,TK_WEAPON,ENTITY_FIRST_CHILD);if(!n)return 0;
  n->payload.enemy.params[0]=n->payload.enemy.params[4]=n->payload.enemy.params[5]=n->payload.enemy.params[7]=0;n->payload.enemy.params[6]=q->params[6];n->payload.enemy.bazooka.variant=q->bazooka.variant;
  q->bazooka.count=0;q->bazooka.wait=tk_rank(w,q,TKR_CREW_WAIT);a->state=TK_ACQUIRE;/* FALLTHROUGH */
 case TK_ACQUIRE:{int i=enemy_nearest_x(w,a,0);q->bazooka.target=(uint8_t)(i<0?1:i);q->bazooka.command=4;q->bazooka.ack=0;tk_start(q,TKA_CREW_IDLE);break;}
 case TK_CHOOSE:q->bazooka.count=(uint8_t)tk_rank(w,q,TKR_CREW_COUNT);a->state=TK_CREW_FIRE;/* FALLTHROUGH */
 case TK_CREW_FIRE:q->bazooka.wait=tk_rank(w,q,TKR_CREW_REPEAT);q->bazooka.ack=0;q->bazooka.command=2;tk_start(q,TKA_CREW_FIRE);break;
 case TK_CREW_HURT:q->bazooka.command=5;tk_start(q,TKA_CREW_HURT);break;
 case TK_CREW_DIE:q->bazooka.command=6;tk_start(q,TKA_CREW_DIE);break;
 case TK_CREW_FADE:q->bazooka.wait=60;tk_defense(q,65535);a->display_flags&=253;q->bazooka.command=6;break;
 case TK_WEAPON:
  palette(q);q->params[0]&=3;q->bazooka.command=0;
  if(q->params[5]){enemy_schedule(q,TK_WEAPON_FIXED);return 2;}q->selector=8;a->state=TK_WEAPON_IDLE;/* FALLTHROUGH */
 case TK_WEAPON_IDLE:q->selector=8;tk_start(q,TKA_WEAPON);break;
 case TK_WEAPON_AIM:p=tk_parent(w,e);if(!p)return 0;q->bazooka.target=p->bazooka.target;q->bazooka.clock=0;tk_start(q,TKA_WEAPON);break;
 case TK_WEAPON_FIRE:tk_start_entry(q,tank_weapon_fire[q->selector&15]);if(!fire(w,e))return 0;break;
 case TK_WEAPON_CENTER:case TK_WEAPON_WAIT:q->bazooka.clock=0;tk_start(q,TKA_WEAPON);break;
 case TK_WEAPON_FIXED:q->bazooka.clock=0;q->selector=q->params[4]?4:12;tk_start(q,TKA_WEAPON);break;
 case TK_WEAPON_HURT:tk_start(q,TKA_WEAPON_DIE);break;
 case TK_WEAPON_FLY:a->vx=tk_faced(a,-34);a->vy=1635;a->ay=-109;a->ax=0;interaction_priority(w,a,0x4000,0);break;
 case TK_WEAPON_FALL:break;
 case TK_CREW_ROCKET_HIT:if(!tk_sound(w,0x1025))return 0;/* FALLTHROUGH */
 case TK_WEAPON_BLAST:interaction_priority(w,a,0x4000,0);tk_defense(q,65535);boss_fx_transform(w,e,BFX_FIRE);return 0;
 case TK_CREW_MUZZLE:
  interaction_priority(w,a,0x4000,28);a->palette=8;transform_integer_offset(&a->transform,(int8_t)q->bazooka.offset_x,(int8_t)q->bazooka.offset_y-8);tk_start(q,TKA_CREW_MUZZLE);a->state=TK_WEAPON_FALL;break;
 case TK_CREW_ROCKET:{
  const int16_t*t=tank_crew_motion[q->params[0]&7][q->selector&15];a->vx=t[0];a->ay=t[1];a->vy=t[2];a->transform.facing&=254;a->palette=0x173;a->hit.type_flags|=16;
  tk_start(q,TKA_CREW_ROCKET);q->bazooka.body_collision=(q->selector&15)>0&&(q->selector&15)<8;interaction_priority(w,a,0xd000,q->bazooka.body_collision?8:28);break;
 }
 default:return enemy_fault(w);
 }return !w->fault;
}
static int crew_tail(struct WeaponWorld*w,struct NativeEntity*e,int hurt){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(!hurt){
  q->bazooka.hurt=0;struct EnemyActor*p=tk_parent(w,e);if(!p)return 0;
  if(p->actor.phase==255&&!(a->hit.flags&1)){
   transform_integer_offset(&a->transform,0,32);q->bazooka.command=7;rocket_vehicle_adopt(e);
   if(entity_parent_deleting(w->entities,e)){tk_schedule_crew_death(e);return 1;}
   return 1;
  }
  if(!tk_hit_sound(w,a))return 0;
  if(a->hit.flags&8){a->hit.flags&=247;if(a->hit.last_kind==2||a->hit.last_kind==3){a->hit.flags|=8;enemy_schedule(q,TK_CREW_HURT);q->bazooka.hurt=1;}}
 }
 if((a->hit.flags&1)||entity_parent_deleting(w->entities,e))enemy_schedule(q,TK_CREW_DIE);return 1;
}
static int aim(struct WeaponWorld*w,struct EnemyActor*q){
 struct PlayerBodyState*p=w->players[q->bazooka.target&1];
 struct WeaponActor*a=&q->actor;int16_t px=p?tk_word((p->actor.transform.x>>8)-w->camera_x):-1,py=p?tk_word(transform_y_up(&p->actor.transform)+w->camera_y):-1;
 int16_t dx=tk_word(px-tk_sx(w,a)),dy=tk_word(py-tk_sy(w,a));
 int x=dx;if(x< -256)x=-256;if(x>256)x=256;unsigned angle=tank_crew_angles[dy>0?0:1][(x+256)>>4];
 if(angle>=16){enemy_fault(w);return 0;}if(a->transform.facing&1)angle=tank_mirror[angle];
 if(dy>=-8&&dy<=40)angle=tk_faced(a,dx)>0?0:8;angle&=15;
 if(angle==q->selector)return 1;q->selector=(uint16_t)(q->selector+(angle>q->selector?1:-1));return 0;
}
static int lookup(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy,*p=tk_parent(w,e);if(!p)return 0;
 if(q->bazooka.command==6){enemy_schedule(q,TK_WEAPON_FLY);return 2;}
 unsigned command=p->bazooka.command;if(command==q->bazooka.command)return 1;if(command>=8)return enemy_fault(w);q->bazooka.command=(uint8_t)command;
 static const uint8_t states[]={TK_WEAPON_IDLE,TK_WEAPON_AIM,TK_WEAPON_FIRE,TK_WEAPON_CENTER,TK_WEAPON_WAIT,TK_WEAPON_HURT,TK_WEAPON_FLY,TK_DELETE};
 unsigned state=states[command];if(q->params[5]&&(command==0||command==1||command==3||command==4))state=TK_WEAPON_FIXED;
 enemy_schedule(q,state);return 2;
}
int tk_crew_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned state){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;int done,r;
 if(state>=TK_ALLY&&state<=TK_ALLY_FADE){
  if(state==TK_ALLY_FADE){if(--q->bazooka.wait<=0)enemy_schedule(q,TK_DELETE_TREE);return 1;}
  if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;done=tk_animate(w,e);
  if(state==TK_ALLY_ACQUIRE){
   /* The source schedules an attack when the count is ZERO. */
   if(!q->bazooka.count)enemy_schedule(q,TK_ALLY_FIRE);
   if(tk_sx(w,a)<=272){q->bazooka.command=1;q->bazooka.wait=tk_word(q->bazooka.wait-1);if(q->bazooka.ack&&q->bazooka.wait<=0)enemy_schedule(q,TK_ALLY_CHOOSE);}
  }else if(state==TK_ALLY_FIRE){
   if(done)tk_start(q,TKA_ALLY_IDLE);q->bazooka.wait=tk_word(q->bazooka.wait-1);
   if(q->bazooka.wait<=0&&q->bazooka.ack){q->bazooka.command=4;enemy_schedule(q,TK_ALLY_FIRE);if((int8_t)--q->bazooka.count<=0){q->bazooka.wait=tk_rank(w,q,TKR_CREW_WAIT);enemy_schedule(q,TK_ALLY_ACQUIRE);}}
  }else if(state==TK_ALLY_HURT){if(done){a->hit.flags&=247;enemy_schedule(q,TK_ALLY_ACQUIRE);}}
  else if(state==TK_ALLY_DIE){if(done)enemy_schedule(q,TK_ALLY_FADE);}
  if(state!=TK_ALLY_DIE){
   if(state!=TK_ALLY_HURT){q->bazooka.hurt=0;if(!tk_hit_sound(w,a))return 0;if(a->hit.flags&8){a->hit.flags&=247;if(a->hit.last_kind==2||a->hit.last_kind==3){a->hit.flags|=8;q->bazooka.hurt=1;enemy_schedule(q,TK_ALLY_HURT);}}}
   struct NativeEntity*owner=entity_get(w->entities,e->parent);if(!owner)return enemy_fault(w);
   uint8_t phase;if(owner->kind==ENTITY_ENEMY)phase=owner->payload.enemy.actor.phase;else if(owner->kind==ENTITY_GROUP)phase=owner->payload.group.phase;else return enemy_fault(w);
   if((a->hit.flags&1)||phase==255)enemy_schedule(q,TK_ALLY_DIE);
  }
  if(interaction_outside(w,a,tank_bounds[2],0)||tk_sx(w,a)<-128)enemy_schedule(q,TK_ALLY_FADE);return !w->fault;
 }
 if(state==TK_ACQUIRE||state==TK_CREW_FIRE||state==TK_CREW_HURT||state==TK_CREW_DIE){
  if(!crew_follow(w,e))return 0;done=tk_animate(w,e);
  if(state==TK_ACQUIRE){
   int16_t goal=(int16_t)((q->params[0]<<8)|q->params[1]),x=tk_sx(w,a);
   if(q->bazooka.direction&1?goal<=x:goal>=x){if(q->bazooka.count)enemy_schedule(q,TK_CREW_FIRE);q->bazooka.command=1;q->bazooka.wait=tk_word(q->bazooka.wait-1);if(q->bazooka.ack&&q->bazooka.wait<=0)enemy_schedule(q,TK_CHOOSE);}
  }else if(state==TK_CREW_FIRE){
   if(done)tk_start(q,TKA_CREW_IDLE);q->bazooka.wait=tk_word(q->bazooka.wait-1);
   if(q->bazooka.wait<=0&&q->bazooka.ack){q->bazooka.command=4;enemy_schedule(q,TK_CREW_FIRE);if((int8_t)--q->bazooka.count<=0){q->bazooka.wait=tk_rank(w,q,TKR_CREW_WAIT);enemy_schedule(q,TK_ACQUIRE);}}
  }else if(state==TK_CREW_HURT){if(done){a->hit.flags&=247;enemy_schedule(q,TK_ACQUIRE);}}
  else{if(done)enemy_schedule(q,TK_CREW_FADE);return 1;}
  return crew_tail(w,e,state==TK_CREW_HURT);
 }
 if(state==TK_CREW_FADE){if(--q->bazooka.wait<=0)enemy_schedule(q,TK_DELETE_TREE);return 1;}
 if(state==TK_WEAPON_FLY){
  r=interaction_move(w,a,WMOVE_BODY);if(r<0)return 0;if(r)enemy_schedule(q,TK_WEAPON_BLAST);tk_animate(w,e);
  if(tk_sy(w,a)<=192)enemy_schedule(q,TK_DELETE);if(entity_parent_deleting(w->entities,e))enemy_schedule(q,TK_WEAPON_BLAST);
  if(interaction_outside(w,a,tank_bounds[2],0)||tk_sx(w,a)<-128)enemy_schedule(q,TK_DELETE_TREE);return 1;
 }
 if(state==TK_WEAPON_FALL){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;if(tk_animate(w,e))enemy_schedule(q,TK_DELETE_TREE);return 1;}
 if(state==TK_CREW_ROCKET){
  r=interaction_move(w,a,q->bazooka.body_collision?WMOVE_BODY:WMOVE_FALL);if(r<0)return 0;if(r)enemy_schedule(q,TK_CREW_ROCKET_HIT);
  q->selector=enemy_angle(a->vx>>4,a->vy>>4)>>3;tk_animate(w,e);if(!enemy_attack(w,a))return 0;if(a->hit.flags&2)enemy_schedule(q,TK_WEAPON_BLAST);
  if(interaction_outside(w,a,tank_bounds[3],1))enemy_schedule(q,TK_DELETE_TREE);return 1;
 }
 if(state<TK_WEAPON_IDLE||state>TK_WEAPON_HURT)return enemy_fault(w);
 p=tk_parent(w,e);if(!p)return 0;
 if(state==TK_WEAPON_AIM||state==TK_WEAPON_CENTER){
  q->bazooka.clock=(q->bazooka.clock+1)&3;
  if(!q->bazooka.clock){
   if(state==TK_WEAPON_AIM){
    if(q->params[7]){
     /* $06B5A4/$05E23A use the stored target and signed absolute words. */
     if(q->bazooka.target>=2)return enemy_fault(w);struct PlayerBodyState*t=w->players[q->bazooka.target];
     int16_t px=t?tk_word((t->actor.transform.x>>8)-w->camera_x):-1,py=t?tk_word(transform_y_up(&t->actor.transform)+w->camera_y):-1;
     int16_t dx=tk_word(tk_sx(w,a)-px),dy=tk_word(tk_sy(w,a)-py);if(dx<0)dx=tk_word(-dx);if(dy<0)dy=tk_word(-dy);
     int16_t hi=dx>dy?dx:dy,lo=dx>dy?dy:dx,distance=tk_word(hi+((uint16_t)lo>>1));q->params[0]=distance<112?0:distance<176?1:2;
    }
    p->bazooka.ack=0;if(aim(w,q)&&!(q->params[6]&&q->selector&&q->selector<13))p->bazooka.ack=1;
   }else if(q->selector==8)p->bazooka.ack=1;else q->selector=(uint16_t)(q->selector+(q->selector<8?1:-1));
  }
 }
 r=lookup(w,e);if(r!=1)return r;if(!weapon_follow(w,e))return 0;
 if(state==TK_WEAPON_HURT)a->transform.priority--;done=tk_animate(w,e);
 if(state==TK_WEAPON_FIRE){if(done)p->bazooka.ack=1;return 1;}
 if(state==TK_WEAPON_HURT)return 1;
 if(done)tk_start(q,TKA_WEAPON);if(p->bazooka.hurt)tk_start(q,TKA_WEAPON_HURT);return 1;
}
