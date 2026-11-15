#include "boss120_internal.h"
static const int16_t default_bounds[]={0,1,0,1};
static int outside(struct WeaponWorld*w,struct WeaponActor*a){return interaction_outside(w,a,default_bounds,1);}
static int16_t px(struct WeaponWorld*w,unsigned i){struct PlayerBodyState*p=i<2?w->players[i]:0;return p?tk_word((p->actor.transform.x>>8)-w->camera_x):-1;}
static int16_t py(struct WeaponWorld*w,unsigned i){struct PlayerBodyState*p=i<2?w->players[i]:0;return p?tk_word(transform_y_up(&p->actor.transform)+w->camera_y):-1;}
static unsigned random_target(struct WeaponWorld*w){unsigned p1=enemy_player_live(w->players[0]),p2=enemy_player_live(w->players[1]);return p1&&p2?(tk_random(w)&1):p2?1:0;}
static int blood(struct WeaponWorld*w,struct NativeEntity*e,int hit){struct WeaponActor*a=&e->payload.enemy.actor;struct NativeEntity*n=b120_child(w,e,B120_BLOOD);if(!n)return 0;struct EnemyActor*q=&n->payload.enemy;q->source_flags=0;q->actor.transform.display_group=0;q->actor.transform.facing=(a->transform.facing&1)?0:1;transform_integer_offset(&q->actor.transform,(a->transform.facing&1)?8:-8,0);q->boss120.alt_shell=hit;return 1;}
static int ground_blood(struct WeaponWorld*w,struct NativeEntity*e,int hit){struct WeaponActor*a=&e->payload.enemy.actor;transform_integer_offset(&a->transform,-32,-8);int r;if(hit)r=blood(w,e,1);else{a->transform.facing^=1;r=blood(w,e,0);a->transform.facing^=1;if(r)r=blood(w,e,0);}transform_integer_offset(&a->transform,32,8);return r;}
static int pair(struct WeaponWorld*w,struct NativeEntity*e,int plain){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(!tk_sound(w,0x1064))return 0;unsigned i=(uint16_t)q->boss120.angle>>1;if(i>=6)i=5;const int16_t*off=boss120_muzzles[i];struct NativeEntity*n=b120_child(w,e,B120_MUZZLE);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,off[0],off[1]);n=b120_child(w,e,B120_SHELL);if(!n)return 0;n->payload.enemy.boss120.angle=i;transform_integer_offset(&n->payload.enemy.actor.transform,off[0],off[1]);n->payload.enemy.boss120.immune_shell=plain?0:q->boss120.immune_shell;n->payload.enemy.boss120.alt_shell=plain?1:q->boss120.alt_shell;if(plain)return 1;q->boss120.command=0x87;(void)a;return b120_writeback(w,e);}
/* $0420E4/$042188: callable alias; no incoming converted animation. */
int boss120_plain_shot(struct WeaponWorld*w,struct NativeEntity*e){return pair(w,e,1);}
int b120_callback(struct WeaponWorld*w,struct NativeEntity*e,unsigned cb){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*n;switch(cb){
 case B120C_283CA:hit_begin_attack(&a->hit);return 1;
 case B120C_42040:return b120_writeback(w,e);
 case B120C_420F6:case B120C_420E8:q->boss120.alt_shell=0;q->boss120.immune_shell=cb==B120C_420E8;return pair(w,e,0);
 case B120C_42206:{if(!tk_sound(w,0x10a3))return 0;unsigned i=boss120_missile_choices[q->boss120.serial++&15]&3;const int16_t*off=boss120_missile_offsets[i];n=b120_child(w,e,B120_MISSILE_MUZZLE);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,off[0],off[1]);n=b120_child(w,e,B120_MISSILE);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,off[0],off[1]);n->payload.enemy.boss120.serial=i;return 1;}
 case B120C_422EA:n=b120_child(w,e,B120_GROUND);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,tk_word(64-tk_word(q->boss120.angle*32)),0);n=b120_child(w,e,B120_TRAIL);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,8,0);n->payload.enemy.selector=q->boss120.angle;return 1;
 case B120C_42324:n=b120_child(w,e,B120_FLASH);if(!n)return 0;n->payload.enemy.actor.transform.priority=0xe000;return 1;
 case B120C_4233E:if(!tk_sound(w,0x109b))return 0;n=b120_child(w,e,B120_BOMB);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,10,-40);return 1;
 case B120C_42366:n=b120_child(w,e,B120_FRAGMENT);if(!n)return 0;transform_integer_offset(&n->payload.enemy.actor.transform,-56,20);n->payload.enemy.actor.vx=a->vx;n->payload.enemy.actor.ay=a->ay;return 1;
 default:return enemy_fault(w);
 }}
