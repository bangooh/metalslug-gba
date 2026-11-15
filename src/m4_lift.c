#include "shield.h"
#include "m4_internal.h"
/* $084F26 lift and $084DB0 breakable rescue emitters are distinct from
 * template 268's carrier. They share only explicit services and assets. */
enum State {NEW,UP,DOWN,STOP,HOLD,SPAWN_WAIT,SPAWN,RESCUE,PATTERN};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static uint16_t bounded(struct WeaponWorld*w,uint16_t n){if(!w->rng){enemy_fault(w);return 0;}return source_random(w->rng,n);}
static void init(struct NativeEntity*e,unsigned s){e->payload.enemy.m4=(__typeof__(e->payload.enemy.m4)){.timer=-1,.remaining=65535,.variant=255,.pattern=255};enemy_schedule(&e->payload.enemy,s);}
static struct NativeEntity*child(struct WeaponWorld*w,struct NativeEntity*e,unsigned s,enum EntityInsert order){struct NativeEntity*n=enemy_allocate(w,entity_ref(e),order,&e->payload.enemy.actor.transform,tick);if(n)init(n,s);return n;}
static int platform(struct WeaponWorld*w,struct WeaponActor*a){struct TerrainMotion m={.platform_ids=a->platform_ids};if(!terrain_platform_publish(w->interactions->terrain,&m,m4_word((a->transform.x>>8)-96),transform_y_up(&a->transform),192,0))return enemy_fault(w);a->platform_ids=m.platform_ids;return 1;}
static int altitude(struct WeaponWorld*w,struct EnemyActor*q){int16_t y=0;if(!m4_secondary_y(w,&q->actor,&y))return -1;return q->actor.vy<0?y<q->params[1]*16:y<=q->params[0]*16;}
static int wobble(struct WeaponWorld*w,struct EnemyActor*q,int arm){if(arm&&q->m4.timer<=0){if(!w->rng)return enemy_fault(w);if(!(source_random_word(w->rng)&31))q->m4.timer=2;}if(q->m4.timer>0){q->m4.timer--;int16_t vy=q->actor.vy;q->actor.vy=q->m4.timer&1?256:-256;int r=interaction_move(w,&q->actor,WMOVE_POINT);q->actor.vy=vy;if(r<0)return 0;}return !w->fault;}
static int emit(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct EnemyActor*p=m4_parent(w,e);if(!p)return 0;const struct BossPatternData*t=&shield_rescue_patterns[q->m4.variant];struct WeaponActor*a=&q->actor;
 /* All three reviewed source patterns are one-shot, one-POW emitters. */
 int16_t dx=p->actor.transform.facing&1?m4_word(-t->dx):t->dx,dy=p->actor.transform.facing&1?m4_word(-t->dy):t->dy;transform_integer_offset(&a->transform,m4_word((p->actor.transform.x>>8)+dx-(a->transform.x>>8)),m4_word(transform_y_up(&p->actor.transform)+dy-transform_y_up(&a->transform)));
 struct NativeEntity*n=pow_spawn_entity(w,entity_ref(e),ENTITY_LAST_CHILD,POW_FREE,&a->transform,0);if(!n)return 0;dx=bounded(w,t->spread_x);dy=bounded(w,t->spread_y);if(a->transform.facing&1)dx=m4_word(-dx);if(a->transform.facing&2)dy=m4_word(-dy);transform_integer_offset(&n->payload.interaction.actor.transform,dx,dy);n->payload.interaction.actor.display_flags|=a->display_flags&64;n->payload.interaction.scale_x=p->scale_x;n->payload.interaction.scale_y=p->scale_y;entity_delete(w->entities,e);return !w->fault;
}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){
 struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;int r;
 switch(a->state){
 case NEW:a->palette=0x1b1;q->m4.timer=0;a->transform.priority=0x2000;m4_reset(a);if(!platform(w,a))return 0;r=altitude(w,q);if(r<0)return 0;enemy_schedule(q,r?DOWN:UP);return 0;
 case UP:case DOWN:if(!interaction_sound(w,0x1074))return 0;shield_start(q,a->state==UP?SHA_LIFT_UP:SHA_LIFT_DOWN);a->vy=m4_word(q->params[2]*8);if(a->state==DOWN){a->vy=m4_word(-a->vy);if((int16_t)w->mission_progress<=264&&!m4_has_children(w,e)&&!child(w,e,SPAWN_WAIT,ENTITY_FIRST_CHILD))return 0;}break;
 case STOP:if(!interaction_sound(w,0x1075))return 0;shield_start(q,SHA_LIFT_STOP);q->m4.timer=10;break;
 case HOLD:shield_start(q,SHA_LIFT_STOP);q->m4.timer=20;break;
 case SPAWN_WAIT:enemy_schedule(q,SPAWN);return 0;
 case SPAWN:{uint8_t params[8]={0,16,0,0,0,0,0,0};struct PlayerTransform t=a->transform;t.display_group=1;struct NativeEntity*n=shield_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,109,&t,params);if(!n)return 0;struct WeaponActor*b=&n->payload.enemy.actor;if(!m4_project(w,b,0,800))return 0;m4_screen(w,b,352,m4_sy(w,b));entity_delete(w->entities,e);return 0;}
 case RESCUE:q->scale_x=q->scale_y=255;a->transform.facing=0;q->m4.variant=q->params[0]<2?q->params[0]:2;shield_defense(q,SHB_RESCUE0+q->m4.variant);a->hit.hp=q->params[2];m4_reset(a);shield_start(q,SHA_RESCUE);break;
 case PATTERN:break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(q->entry){q->entry=0;if(!enter(w,e)||w->fault)return;}unsigned s=a->state;
 if(s==PATTERN){emit(w,e);return;}
 if(s==RESCUE){if(interaction_move(w,a,WMOVE_SYNC)<0)return;shield_animate(w,e);if(a->hit.flags&8){if(!shield_hurt(w,a))return;a->hit.flags&=247;}if(a->hit.flags&1){for(unsigned budget=0;q->params[1];budget++){if(budget>=ENTITY_CAPACITY){enemy_fault(w);return;}struct NativeEntity*n=child(w,e,PATTERN,ENTITY_LAST_CHILD);if(!n)return;n->payload.enemy.m4.variant=q->m4.variant;q->params[1]--;}entity_delete(p,e);return;}if(interaction_outside(w,a,shield_bounds[2+q->m4.variant],0))entity_delete(p,e);return;}
 if(s==UP||s==DOWN){if(interaction_move(w,a,WMOVE_POINT)<0||!wobble(w,q,1))return;shield_animate(w,e);if(!platform(w,a))return;int r=altitude(w,q);if(r<0)return;if(s==UP?r:!r)enemy_schedule(q,STOP);}
 else if(s==STOP){if(interaction_move(w,a,WMOVE_SYNC)<0||!wobble(w,q,0))return;shield_animate(w,e);if(!platform(w,a))return;if(q->m4.timer<=0)enemy_schedule(q,HOLD);}
 else if(s==HOLD){if(interaction_move(w,a,WMOVE_SYNC)<0)return;shield_animate(w,e);if(!platform(w,a))return;q->m4.timer=m4_word(q->m4.timer-1);if(q->m4.timer<0){int r=altitude(w,q);if(r<0)return;enemy_schedule(q,r?DOWN:UP);}}
 else{enemy_fault(w);return;}
 if(interaction_outside(w,a,shield_bounds[1],0))entity_delete(p,e);
}
int m4_lift_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){if(id!=267&&id!=275)return enemy_fault(w);struct NativeEntity*n=enemy_allocate(w,parent,order,t,tick);if(!n)return 0;init(n,id==267?NEW:RESCUE);if(params)for(unsigned i=0;i<8;i++)n->payload.enemy.params[i]=params[i];return 1;}
