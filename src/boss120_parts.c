#include "boss120_internal.h"
/* Eight $040F00 parts; command $80 and acknowledged copy $81 are distinct. */
int b120_follow(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy,*p=tk_parent(w,e);if(!p)return 0;uint8_t group=q->actor.transform.display_group;q->actor.transform=p->actor.transform;q->actor.transform.display_group=group;q->actor.transform.priority+=(int8_t)q->boss120.priority;transform_integer_offset(&q->actor.transform,(int8_t)q->boss120.offset_x,tk_word((int8_t)q->boss120.offset_y+p->boss120.bob_y));q->boss120.upgraded=p->boss120.upgraded;return 1;}
int b120_ack(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*p=tk_parent(w,e);if(!p)return 0;p->dead|=1u<<(e->payload.enemy.boss120.slot&7);return 1;}
int b120_writeback(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy,*p=tk_parent(w,e);if(!p)return 0;unsigned i=q->boss120.slot&7;if(p->boss120.commands[i]==q->boss120.previous){p->boss120.commands[i]=q->boss120.command;q->boss120.previous=q->boss120.command;}return 1;}
int b120_command(struct WeaponWorld*w,struct NativeEntity*e){static const uint8_t states[]={B120_PART_RESTART,B120_PART_BURST,B120_PART_AIM_INIT,B120_PART_HOLD,B120_PART_AIM,B120_PART_CENTER,B120_PART_FIRE,B120_PART_FIRE_END,B120_PART_SCRIPT_AIM_INIT,B120_PART_SCRIPT_FIRE,B120_DELETE_TREE};struct EnemyActor*q=&e->payload.enemy,*p=tk_parent(w,e);if(!p)return 0;uint8_t c=p->boss120.commands[q->boss120.slot&7];if(c==q->boss120.command)return 1;q->boss120.command=q->boss120.previous=c;if(c&128){unsigned i=c&127;if(i>=sizeof(states))return enemy_fault(w);b120_schedule(e,states[i]);}else b120_schedule(e,B120_PART_ANIM);return 1;}
static int parent_events(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*p=tk_parent(w,e);if(!p)return 0;if(p->actor.hit.events&128)e->payload.enemy.actor.hit.events|=1;return 1;}
static int parent_tail(struct WeaponWorld*w,struct NativeEntity*e){struct NativeEntity*p=entity_get(w->entities,e->parent);if(!p)return enemy_fault(w);if(p->deleting)b120_schedule(e,B120_DELETE_TREE);return 1;}
static int hit_tail(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;if(q->actor.hit.flags&8){q->boss120.hit_count++;q->actor.hit.invulnerable=30;b120_schedule(e,B120_PART_HURT);}return parent_tail(w,e);}
static int16_t target_x(struct WeaponWorld*w,unsigned i){struct PlayerBodyState*p=i<2?w->players[i]:0;return p?tk_word((p->actor.transform.x>>8)-w->camera_x):-1;}
static int16_t target_y(struct WeaponWorld*w,unsigned i){struct PlayerBodyState*p=i<2?w->players[i]:0;return p?tk_word(transform_y_up(&p->actor.transform)+w->camera_y):-1;}
static unsigned aim_sector(unsigned angle){if(angle<=112)angle=112;if(angle>=192)angle=192;angle=(angle-104)>>3;if(angle>=10)angle=10;return angle&254;}
int b120_part_enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned s=a->state,late=q->boss120.upgraded==255;int i;struct EnemyActor*p;
 switch(s){
 case B120_PART_NEW:a->hit_palette=0x87;a->palette=boss120_palettes[q->boss120.slot&7];/* FALLTHROUGH */
 case B120_PART_RESTART:q->boss120.command=q->boss120.previous=128;b120_start_entry(q,boss120_part[q->boss120.slot&7]);a->state=B120_PART_IDLE;break;
 case B120_PART_ANIM:q->boss120.ack=0;b120_start_entry(q,boss120_command[q->boss120.command&15]);break;
 case B120_PART_BURST:q->boss120.angle=0;q->boss120.ack=0;q->boss120.count=b120_rank(w,q,B120R_BURST_COUNT+late);q->boss120.period=b120_rank(w,q,B120R_BURST_RELOAD+late);q->boss120.timer=q->boss120.period;q->boss120.remaining=q->boss120.count;b120_start(q,B120A_BURST);break;
 case B120_PART_AIM_INIT:q->boss120.angle=2;/* FALLTHROUGH */
 case B120_PART_AIM:q->boss120.goal_angle=10;b120_start(q,B120A_AIM);i=enemy_nearest_x(w,a,0);q->boss120.target=i<0?1:(uint8_t)i;p=tk_parent(w,e);if(!p)return 0;p->boss120.target=q->boss120.target;a->hit.flags&=247;a->state=B120_PART_AIM;break;
 case B120_PART_HOLD:a->hit.flags&=247;b120_start(q,B120A_AIM);break;
 case B120_PART_CENTER:a->hit.flags&=247;q->boss120.timer=0;q->boss120.ack=0;q->boss120.goal_angle=2;b120_start(q,B120A_AIM);break;
 case B120_PART_FIRE:a->hit.flags&=247;q->boss120.ack=0;b120_defense(q,65535);i=(uint16_t)q->boss120.angle>>1;if(i>=6)i=5;b120_start_entry(q,boss120_fire[i]);break;
 case B120_PART_FIRE_END:a->hit.flags&=247;b120_start(q,B120A_AIM);break;
 case B120_PART_HURT:a->hit.hp=1;b120_start(q,B120A_HURT);break;
 case B120_PART_SCRIPT_AIM_INIT:q->boss120.angle=2;/* FALLTHROUGH */
 case B120_PART_SCRIPT_AIM:b120_defense(q,65535);q->boss120.timer=0;q->boss120.ack=0;q->boss120.goal_angle=8;b120_start(q,B120A_AIM_SCRIPTED);a->state=B120_PART_SCRIPT_AIM;break;
 case B120_PART_SCRIPT_FIRE:q->boss120.ack=0;b120_defense(q,65535);i=(uint16_t)q->boss120.angle>>1;if(i>=6)i=5;b120_start_entry(q,boss120_scripted_fire[i]);break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static int animation_frame(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;if(!b120_follow(w,e)||!parent_events(w,e))return 0;if(b120_animate(w,e)){if(!b120_command(w,e))return 0;if(!q->boss120.ack){q->boss120.ack=255;if(!b120_ack(w,e))return 0;}}return parent_tail(w,e);}
int b120_part_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;int done;
 if(s==B120_PART_ANIM)return animation_frame(w,e);
 if(s==B120_PART_BURST){p=tk_parent(w,e);if(!p)return 0;if(p->actor.hit.flags&1)a->state=B120_PART_ANIM;else{q->boss120.timer=tk_word(q->boss120.timer-1);if(q->boss120.timer<=0){q->boss120.timer=q->boss120.period;b120_start(q,B120A_BURST);q->boss120.remaining--;if((int8_t)q->boss120.remaining<=0){q->boss120.remaining=q->boss120.count;q->boss120.angle=tk_word(q->boss120.angle+1);if(q->boss120.angle>10){b120_start(q,B120A_BURST_END);a->state=B120_PART_ANIM;}}}}return animation_frame(w,e);}
 if(!b120_follow(w,e))return 0;
 if(s==B120_PART_IDLE){if(!parent_events(w,e))return 0;b120_animate(w,e);if(!b120_command(w,e))return 0;return parent_tail(w,e);}
 if(s==B120_PART_AIM){p=tk_parent(w,e);if(!p)return 0;p->boss120.aimed=0;q->boss120.timer=(q->boss120.timer+1)&1;if(!q->boss120.timer){uint8_t angle=enemy_angle(tk_word(target_x(w,q->boss120.target)-tk_sx(w,a)+32),tk_word(target_y(w,q->boss120.target)-tk_sy(w,a)));q->boss120.goal_angle=aim_sector(angle);if(q->boss120.goal_angle==q->boss120.angle)p->boss120.aimed=1;else q->boss120.angle=tk_word(q->boss120.angle+((uint16_t)q->boss120.goal_angle>(uint16_t)q->boss120.angle?1:-1));}}
 if(s==B120_PART_FIRE)a->hit.invulnerable=2;done=b120_animate(w,e);if(w->fault)return 0;
 switch(s){
 case B120_PART_HOLD:case B120_PART_AIM:break;
 case B120_PART_CENTER:case B120_PART_SCRIPT_AIM:q->boss120.timer=(q->boss120.timer+1)&1;if(!q->boss120.timer&&b120_turn(q,1)&&!q->boss120.ack){q->boss120.ack=255;if(!b120_ack(w,e))return 0;}break;
 case B120_PART_FIRE:case B120_PART_SCRIPT_FIRE:if(done&&!q->boss120.ack){q->boss120.ack=255;if(!b120_ack(w,e))return 0;q->boss120.command=0x83;if(!b120_writeback(w,e))return 0;b120_schedule(e,s==B120_PART_FIRE?B120_PART_HOLD:B120_PART_SCRIPT_AIM);}break;
 case B120_PART_FIRE_END:if(!b120_ack(w,e))return 0;q->boss120.command=0x83;if(!b120_writeback(w,e))return 0;b120_schedule(e,B120_PART_HOLD);break;
 case B120_PART_HURT:if(done){a->hit.flags&=247;a->hit.hp=1;q->boss120.command=0;}break;
 default:return enemy_fault(w);
 }if(!b120_command(w,e))return 0;if(s==B120_PART_SCRIPT_AIM||s==B120_PART_SCRIPT_FIRE)return !w->fault;if(s==B120_PART_HURT)return parent_tail(w,e);return hit_tail(w,e);
}
