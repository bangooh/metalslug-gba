#include "assault76_internal.h"
/* $065166..$065F24: projectile/fragment lifetimes and ordered emissions. */
static void velocity(struct EnemyActor*q){unsigned i=((uint16_t)q->assault76.angle<<3)&255;q->actor.vy=tk_word((int32_t)boss_trig[0][i]*q->scale>>8);q->actor.vx=tk_word((int32_t)boss_trig[1][i]*q->scale>>8);}
static int blink(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;q->assault76.timer=tk_word(q->assault76.timer-1);return (q->assault76.timer>15||(q->assault76.timer&1))?r76_animate(w,e):0;}
static int target(struct WeaponWorld*w,struct EnemyActor*q){q->assault76.timer=tk_word(q->assault76.timer-1);if(q->assault76.timer>0)return 1;q->assault76.timer=(tk_random(w)&31)+48;q->assault76.stage=tk_word(q->assault76.stage+1);if(q->assault76.stage<=1){unsigned i=tk_random(w)&15;q->assault76.limit=assault76_targets[i][0];q->assault76.speed=assault76_targets[i][1];q->scale=(tk_random(w)&0x1f0)+256;}else{int i=enemy_nearest_x(w,&q->actor,0);if(i<0)i=1;struct PlayerBodyState*p=w->players[i];q->assault76.limit=p?tk_word((p->actor.transform.x>>8)-w->camera_x):-1;q->assault76.speed=p?tk_word(transform_y_up(&p->actor.transform)+w->camera_y):-1;q->scale=768;if(q->assault76.stage>3)return !w->fault;}q->assault76.limit=tk_word(q->assault76.limit+(tk_random(w)&31));q->assault76.speed=tk_word(q->assault76.speed+(tk_random(w)&31));return !w->fault;}
static int steering(struct WeaponWorld*w,struct EnemyActor*q){struct WeaponActor*a=&q->actor;if(!(w->frame_phase&3)){unsigned goal=enemy_angle(tk_word(q->assault76.limit-tk_sx(w,a)),tk_word(q->assault76.speed-tk_sy(w,a)))>>3;if(goal!=(uint16_t)q->assault76.angle){int step=q->assault76.serial?q->assault76.serial:goal>(uint16_t)q->assault76.angle?1:-1;q->assault76.angle=(q->assault76.angle+step)&31;}q->assault76.timer>>=1;}if(!(tk_random(w)&31))q->assault76.serial=tk_word(-q->assault76.serial);int16_t dx=tk_word(tk_sx(w,a)-q->assault76.limit),dy=tk_word(tk_sy(w,a)-q->assault76.speed);if(dx>=-16&&dx<=16&&dy>=-16&&dy<=16)q->assault76.timer=0;return !w->fault;}
int r76_ordnance_enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned i;
 switch(a->state){
 case R76_GRENADE:a->palette=0xa1;r76_start(q,tk_random(w)&1?R76A_GRENADE_A:R76A_GRENADE_B);a->transform.priority=0xd000;break;
 case R76_SUBMERGE:q->assault76.timer=30;m4_reset(a);a->display_flags|=64;interaction_priority(w,a,0xf000,28);if(!boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SURFACE))return 0;break;
 case R76_FLOAT:transform_integer_offset(&a->transform,0,-24);a->vx=a->transform.facing&1?256:-256;a->palette=0x160;r76_start(q,R76A_SURFACE);break;
 case R76_FLOAT_HIT:a->vx>>=1;r76_start(q,R76A_SURFACE_HIT);break;
 case R76_SMOKE:transform_integer_offset(&a->transform,0,24);a->palette=12;r76_start(q,R76A_SMOKE);break;
 case R76_FLASH:interaction_priority(w,a,0xc000,28);r76_start(q,R76A_FLASH);break;
 case R76_DEBRIS:q->assault76.limit=4;interaction_priority(w,a,0x4000,28);a->palette=0x45;if(!(tk_random(w)&7)){r76_schedule(e,R76_DEBRIS_SLIDE);return 2;}i=tk_random(w)&3;a->vy=assault76_debris[i][2];a->ay=assault76_debris[i][1];a->vx=tk_word(-tk_faced(a,assault76_debris[i][0]));q->assault76.timer=(tk_random(w)&31)+24;r76_start(q,R76A_DEBRIS);a->state=R76_DEBRIS_FALL;break;
 case R76_DEBRIS_FALL:a->vy=tk_word(-(a->vy>>1));r76_start(q,R76A_DEBRIS);break;
 case R76_DEBRIS_BOUNCE:q->assault76.limit=tk_word(q->assault76.limit-1);if(q->assault76.limit<0){r76_schedule(e,R76_DEBRIS_SLIDE);return 2;}r76_start(q,R76A_BOUNCE);break;
 case R76_DEBRIS_SLIDE:{int16_t vx=a->vx;m4_reset(a);if(vx<512&&vx>-512)vx=vx>=0?512:-512;a->vx=vx;r76_start(q,R76A_DEBRIS);break;}
 case R76_DEBRIS_HOP:a->vy=0x2a9;a->ay=-227;r76_start(q,R76A_DEBRIS);a->state=R76_DEBRIS_FALL;break;
 case R76_MUZZLE:if(q->assault76.form>=4)return enemy_fault(w);r76_start(q,R76A_MUZZLE0+q->assault76.form);a->palette=4;break;
 case R76_MISSILE:{i=q->assault76.form;if(i>=4)return enemy_fault(w);static const uint8_t angles[]={18,21,27,30};q->assault76.angle=angles[a->transform.facing&1?3-i:i];struct EnemyActor*p=tk_parent(w,e);struct NativeEntity*pe=entity_get(w->entities,e->parent),*root=pe?entity_get(w->entities,pe->parent):0;if(!p||!root||root->kind!=ENTITY_ENEMY)return enemy_fault(w);q->assault76.serial=assault76_turn[p->assault76.serial&3];q->assault76.life=root->payload.enemy.params[3]?root->payload.enemy.params[3]:32767;a->palette=0x46;r76_start(q,R76A_MISSILE);}
 /* FALLTHROUGH */
 case R76_MISSILE_RESUME:a->hit.type_flags|=16;if(!tk_sound(w,0x10a4))return 0;q->assault76.timer=q->assault76.stage=0;a->transform.facing=0;a->hit.hp=5;q->scale=512;q->assault76.angle&=31;interaction_priority(w,a,0xd000,28);a->state=R76_MISSILE;break;
 case R76_SPARK:a->palette=4;r76_start(q,R76A_SPARK);interaction_priority(w,a,0x4000,28);break;
 case R76_BOMB:a->hit.type_flags|=16;a->palette=0x15d;q->assault76.cycle=0;a->hit.hp=8;if(tk_sy(w,a)>=376){a->vx=tk_faced(a,0x111);a->vy=0x21c;a->ay=-36;}else{a->vx=tk_faced(a,0x5b);a->vy=0x5a0;a->ay=-32;}a->ax=0;interaction_priority(w,a,0xd000,0);r76_start(q,R76A_BOMB);break;
 case R76_DUST:a->palette=4;a->vy=32;r76_start(q,R76A_DUST);break;
 case R76_EXPLODE:if(!tk_sound(w,0x1022))return 0;/* FALLTHROUGH */
 case R76_FIRE:a->palette=65535;interaction_priority(w,a,0x4000,28);r76_defense(q,65535);return boss_fx_transform(w,e,BFX_FIRE),0;
 default:return enemy_fault(w);
 }return !w->fault;
}
int r76_ordnance_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;int r,done;
 if(s==R76_GRENADE){r=interaction_move(w,a,WMOVE_FALL);if(r<0)return 0;if(r)r76_schedule(e,R76_SUBMERGE);r76_animate(w,e);if(tk_sy(w,a)<=288)r76_schedule(e,R76_SUBMERGE);if(!human_death_hit_tail(w,e)||!tk_hit_sound(w,a))return 0;if(r76_outside(w,a,0))r76_schedule(e,R76_DELETE_TREE);return !w->fault;}
 if(s==R76_SUBMERGE){a->hit.invulnerable=1;if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;q->assault76.timer=tk_word(q->assault76.timer-1);if(q->assault76.timer<=0)r76_schedule(e,R76_FLOAT);}
 else if(s==R76_FLOAT||s==R76_FLOAT_HIT){if(interaction_move(w,a,WMOVE_POINT)<0)return 0;done=r76_animate(w,e);if(done){if(s==R76_FLOAT)r76_start(q,R76A_SURFACE_LOOP);else r76_schedule(e,R76_SMOKE);}if(s==R76_FLOAT){if(a->hit.flags&8)r76_schedule(e,R76_FLOAT_HIT);if(!tk_hit_sound(w,a))return 0;}}
 else if(s==R76_SMOKE||s==R76_FLASH||s==R76_MUZZLE||s==R76_SPARK){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;if(r76_animate(w,e))r76_schedule(e,R76_DELETE_TREE);/* Original muzzle/spark are animation-only, with no bounds tail. */if(s==R76_MUZZLE||s==R76_SPARK)return !w->fault;}
 else if(s==R76_DEBRIS_FALL||s==R76_DEBRIS_BOUNCE||s==R76_DEBRIS_SLIDE){r=interaction_move(w,a,s==R76_DEBRIS_FALL?WMOVE_FALL:s==R76_DEBRIS_BOUNCE?WMOVE_SYNC:WMOVE_GROUND);if(r<0)return 0;if(s==R76_DEBRIS_FALL&&r)r76_schedule(e,R76_DEBRIS_BOUNCE);if(s==R76_DEBRIS_SLIDE&&!(tk_random(w)&15))r76_schedule(e,R76_DEBRIS_HOP);done=blink(w,e);if(s==R76_DEBRIS_BOUNCE&&done)r76_schedule(e,R76_DEBRIS_FALL);if(q->assault76.timer<=0||tk_sx(w,a)<=-48)r76_schedule(e,R76_DELETE_TREE);}
 else if(s==R76_MISSILE){q->assault76.life=tk_word(q->assault76.life-1);if(q->assault76.life>0){if(!target(w,q)||!steering(w,q))return 0;velocity(q);}r=interaction_move(w,a,WMOVE_POINT);if(r<0)return 0;if(r)r76_schedule(e,R76_EXPLODE);if((uint16_t)q->assault76.angle>=32)return enemy_fault(w);q->selector=q->assault76.angle;r76_animate(w,e);if(!enemy_attack(w,a))return 0;if(a->hit.flags&2)r76_schedule(e,R76_FIRE);if(a->hit.flags&1)r76_schedule(e,R76_EXPLODE);a->hit.flags&=247;}
 else if(s==R76_BOMB){if(!(w->frame_phase&1)){a->palette=assault76_bomb_palettes[q->assault76.cycle&3];q->assault76.cycle++;}r=interaction_move(w,a,WMOVE_FALL);if(r<0)return 0;if(r)r76_schedule(e,R76_EXPLODE);r76_animate(w,e);if(!enemy_attack(w,a))return 0;if(a->hit.flags&2)r76_schedule(e,R76_FIRE);a->hit.flags&=247;if(a->hit.flags&1)r76_schedule(e,R76_FIRE);if(!tk_hit_sound(w,a))return 0;}
 else if(s==R76_DUST){if(interaction_move(w,a,WMOVE_POINT)<0)return 0;if(r76_animate(w,e))r76_schedule(e,R76_DELETE_TREE);}
 else return enemy_fault(w);
 if(r76_outside(w,a,0))r76_schedule(e,R76_DELETE_TREE);return !w->fault;
}
int r76_emit(struct WeaponWorld*w,struct NativeEntity*e,unsigned id){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*n;
 if(id<5){for(unsigned i=0;i<assault76_spawn_counts[id];i++){const struct Assault76Spawn*t=&assault76_spawns[id][i];unsigned s=t->kind==0?R76_BOMB:t->kind<5?R76_MUZZLE:R76_MISSILE;n=r76_child(w,e,s);if(!n)return 0;n->payload.enemy.assault76.form=t->kind<5?t->kind-1:t->kind-5;transform_integer_offset(&n->payload.enemy.actor.transform,tk_faced(a,t->x),t->y);n->payload.enemy.actor.transform.facing^=t->facing;n->payload.enemy.actor.transform.priority+=t->priority;}return 1;}
 if(id==5){interaction_priority(w,a,0x4000,0);n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_SMOKE);if(!n)return 0;n->payload.enemy.actor.transform.priority--;n=r76_child(w,e,R76_DEBRIS);if(!n)return 0;n=r76_child(w,e,R76_DEBRIS);if(!n)return 0;n->payload.enemy.actor.transform.facing^=1;return 1;}
 if(id==6){n=r76_child(w,e,R76_FLASH);if(!n)return 0;n->payload.enemy.actor.transform.priority--;transform_integer_offset(&n->payload.enemy.actor.transform,0,16);if(q->assault76.rider==1){n=r76_child(w,e,R76_FLASH);if(!n)return 0;n->payload.enemy.actor.transform.priority--;transform_integer_offset(&n->payload.enemy.actor.transform,0,8);}return 1;}
 if(id==7){for(unsigned i=0;i<(q->assault76.rider==1?2u:1u);i++){if(!q->assault76.variant)n=human_corpse_entity(w,entity_ref(e),a,0);else{n=r76_child(w,e,R76_DELETE);if(n&&!human_charred_transform(w,n))return 0;}if(!n)return 0;if(!i)n->payload.enemy.actor.transform.priority=a->transform.priority-1;transform_integer_offset(&n->payload.enemy.actor.transform,0,i?8:16);}return 1;}
 return enemy_fault(w);
}
/* Unreferenced source helpers remain explicit APIs, not invented AI branches. */
int assault76_spark(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct NativeEntity*n=r76_child(w,e,R76_SPARK);if(!n)return 0;unsigned i=((uint16_t)q->assault76.angle<<3)&255;int16_t y=boss_trig[0][i]>>4,x=boss_trig[1][i]>>4;transform_integer_offset(&n->payload.enemy.actor.transform,-(x+(x>>1)),-(y+(y>>1)));return 1;}
int assault76_spawn_corpses(struct WeaponWorld*w,struct NativeEntity*e){return r76_emit(w,e,7);}
int assault76_missile_resume(struct WeaponWorld*w,struct NativeEntity*e,struct NativeEntity*timer_owner){if(!e||e->kind!=ENTITY_ENEMY||!timer_owner||timer_owner->kind!=ENTITY_ENEMY)return enemy_fault(w);e->payload.enemy.assault76.serial=0;timer_owner->payload.enemy.assault76.life=32767;r76_schedule(e,R76_MISSILE_RESUME);e->tick(w->entities,e,w);return !w->fault;}
struct NativeEntity*assault76_thrown_soldier(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,const struct WeaponActor*a){struct NativeEntity*e=r76_allocate(w,parent,order,&a->transform,R76_GRENADE);if(!e)return 0;/* $0651E6 retains the caller's ballistic launch. */e->payload.enemy.actor.vx=a->vx;e->payload.enemy.actor.vy=a->vy;e->payload.enemy.actor.ax=a->ax;e->payload.enemy.actor.ay=a->ay;r76_schedule(e,R76_GRENADE);return e;}
int assault76_pilot_done(struct WeaponWorld*w,struct NativeEntity*e){if(!e||e->kind!=ENTITY_ENEMY)return enemy_fault(w);r76_schedule(e,R76_PILOT_DONE);e->tick(w->entities,e,w);return !w->fault;}

/* $065F24 compares this depth with the NEXT task depth: child presence. */
int assault76_has_children(struct WeaponWorld*w,const struct NativeEntity*e){return m4_has_children(w,e);}
