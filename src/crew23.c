#include "tank_internal.h"
#include "m4_internal.h"
#include "../generated/crew23-programs.h"
enum State {ROOT,IDLE,OPEN,FADE,CREW,DEBRIS};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static struct NativeEntity*allocate(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,unsigned s){struct NativeEntity*e=enemy_allocate(w,p,o,t,tick);if(e){e->payload.enemy.crew23=(__typeof__(e->payload.enemy.crew23)){.offset_x=-1,.offset_y=-1,.step=255,.variant=255};enemy_schedule(&e->payload.enemy,s);}return e;}
static void start(struct EnemyActor*q,unsigned id){animation_begin(&q->actor.animation,crew23_entries[id]);q->actor.movement_flags|=32;}
struct Context{struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read(void*ctx,unsigned f,unsigned width){(void)width;struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;if(f==C23F_59)return q->actor.damage_timer;if(f==C23F_74)return (uint16_t)q->crew23.offset_x;if(f==C23F_78)return (uint16_t)q->crew23.offset_y;enemy_fault(c->w);return 0;}
static int field(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;uint32_t old=read(ctx,f,width);if(op==AN_FIELD_AND)v&=old;else if(op==AN_FIELD_OR)v|=old;else if(op==AN_FIELD_XOR)v^=old;else if(op!=AN_FIELD_SET)return enemy_fault(c->w);if(f==C23F_59)q->actor.damage_timer=v;else if(f==C23F_74)q->crew23.offset_x=v;else if(f==C23F_78)q->crew23.offset_y=v;else return enemy_fault(c->w);return !c->w->fault;}
static int sound(void*ctx,uint16_t n,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,n,stop)?1:enemy_fault(c->w);}
static int call(void*ctx,unsigned n,struct Animation*an){(void)an;struct Context*c=ctx;struct EnemyActor*q=&c->e->payload.enemy;struct WeaponActor*a=&q->actor;if(n!=C23C_78EDE&&n!=C23C_78F14)return enemy_fault(c->w);/* These callbacks copy only face and absolute integer XY. */struct PlayerTransform t={.x=(int32_t)tk_word((a->transform.x>>8)+tk_faced(a,q->crew23.offset_x))*256,.y=(int32_t)tk_word((a->transform.y>>8)-q->crew23.offset_y)*256,.facing=a->transform.facing^(n==C23C_78F14),.display_group=255};return allocate(c->w,entity_ref(c->e),ENTITY_FIRST_CHILD,&t,DEBRIS)!=0;}
/* CREW starts with the source null-defense command; no binding table exists. */
static int bind(void*ctx,unsigned kind,uint32_t value){
 struct Context*c=ctx;
 if(kind!=AN_DEFENSE||value!=0xffffffffu)return enemy_fault(c->w);
 c->e->payload.enemy.actor.defense=0;
 c->e->payload.enemy.actor.defense_count=0;
 return 1;
}
static int animate(struct WeaponWorld*w,struct NativeEntity*e){struct Context c={w,e};struct AnimationOwner owner={.context=&c,.read=read,.write=field,.sound=sound,.call=call,.bind=bind};int done=animation_step(&e->payload.enemy.actor.animation,&crew23_program,&owner);if(e->payload.enemy.actor.animation.fault)enemy_fault(w);return done;}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct NativeEntity*n;struct EnemyActor*p;
 switch(a->state){
 case ROOT:a->display_flags&=253;a->palette=0x1e;start(q,C23A_IDLE);a->state=IDLE;break;
 case OPEN:{start(q,C23A_OPEN);struct PlayerTransform blank={.x=-256,.y=513*256,.display_group=255};n=allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&blank,CREW);if(!n)return 0;n->payload.enemy.crew23.variant=q->crew23.variant;break;}
 case FADE:a->damage_timer=30;break;
 case CREW:interaction_priority(w,a,0x8000,24);a->display_flags|=2;a->palette=14;start(q,C23A_CREW);p=tk_parent(w,e);if(!p)return 0;for(unsigned i=0;i<8;i++)q->params[i]=p->params[i];a->transform.facing=p->actor.transform.facing;a->animation.pose=p->actor.animation.pose;q->crew23.step=0;break;
 case DEBRIS:a->palette=0x1e;a->ay=-4;start(q,C23A_DEBRIS);break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static void tick(struct EntityPool*pool,struct NativeEntity*e,void*world){struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(q->entry){q->entry=0;if(!enter(w,e))return;}unsigned s=a->state;
 if(s==CREW){struct EnemyActor*p=tk_parent(w,e);if(!p)return;unsigned i=q->crew23.step;if(i>=33){enemy_fault(w);return;}transform_integer_offset(&a->transform,tk_word((p->actor.transform.x>>8)+tk_faced(a,crew23_path[i][0])-(a->transform.x>>8)),tk_word(transform_y_up(&p->actor.transform)+crew23_path[i][1]-transform_y_up(&a->transform)));q->crew23.step++;animate(w,e);if(q->crew23.step==33){/* $04498E creates a counted FIRST child under $100800. */struct NativeEntity*n=soldier_spawn_jump_entity(w,w->projectile_group,ENTITY_FIRST_CHILD,&a->transform,q->crew23.variant==1);if(!n)return;n->payload.enemy.actor.display_flags|=2;for(unsigned k=1;k<8;k++)n->payload.enemy.params[k]=q->params[k];entity_delete(pool,e);}return;}
 if(s==DEBRIS){int r=interaction_move(w,a,WMOVE_FALL);if(r<0)return;if(r){entity_delete(pool,e);return;}if(animate(w,e))entity_delete(pool,e);return;}
 if(interaction_move(w,a,WMOVE_SYNC)<0)return;int done=animate(w,e);if(s==IDLE){uint16_t trigger=(uint16_t)tk_word(320-(q->params[0]<<3));if(trigger>=(uint16_t)tk_sx(w,a))enemy_schedule(q,OPEN);}else if(s==OPEN){if(done)enemy_schedule(q,FADE);}else if(s==FADE){if(!a->damage_timer){entity_delete(pool,e);return;}}else{enemy_fault(w);return;}if(tk_word(tk_sx(w,a)+64)<0)entity_delete(pool,e);
}
int crew23_spawn(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){if(id<23||id>26)return enemy_fault(w);struct NativeEntity*e=allocate(w,p,o,t,ROOT);if(!e)return 0;e->payload.enemy.crew23.variant=id<=24;if(id==24||id==26)e->payload.enemy.actor.transform.facing|=1;if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];return 1;}
int crew23_has_children(struct WeaponWorld*w,const struct NativeEntity*e){return m4_has_children(w,e);}
