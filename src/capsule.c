#include "scene_runtime.h"
#include "tank_internal.h"
#include "../generated/capsule-programs.h"
/* $08CCC6..$08CE64 and $08D3B4..$08D994. Scene/camera outputs remain typed. */
enum State {FLY,DESCEND,REST,DORMANT,OPEN,FLASH,FADE_IN,GLOW,GLOW_DOWN,GLOW2,GLOW_UP,
 RISE,RETURN_UP,OUT,END_FLASH,END_DRAW,DIM,FALL,VANISH,MARKER_FIXED,MARKER,MARKER2,MARKER3,
 SCROLL_UP,WAIT,IDLE,DROPPER,DROP_WAIT,DROP_FINISH,DROP_EXIT,ITEM,PLAYBACK,VARIANT0,VARIANT1,VARIANT2,
 DROP_CREDIT_OUT};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static void schedule(struct NativeEntity*e,unsigned s){e->tick=tick;enemy_schedule(&e->payload.enemy,s);}
static struct NativeEntity*allocate(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,unsigned s){
 struct NativeEntity*e=enemy_allocate(w,p,o,t,tick);if(e){struct EnemyActor*q=&e->payload.enemy;q->capsule=(__typeof__(q->capsule)){.velocity_x=-1,.velocity_y=-1,.accel_x=-1,.accel_y=-1,.timer=-1,.origin=-1};schedule(e,s);}return e;
}
static void screen(struct WeaponWorld*w,struct PlayerTransform*t,int x,int y){t->x=tk_word(x+w->camera_x)*256;t->y=(512-tk_word(y-w->camera_y))*256;}
/* $5DD02 copies integer XY, priority, facing and source+$11, but no fractions. */
static struct PlayerTransform copy(const struct EnemyActor*q){struct PlayerTransform t=q->actor.transform;t.display_group=q->source_flags;transform_integer_copy(&t,&q->actor.transform);return t;}
static struct NativeEntity*child(struct WeaponWorld*w,struct NativeEntity*p,unsigned s){struct PlayerTransform t=copy(&p->payload.enemy);return allocate(w,entity_ref(p),ENTITY_FIRST_CHILD,&t,s);}
/* The source ending has completed NAZCA before requesting its final dropper.
 * Reuse that actor for THANKS -> SPECIAL THANKS -> GBA PORTING -> original FIN.
 * Existing page0..9 events and FIN's audio/fade/unlock behavior stay intact. */
