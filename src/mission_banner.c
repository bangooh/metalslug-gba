#include "mission_result_internal.h"
/*7A970/7A9F0 and their FIRST letter children; all four layouts are source data. */
enum State {B_NEW,B_WAIT,B_PAUSE,B_RELEASE,B_FADE,B_LETTER,B_APPROACH,B_ARRIVE,B_HOLD,B_EXIT};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static void schedule(struct EnemyActor*q,unsigned s){enemy_schedule(q,s);}
/* Shared END handling republishes held letters, including after blink. */
static int animate(struct WeaponWorld*w,struct NativeEntity*e){return mr_animate(w,e);}
static int16_t word(int n){return (int16_t)(uint16_t)n;}
static int16_t sx(struct WeaponWorld*w,struct WeaponActor*a){return word((a->transform.x>>8)-w->camera_x);}
static int16_t sy(struct WeaponWorld*w,struct WeaponActor*a){return word(transform_y_up(&a->transform)+w->camera_y);}
static const int16_t native_banner_x[]={
#include "../generated/mission-banner-spacing.inc"
};
static int layout(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;unsigned id=(q->mission_result.mode?2:0)+(w->results->mission==5);unsigned first=mission_banner_layouts[id][0],count=mission_banner_layouts[id][1];
 for(unsigned i=0;i<count;i++){const struct MissionBannerLetter*l=&mission_banner_layout[first+i];struct NativeEntity*n=mr_allocate(w,entity_ref(e),B_LETTER);if(!n)return 0;n->tick=tick;struct EnemyActor*r=&n->payload.enemy;r->actor.transform=q->actor.transform;r->mission_result.clock=l->delay;r->mission_result.index=l->glyph;r->mission_result.blink=l->blink;r->mission_result.mode=l->number;r->mission_result.target_x=native_banner_x[first+i];r->mission_result.target_y=l->y;r->source_flags=q->source_flags;}
 return 1;
}
static int distance(struct WeaponWorld*w,struct EnemyActor*q){int16_t x=word(sx(w,&q->actor)-q->mission_result.target_x),y=word(sy(w,&q->actor)-q->mission_result.target_y);uint16_t ax=x<0?(uint16_t)-x:(uint16_t)x,ay=y<0?(uint16_t)-y:(uint16_t)y;return ax>ay?(uint16_t)(ax+(ay>>1)):(uint16_t)(ay+(ax>>1));}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct MissionResultActor*m=&q->mission_result;
 switch(a->state){
 case B_NEW:if(!interaction_sound(w,m->mode?0x20:mission_banner_sounds[0][w->results->level&7]))return 0;mr_screen(w,a,0,511);a->transform.priority=65535;m->clock=0;if(!layout(w,e))return 0;m->ack=0;a->phase=0;a->state=B_WAIT;break;
 case B_RELEASE:a->phase=255;if(!m->mode){if(!result_parent_signal(w,e,0,0,0))return 0;entity_delete(w->entities,e);return 0;}if(!scene_fade_spawn(w,entity_ref(e),2,0,0))return 0;m->clock=40;a->state=B_FADE;break;
 case B_ARRIVE:a->palette=0x1aa;{struct EnemyActor*p=mr_parent(w,e);if(!p)return 0;p->mission_result.ack++;}q->scale_x=q->scale_y=255;a->state=B_HOLD;break;
 case B_EXIT:a->trail_angle=0;break;
 default:break;
 }return 1;
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){(void)p;struct WeaponWorld*w=world;if(w->fault)return;if(!w->results||!w->results->mission_valid){enemy_fault(w);return;}struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct MissionResultActor*m=&q->mission_result;
 if(a->state>=B_LETTER&&!scene_preserve_screen(w,a))return;
 if(q->entry){q->entry=0;if(!enter(w,e))return;}if(e->deleting)return;
 switch(a->state){
 case B_WAIT:{unsigned goal=m->mode?(w->results->mission==5?21:17):(w->results->mission==5?18:14);if(m->ack==goal){if(!interaction_sound(w,m->mode?0x1137:mission_banner_sounds[1][w->results->level&7]))return;schedule(q,B_PAUSE);}break;}
 case B_PAUSE:if(++m->clock>=38)schedule(q,B_RELEASE);break;
 case B_FADE:m->clock--;if((int16_t)m->clock<0){w->enemies->scripted_player_lock=0;entity_delete(w->entities,e);}break;
 case B_LETTER:m->clock--;if((int16_t)m->clock>=0)return;a->palette=0x1be;a->transform.facing|=1;m->roll=511;q->scale_y=255;m->full=0;if(!m->mode)a->transform.priority=65535;a->vx=a->vy=a->ax=a->ay=0;transform_integer_copy(&a->transform,&a->transform);{unsigned i=m->mode?w->results->mission:m->index;if(i>(m->mode?5u:13u))i=0;mr_start(q,m->mode?mission_banner_numbers[i]:mission_banner_letters[i]);}a->state=B_APPROACH;/* fall through */
 case B_APPROACH:{int16_t dx=word(m->target_x-sx(w,a)),dy=word(m->target_y-sy(w,a));if(dx||dy){a->vx=word((uint16_t)dx<<6);a->vy=word((uint16_t)dy<<6);a->movement_flags|=64;if(interaction_move(w,a,WMOVE_POINT)<0)return;}if(!scene_preserve_screen(w,a)||!animate(w,e))return;
 int arrived=!m->roll;if(!arrived&&(int16_t)distance(w,q)<=48){m->roll-=24;if((int16_t)m->roll<0){m->roll=0;arrived=1;}}
 if(arrived){a->palette=0;schedule(q,B_ARRIVE);return;}q->scale_x=(m->roll&256)?(uint8_t)m->roll:(uint8_t)-m->roll;if(!(m->roll&256)&&!m->full){a->palette=0x1aa;m->full=255;a->transform.facing=0;}break;}
 case B_HOLD:{struct EnemyActor*parent=mr_parent(w,e);if(!parent)return;if(!m->blink||!(parent->mission_result.clock&8)){if(!scene_preserve_screen(w,a)||!animate(w,e))return;}else a->animation.drawing=65535;if(parent->actor.phase==255)schedule(q,B_EXIT);break;}
 case B_EXIT:a->trail_angle+=511;if(a->trail_angle&32768)a->trail_angle=32767;{uint8_t angle=enemy_angle(word(160-sx(w,a)),word(400-sy(w,a)))+127;weapon_vector(angle,(int16_t)a->trail_angle,&a->vx,&a->vy);}a->movement_flags|=64;if(interaction_move(w,a,WMOVE_POINT)<0||!scene_preserve_screen(w,a)||!animate(w,e))return;if(interaction_outside(w,a,mission_banner_bounds,0))entity_delete(w->entities,e);break;
 default:enemy_fault(w);break;
 }
}
int mission_banner_spawn(struct WeaponWorld*w,struct EntityRef p,int complete){struct NativeEntity*e=mr_allocate(w,p,B_NEW);if(!e)return 0;e->tick=tick;e->payload.enemy.mission_result.mode=!!complete;return 1;}
