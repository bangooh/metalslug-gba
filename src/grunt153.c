#include "tank_internal.h"
#include "m4_internal.h"
#include "../generated/grunt153-programs.h"
/* The original labels call these physics; $08F040 actually gates on camera X. */
enum State {ROOT,WAIT_SCROLL,STATIC,CARRIER_WAIT,CARRIER_DROP,CARRIER_IDLE,DROPPED,HOP_UP,HOP_DOWN,
 STAND,STAND2,WALK,HIT,HIT_ANGLE,HIT_LAUNCH,HIT_FLY,STANDING,RUN_FIXED,RUN,STAND_B,STAND_B2,WALK_B,WALK_START,
 SENTRY,SENTRY_WAIT,ONE_SHOT,SWING_START,SWING,PAUSE,BACK,TURN,LEAP1,LEAP2,LEAP3};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static void schedule(struct NativeEntity*e,unsigned s){enemy_schedule(&e->payload.enemy,s);}
static struct NativeEntity*allocate(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,unsigned s){struct NativeEntity*e=enemy_allocate(w,p,o,t,tick);if(e){struct EnemyActor*q=&e->payload.enemy;q->grunt153=(__typeof__(q->grunt153)){.signal=-1,.timer=-1,.scroll_trigger=-1,.amplitude=-1,.variant=255,.bob=0,.wait=255};q->selector=q->scale=65535;schedule(e,s);}return e;}
static unsigned random_mask(struct WeaponWorld*w,unsigned mask){if(!w->rng){enemy_fault(w);return 0;}return source_random_mask(w->rng,mask);}
static void start_entry(struct EnemyActor*q,uint16_t n){if(n!=65535){animation_begin(&q->actor.animation,n);q->actor.movement_flags|=32;}}
static void start(struct EnemyActor*q,unsigned n){start_entry(q,grunt153_entries[n]);}
struct Context{struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read(void*ctx,unsigned f,unsigned width){(void)width;struct Context*c=ctx;if(f!=G153F_SIGNAL){enemy_fault(c->w);return 0;}return (uint16_t)c->e->payload.enemy.grunt153.signal;}
static int field(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){struct Context*c=ctx;uint32_t old=read(ctx,f,width);if(op==AN_FIELD_AND)v&=old;else if(op==AN_FIELD_OR)v|=old;else if(op==AN_FIELD_XOR)v^=old;else if(op!=AN_FIELD_SET)return enemy_fault(c->w);c->e->payload.enemy.grunt153.signal=v;return !c->w->fault;}
static int sound(void*ctx,uint16_t n,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,n,stop)?1:enemy_fault(c->w);}
static int animate(struct WeaponWorld*w,struct NativeEntity*e){struct Animation*a=&e->payload.enemy.actor.animation;if(a->cursor==65535)return 0;struct Context c={w,e};struct AnimationOwner owner={.context=&c,.read=read,.write=field,.sound=sound};int done=animation_step(a,&grunt153_program,&owner);if(a->fault)enemy_fault(w);return done;}
static void face(struct EnemyActor*q){q->params[0]&=1;q->actor.transform.facing=q->params[0];}
static void speed(struct EnemyActor*q){q->actor.vx=tk_word(q->params[1]<<5);if(!q->actor.transform.facing)q->actor.vx=tk_word(-q->actor.vx);}
static int scroll_init(struct WeaponWorld*w,struct EnemyActor*q){if(!w->secondary_view_valid)return enemy_fault(w);q->grunt153.scroll_trigger=tk_word(w->secondary_x_offset+q->params[1]);return 1;}
static int scroll_reached(struct WeaponWorld*w,struct EnemyActor*q){if(!q->params[1])return 0;if(!w->secondary_view_valid){enemy_fault(w);return 0;}return (uint16_t)w->secondary_x_offset>=(uint16_t)q->grunt153.scroll_trigger;}
static int near(struct WeaponWorld*w,struct WeaponActor*a){
 /* $08EFCE compares BOTH fixed slots, including a never-joined slot whose
  * source X is FFFF. Native world X includes camera carry; reconstruct the
  * absent slot at screen -1 instead of treating single-player as a fault. */
 for(unsigned i=0;i<2;i++){int16_t x=w->players[i]?(int16_t)(w->players[i]->actor.transform.x>>8):tk_word(w->camera_x-1);if(tk_word((a->transform.x>>8)-x)<96)return 1;}return 0;
}
static int bounds(struct WeaponWorld*w,struct NativeEntity*e){static const int16_t box[]={0,1,0,1};if(interaction_outside(w,&e->payload.enemy.actor,box,0)){entity_delete_children(w->entities,e);entity_delete(w->entities,e);return 0;}return !w->fault;}
static int latched(struct WeaponWorld*w,struct NativeEntity*e,int wide){struct WeaponActor*a=&e->payload.enemy.actor;int16_t x=tk_sx(w,a),y=tk_sy(w,a);if(a->hit.flags&128){if(x<(wide?-32:0)||x>320){entity_delete(w->entities,e);return 0;}}else if(x>0&&x<320&&y>256&&y<496)a->hit.flags|=128;return !w->fault;}
static void integrate(struct WeaponActor*a,unsigned mode){/* $08D2B0/$08D2F8/$08D34E: advance first; then optional acceleration, no clamp. */int16_t vy=a->vy;if(!mode)a->vy=0;weapon_integrate_free(a);if(!mode)a->vy=vy;if(mode==2){a->vx=tk_word(a->vx+a->ax);a->vy=tk_word(a->vy+a->ay);}}
static void direction_graph(struct EnemyActor*q){start_entry(q,grunt153_directions[(q->params[1]>>4)&7]);}
static int release_run(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;q->params[1]=32+random_mask(w,15);q->params[0]=1;face(q);speed(q);direction_graph(q);schedule(e,RUN);return !w->fault;}
static int choose(struct WeaponWorld*w,struct NativeEntity*e,int bank){static const uint8_t states[2][3]={{STAND,STAND2,WALK},{STAND_B,STAND_B2,WALK_B}};schedule(e,states[bank][grunt153_choices[bank][random_mask(w,3)]]);return !w->fault;}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned v=q->grunt153.variant;
 switch(a->state){
 case ROOT:face(q);
  if(v==157||v==158||v==159||v==160||v==161||v==163){a->transform.priority=0x8018;if(!scroll_init(w,q))return 0;
   if(v==157||v==158){a->palette=q->params[2]==1?0x13d:0x14a;start(q,v==157?G153A_A_WAIT:G153A_B_WAIT);a->state=WAIT_SCROLL;}
   else if(v==159){a->palette=q->params[2]==1?0xf7:0x149;q->grunt153.signal=0;start(q,G153A_CARRIER);a->state=CARRIER_WAIT;}
   else if(v==160){a->palette=0xfd;start(q,G153A_WIDE_WAIT);a->state=WAIT_SCROLL;}
   else{a->palette=0xf8;start(q,G153A_STAND);if(interaction_move(w,a,WMOVE_POINT)<0)return 0;animate(w,e);schedule(e,STAND);bounds(w,e);return 0;}
  }else if(v==162||v==182){a->transform.priority=v==162?0xd000:0xc000;speed(q);a->palette=v==162?0xf8:0x12a;q->grunt153.bob=v==182;if(v==162)start(q,G153A_HOP_UP);schedule(e,HOP_UP);return 2;}
  else if(v==164){a->transform.priority=0x8018;speed(q);a->palette=0xf8;schedule(e,STANDING);return 2;}
  else if(v==165||v==166){a->transform.priority=0x8018;if(v==165&&!q->params[1])q->params[1]=16;speed(q);a->palette=0xf9;start(q,G153A_RUN_FIXED);a->state=RUN_FIXED;}
  else if(v==153||v==154||v==155||v==184){a->transform.priority=0x8018;speed(q);a->palette=v==184?0x128:0xfa+q->params[2];if(v==153)direction_graph(q);if(v==184)start(q,G153A_RUN184);if(interaction_move(w,a,WMOVE_POINT)<0)return 0;animate(w,e);if(v==155){start(q,G153A_WALK2);a->state=WALK_START;}else if(v==154)schedule(e,STAND_B);else a->state=RUN;return 0;}
  else if(v==167){speed(q);a->transform.facing^=1;a->palette=0xfe;start(q,G153A_SENTRY);a->transform.priority=65535;a->display_flags|=64;a->state=SENTRY;}
  else if(v==180){a->transform.priority=0x8018;if(!scroll_init(w,q))return 0;a->palette=0x124;start(q,G153A_ONE_SHOT);a->transform.priority=65535;a->display_flags|=64;(void)scroll_reached(w,q);a->state=ONE_SHOT;return 0;}
  else if(v==181){speed(q);a->vx>>=1;a->palette=0x125;a->transform.priority=65535;a->display_flags|=64;q->grunt153.amplitude=a->vx;schedule(e,SWING_START);return 2;}
  else if(v==183){a->transform.priority=0x8018;speed(q);a->vy=-768;a->ay=16;a->palette=0x124;start(q,G153A_LEAP1);if(interaction_move(w,a,WMOVE_POINT)<0)return 0;animate(w,e);q->grunt153.timer=12+random_mask(w,15);a->state=LEAP1;return 0;}
  else return enemy_fault(w);break;
 case RUN:case CARRIER_DROP:break;
 case DROPPED:a->transform.priority=0xc01c;face(q);speed(q);a->vx=tk_word(-a->vx);a->ay=16;transform_integer_offset(&a->transform,0,32);a->palette=q->params[2]==1?0xf7:0x149;start(q,G153A_DROPPED);break;
 case HOP_UP:case HOP_DOWN:a->ay=q->grunt153.bob?4:14;if(a->state==HOP_DOWN)a->ay=tk_word(-a->ay);q->grunt153.timer=(q->grunt153.bob?6:8)+random_mask(w,7);start(q,q->grunt153.bob?G153A_BOB:a->state==HOP_UP?G153A_HOP_UP:G153A_HOP_DOWN);break;
 case STAND:start(q,G153A_STAND);break;
 case STAND2:start(q,G153A_STAND2);break;
 case WALK:q->params[0]=random_mask(w,1);a->vx=q->params[0]?128:-128;face(q);start(q,G153A_WALK);break;
 case HIT:transform_integer_offset(&a->transform,0,4);q->grunt153.timer=30+random_mask(w,15);if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;animate(w,e);schedule(e,HIT_ANGLE);bounds(w,e);return 0;
 case HIT_ANGLE:q->selector=16+random_mask(w,31);if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;animate(w,e);schedule(e,HIT_LAUNCH);bounds(w,e);return 0;
 case HIT_LAUNCH:q->scale=512+random_mask(w,255);weapon_vector(q->selector,q->scale,&a->vx,&a->vy);a->transform.facing=1;start(q,G153A_HOP_UP);if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;animate(w,e);a->state=HIT_FLY;bounds(w,e);return 0;
 case STANDING:start(q,G153A_STAND);break;
 case STAND_B:start(q,G153A_STAND_B);break;
 case STAND_B2:start(q,G153A_STAND_B2);break;
 case WALK_B:start(q,G153A_WALK2);break;
 case SWING_START:start(q,G153A_SWING);a->vx=0;a->ax=q->params[0]?8:-8;face(q);a->state=SWING;break;
 case PAUSE:q->grunt153.timer=tk_word(q->grunt153.timer+random_mask(w,7));break;/* source adds to allocator/prior timer, does NOT reset */
 case BACK:q->grunt153.timer=12+random_mask(w,7);a->ax=tk_word(-a->ax);start(q,G153A_BACK);break;
 case TURN:start(q,G153A_TURN);break;
 case LEAP2:a->vy=tk_word(-(64+random_mask(w,63)));a->ay=4;q->grunt153.timer=11;start(q,G153A_LEAP2);break;
 case LEAP3:start(q,G153A_LEAP3);break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static int frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;unsigned v=q->grunt153.variant;int done;if((s==WALK_START||s==SENTRY||s==SENTRY_WAIT)&&!scene_preserve_screen(w,a))return 0;
 if(s==WAIT_SCROLL||s==STATIC||s==CARRIER_WAIT||s==CARRIER_DROP||s==CARRIER_IDLE){
  if(s==WAIT_SCROLL&&scroll_reached(w,q)){start(q,v==157?G153A_A_ACTIVE:v==158?G153A_B_ACTIVE:G153A_WIDE_ACTIVE);a->state=STATIC;}
  if(s==CARRIER_WAIT&&scroll_reached(w,q)){start(q,G153A_CARRIER_RELEASE);schedule(e,CARRIER_DROP);}
  if(s==CARRIER_DROP&&q->grunt153.signal){struct NativeEntity*n=allocate(w,entity_ref(e),ENTITY_FIRST_CHILD,&a->transform,DROPPED);if(!n)return 0;n->payload.enemy.params[0]=a->transform.facing;n->payload.enemy.params[1]=48;n->payload.enemy.params[2]=q->params[2];a->state=CARRIER_IDLE;}
  if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;if(v==160){if(!latched(w,e,1))return 0;}else if(!bounds(w,e))return 0;animate(w,e);return !w->fault;
 }
 if(s==DROPPED){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;integrate(a,2);static const int16_t box[]={0,1,0,1};if(interaction_outside(w,a,box,1)){entity_delete_children(w->entities,e);entity_delete(w->entities,e);return 0;}animate(w,e);return !w->fault;}
 if(s==HOP_UP||s==HOP_DOWN){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;integrate(a,2);animate(w,e);if((s==HOP_UP&&tk_sy(w,a)>=464)||(s==HOP_DOWN&&tk_sy(w,a)<352)){schedule(e,s==HOP_UP?HOP_DOWN:HOP_UP);return !w->fault;}q->grunt153.timer=tk_word(q->grunt153.timer-1);if(!(uint16_t)q->grunt153.timer)schedule(e,s==HOP_UP?HOP_DOWN:HOP_UP);return bounds(w,e);}
 if(s==STAND||s==STAND2||s==WALK||s==STANDING||s==HIT_FLY){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;if(s==WALK||s==HIT_FLY)integrate(a,1);done=animate(w,e);
  if(s==STAND&&done){int hit=q->params[1]?scroll_reached(w,q):near(w,a);if(hit)schedule(e,HIT);else choose(w,e,0);}
  if(s==STAND2&&done)choose(w,e,0);if(s==WALK&&done)schedule(e,STAND);if(s==STANDING&&near(w,a))schedule(e,HIT);
  if(s==HIT_FLY){q->grunt153.timer=tk_word(q->grunt153.timer-1);if(!(uint16_t)q->grunt153.timer){a->transform.priority=0xd000;a->vx=(int16_t)q->scale;a->vy=0;schedule(e,HOP_UP);}}return bounds(w,e);
 }
 if(s==RUN_FIXED){if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;integrate(a,0);if(!bounds(w,e))return 0;animate(w,e);return !w->fault;}
 if(s==RUN){integrate(a,0);if(interaction_move(w,a,WMOVE_SYNC)<0||!latched(w,e,0))return 0;animate(w,e);return !w->fault;}
 if(s==STAND_B||s==STAND_B2||s==WALK_B||s==WALK_START){if(s!=WALK_START&&interaction_move(w,a,WMOVE_SYNC)<0)return 0;if(s==WALK_B||s==WALK_START)integrate(a,0);done=animate(w,e);if(done){if(s==STAND_B||s==WALK_START){if(near(w,a))release_run(w,e);else if(s==STAND_B)choose(w,e,1);else start(q,G153A_WALK2);}else if(s==STAND_B2)choose(w,e,1);else schedule(e,STAND_B);}return latched(w,e,0);}
 if(s==SENTRY){if(!bounds(w,e))return 0;if(animate(w,e)){transform_integer_offset(&a->transform,q->params[0]?72:-72,0);q->grunt153.wait=q->params[2];a->state=SENTRY_WAIT;}return !w->fault;}
 if(s==SENTRY_WAIT){q->grunt153.wait--;if(!q->grunt153.wait){start(q,G153A_SENTRY);a->state=SENTRY;}integrate(a,1);return bounds(w,e);}
 if(s==ONE_SHOT){if(interaction_move(w,a,WMOVE_SYNC)<0||!bounds(w,e))return 0;if(animate(w,e))entity_delete(w->entities,e);return !w->fault;}
 if(s==SWING||s==PAUSE||s==BACK||s==TURN){if(s==PAUSE){q->grunt153.timer=tk_word(q->grunt153.timer-1);if(q->grunt153.timer<=0)schedule(e,BACK);}if(s!=TURN)integrate(a,0);if(interaction_move(w,a,WMOVE_SYNC)<0)return 0;
  if(s==SWING){a->vx=tk_word(a->vx+a->ax);if(q->params[0]?a->vx>=q->grunt153.amplitude:a->vx<=q->grunt153.amplitude)schedule(e,PAUSE);}
  else if(s==BACK){a->vx=tk_word(a->vx+a->ax);q->grunt153.timer=tk_word(q->grunt153.timer-1);if(q->grunt153.timer<=0||(q->params[0]?a->vx<=0:a->vx>=0))schedule(e,TURN);}
  if(s==TURN){if(animate(w,e)){q->grunt153.amplitude=tk_word(-q->grunt153.amplitude);q->params[0]^=1;schedule(e,SWING_START);}return latched(w,e,0);}
  if(!latched(w,e,0))return 0;animate(w,e);return !w->fault;
 }
 if(s==LEAP1||s==LEAP2||s==LEAP3){if(s!=LEAP3){q->grunt153.timer=tk_word(q->grunt153.timer-1);if(q->grunt153.timer<=0||(s==LEAP1&&a->vy>=64))schedule(e,s==LEAP1?LEAP2:LEAP3);}integrate(a,s==LEAP3?1:2);if(interaction_move(w,a,WMOVE_SYNC)<0||!bounds(w,e))return 0;animate(w,e);return !w->fault;}
 return enemy_fault(w);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){(void)p;struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;for(unsigned b=0;b<6;b++){if(q->entry){q->entry=0;int r=enter(w,e);if(!r||w->fault)return;if(r==2)continue;}if(!e->active||e->deleting)return;frame(w,e,q->actor.state);return;}enemy_fault(w);}
