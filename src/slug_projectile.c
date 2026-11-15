#include "slug_projectile.h"
#include "bosses.h"
#include "../generated/slug-projectile-programs.h"
enum Kind {K_NORMAL,K_IDLE,K_LONG,K_LIGHT,K_DROP_A,K_DROP_B,K_EXHAUST,K_EXHAUST_AIR,K_TRAIL,K_SPARK,K_SMOKE,K_BLAST};
enum State {NEW,FLY_START,FLY,FALL,GLIDE_START,GLIDE,CRASH,DESPAWN,RELEASE,DROP,
 EXHAUST,TRAIL,SMOKE_START,SMOKE,BLAST_A,BLAST_B,BLAST_C,DELETE};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static int fail(struct WeaponWorld*w){if(!w->fault)w->fault=24;return 0;}
static int16_t word(int v){return (int16_t)(uint16_t)v;}
static int16_t clamp(int16_t v,int n){return v>n?(int16_t)n:v<-n?(int16_t)-n:v;}
static struct SlugSharedState*slug_parent(struct WeaponWorld*w,struct NativeEntity*e){
 struct NativeEntity*p=entity_get(w->entities,e->parent);
 if(!p||p->kind!=ENTITY_SLUG||!p->payload.slug){fail(w);return 0;}return p->payload.slug;
}
static struct NativeEntity*spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,unsigned kind,const struct PlayerTransform*t){
 if(w->fault||!w->entities){fail(w);return 0;}
 struct NativeEntity*e=entity_spawn(w->entities,parent,ENTITY_SLUG_AUX,order,tick);
 if(!e){fail(w);return 0;}struct SlugAuxActor*q=&e->payload.slug_aux;
 q->actor=(struct WeaponActor){.active=1,.owner=255,.damage_owner=255,.score_owner=255,.platform_ids=255,
  .ground_slope=-1,.ground_tag=-1,.phase=255,.ttl=255,.raw_surface=255,.origin_y_up=-1,.trail_drawing=65535,
  .hit={.id=e->index,.hp=-1,.impact_x=-1,.impact_y=-1,.last_attacker=65535,.last_kind=255,.flags=64},
  .animation={.drawing=65535,.cursor=65535,.last_draw_cursor=65535}};
 q->kind=(uint8_t)kind;q->actor.transform=*t;q->actor.transform.display_group=0;
 transform_integer_copy(&q->actor.transform,t);return e;
}
static void retire(struct WeaponWorld*w,struct NativeEntity*e,int children){
 e->payload.slug_aux.actor.active=0;if(children)entity_delete_children(w->entities,e);entity_delete(w->entities,e);
}
static int release_count(struct WeaponWorld*w,struct NativeEntity*e){
 struct SlugAuxActor*q=&e->payload.slug_aux;struct SlugSharedState*s=slug_parent(w,e);if(!s)return 0;
 if(!q->counted)return fail(w);s->cannon_count--;q->counted=0;return 1;
}
static int noise(struct WeaponWorld*w,uint16_t command){return w->sound&&w->sound(w->scene,command,0)?1:fail(w);}
static void start(struct SlugAuxActor*q,unsigned sequence){animation_begin(&q->actor.animation,slug_projectile_entries[sequence]);q->actor.movement_flags|=32;}
static uint32_t read_field(void*ctx,unsigned field,unsigned width){
 struct SlugAuxActor*q=ctx;if(field||width!=2){q->actor.animation.fault=3;return 0;}return q->actor.variant;
}
static int animate(struct WeaponWorld*w,struct SlugAuxActor*q){
 struct AnimationOwner owner={.context=q,.read=read_field};
 int done=animation_step(&q->actor.animation,&slug_projectile_program,&owner);
 if(q->actor.animation.fault)fail(w);return done;
}
static int move(struct WeaponWorld*w,struct WeaponActor*a,enum WeaponMove policy){
 struct WeaponContact hit={0};if(!w->move){fail(w);return -1;}
 int result=w->move(w->scene,a,policy,&hit);if(result<0)fail(w);return result;
}
static int attack(struct WeaponWorld*w,struct WeaponActor*a){
 if(!a->attack)return 1;if(!w->attack)return fail(w);
 a->hit.x=word(a->transform.x>>8);a->hit.y=transform_y_up(&a->transform);a->hit.flip=a->transform.facing;
 for(unsigned i=0;i<a->attack->count;i++)if(!w->attack(w->scene,&a->hit,&a->attack->records[i],0))return fail(w);
 return 1;
}
static int outside(struct WeaponWorld*w,struct WeaponActor*a,const int16_t*b){
 int16_t x=word((a->transform.x>>8)-w->camera_x),y=word(transform_y_up(&a->transform)+w->camera_y);
 a->hit.flags|=128;return x<b[0]||x>word(320+b[1])||y<word(256-b[3])||y>word(496-b[2]);
}
static uint8_t angle256(int16_t vx,int16_t vy){
 /* $05E018: byte log/exp approximation, including complement-based
  * magnitude and the sign of the BYTE subtraction, not floating atan2. */
 int16_t hx=vx>>1,hy=vy>>1;uint8_t sx=hx<0?255:0,sy=hy<0?255:0;
 uint8_t x=(uint8_t)hx^sx,y=(uint8_t)hy^sy,swap_sign=((uint8_t)(x-y)&128)?255:0;
 if(x>=y){uint8_t t=x;x=y;y=t;}
 uint8_t angle;if(!x)angle=swap_sign&64;
 else{angle=slug_atan_exp[(uint8_t)(slug_atan_log[x]-slug_atan_log[y])];angle=(uint8_t)((angle^swap_sign)-swap_sign+32);}
 uint8_t sign=sx^sy;return (uint8_t)((angle^sign)-sign+(sx&128));
}
static void aim(struct SlugAuxActor*q){q->actor.variant=angle256(q->actor.vx>>4,q->actor.vy>>4)>>3;}
static int offset_mount(struct WeaponWorld*w,struct SlugSharedState*s,struct WeaponActor*a){
 const struct SlugMount*m=slug_mount(s);if(!m)return fail(w);
 transform_integer_offset(&a->transform,m->cannon_x,m->cannon_y);return 1;
}
static int initialize(struct WeaponWorld*w,struct NativeEntity*e){
 struct SlugAuxActor*q=&e->payload.slug_aux;struct WeaponActor*a=&q->actor;
 if(q->kind==K_BLAST){a->state=BLAST_A;return 1;}
 if(q->kind==K_TRAIL||q->kind==K_SPARK){
  a->palette=q->kind==K_TRAIL?4:0x58;start(q,q->kind==K_TRAIL?SPA_TRAIL:SPA_SPARK);a->timer=2;a->state=TRAIL;return 1;
 }
 if(q->kind==K_SMOKE){a->state=SMOKE_START;return 1;}
 struct SlugSharedState*s=slug_parent(w,e);if(!s)return 0;
 if(q->kind==K_EXHAUST||q->kind==K_EXHAUST_AIR){
  if(q->kind==K_EXHAUST_AIR||(s->status_flags&4)){
   if(!offset_mount(w,s,a))return 0;
   int16_t angle=s->angle,vx,vy;if(angle>32)angle=64;if(angle<-32)angle=-32;
   weapon_vector((uint8_t)angle,16,&vx,&vy);transform_integer_offset(&a->transform,vx,vy);start(q,SPA_EXHAUST_ANGLE);
  }else{
   if(s->slope_index>=5)return fail(w);q->angle=s->slope_index;a->bounces=s->slope_index;
   uint16_t entry=slug_exhaust_entries[s->slope_index];if(entry!=65535){animation_begin(&a->animation,entry);a->movement_flags|=32;}
  }
  a->palette=8;transform_integer_offset(&a->transform,-4,0);a->state=EXHAUST;return 1;
 }
 a->attack=&slug_projectile_attacks[q->kind==K_LIGHT?SPH_LIGHT:SPH_CANNON];hit_begin_attack(&a->hit);
 if(q->kind==K_IDLE||q->kind==K_DROP_A||q->kind==K_DROP_B){
  if(s->slope_index>=5)return fail(w);
  const int16_t*row=slug_cannon_offsets[q->kind==K_DROP_B][s->slope_index];
  start(q,q->kind==K_IDLE?SPA_CANNON:SPA_DROP);transform_integer_offset(&a->transform,row[2],row[3]);
  q->angle=(uint16_t)row[4];q->range=0x4400;q->speed=0x600;
 }else{
  start(q,SPA_CANNON);q->angle=(uint8_t)(s->angle+(s->angle>0?8:16));q->speed=0x600;
  q->range=q->kind==K_LONG?0x6000:0x1000;q->frame=q->kind==K_LONG?0:255;
  if(q->kind!=K_LONG){
   unsigned bits=q->input_held&0xf0;
   if(q->layout==5&&bits){
    unsigned count=0;for(unsigned bit=16;bit<=128;bit<<=1)if(bits&bit)count++;
    if(count>=2){q->speed=0xc00;q->range=0x7400;q->angle=(q->angle-(count==4?-8:count==2?8:0))&0xf8;}
   }else if(q->layout==3&&bits&&(q->input_held&PB_RIGHT)){
    q->speed=0xc00;q->range=0x26aa;q->angle=(q->angle-8)&0xf8;q->frame=0;
   }
  }
  if(!offset_mount(w,s,a))return 0;if(q->kind!=K_LONG)transform_integer_offset(&a->transform,0,8);
 }
 if(!noise(w,0x1084))return 0;
 if(q->kind==K_DROP_A||q->kind==K_DROP_B){s->cannon_count++;q->counted=1;hit_begin_attack(&a->hit);a->state=DROP;return 1;}
 a->variant=q->angle>>3;weapon_vector((uint8_t)q->angle,0x700,&a->vx,&a->vy);
 a->transform.priority=0xd000;if(!weapon_spawn_vehicle_surface(w,entity_ref(e)))return 0;
 a->palette=0x58;q->distance=0;s->cannon_count++;q->counted=1;a->transform.priority&=0xffe3;
 hit_begin_attack(&a->hit);a->state=FLY_START;return 1;
}
static int trail(struct WeaponWorld*w,struct NativeEntity*e,int include_angle){
 struct SlugAuxActor*q=&e->payload.slug_aux;if(q->frame==255)return 1;
 q->frame++;if(q->layout!=2&&q->layout!=3&&(q->frame&11))return 1;
 struct NativeEntity*child=spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,K_TRAIL,&q->actor.transform);if(!child)return 0;
 struct SlugAuxActor*r=&child->payload.slug_aux;r->actor.variant=q->actor.variant;
 r->actor.transform.priority=q->actor.transform.priority;if(include_angle)r->angle=q->angle;return 1;
}
static void flight_tail(struct WeaponWorld*w,struct NativeEntity*e){
 struct SlugAuxActor*q=&e->payload.slug_aux;struct WeaponActor*a=&q->actor;
 animate(w,q);if(w->fault||!attack(w,a))return;
 if(a->hit.events&32)a->state=CRASH;
 if(a->hit.flags&2)a->state=a->hit.contact_resistance==6?DESPAWN:CRASH;
 if(outside(w,a,slug_cannon_bounds))a->state=RELEASE;
}
static int follow_parent(struct WeaponWorld*w,struct NativeEntity*e){
 struct WeaponActor*a=&e->payload.slug_aux.actor,*p=entity_weapon_actor(entity_get(w->entities,e->parent));
 if(!p)return fail(w);a->hit.flags|=64;a->transform.priority=p->transform.priority;
 transform_integer_offset(&a->transform,word((p->transform.x>>8)-(a->transform.x>>8)),word(transform_y_up(&p->transform)-transform_y_up(&a->transform)));return 1;
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 (void)p;struct WeaponWorld*w=world;struct SlugAuxActor*q=&e->payload.slug_aux;struct WeaponActor*a=&q->actor;
 static const int16_t default_bounds[4]={0,1,0,1};
 if(w->fault)return;if(a->state==NEW&&!initialize(w,e))return;
 unsigned state=a->state;
 if(state==SMOKE_START){
  if(!w->rng){fail(w);return;}a->palette=0x178;start(q,SPA_SMOKE);a->transform.priority=0x2000;
  transform_integer_offset(&a->transform,(source_random_word(w->rng)&15)-8,0);a->state=state=SMOKE;
 }
 if(state==FLY_START){
  if(q->distance<q->range){weapon_vector((uint8_t)q->angle,q->speed,&a->vx,&a->vy);a->state=FLY;}
  state=FLY;
 }
 if(state==FLY){
  q->distance=(uint16_t)(q->distance+a->vx);
  if(q->distance>=q->range){a->state=FALL;a->ay=word(-(q->speed>>4));state=FALL;}
 }
 if(state==FALL&&a->vy<=0)a->state=GLIDE_START;
 if(state==FLY||state==FALL){
  aim(q);if(!trail(w,e,1))return;if(!scene_preserve_screen(w,a))return;
  int collision=move(w,a,WMOVE_POINT);if(collision<0)return;if(collision)a->state=CRASH;
  flight_tail(w,e);return;
 }
 if(state==GLIDE_START){
  if(q->range==0x7400){int16_t unused;q->speed=0x600;weapon_vector((uint8_t)q->angle,q->speed,&a->vx,&unused);}
  a->ax=-32;a->ay=-278;a->state=state=GLIDE;
 }
 switch(state){
 case GLIDE:{
  if(!trail(w,e,0))return;a->vy=clamp(a->vy,0x700);if(a->vx<=0)a->vx=a->ax=0;if(!scene_preserve_screen(w,a))return;
  int collision=move(w,a,WMOVE_FALL);if(collision<0)return;
  if(collision){a->state=CRASH;if(!w->foot_contact||w->foot_contact(w->scene,a)<0){fail(w);return;}}
  aim(q);flight_tail(w,e);break;
 }
 case CRASH:{
  w->shotgun_flash=1;if(!noise(w,0x10f2))return;a->transform.priority=0xd000; /* $0434DC is RTS */
  a->attack=0;hit_begin_attack(&a->hit);if(!release_count(w,e))return;a->state=SMOKE_START;
  struct NativeEntity*child=spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,K_BLAST,&a->transform);if(!child)return;
  child->payload.slug_aux.actor.owner=a->owner;child->payload.slug_aux.actor.damage_owner=a->damage_owner;break;
 }
 case DESPAWN:entity_delete_children(w->entities,e);a->attack=0;hit_begin_attack(&a->hit); /* fall through */
 case RELEASE:if(release_count(w,e))retire(w,e,0);break;
 case DROP:
  if(move(w,a,WMOVE_SYNC)<0)return;if(animate(w,q))a->state=RELEASE;
  if(w->fault||!attack(w,a))return;if(outside(w,a,default_bounds))a->state=RELEASE;break;
 case EXHAUST:case SMOKE:
  if(move(w,a,WMOVE_SYNC)<0)return;if(animate(w,q)&&!w->fault)retire(w,e,1);break;
 case TRAIL:
  a->transform.priority--;if(move(w,a,WMOVE_SYNC)<0)return;animate(w,q);if(w->fault)return;
  if(!a->animation.delay)a->animation.delay--;if((int16_t)--a->timer<=0)retire(w,e,1);break;
 case BLAST_A:case BLAST_B:case BLAST_C:
  hit_begin_attack(&a->hit);a->attack=&slug_projectile_attacks[SPH_CRASH_A+state-BLAST_A];hit_begin_attack(&a->hit);
  if(!attack(w,a)||!follow_parent(w,e))return;
  a->state=(uint8_t)(state==BLAST_A?BLAST_B:state==BLAST_B?BLAST_C:DELETE);
  if(state==BLAST_C){a->attack=0;hit_begin_attack(&a->hit);}break;
 case DELETE:retire(w,e,0);break;
 default:fail(w);break;
 }
}
int slug_spawn_cannon(struct SlugAnimationContext*c,enum SlugCannonKind kind,struct PlayerSourceInput input,uint8_t layout){
 if((unsigned)kind>SCANNON_DROP_B)return 0;
 struct NativeEntity*e=spawn(c->weapons,c->entity,ENTITY_LAST_CHILD,(unsigned)kind,&c->slug->actor.transform);if(!e)return 0;
 e->payload.slug_aux.actor.owner=c->slug->actor.owner;e->payload.slug_aux.actor.damage_owner=c->slug->actor.damage_owner;
 e->payload.slug_aux.input_held=input.held;e->payload.slug_aux.layout=layout;return 1;
}
int slug_spawn_exhaust(struct SlugAnimationContext*c,int always_angled){
 return spawn(c->weapons,c->entity,ENTITY_LAST_CHILD,always_angled?K_EXHAUST_AIR:K_EXHAUST,&c->slug->actor.transform)!=0;
}