int b120_effect_enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;unsigned i;
 switch(a->state){
 case B120_WRECK:interaction_priority(w,a,0xd000,24);a->vx=-1024;a->ax=32;a->vy=256;a->ay=-40;a->palette=0x98;q->dead=0;b120_start(q,B120A_WRECK_FALL);break;
 case B120_WRECK_BOUNCE:if(!ground_blood(w,e,0))return 0;m4_reset(a);a->vy=540;a->ay=-36;b120_start(q,B120A_WRECK_BOUNCE);break;
 case B120_WRECK_GROUND:break;
 case B120_WRECK_WAIT:m4_reset(a);break;
 case B120_WRECK_REMOVE:entity_delete(w->entities,e);return 0;
 case B120_MUZZLE:case B120_MISSILE_MUZZLE:interaction_priority(w,a,0xd000,a->state==B120_MUZZLE?20:28);a->palette=0xb3;b120_start(q,a->state==B120_MUZZLE?B120A_MUZZLE:B120A_MISSILE_MUZZLE);break;
 case B120_SHELL:a->hit.type_flags|=16;if(q->boss120.alt_shell)q->boss120.speed=b120_rank(w,q,B120R_SHELL_ALT_SPEED);else{p=tk_parent(w,e);if(!p)return 0;q->boss120.speed=b120_rank(w,q,B120R_SHELL_SPEED+(p->boss120.upgraded==255));}i=(uint16_t)q->boss120.angle;if(i>=6)return enemy_fault(w);b120_velocity(q,boss120_angles[i]);b120_attack(q,B120B_SHELL_ATTACK);b120_defense(q,B120B_SHELL);a->hit.hp=50;a->palette=0xb4;hit_begin_attack(&a->hit);b120_start_entry(q,boss120_shell[i]);interaction_priority(w,a,0xd000,20);break;
 case B120_SHELL_HIT:if(!tk_sound(w,0x1022))return 0;if(q->boss120.immune_shell){b120_attack(q,B120B_SHELL_IMPACT);hit_begin_attack(&a->hit);if(!enemy_attack(w,a))return 0;}/* FALLTHROUGH */
 case B120_SHELL_SMOKE:b120_defense(q,65535);interaction_priority(w,a,0x4000,0);boss_fx_transform(w,e,BFX_SMOKE);return 0;
 case B120_FIRE:interaction_priority(w,a,0x4000,0);b120_defense(q,65535);boss_fx_transform(w,e,BFX_FIRE);return 0;
 case B120_GROUND:a->hit.type_flags|=16;b120_start(q,B120A_GROUND);transform_integer_offset(&a->transform,(tk_random(w)&7)-4,0);if(!w->secondary_view_valid)return enemy_fault(w);m4_screen(w,a,tk_sx(w,a),tk_word(512-(1376-640)+w->secondary_y_offset));if(outside(w,a)){entity_delete_children(w->entities,e);entity_delete(w->entities,e);return 0;}interaction_priority(w,a,0xd000,20);a->palette=0x193;b120_schedule(e,B120_GROUND_ACTIVE);return 0;
 case B120_GROUND_ACTIVE:break;
 case B120_TRAIL:{const int16_t*t=boss120_trail[tk_random(w)&3][q->selector&15];a->vx=t[0];a->ay=tk_word(t[1]*2);a->vy=0;q->boss120.timer=0;a->palette=0x9b;interaction_priority(w,a,0x4000,28);b120_start(q,B120A_TRAIL);break;}
 case B120_BOMB:a->hit_palette=0x89;a->hit.type_flags|=16;a->palette=0x99;a->vy=-512;a->ay=-64;a->hit.hp=b120_rank(w,q,B120R_BOMB_HP);b120_start(q,B120A_BOMB);break;
 case B120_BOMB_HIT:case B120_MISSILE_HIT:if(!tk_sound(w,0x1022))return 0;interaction_priority(w,a,0x4000,0);b120_defense(q,65535);boss_fx_transform(w,e,BFX_FIRE);return 0;
 case B120_MISSILE:a->hit.type_flags|=16;q->selector=0;q->boss120.speed=512;interaction_priority(w,a,0xd000,20);a->palette=0x9c;b120_start(q,B120A_MISSILE);q->boss120.path=(struct FlightState){.cursor=boss120_paths[1+(q->boss120.serial&3)]};break;
 case B120_MISSILE_FLY:q->boss120.timer=0;q->boss120.angle=128;q->scale_x=q->scale_y=255;a->hit.hp=b120_rank(w,q,B120R_MISSILE_HP);p=tk_parent(w,e);if(!p)return 0;i=(uint16_t)b120_rank(w,q,B120R_MISSILE_CONFIG+(p->boss120.upgraded==255));if(i>=8)return enemy_fault(w);q->boss120.speed=boss120_missile_configs[i][0];q->boss120.period=boss120_missile_configs[i][1];q->boss120.target=random_target(w);q->boss120.life=0;b120_start(q,B120A_MISSILE_FLY);break;
 case B120_FRAGMENT:a->palette=0x98;b120_start(q,B120A_FRAGMENT);break;
 case B120_FLASH:a->palette=4;b120_start(q,B120A_FLASH);a->transform.facing|=tk_random(w)&1;q->scale_x=q->scale_y=255;if(interaction_outside(w,a,boss120_bounds[2],1)){entity_delete(w->entities,e);return 0;}b120_defense(q,65535);break;
 case B120_BLOOD:a->palette=w->enemies->blood_enabled?0x17a:0x1ba;interaction_priority(w,a,0xc000,16);b120_start(q,q->boss120.alt_shell?B120A_BLOOD_HIT:B120A_BLOOD_GROUND);break;
 case B120_RADIAL:{unsigned angle=(tk_random(w)&63)+64*q->boss120.position;if(angle>=256)return enemy_fault(w);a->vx=tk_word(boss_trig[1][angle]*32);a->vy=tk_word(boss_trig[0][angle]*32);q->boss120.timer=q->boss120.period=3;q->boss120.dash_speed=4;b120_schedule(e,B120_RADIAL_ACTIVE);return 0;}
 case B120_RADIAL_ACTIVE:break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static int missile_tail(struct WeaponWorld*w,struct NativeEntity*e){struct NativeEntity*p=entity_get(w->entities,e->parent),*root=p?entity_get(w->entities,p->parent):0;if(!root||root->kind!=ENTITY_ENEMY)return enemy_fault(w);root->payload.enemy.boss120.missiles++;if(root->payload.enemy.actor.hit.flags&1)b120_schedule(e,B120_FIRE);if(interaction_outside(w,&e->payload.enemy.actor,boss120_bounds[1],1))b120_schedule(e,B120_DELETE_TREE);return !w->fault;}
static int wreck_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;int r=interaction_move(w,a,s==B120_WRECK||s==B120_WRECK_BOUNCE?WMOVE_FALL:WMOVE_SYNC);if(r<0)return 0;if(s==B120_WRECK){if(r)b120_schedule(e,B120_WRECK_BOUNCE);b120_animate(w,e);if(a->vx>=-128)a->ax=0;if(q->dead)b120_schedule(e,B120_WRECK_WAIT);return !w->fault;}if(s==B120_WRECK_WAIT){if(q->dead!=1)b120_schedule(e,B120_WRECK_REMOVE);return 1;}if(s==B120_WRECK_BOUNCE&&r){a->display_flags|=2;b120_defense(q,B120B_WRECK);b120_start(q,B120A_WRECK_GROUND);a->state=B120_WRECK_GROUND;if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;}b120_animate(w,e);if(a->hit.flags&8){a->hit.flags&=247;if(!tk_sound(w,0x103f)||!ground_blood(w,e,1))return 0;b120_start(q,B120A_WRECK_HIT);}return !w->fault;}
int b120_effect_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;int r,done;
 if(s>=B120_WRECK&&s<=B120_WRECK_WAIT)return wreck_frame(w,e,s);
 if(s==B120_MISSILE){if(b120_path(w,e)>0)b120_schedule(e,B120_MISSILE_FLY);unsigned angle=q->selector>>8,v=angle;if(v>=128)v=255-v;if(v>=64)v=127-v;if(v<24)v=0;q->scale_x=q->scale_y=128+2*v;a->transform.priority+=v;if(angle>=128)angle=255-angle;q->boss120.angle=angle>>3;b120_animate(w,e);return missile_tail(w,e);}
 if(s==B120_MISSILE_FLY){q->boss120.life=tk_word(q->boss120.life+1);if(q->boss120.life<=180){q->boss120.timer=tk_word(q->boss120.timer-1);if(q->boss120.timer<=0){q->boss120.timer=q->boss120.period;q->boss120.goal_angle=enemy_angle(tk_word(px(w,q->boss120.target)-tk_sx(w,a)),tk_word(py(w,q->boss120.target)-tk_sy(w,a)));b120_turn(q,4);b120_velocity(q,q->boss120.angle);}}r=interaction_move(w,a,WMOVE_POINT);if(r<0)return 0;if(r)b120_schedule(e,B120_MISSILE_HIT);q->selector=(uint16_t)q->boss120.angle>>3;b120_animate(w,e);if(!enemy_attack(w,a))return 0;if(a->hit.flags&2)b120_schedule(e,B120_FIRE);a->hit.flags&=247;if(a->hit.flags&1)b120_schedule(e,B120_MISSILE_HIT);return missile_tail(w,e);}
 if(s==B120_SHELL){if(q->boss120.alt_shell)a->hit.flags|=64;r=interaction_move(w,a,q->boss120.alt_shell?WMOVE_BODY:WMOVE_POINT);if(r<0)return 0;if(r)b120_schedule(e,B120_SHELL_HIT);b120_animate(w,e);if(!q->boss120.immune_shell){if(!enemy_attack(w,a))return 0;if(a->hit.flags&2)b120_schedule(e,B120_SHELL_SMOKE);a->hit.flags&=247;if(a->hit.flags&1)b120_schedule(e,B120_SHELL_HIT);}if(outside(w,a))b120_schedule(e,B120_DELETE_TREE);return !w->fault;}
 if(s==B120_RADIAL_ACTIVE){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;q->boss120.timer=tk_word(q->boss120.timer-1);if(q->boss120.timer)return 1;q->boss120.timer=q->boss120.period;/* $0812D2 moves only position, without changing velocity/acceleration. */int32_t x=a->transform.x+a->vx,y=131072-a->transform.y+a->vy;a->transform.x=(int16_t)(x>>8)*256+(x&255);a->transform.y=131072-((int16_t)(y>>8)*256+(y&255));if(!boss_sprite_admitted(w)){if(!w->fault)entity_delete(w->entities,e);return !w->fault;}struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_BLAST);if(!n)return 0;n->payload.enemy.scale_x=q->scale_x;n->payload.enemy.scale_y=q->scale_y;q->boss120.dash_speed=tk_word(q->boss120.dash_speed-1);if(!q->boss120.dash_speed)entity_delete(w->entities,e);return 1;}
 r=interaction_move(w,a,s==B120_TRAIL||s==B120_BOMB||s==B120_FRAGMENT?WMOVE_FALL:WMOVE_SYNC);if(r<0)return 0;
 if(s==B120_TRAIL){if(r)b120_schedule(e,B120_DELETE_TREE);q->boss120.timer=tk_word(q->boss120.timer+1);if(q->boss120.timer>=15)b120_schedule(e,B120_DELETE_TREE);if(q->boss120.timer<8||(q->boss120.timer&1))b120_animate(w,e);if(outside(w,a))b120_schedule(e,B120_DELETE_TREE);return !w->fault;}
 if(s==B120_BOMB&&r)b120_schedule(e,B120_BOMB_HIT);if(s==B120_FRAGMENT&&r)b120_schedule(e,B120_FIRE);done=b120_animate(w,e);if(w->fault)return 0;
 switch(s){
 case B120_MUZZLE:case B120_MISSILE_MUZZLE:if(done)b120_schedule(e,B120_DELETE_TREE);break;
 case B120_GROUND_ACTIVE:if(done)b120_schedule(e,B120_DELETE_TREE);if(!enemy_attack(w,a))return 0;break;
 case B120_BOMB:if(!enemy_attack(w,a))return 0;if(a->hit.flags&2)b120_schedule(e,B120_FIRE);a->hit.flags&=247;if(a->hit.flags&1)b120_schedule(e,B120_BOMB_HIT);break;
 case B120_FRAGMENT:if(done)b120_schedule(e,B120_FIRE);break;
 case B120_FLASH:if(done||interaction_outside(w,a,boss120_bounds[2],1))entity_delete(w->entities,e);return 1;
 case B120_BLOOD:if(done)entity_delete(w->entities,e);return 1;
 default:return enemy_fault(w);
 }if(outside(w,a))b120_schedule(e,B120_DELETE_TREE);return !w->fault;
}
