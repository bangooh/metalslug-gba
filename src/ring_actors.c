#include "enemies.h"
/* $05A72E: independent walker claims enemy-enabled gun/attachment records. */
enum State {NEW,WALK,HOLD_NEW,HOLD};
static int16_t word(int n){return (int16_t)(uint16_t)n;}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(q->entry){q->entry=0;if(a->state==NEW){a->palette=14;interaction_priority(w,a,0x8000,20);enemy_start(q,EAN_RING_WALKER);a->vx=0;a->state=WALK;return;}if(a->state==HOLD_NEW){enemy_defense(q,ENB_RING_WALKER);a->state=HOLD;}else{enemy_fault(w);return;}}
 struct TargetRing*r=w->interactions->targets;if(!r){enemy_fault(w);return;}
 if(a->state==WALK){if(interaction_move(w,a,WMOVE_SYNC)<0)return;int ground=terrain_weapon_foot_contact(w->interactions->terrain,a);if(ground<0){enemy_fault(w);return;}if(interaction_move(w,a,ground?WMOVE_GROUND:WMOVE_ITEM)<0)return;
  int16_t key;if(target_ring_find(r,2,word(a->transform.x>>8),transform_y_up(&a->transform),&key)&&key>=0){q->infantry.key=(uint8_t)key;enemy_schedule(q,HOLD_NEW);target_ring_claim(r,2,(uint8_t)key);}
  if(a->hit.events&32){a->transform.facing^=1;a->vx=word(-a->vx);}enemy_animate(w,e);
 }else if(a->state==HOLD){int16_t x,y;if(!target_ring_follow(r,2,q->infantry.key,&x,&y)){a->state=WALK;return;}transform_integer_offset(&a->transform,word(x-(a->transform.x>>8)),word(y-transform_y_up(&a->transform)));}
 else{enemy_fault(w);return;}
 if(!human_death_hit_tail(w,e))return;static const int16_t b[]={-32,32,-16,64};if(interaction_outside(w,a,b,1))entity_delete(p,e);
}
int ring_walker_spawn(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,const uint8_t params[8]){struct NativeEntity*e=enemy_allocate(w,p,o,t,tick);if(!e)return 0;if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];enemy_schedule(&e->payload.enemy,NEW);return 1;}
