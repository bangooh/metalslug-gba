#include "bosses.h"
#include "items.h"
/* Templates 228/229 ($046140), used by both tank mission watchers. The
 * animation owns hit boxes; the source has no firing loop in this actor. */
enum State {NEW,ENTRANCE,MOVE,MOVING,LAND,TURN,WAIT,DELETE};
static int16_t word(int x){return (int16_t)(uint16_t)x;}
static uint16_t random_word(struct WeaponWorld*w){if(!w->rng){enemy_fault(w);return 0;}return source_random_word(w->rng);}
static int settle(struct WeaponWorld*w,struct WeaponActor*a){
 int16_t y=word(transform_y_up(&a->transform)+w->camera_y);transform_integer_offset(&a->transform,0,word((y&1023)-y));
 for(unsigned i=0;i<2048;i++){
  if(word(transform_y_up(&a->transform)+w->camera_y)<0){transform_integer_offset(&a->transform,word(160+w->camera_x-(a->transform.x>>8)),word(384-w->camera_y-transform_y_up(&a->transform)));return 1;}
  int r=interaction_move(w,a,WMOVE_ITEM);if(r<0)return 0;if(r)return 1;
 }return enemy_fault(w);
}
static int enter(struct WeaponWorld*w,struct EnemyActor*q){
 struct WeaponActor*a=&q->actor;q->entry=0;
 switch(a->state){
 case NEW:
  if(!settle(w,a))return 0;a->palette=q->params[2]?0x19f:0x1a;a->transform.facing=q->params[1];interaction_priority(w,a,0x8000,24);
  if(q->params[5]){enemy_start(q,EAN_DEPLOY_ENTER);a->state=ENTRANCE;return 1;}a->state=MOVE;/* FALLTHROUGH */
 case MOVE:{
  struct InteractionWorld*r=w->interactions;if(!r||!r->rank_valid)return enemy_fault(w);
  int16_t speed=deploy_rank[(interaction_rank(w,&q->actor,runtime_owner_deploy[0]))*8+(q->source_flags&3)*2+((r->player_count_flag&2)>>1)];a->vx=a->transform.facing&1?speed:word(-speed);enemy_start(q,EAN_RUN);a->state=MOVING;break;
 }
 case LAND:enemy_start(q,EAN_EXIT_LAND_HORIZONTAL);break;
 case TURN:q->infantry.radius++;enemy_start(q,EAN_DEPLOY_TURN);break;
 case WAIT:a->timer=(random_word(w)&31)+20;animation_begin(&a->animation,deploy_idle[random_word(w)&3]);a->movement_flags|=32;break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static int tail(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(a->hit.flags&8){
  struct PlayerTransform t=a->transform;transform_integer_offset(&t,0,4);
  /* $09A7AA cancels the first spawn helper's mirrored X offset. */
  if(!item_spawn_drop(w,&t,q->params[2],q->params[3],q->params[4],1,0))return 0;
  if(q->params[2]){if(!human_smoke_transform(w,e))return 0;}else if(!human_death_hit_tail(w,e))return 0;
 }
 if(interaction_outside(w,a,boss_fx_bounds[8],0)){entity_delete(w->entities,e);return 0;}return 1;
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(a->state==DELETE){entity_delete(p,e);return;}if(q->entry&&!enter(w,q))return;unsigned state=a->state;
 if(interaction_move(w,a,WMOVE_SYNC)<0)return;
 if(state==ENTRANCE||state==MOVING){
  if(!w->foot_contact){enemy_fault(w);return;}int ground=w->foot_contact(w->scene,a);if(ground<0)return;
  if(ground){if(interaction_move(w,a,WMOVE_GROUND)<0)return;}
  else if(state==MOVING){if(interaction_move(w,a,WMOVE_ITEM)<0)return;if(a->hit.events&32)enemy_schedule(q,LAND);}
 }
 int done=enemy_animate(w,e);
 if(state==ENTRANCE&&done)enemy_schedule(q,MOVE);
 if(state==LAND&&done)enemy_schedule(q,TURN);
 if(state==TURN&&done){a->transform.facing^=1;enemy_schedule(q,WAIT);}
 if(state==WAIT&&(int16_t)--a->timer<=0)enemy_schedule(q,MOVE);tail(w,e);
}
int deploy_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,const struct PlayerTransform*t,const uint8_t params[8]){
 struct NativeEntity*e=enemy_allocate(w,parent,order,t,tick);if(!e)return 0;
 if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];enemy_schedule(&e->payload.enemy,NEW);return 1;
}
