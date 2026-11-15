#include "mission_result_internal.h"
#include "scene_runtime.h"
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static void schedule(struct EnemyActor*q,unsigned s){enemy_schedule(q,s);}
static int16_t word(int n){return (int16_t)(uint16_t)n;}
struct NativeEntity*mr_allocate(struct WeaponWorld*w,struct EntityRef p,unsigned s){struct NativeEntity*e=result_allocate(w,p,ENTITY_FIRST_CHILD,s);if(e){e->tick=tick;e->payload.enemy.mission_result=(struct MissionResultActor){0};}return e;}
struct NativeEntity*mr_child(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct NativeEntity*n=mr_allocate(w,entity_ref(e),s);if(n){struct MissionResultActor*a=&n->payload.enemy.mission_result,*b=&e->payload.enemy.mission_result;a->slot=b->slot;a->cell=b->cell;n->payload.enemy.actor.owner=e->payload.enemy.actor.owner;n->payload.enemy.actor.score_owner=e->payload.enemy.actor.score_owner;}return n;}
struct EnemyActor*mr_parent(struct WeaponWorld*w,struct NativeEntity*e){struct NativeEntity*n=entity_get(w->entities,e->parent);if(!n||n->kind!=ENTITY_ENEMY){enemy_fault(w);return 0;}return &n->payload.enemy;}
void mr_start(struct EnemyActor*q,uint16_t graph){animation_begin(&q->actor.animation,graph);q->actor.movement_flags|=32;}
int mr_animate(struct WeaponWorld*w,struct NativeEntity*e){struct Animation*a=&e->payload.enemy.actor.animation;animation_step(a,&mission_result_program,0);return a->fault?enemy_fault(w):1;}
void mr_screen(struct WeaponWorld*w,struct WeaponActor*a,int16_t x,int16_t y){a->transform.x=(int32_t)word(w->camera_x+x)*256;a->transform.y=(int32_t)word(512+w->camera_y-y)*256;}
static void erase(struct WeaponWorld*w,struct NativeEntity*e){entity_delete_children(w->entities,e);entity_delete(w->entities,e);}
static int accel(struct MissionResultActor*m,const struct MissionResultActor*p){if(p->skip==1&&(int16_t)m->clock>1)m->clock=1;m->clock--;return (int16_t)m->clock<0;}
static int vehicle(struct WeaponWorld*w,struct EnemyActor*q){uint32_t value;return w->enemies->slug&&slug_rider_parameter(w->enemies->slug,q->actor.score_owner,&value);}
void results_snapshot(struct ResultWorld*r){r->progress_snapshot=r->progress;for(unsigned i=0;i<2;i++)r->active[i]=r->live_active[i];r->summary_valid=r->mission_valid;}
void results_continue(struct ResultWorld*r,unsigned slot){if(slot<2&&r->continues[slot]!=255)r->continues[slot]++;}
void results_clear_prisoners(struct WeaponWorld*w){if(w->interactions)w->interactions->rescued[0]=w->interactions->rescued[1]=0;else enemy_fault(w);}
static int setup(struct WeaponWorld*w,struct NativeEntity*e){struct MissionResultActor*m=&e->payload.enemy.mission_result;struct ResultWorld*r=w->results;uint16_t count[2]={0,0};m->counting=m->bonuses=m->skip=m->done=0;
 for(unsigned i=0;i<2;i++)if(r->live_active[i]==1){struct NativeEntity*n=mr_child(w,e,MR_COLUMN);if(!n)return 0;n->payload.enemy.mission_result.slot=i;m->counting++;m->bonuses++;count[i]=w->interactions->rescued[i];}
 for(unsigned i=0;i<2;i++){unsigned n=r->prisoners[i]+(uint8_t)count[i];r->prisoners[i]=n>255?255:n;}
 m->winner=count[1]<count[0]?0:count[1]>count[0]?1:2;m->participants=m->counting;if(!m->counting){schedule(&e->payload.enemy,MR_FADEOUT);return 1;}
 if(!mr_text(w,0x146,MRSTR_TITLE,3,0))return 0;
 schedule(&e->payload.enemy,MR_WAIT);return 1;
}
static int label(struct WeaponWorld*w,struct NativeEntity*e,int final){struct EnemyActor*q=&e->payload.enemy;struct MissionResultActor*m=&q->mission_result;unsigned string=m->mode==0?MRSTR_WINNER:m->mode==1?MRSTR_NO_PRISONER:MRSTR_DRAW;
 if(final&&m->mode==0){if(!interaction_sound(w,0x1083)||!item_score_add(w->items,q->actor.score_owner,0x100000)||!result_rect(w,0xcf+m->cell,0x4b62,2,2,1))return 0;if(m->count<10&&!vehicle(w,q)&&!result_rect(w,0x10f+m->cell,0x4b60,2,2,1))return 0;}
 return mr_text(w,(m->mode==2?0xf1:0xd1)+m->cell,string+(!final&&m->blink?1:0),4,m->mode!=1);
}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct MissionResultActor*m=&q->mission_result;struct ResultWorld*r=w->results;struct NativeEntity*n;
 switch(a->state){
 case MR_NEW:if(r->live_active[0]!=1&&r->live_active[1]!=1){w->enemies->scripted_player_lock=0;erase(w,e);return 0;}m->clock=mission_result_delays[0][r->level&7];schedule(q,MR_COUNTDOWN);return 0;
 case MR_FADE:if(!w->scene_runtime)return enemy_fault(w);source_audio_byte(&w->scene_runtime->audio,6);if(!w->audio_control||!w->audio_control(w->scene,10,0x80)||!scene_rgb_fade_spawn(w,entity_ref(e),24,24,24,5))return enemy_fault(w);m->clock=15;a->state=MR_FADE_WAIT;break;
 case MR_SETUP:setup(w,e);return 0;
 case MR_FADEOUT:if(!scene_rgb_fade_spawn(w,entity_ref(e),0,0,0,3))return 0;m->clock=10;schedule(q,MR_TEARDOWN);return 0;
 case MR_COLUMN:a->phase=0;a->owner=a->score_owner=m->slot;m->cell=m->slot?0x220:0;if(!mr_child(w,e,MR_ROSTER))return 0;a->palette=0x182;q->private_palette_flags=8;mr_screen(w,a,m->slot?184:48,400);m->count=w->interactions->rescued[m->slot];if(!mr_child(w,e,MR_FRAME)||!mr_text(w,0x149+m->cell,m->slot?MRSTR_P2:MRSTR_P1,4,1))return 0;m->remaining=m->count;a->transform.priority=0xe000;mr_start(q,MRA_COLUMN);m->clock=5;schedule(q,MR_SCORE);return 0;
 case MR_PAUSE:m->clock=10;break;
 case MR_VEHICLE:if(vehicle(w,q)){if(!item_score_add(w->items,a->score_owner,0x10000)||!interaction_sound(w,0x10d8))return 0;n=mr_child(w,e,MR_VEHICLE_ICON);if(!n)return 0;n->payload.enemy.actor.transform=a->transform;mr_screen(w,&n->payload.enemy.actor,m->slot?184:48,372);if(!mr_two_digits(w,0x10f+m->cell,(uint16_t)(m->count+10),0))return 0;if(!m->count&&(!result_rect(w,0x18f+m->cell,0x4b60,2,2,1)||!result_rect(w,0x1cf+m->cell,0x4b60,2,2,1)))return 0;}a->state=MR_VEHICLE_WAIT;break;
 case MR_PRIZE:{a->palette=0xa2;q->private_palette_flags=8;a->transform.priority=65535;a->display_flags|=64;mr_start(q,MRA_PRIZE);a->vx=a->vy=a->ax=a->ay=0;unsigned i=m->remaining>14?14:m->remaining;int x=mission_prize_x[m->slot][i];mr_screen(w,a,x,284);unsigned angle=enemy_angle(word((m->slot?232:96)-x),125);a->trail_angle=angle;weapon_vector(angle,0x1000,&a->vx,&a->vy);a->state=MR_PRIZE_FLY;break;}
 case MR_PRIZE_DONE:{struct EnemyActor*p=mr_parent(w,e);if(!p||!interaction_sound(w,0x10d6))return 0;p->actor.phase=255;if(!mr_two_digits(w,0x14c+m->cell,m->count,0))return 0;erase(w,e);return 0;}
 case MR_LABEL:m->remaining=6;m->clock=0;m->blink=0;break;
 case MR_LABEL_FINISH:if(!label(w,e,1))return 0;a->state=MR_IDLE;break;
 case MR_VEHICLE_ICON:a->palette=3;q->private_palette_flags=1;q->scale_x=q->scale_y=127;interaction_priority(w,a,0xe000,16);mr_start(q,MRA_VEHICLE);a->vx=a->vy=a->ax=a->ay=0;break;
 case MR_FRAME:mr_frame(w,e);return 0;
 default:break;
 }return !w->fault;
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){(void)p;struct WeaponWorld*w=world;if(w->fault)return;if(!w->results||!w->results->ready||!w->results->mission_valid||!w->interactions||!w->enemies||!w->items){enemy_fault(w);return;}struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct MissionResultActor*m=&q->mission_result;if(q->entry){q->entry=0;if(!enter(w,e))return;}if(e->deleting)return;
 if(((a->state>=MR_SCORE&&a->state<=MR_ANIMATE)||a->state==MR_PRIZE_FLY||a->state==MR_VEHICLE_ICON)&&!scene_preserve_screen(w,a))return;
 struct EnemyActor*parent=0;struct MissionResultActor*root=0;if(a->state>=MR_SCORE&&a->state<=MR_ANIMATE){parent=mr_parent(w,e);if(!parent)return;root=&parent->mission_result;}
 int draw=0;switch(a->state){
 case MR_COUNTDOWN:case MR_FADE_WAIT:m->clock--;if((int16_t)m->clock<=0)schedule(q,a->state==MR_COUNTDOWN?MR_FADE:MR_SETUP);break;
 case MR_WAIT:if(!w->results->start_valid){enemy_fault(w);return;}m->skip=!!((w->results->start[0]|w->results->start[1])&32);if(m->done)schedule(q,MR_FADEOUT);break;
 case MR_TEARDOWN:m->clock--;if((int16_t)m->clock<=0){if(!result_rect(w,0x46,0x2320,36,22,0))return;entity_delete_children(w->entities,e);m->clock=mission_result_delays[1][w->results->level&7];schedule(q,MR_BANNER_WAIT);}break;
 case MR_BANNER_WAIT:m->clock--;if((int16_t)m->clock<=0){if(!mission_banner_spawn(w,entity_ref(e),1))return;schedule(q,MR_IDLE);}break;
 case MR_IDLE:break;
 case MR_SCORE:if(accel(m,root)){if(!mr_two_digits(w,0x14c+m->cell,0,0))return;m->clock=30;schedule(q,MR_COUNT);}break;
 case MR_COUNT:if(accel(m,root)){if(!m->remaining){root->counting--;m->clock=30;schedule(q,MR_WAITOTHER);}else{m->remaining--;w->interactions->rescued[m->slot]=m->remaining;struct NativeEntity*n=mr_child(w,e,MR_PRIZE);if(!n)return;n->payload.enemy.mission_result.count=m->count-m->remaining;n->payload.enemy.mission_result.remaining=m->remaining;m->clock=10;}}draw=1;break;
 case MR_WAITOTHER:if(!root->counting){m->clock=20;schedule(q,MR_BONUS);}draw=1;break;
 case MR_BONUS:if(accel(m,root)){if(!result_stream(w,0x1cd+m->cell,&mission_result_bonus,0x2300)||!mr_two_digits(w,0x1cf+m->cell,0,0))return;m->remaining=0;schedule(q,MR_TOTAL);}draw=1;break;
 case MR_TOTAL:if(accel(m,root)){m->clock=3;if(m->remaining&&(!interaction_sound(w,0x10d8)||!item_score_add(w->items,a->score_owner,0x1000)||!mr_two_digits(w,0x14f+m->cell,0,0)||!mr_two_digits(w,0x1cf+m->cell,0,1)||!mr_two_digits(w,0x10f+m->cell,m->remaining,0)))return;m->remaining++;if(m->count<m->remaining){root->bonuses--;schedule(q,MR_PAUSE);}}draw=1;break;
 case MR_PAUSE:m->clock--;if((int16_t)m->clock<0)schedule(q,MR_WAITALL);draw=1;break;
 case MR_WAITALL:if(!root->bonuses){m->clock=30;schedule(q,MR_VEHICLE);}draw=1;break;
 case MR_VEHICLE_WAIT:m->clock--;if((int16_t)m->clock<0){m->clock=30;w->results->vehicle_award=255;schedule(q,MR_AWARD);}draw=1;break;
 case MR_AWARD:{int mode=-1;if(!m->count)mode=1;else if(root->participants==2){if(root->winner==2)mode=2;else if(root->winner!=m->slot){schedule(q,MR_ANIMATE);draw=1;break;}else mode=0;}else if(m->count>=10)mode=0;
 if(mode>=0){struct NativeEntity*n=mr_child(w,e,MR_LABEL);if(!n)return;n->payload.enemy.mission_result.mode=mode;n->payload.enemy.mission_result.count=m->count;}m->clock=mode==0?100:60;schedule(q,MR_FINISH);if(mode==0){q->entry=0;m->clock--;if((int16_t)m->clock<=0){root->done=1;schedule(q,MR_ANIMATE);}}draw=1;break;}
 case MR_FINISH:m->clock--;if((int16_t)m->clock<=0){root->done=1;schedule(q,MR_ANIMATE);}draw=1;break;
 case MR_ANIMATE:draw=1;break;
 case MR_ROSTER:mr_roster(w,e);break;
 case MR_PRIZE_FLY:{struct EnemyActor*column=mr_parent(w,e);struct NativeEntity*column_e=entity_get(w->entities,e->parent);struct EnemyActor*r=column_e?mr_parent(w,column_e):0;if(!column||!r)return;if(r->mission_result.skip==1)schedule(q,MR_PRIZE_DONE);else{weapon_integrate_free(a);if(word(transform_y_up(&a->transform)+w->camera_y)>408)schedule(q,MR_PRIZE_DONE);}draw=1;break;}
 case MR_LABEL:m->clock--;if((int16_t)m->clock<=0){if(!m->remaining){schedule(q,MR_LABEL_FINISH);break;}if(!label(w,e,0))return;m->clock=8;m->blink=~m->blink;m->remaining--;}break;
 case MR_VEHICLE_ICON:draw=1;break;
 default:enemy_fault(w);break;
 }
 if(draw){if(!scene_preserve_screen(w,a)||!mr_animate(w,e))return;if(a->state==MR_VEHICLE_ICON&&a->animation.pose>60)entity_delete(w->entities,e);}
}
int mission_results_spawn(struct WeaponWorld*w,struct EntityRef p){return mr_allocate(w,p,MR_NEW)!=0;}