int grunt153_spawn(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){if(!((id>=153&&id<=155)||(id>=157&&id<=167)||(id>=180&&id<=184)))return enemy_fault(w);struct NativeEntity*e=allocate(w,p,o,t,ROOT);if(!e)return 0;e->payload.enemy.grunt153.variant=id;if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];return 1;}
/* Unreferenced source entry wrappers remain explicit, rather than injecting them
 * into authored template paths. Their unspecified parameters stay allocator FF. */
struct NativeEntity*grunt153_spawn_alias(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,enum Grunt153Alias alias){static const uint8_t ids[]={159,161,164,165,155,167};if((unsigned)alias>=6){enemy_fault(w);return 0;}struct NativeEntity*e=allocate(w,p,o,t,ROOT);if(!e)return 0;struct EnemyActor*q=&e->payload.enemy;q->grunt153.variant=ids[alias];if(alias==GRUNT_CARRIER_FIXED){m4_screen(w,&q->actor,64,336);q->params[0]=q->params[1]=0;}else if(alias==GRUNT_RANDOM_FIXED)m4_screen(w,&q->actor,64,336);else if(alias==GRUNT_STANDING_FIXED){q->params[0]=1;q->params[1]=8;}else if(alias==GRUNT_RUNNER_FIXED){m4_screen(w,&q->actor,96,336);q->params[0]=1;q->params[1]=16;}else if(alias==GRUNT_WALKER_FIXED){m4_screen(w,&q->actor,128,336);q->params[0]=1;q->params[1]=8;q->params[2]=2;}else{m4_screen(w,&q->actor,64,384);q->params[0]=1;q->params[1]=32;q->params[2]=20;}return e;}
int grunt153_has_children(struct WeaponWorld*w,const struct NativeEntity*e){return m4_has_children(w,e);}
