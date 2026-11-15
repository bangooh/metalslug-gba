#include "tank_internal.h"
#include "m4_internal.h"
#include "slug_surface.h"
#include "../generated/captor-programs.h"
/* $079A6C/$079C8C, $079EB8..$07A456 and $05FA00..$05FD78.
 * Parent signals are typed phase/command/completion fields, not source RAM. */
enum State {CREW_NEW,CREW_SETTLE,CREW_IDLE,CREW_HIT,CREW_FADE,ROOT,GUARD_NEW,GUARD_SETTLE,GUARD,
 LEFT_NEW,LEFT_SETTLE,LEFT_IDLE,LEFT_ACTION,CENTER_NEW,CENTER_SETTLE,CENTER_IDLE,CENTER_RETURN,CENTER_ACTION,CENTER_TURN,CENTER_KILLED,CENTER_REMOVE,
 OVERLAY,OVERLAY_FADE,ESCAPE_STAND,ESCAPE_RUN,HUT_NEW,HUT_WAIT,HUT_OPEN,HUT_ACTIVE,HUT_RETIRE,DOOR_NEW,DOOR_WAIT,DOOR_ACTIVE,DOOR_HIT,DOOR_REMOVE,DELETE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static void schedule(struct NativeEntity*e,unsigned s){e->tick=tick;enemy_schedule(&e->payload.enemy,s);}
static void init(struct NativeEntity*e,unsigned s){uint8_t*p=(uint8_t*)&e->payload.enemy.captor;for(unsigned i=0;i<sizeof(e->payload.enemy.captor);i++)p[i]=255;schedule(e,s);}
static struct NativeEntity*child(struct WeaponWorld*w,struct NativeEntity*e,unsigned s,int16_t dx){struct NativeEntity*n=enemy_allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&e->payload.enemy.actor.transform,tick);if(n){init(n,s);transform_integer_offset(&n->payload.enemy.actor.transform,dx,0);}return n;}
static void start(struct EnemyActor*q,unsigned n){animation_begin(&q->actor.animation,captor_entries[n]);q->actor.movement_flags|=32;}
static int outside(struct WeaponWorld*w,struct WeaponActor*a,unsigned n){return interaction_outside(w,a,captor_bounds[n],0);}
static struct NativeEntity*parent(struct WeaponWorld*w,struct NativeEntity*e){struct NativeEntity*p=entity_get(w->entities,e->parent);if(!p)enemy_fault(w);return p;}
static struct NativeEntity*external_owner(struct WeaponWorld*w,struct NativeEntity*root){struct NativeEntity*p=parent(w,root);return p?parent(w,p):0;}
static uint8_t phase(struct WeaponWorld*w,struct NativeEntity*e){if(e->kind==ENTITY_GROUP)return e->payload.group.phase;struct WeaponActor*a=entity_weapon_actor(e);if(a)return a->phase;enemy_fault(w);return 0;}
static int owner_signal(struct WeaponWorld*w,struct NativeEntity*root,uint8_t value,int done){if(!root->payload.enemy.captor.external)return 1;struct NativeEntity*p=external_owner(w,root);if(!p)return 0;if(p->kind==ENTITY_GROUP){if(done)p->payload.group.captor_done=value;else p->payload.group.command=value;return 1;}if(p->kind!=ENTITY_ENEMY)return enemy_fault(w);if(done)p->payload.enemy.captor_done=value;else p->payload.enemy.dead=value;return 1;}
static int stop(struct WeaponWorld*w,uint16_t n){return w->sound&&w->sound(w->scene,n,1)?1:enemy_fault(w);}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;};
static int bind(void*ctx,unsigned kind,uint32_t v){struct Context*c=ctx;struct WeaponActor*a=&c->e->payload.enemy.actor;if(kind!=AN_DEFENSE)return enemy_fault(c->w);if(v==0xffffffffu){a->defense=0;a->defense_count=0;return 1;}if(v>=sizeof(captor_bindings)/sizeof(*captor_bindings))return enemy_fault(c->w);a->defense=captor_bindings[v].defense;a->defense_count=captor_bindings[v].count;return 1;}
static int sound(void*ctx,uint16_t n,int off){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,n,off)?1:enemy_fault(c->w);}
static int call(void*ctx,unsigned cb,struct Animation*an){(void)an;struct Context*c=ctx;if(cb!=CPC_7A41C)return enemy_fault(c->w);struct EnemyActor*p=tk_parent(c->w,c->e);if(!p)return 0;p->captor.command=(int16_t)captor_commands[tk_random(c->w)&3];return !c->w->fault;}
static int animate(struct WeaponWorld*w,struct NativeEntity*e){struct WeaponActor*a=&e->payload.enemy.actor;if(a->animation.cursor==65535)return 0;struct Context c={w,e};struct AnimationOwner o={.context=&c,.bind=bind,.sound=sound,.call=call};int done=animation_step(&a->animation,&captor_program,&o);if(a->animation.fault)enemy_fault(w);return done;}
static int center_signal(struct WeaponWorld*w,struct NativeEntity*e,int killed){struct NativeEntity*p=parent(w,e);if(!p||p->kind!=ENTITY_ENEMY)return enemy_fault(w);p->payload.enemy.dead=255;if(killed)e->payload.enemy.actor.phase=255;return owner_signal(w,p,255,1);}
static int near_fixed_players(struct WeaponWorld*w,struct WeaponActor*a){for(unsigned i=0;i<2;i++){struct PlayerBodyState*p=w->players[i];int16_t x=p?tk_word((p->actor.transform.x>>8)-w->camera_x):-1;if(tk_word(tk_sx(w,a)-x)<160)return 1;}return 0;}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;struct NativeEntity*n;unsigned s=a->state;
 switch(s){
 case CREW_NEW:a->palette=25;a->hit.hp=1;m4_reset(a);schedule(e,CREW_SETTLE);return 0;
 case CREW_SETTLE:if(!tk_settle(w,a))return 0;interaction_priority(w,a,0x8000,12);start(q,q->captor.variant?CPA_IDLE_B:CPA_IDLE_A);a->state=CREW_IDLE;break;
 case CREW_HIT:if(q->captor.variant&&!w->enemies->blood_enabled)a->palette=0x1d5;a->display_flags&=253;start(q,q->captor.variant?CPA_HIT_B:CPA_HIT_A);break;
 case CREW_FADE:a->damage_timer=40;break;
 case ROOT:a->phase=q->dead=0;if(!child(w,e,LEFT_NEW,-64)||!child(w,e,CENTER_NEW,-32)||!child(w,e,GUARD_NEW,0))return 0;a->display_flags&=253;break;
 case GUARD_NEW:case LEFT_NEW:case CENTER_NEW:a->palette=s==CENTER_NEW?46:14;a->phase=q->dead=0;a->hit.hp=1;schedule(e,s==GUARD_NEW?GUARD_SETTLE:s==LEFT_NEW?LEFT_SETTLE:CENTER_SETTLE);return 0;
 case GUARD_SETTLE:case LEFT_SETTLE:case CENTER_SETTLE:
  if(!tk_settle(w,a))return 0;interaction_priority(w,a,0x8000,12);if(s!=GUARD_SETTLE)a->transform.priority++;
  if(s==GUARD_SETTLE){start(q,CPA_GUARD);a->state=GUARD;}else if(s==LEFT_SETTLE){start(q,CPA_LEFT_IDLE);a->state=LEFT_IDLE;}else{if(!child(w,e,OVERLAY,0))return 0;start(q,CPA_CENTER_IDLE);a->state=CENTER_IDLE;}break;
 case LEFT_IDLE:start(q,CPA_LEFT_IDLE);break;
 case LEFT_ACTION:start(q,CPA_LEFT_ACTION);break;
 case CENTER_RETURN:n=parent(w,e);if(!n||!owner_signal(w,n,2,0))return 0;/* FALLTHROUGH */
 case CENTER_IDLE:start(q,CPA_CENTER_IDLE);a->state=CENTER_IDLE;break;
 case CENTER_ACTION:n=parent(w,e);if(!n||!owner_signal(w,n,1,0))return 0;start(q,q->captor.command==1?CPA_CENTER_B:CPA_CENTER_A);break;
 case CENTER_TURN:a->transform.facing^=1;start(q,CPA_CENTER_TURN);break;
 case CENTER_KILLED:if(!center_signal(w,e,1)||!tk_sound(w,0x109f))return 0;boss_fx_transform(w,e,BFX_FIRE);return 0;
 case CENTER_REMOVE:if(!center_signal(w,e,0))return 0;entity_delete_children(w->entities,e);entity_delete(w->entities,e);return 0;
 case OVERLAY:a->palette=8;start(q,CPA_OVERLAY);break;
 case OVERLAY_FADE:a->animation.freeze=0;a->damage_timer=20;q->captor.timer=20;break;
 case ESCAPE_STAND:infantry_stand_escape(e,(uint16_t)q->captor.command>>8);e->tick(w->entities,e,w);return 0;
 case ESCAPE_RUN:infantry_force_escape(e,(uint16_t)q->captor.command>>8);e->tick(w->entities,e,w);return 0;
 case HUT_NEW:a->palette=14;a->owner=a->damage_owner=255;a->transform.priority+=2;a->phase=q->dead=0;a->hit.hp=16;m4_reset(a);start(q,CPA_HUT_WAIT);if(!child(w,e,DOOR_NEW,0))return 0;schedule(e,HUT_WAIT);return 0;
 case HUT_WAIT:break;
 case HUT_OPEN:m4_reset(a);start(q,CPA_HUT_OPEN);break;
 case HUT_ACTIVE:m4_reset(a);start(q,CPA_HUT_ACTIVE);break;
 case HUT_RETIRE:break;
 case DOOR_NEW:a->palette=25;a->animation.delay=0;a->owner=a->damage_owner=255;a->animation.freeze=0;a->hit.hp=16;a->transform.priority--;m4_reset(a);start(q,CPA_DOOR_WAIT);schedule(e,DOOR_WAIT);return 0;
 case DOOR_WAIT:break;
 case DOOR_ACTIVE:start(q,CPA_DOOR_ACTIVE);break;
 case DOOR_HIT:if(!stop(w,0x10ae))return 0;p=tk_parent(w,e);if(!p)return 0;p->dead=255;if(!tk_sound(w,0x108e))return 0;start(q,CPA_DOOR_HIT);break;
 case DOOR_REMOVE:if(!stop(w,0x10ae))return 0;p=tk_parent(w,e);if(!p)return 0;p->dead=255;entity_delete(w->entities,e);return 0;
 default:return enemy_fault(w);
 }return !w->fault;
}
static void guard_tail(struct WeaponWorld*w,struct NativeEntity*e,int left){struct EnemyActor*p=tk_parent(w,e);if(!p)return;struct WeaponActor*a=&e->payload.enemy.actor;if(p->actor.phase==255||p->dead==255){if(left)a->transform.facing^=1;schedule(e,ESCAPE_STAND);}if(!human_death_hit_tail(w,e))return;if(left&&(a->hit.flags&8)){a->transform.facing^=1;return;}if(outside(w,a,2))schedule(e,DELETE);}
static void hut_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;if(s==HUT_RETIRE){if(q->dead==255)entity_delete(w->entities,e);return;}if(interaction_move(w,a,WMOVE_SYNC)<0)return;int done=animate(w,e);if(w->fault)return;
 if(s==HUT_WAIT){if(q->dead==255||near_fixed_players(w,a))schedule(e,HUT_OPEN);}else if(s==HUT_OPEN){if(done){a->phase=128;schedule(e,HUT_ACTIVE);if(q->dead==255){schedule(e,ESCAPE_RUN);if(!pow_spawn_surface(w,entity_ref(e),&a->transform))return;}}}else if(s==HUT_ACTIVE){if(q->dead==255)schedule(e,HUT_OPEN);}else{enemy_fault(w);return;}
 if(a->hit.flags&8){a->phase=255;if(s==HUT_ACTIVE&&!stop(w,0x10ae))return;}if(!human_death_hit_tail(w,e))return;if(outside(w,a,4))schedule(e,HUT_RETIRE);
}
static void door_frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*p=tk_parent(w,e);if(!p)return;struct WeaponActor*a=&e->payload.enemy.actor;if(interaction_move(w,a,WMOVE_SYNC)<0)return;if(s==DOOR_ACTIVE&&p->actor.phase==255)a->animation.delay=2;int done=animate(w,e);if(w->fault)return;
 if(s==DOOR_WAIT&&p->actor.phase==128)schedule(e,DOOR_ACTIVE);if(s==DOOR_WAIT||s==DOOR_ACTIVE){if(a->hit.flags&8)schedule(e,DOOR_HIT);}else if(s==DOOR_HIT&&done){struct NativeEntity*n=boss_fx_spawn(w,entity_ref(e),ENTITY_LAST_CHILD,&a->transform,BFX_BLAST);if(!n)return;transform_integer_offset(&n->payload.enemy.actor.transform,-16,0);schedule(e,DOOR_REMOVE);}if(outside(w,a,4))schedule(e,DOOR_REMOVE);
}
static void frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;struct EnemyActor*p;int done;
 if(s>=HUT_WAIT&&s<=HUT_RETIRE){hut_frame(w,e,s);return;}if(s>=DOOR_WAIT&&s<=DOOR_HIT){door_frame(w,e,s);return;}
 if(s==ROOT){if(q->captor.external){struct NativeEntity*n=external_owner(w,e);if(!n)return;a->phase=phase(w,n);}if(q->dead==255){entity_delete(w->entities,e);return;}q->captor.command=0;return;}
 if(interaction_move(w,a,WMOVE_SYNC)<0)return;done=animate(w,e);if(w->fault)return;
 switch(s){
 case CREW_IDLE:if(a->hit.flags&8)schedule(e,CREW_HIT);if(outside(w,a,q->captor.variant?1:0))schedule(e,DELETE);return;
 case CREW_HIT:if(done)schedule(e,CREW_FADE);if(outside(w,a,q->captor.variant?1:0))schedule(e,DELETE);return;
 case CREW_FADE:if(!a->damage_timer)entity_delete(w->entities,e);return;
 case GUARD:guard_tail(w,e,0);return;
 case LEFT_IDLE:p=tk_parent(w,e);if(!p)return;if(p->captor.command==2)schedule(e,LEFT_ACTION);guard_tail(w,e,1);return;
 case LEFT_ACTION:if(done)schedule(e,LEFT_IDLE);guard_tail(w,e,1);return;
 case CENTER_IDLE:p=tk_parent(w,e);if(!p)return;if(p->captor.command==1||p->captor.command==2){q->captor.command=p->captor.command;schedule(e,CENTER_ACTION);}break;
 case CENTER_ACTION:if(done)schedule(e,CENTER_RETURN);break;
 case CENTER_TURN:if(done)schedule(e,HUT_NEW);return;
 case OVERLAY:p=tk_parent(w,e);if(!p)return;a->animation.freeze=p->captor.command?0:1;if(p->actor.phase==255)schedule(e,OVERLAY_FADE);if(outside(w,a,2))schedule(e,DELETE);return;
 case OVERLAY_FADE:if(outside(w,a,2))schedule(e,DELETE);q->captor.timer=tk_word(q->captor.timer-1);if(!q->captor.timer)entity_delete(w->entities,e);return;
 default:enemy_fault(w);return;
 }if(a->hit.flags&8)schedule(e,CENTER_KILLED);if(outside(w,a,3))schedule(e,CENTER_REMOVE);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;if(q->actor.state==DELETE){entity_delete(p,e);return;}if(q->entry){q->entry=0;if(!enter(w,e)||w->fault)return;}frame(w,e,q->actor.state);}
int captor_spawn(struct WeaponWorld*w,struct EntityRef parent_ref,enum EntityInsert order,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){if(id!=30&&(id<125||id>128))return enemy_fault(w);struct NativeEntity*e=enemy_allocate(w,parent_ref,order,t,tick);if(!e)return 0;init(e,id==30?HUT_NEW:id<=126?CREW_NEW:ROOT);struct EnemyActor*q=&e->payload.enemy;q->captor.variant=id==126;q->captor.external=id==127?-1:0;if(params)for(unsigned i=0;i<8;i++)q->params[i]=params[i];return 1;}
