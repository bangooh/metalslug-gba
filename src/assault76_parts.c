#include "assault76_internal.h"
/* $064D98: rider commands are consumed once and acknowledged on the parent. */
static int pilot_follow(struct WeaponWorld*w,struct NativeEntity*e){if(!r76_follow(w,e))return -1;struct EnemyActor*q=&e->payload.enemy,*p=tk_parent(w,e);if(!p)return -1;p->dead=q->dead;q->assault76.drive=p->assault76.drive;unsigned cmd=p->actor.phase&15;if(cmd!=15){p->actor.phase=15;q->actor.phase=cmd;r76_entry(q,assault76_pilot[cmd]);}return (int)cmd;}
static int aim(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;int i=enemy_nearest_x(w,a,0);if(i<0){q->assault76.angle&=7;return 0;}struct PlayerTransform*t=&w->players[i]->actor.transform;unsigned angle=enemy_angle(tk_word((t->x-a->transform.x)>>8),tk_word(transform_y_up(t)-transform_y_up(&a->transform)));if(angle<128)angle=256-angle;unsigned sector=(angle-128)>>3;if(sector>16)return enemy_fault(w);int target=assault76_aim[a->transform.facing&1][sector];if(target==q->assault76.angle){q->assault76.angle&=7;return 1;}if(!(w->frame_phase&1))q->assault76.angle+=target>q->assault76.angle?1:-1;q->assault76.angle&=7;return 0;}
int r76_parts_enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned i;
 switch(a->state){
 case R76_PILOT:q->assault76.count=r76_rank(w,q,R76R_SHOTS);q->assault76.clock=8;a->phase=0;a->palette=0x45;a->state=R76_PILOT_IDLE;/* FALLTHROUGH */
 case R76_PILOT_IDLE:r76_entry(q,assault76_pilot[a->phase&15]);q->dead=1;break;
 case R76_PILOT_SHOT:q->assault76.count=tk_word(q->assault76.count-1);if(q->assault76.count<=0){q->assault76.count=r76_rank(w,q,R76R_SHOTS);q->assault76.clock=r76_rank(w,q,R76R_SHOT_LONG);}else q->assault76.clock=r76_rank(w,q,R76R_SHOT_SHORT);r76_start(q,R76A_PILOT_SHOT);break;
 case R76_PILOT_OPEN:q->assault76.angle=0;q->dead=2;r76_start(q,R76A_PILOT_OPEN);break;
 case R76_PILOT_REARM:q->assault76.clock=6;q->assault76.count=r76_rank(w,q,R76R_BURST);q->assault76.serial=0;q->dead=3;r76_start(q,R76A_PILOT_AIM);a->state=R76_PILOT_AIM;break;
 case R76_PILOT_NEXT:q->assault76.serial=tk_word(q->assault76.serial+1);q->assault76.count=tk_word(q->assault76.count-1);if(q->assault76.count<=0){q->assault76.clock=r76_rank(w,q,R76R_LONG_WAIT);q->assault76.count=r76_rank(w,q,R76R_BURST);q->assault76.serial=0;}else q->assault76.clock=r76_rank(w,q,R76R_SHORT_WAIT);q->dead=3;r76_start(q,R76A_PILOT_AIM);a->state=R76_PILOT_AIM;break;
 case R76_PILOT_FIRE:i=(uint16_t)assault76_fire_sector[q->assault76.angle&7];if(i>=4)return enemy_fault(w);r76_entry(q,assault76_fire[i]);break;
 case R76_PILOT_EJECT:if(pilot_follow(w,e)<0||!r76_child(w,e,R76_DEBRIS))return 0;r76_schedule(e,R76_DELETE);return 0;
 case R76_PILOT_DONE:q->dead=4;break;
 case R76_OVERLAY:a->palette=16;r76_start(q,R76A_OVERLAY);break;
 case R76_OVERLAY_DEAD:r76_start(q,R76A_OVERLAY_DEAD);break;
 case R76_TRAIL:q->assault76.offset=tk_faced(a,20);break;
 case R76_ESCAPE:a->vy=0x32a;a->ay=-81;a->vx=tk_word(-a->vx);a->palette=14;infantry_free_escape(e,255);e->tick(w->entities,e,w);return 0;
 default:return enemy_fault(w);
 }return !w->fault;
}
int r76_parts_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;int cmd;
 if(s==R76_TRAIL){if(!(w->frame_phase&3)){if(!r76_follow(w,e))return 0;transform_integer_offset(&a->transform,q->assault76.offset,0);struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,BFX_TRAIL77);if(!n)return 0;n->payload.enemy.actor.transform.priority=0x4000;}if(r76_parent_done(w,e))r76_schedule(e,R76_DELETE_TREE);return !w->fault;}
 if(s==R76_OVERLAY||s==R76_OVERLAY_DEAD){if(s==R76_OVERLAY){if(r76_parent_done(w,e))r76_schedule(e,R76_DELETE_TREE);if(!r76_follow(w,e))return 0;struct EnemyActor*p=tk_parent(w,e);if(!p)return 0;if(p->actor.phase==9)r76_schedule(e,R76_OVERLAY_DEAD);}else if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;if(r76_animate(w,e)&&s==R76_OVERLAY_DEAD)r76_schedule(e,R76_DELETE_TREE);if(r76_outside(w,a,1))r76_schedule(e,R76_DELETE_TREE);return !w->fault;}
 cmd=pilot_follow(w,e);if(cmd<0)return 0;if(cmd==9){r76_schedule(e,R76_PILOT_EJECT);return 2;}if(s==R76_PILOT_IDLE&&cmd==8){r76_schedule(e,R76_PILOT_OPEN);return 2;}
 switch(s){
 case R76_PILOT_IDLE:r76_animate(w,e);if(tk_sx(w,a)>=32&&tk_sx(w,a)<=288){uint16_t before=q->assault76.clock;q->assault76.clock=tk_word(before-1);if(!before)q->assault76.clock=0;if(q->assault76.drive&&q->assault76.clock<=0)r76_schedule(e,R76_PILOT_SHOT);}break;
 case R76_PILOT_SHOT:if(r76_animate(w,e))r76_schedule(e,R76_PILOT_IDLE);break;
 case R76_PILOT_OPEN:if(r76_animate(w,e))r76_schedule(e,R76_PILOT_REARM);break;
 case R76_PILOT_AIM:if(aim(w,e)){q->assault76.clock=tk_word(q->assault76.clock-1);if(q->assault76.clock<=0){q->assault76.clock=0;if(tk_sx(w,a)>=16)r76_schedule(e,R76_PILOT_FIRE);}}if((uint16_t)q->assault76.angle>=8)return enemy_fault(w);r76_animate(w,e);break;
 case R76_PILOT_FIRE:if(r76_animate(w,e))r76_schedule(e,R76_PILOT_NEXT);break;
 case R76_PILOT_DONE:r76_animate(w,e);r76_schedule(e,R76_PILOT_IDLE);a->phase=0;/* $065016 reads ROM byte $000004, verified zero, not immediate 4. */break;
 default:return enemy_fault(w);
 }if(r76_parent_done(w,e))r76_schedule(e,R76_DELETE);return !w->fault;
}