static int dropper(struct WeaponWorld*w,struct NativeEntity*p){struct PlayerTransform t={.display_group=255};screen(w,&t,160,256);struct NativeEntity*e=allocate(w,entity_ref(p),ENTITY_FIRST_CHILD,&t,DROPPER);if(!e)return 0;e->payload.enemy.params[0]=11;e->payload.enemy.params[1]=8;return 1;}
static void start_entry(struct EnemyActor*q,uint16_t n){if(n!=65535){animation_begin(&q->actor.animation,n);q->actor.movement_flags|=32;}}
static void start(struct EnemyActor*q,unsigned n){start_entry(q,capsule_entries[n]);}
/* +$5C is both a WORD saved Y and a BYTE signal (the high byte on 68000). */
static uint8_t signal(const struct EnemyActor*q){return (uint16_t)q->capsule.origin>>8;}
static void set_signal(struct EnemyActor*q,uint8_t n){q->capsule.origin=tk_word(((unsigned)n<<8)|((uint16_t)q->capsule.origin&255));}
struct Context{struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read(void*ctx,unsigned f,unsigned width){struct Context*c=ctx;if(f!=CAPF_FADE_STEP||width!=1){enemy_fault(c->w);return 0;}return signal(&c->e->payload.enemy);}
static int field(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t v){struct Context*c=ctx;uint32_t old=read(ctx,f,width);if(op==AN_FIELD_AND)v&=old;else if(op==AN_FIELD_OR)v|=old;else if(op==AN_FIELD_XOR)v^=old;else if(op!=AN_FIELD_SET)return enemy_fault(c->w);set_signal(&c->e->payload.enemy,v);return !c->w->fault;}
static int sound(void*ctx,uint16_t n,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,n,stop)?1:enemy_fault(c->w);}
static int animate(struct WeaponWorld*w,struct NativeEntity*e){struct Animation*a=&e->payload.enemy.actor.animation;if(a->cursor==65535)return 0;struct Context c={w,e};struct AnimationOwner o={.context=&c,.read=read,.write=field,.sound=sound};int r=animation_step(a,&capsule_program,&o);if(a->fault)enemy_fault(w);return r;}
static int sync(struct WeaponWorld*w,struct WeaponActor*a){return interaction_move(w,a,WMOVE_SYNC)>=0;}
static void integrate(struct WeaponActor*a,int vertical){int16_t saved=vertical?a->vx:a->vy;if(vertical)a->vx=0;else a->vy=0;weapon_integrate_free(a);if(vertical)a->vx=saved;else a->vy=saved;}
static int outside(struct WeaponWorld*w,struct WeaponActor*a){static const int16_t box[]={0,1,0,1};return interaction_outside(w,a,box,0);}
static void erase(struct WeaponWorld*w,struct NativeEntity*e,int tree){if(tree)entity_delete_children(w->entities,e);entity_delete(w->entities,e);}
static int bounds(struct WeaponWorld*w,struct NativeEntity*e){if(outside(w,&e->payload.enemy.actor)){erase(w,e,1);return 0;}return !w->fault;}
static int latched_y(struct WeaponWorld*w,struct NativeEntity*e){struct WeaponActor*a=&e->payload.enemy.actor;int16_t x=tk_sx(w,a),y=tk_sy(w,a);if(a->hit.flags&128){if(y<256||y>511){erase(w,e,0);return 0;}}else if(x>0&&x<320&&y>256&&y<511)a->hit.flags|=128;return !w->fault;}
static int reached(struct WeaponWorld*w,unsigned n){if(!w->mission_valid){enemy_fault(w);return 0;}return w->mission_progress>=n;}
static int mission_end(struct WeaponWorld*w,struct NativeEntity*e){if(w->scene_number==11&&reached(w,0x42c0))schedule(e,OUT);if(w->scene_number==12&&reached(w,0x4280))schedule(e,DIM);return !w->fault;}
static void publish(struct WeaponWorld*w,struct EnemyActor*q,int vertical){if(vertical){w->enemies->cutscene.velocity_y_q16=q->capsule.velocity_y;w->enemies->cutscene.velocity_mask|=2;if(w->scene_runtime)w->scene_runtime->camera.seed_y=w->enemies->cutscene.velocity_y_q16;q->capsule.velocity_y=(int32_t)((uint32_t)q->capsule.velocity_y+(uint32_t)q->capsule.accel_y);}else{w->enemies->cutscene.velocity_x_q16=q->capsule.velocity_x;w->enemies->cutscene.velocity_mask|=1;if(w->scene_runtime)w->scene_runtime->camera.seed_x=w->enemies->cutscene.velocity_x_q16;q->capsule.velocity_x=(int32_t)((uint32_t)q->capsule.velocity_x+(uint32_t)q->capsule.accel_x);}}
static void stop_y(struct WeaponWorld*w){w->enemies->cutscene.velocity_y_q16=0;w->enemies->cutscene.velocity_mask|=2;if(w->scene_runtime)w->scene_runtime->camera.seed_y=w->enemies->cutscene.velocity_y_q16;}
static void scale_add(struct EnemyActor*q,int n){q->scale_x+=n;q->scale_y+=n;}
static void scale_set(struct EnemyActor*q,unsigned n){q->scale_x=q->scale_y=n;}
static int timer_done(struct EnemyActor*q){q->capsule.timer=tk_word(q->capsule.timer-1);return q->capsule.timer<=0;}
static int enter(struct WeaponWorld*w,struct NativeEntity*e){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 switch(a->state){
 case FLY:a->transform.priority=0x8000;a->palette=0x0e;start(q,CAPA_FLY);a->transform.facing=1;screen(w,&a->transform,288,400);if(!tk_settle(w,a))return 0;break;
 case OPEN:a->transform.facing=0;a->transform.priority=0xf000;a->palette=0xf5;start(q,CAPA_OPEN);transform_integer_offset(&a->transform,4,32);scale_set(q,255);set_signal(q,0);a->vx=512;q->capsule.timer=4;a->state=FLASH;return 0;
 case GLOW:start(q,CAPA_GLOW);break;
 case GLOW2:start(q,CAPA_GLOW2);break;
 case RISE:if(!scene_preserve_screen(w,a))return 0;q->capsule.origin=tk_sy(w,a);a->vy=-64;a->transform.priority=0x2000;q->capsule.timer=200;break;
 case OUT:set_signal(q,0);q->capsule.velocity_x=0x20000;q->capsule.accel_x=0;q->capsule.velocity_y=-0x10000;q->capsule.accel_y=0;start(q,CAPA_OUT);break;
 case END_FLASH:a->palette=0xf4;start(q,CAPA_END_FLASH);q->capsule.timer=20;break;
 case MARKER_FIXED:screen(w,&a->transform,240,416);a->state=MARKER;/* fall through */
 case MARKER:a->palette=0x98;start(q,CAPA_MARKER);break;
 case DROPPER:a->transform.priority=0xfffe;a->display_flags|=64;a->vy=tk_word(q->params[1]<<5);a->palette=0x136;start_entry(q,capsule_droppers[q->params[0]<=13?q->params[0]:0]);break;
 case ITEM:a->transform.priority=0xffff;a->display_flags|=64;a->palette=0x5f;start(q,CAPA_ITEM);break;
 case PLAYBACK:w->enemies->cutscene.playback_mode=2;erase(w,e,0);return 0;
 case VARIANT0:case VARIANT1:case VARIANT2:w->enemies->cutscene.variant=a->state-VARIANT0;erase(w,e,0);return 0;
 case GLOW_DOWN:case GLOW_UP:case DIM:break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static int frame(struct WeaponWorld*w,struct NativeEntity*e,unsigned s){struct EnemyActor*q=&e->payload.enemy;struct WeaponActor*a=&q->actor;
 if(s==DORMANT)return 1;
 if(s==FLY||s==DESCEND||s==REST){if(!sync(w,a))return 0;int done=animate(w,e);if(s==FLY&&tk_sx(w,a)<=256){start(q,CAPA_DESCEND);a->state=DESCEND;}if(s==DESCEND&&done){if(!child(w,e,OPEN))return 0;start(q,CAPA_REST);a->state=REST;}if(s==REST&&outside(w,a))a->state=DORMANT;return !w->fault;}
 if(s==END_FLASH||s==END_DRAW){if(!scene_scroll_tertiary(w,a))return 0;if(s==END_FLASH){if(timer_done(q))a->state=END_DRAW;}else if(animate(w,e)){struct PlayerTransform t=copy(q);if(!nest185_spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,189,&t,0)||!dropper(w,e))return 0;erase(w,e,0);}return !w->fault;}
 if(s==FALL){if(!sync(w,a))return 0;int r=interaction_move(w,a,WMOVE_BODY);if(r<0)return 0;if(r){q->capsule.timer=117;a->state=VANISH;}animate(w,e);return !w->fault;}
 if(s==VANISH){if(timer_done(q)){erase(w,e,0);return 0;}if(!sync(w,a))return 0;animate(w,e);return !w->fault;}
 if(s>=MARKER&&s<=IDLE){
  if(s==MARKER&&reached(w,0x4300)){start(q,CAPA_MARKER2);a->state=MARKER2;}
  else if(s==MARKER2&&reached(w,0x433f)){start(q,CAPA_MARKER3);a->state=MARKER3;}
  else if(s==SCROLL_UP){if(!w->secondary_view_valid)return enemy_fault(w);if((uint16_t)w->secondary_y_offset<=216){q->capsule.timer=180;stop_y(w);q->capsule.velocity_y=q->capsule.accel_y=0;a->state=WAIT;}publish(w,q,1);}
  else if(s==WAIT&&timer_done(q)){if(!dropper(w,e))return 0;stop_y(w);a->state=IDLE;}
  if(w->fault||!sync(w,a))return 0;int done=animate(w,e);if(s==MARKER3&&done){q->capsule.timer=60;q->capsule.velocity_y=-0x8000;q->capsule.accel_y=0;a->state=SCROLL_UP;}return !w->fault;
 }
 /* These original handlers deliberately do not call the primary SYNC helper. */
 if(!scene_preserve_screen(w,a))return 0;
 switch(s){
 case FLASH:integrate(a,0);animate(w,e);if(timer_done(q)){scale_set(q,56);start(q,CAPA_FLASH);a->state=FADE_IN;}break;
 case FADE_IN:scale_add(q,2);if(q->scale_x>=127)scale_set(q,127);integrate(a,0);animate(w,e);if(tk_sx(w,a)>=160)schedule(e,GLOW_UP);break;
 case GLOW:if(animate(w,e))schedule(e,GLOW_DOWN);break;
 case GLOW_DOWN:scale_add(q,-2);if(q->scale_x<=127)schedule(e,GLOW2);if(!mission_end(w,e))return 0;animate(w,e);break;
 case GLOW2:if(animate(w,e))schedule(e,GLOW_UP);mission_end(w,e);break;
 case GLOW_UP:scale_add(q,2);if(q->scale_x>=208)schedule(e,GLOW);animate(w,e);mission_end(w,e);break;
 case RISE:scale_add(q,-2);if(q->scale_x<=52)scale_set(q,52);integrate(a,1);animate(w,e);if(timer_done(q)){a->vy=128;a->state=RETURN_UP;}break;
 case RETURN_UP:scale_add(q,2);if(q->scale_x>=127)scale_set(q,127);integrate(a,1);animate(w,e);if(q->capsule.origin<tk_sy(w,a)){a->transform.priority=0xf000;schedule(e,GLOW2);}break;
 case OUT:publish(w,q,0);publish(w,q,1);if(!bounds(w,e))return 0;if(q->scale_x<2){if(!child(w,e,END_FLASH))return 0;erase(w,e,0);return 0;}scale_add(q,signal(q));animate(w,e);break;
 case DIM:scale_add(q,-2);if(q->scale_x<52)scale_set(q,52);animate(w,e);if(reached(w,0x433f)){a->vx=0;a->vy=-128;a->ay=-64;a->state=FALL;}break;
 case DROPPER:integrate(a,1);if(!q->params[0]&&w->scene_number==11&&tk_sy(w,a)>=368)a->state=DROP_WAIT;if(q->params[0]>=10&&tk_sy(w,a)>=368){q->capsule.timer=120;a->state=DROP_FINISH;}if(!latched_y(w,e))return 0;animate(w,e);break;
 case DROP_WAIT:animate(w,e);if(reached(w,0x1000))erase(w,e,0);break;
 case DROP_FINISH:if(timer_done(q)){if(q->params[0]>=11){a->vy=512;a->state=DROP_CREDIT_OUT;}else{if(w->scene_number==12&&(!w->audio_control||!w->audio_control(w->scene,0x0a,0x80)))return enemy_fault(w);if(!scene_fade_spawn(w,entity_ref(e),4,0,0))return 0;q->capsule.timer=160;a->state=DROP_EXIT;}}animate(w,e);break;
 case DROP_CREDIT_OUT:
  integrate(a,1);
  if(tk_sy(w,a)>=560){
   q->params[0]=q->params[0]==13?10:q->params[0]+1;
   screen(w,&a->transform,160,256);a->hit.flags&=(uint8_t)~128u;
   a->animation.drawing=65535;schedule(e,DROPPER);
  }else animate(w,e);
  break;
 case DROP_EXIT:if(timer_done(q)){w->enemies->scripted_player_lock=0;erase(w,e,0);return 0;}animate(w,e);break;
 case ITEM:integrate(a,1);q->params[1]--;if(!q->params[1]){erase(w,e,0);return 0;}if(!bounds(w,e))return 0;animate(w,e);break;
 default:return enemy_fault(w);
 }return !w->fault;
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){(void)p;struct WeaponWorld*w=world;if(w->fault)return;struct EnemyActor*q=&e->payload.enemy;if(q->entry){q->entry=0;if(!enter(w,e)||w->fault)return;}if(e->active&&!e->deleting)frame(w,e,q->actor.state);}
struct NativeEntity*capsule_spawn(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,enum CapsuleEntry which){static const uint8_t state[]={FLY,OPEN,MARKER_FIXED};if((unsigned)which>=sizeof(state)){enemy_fault(w);return 0;}return allocate(w,p,o,t,state[which]);}
int capsule_spawn_template(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){unsigned s;switch(id){case 156:s=MARKER;break;case 244:s=DROPPER;break;case 245:s=ITEM;break;case 246:s=PLAYBACK;break;case 247:s=VARIANT0;break;case 248:s=VARIANT1;break;case 320:s=VARIANT2;break;default:return enemy_fault(w);}struct NativeEntity*e=allocate(w,p,o,t,s);if(!e)return 0;if(params)for(unsigned i=0;i<8;i++)e->payload.enemy.params[i]=params[i];return 1;}
int capsule_rise(struct WeaponWorld*w,struct NativeEntity*e){if(!e||e->kind!=ENTITY_ENEMY)return enemy_fault(w);schedule(e,RISE);tick(w->entities,e,w);return !w->fault;}
