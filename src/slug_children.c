#include "slug_children.h"
#include "slug_body.h"
#include "../generated/slug-child-programs.h"
enum Kind {CH_TURRET,CH_WHEEL,CH_MARKER,CH_THROWN,CH_FIRE,CH_SPARK};
enum TurretState {T_NEW,T_AIM,T_DETACH,T_TRAVEL,T_FLAG,T_WAIT,T_RESTORE,T_RESTORE_FRAME};
static void tick(struct EntityPool*,struct NativeEntity*,void*);
static int fail(struct WeaponWorld*w){if(!w->fault)w->fault=26;return 0;}
static int16_t word(int v){return (int16_t)(uint16_t)v;}
static int sound(struct WeaponWorld*w,uint16_t cmd){return w->sound&&w->sound(w->scene,cmd,0)?1:fail(w);}
static struct NativeEntity*spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,unsigned kind,const struct PlayerTransform*t){
 if(!w||w->fault||!w->entities)return 0;
 struct NativeEntity*e=entity_spawn(w->entities,parent,ENTITY_SLUG_AUX,order,tick);if(!e){fail(w);return 0;}
 struct WeaponActor*a=&e->payload.slug_aux.actor;e->payload.slug_aux.kind=(uint8_t)kind;
 *a=(struct WeaponActor){.active=1,.owner=255,.damage_owner=255,.score_owner=255,.platform_ids=255,.ground_slope=-1,.ground_tag=-1,
  .phase=255,.ttl=255,.raw_surface=255,.origin_y_up=-1,.trail_drawing=65535,
  .hit={.id=e->index,.hp=-1,.impact_x=-1,.impact_y=-1,.last_attacker=65535,.last_kind=255,.flags=64},
  .animation={.drawing=65535,.cursor=65535,.last_draw_cursor=65535}};
 if(t){a->transform=*t;transform_integer_copy(&a->transform,t);a->animation.pose=0;}return e;
}
static void retire(struct WeaponWorld*w,struct NativeEntity*e,int children){
 e->payload.slug_aux.actor.active=0;if(children)entity_delete_children(w->entities,e);entity_delete(w->entities,e);
}
static int outside(struct WeaponWorld*w,struct WeaponActor*a,const int16_t*b){
 int16_t x=word((a->transform.x>>8)-w->camera_x),y=word(transform_y_up(&a->transform)+w->camera_y);
 a->hit.flags|=128;return x<b[0]||x>word(320+b[1])||y<word(256-b[3])||y>word(496-b[2]);
}
static void start(struct WeaponActor*a,unsigned entry){animation_begin(&a->animation,slug_child_entries[entry]);a->movement_flags|=32;}
static int child_bind(void*ctx,unsigned kind,uint32_t value){
 /*2DD594 fire stream explicitly clears attack60 with FFFFFFFF. It has
  * no binding-table entry, but still requires the native owner operation. */
 if(kind!=AN_ATTACK||value!=UINT32_MAX)return 0;
 ((struct WeaponActor*)ctx)->attack=0;return 1;
}
static int animate(struct WeaponWorld*w,struct WeaponActor*a){
 const struct AnimationOwner owner={.context=a,.bind=child_bind};int complete=animation_step(&a->animation,&slug_child_program,&owner);
 if(a->animation.fault)fail(w);return complete;
}
static int move(struct WeaponWorld*w,struct WeaponActor*a,enum WeaponMove policy){
 struct WeaponContact hit;return w->move&&w->move(w->scene,a,policy,&hit)>=0?1:fail(w);
}
static uint16_t owner_palette(uint8_t owner){return owner==1||owner==2?0x1d6:2;}
static int fire_begin(struct WeaponWorld*w,struct WeaponActor*a){
 if(!w->rng)return fail(w);a->palette=4;start(a,SCA_FIRE);a->transform.facing|=source_random_word(w->rng)&1;
 a->defense=0;a->defense_count=0;a->state=1;return 1;
}
static int throw_part(struct WeaponWorld*w,struct NativeEntity*e){
 struct WeaponActor*a=&e->payload.slug_aux.actor;
 /* $045BE6/$045C20 copy only integer X/Y and attribution into the global
  * projectile group. Facing/priority remain allocation defaults. */
 struct NativeEntity*n=spawn(w,w->projectile_group,ENTITY_FIRST_CHILD,CH_THROWN,0);if(!n)return 0;
 struct WeaponActor*b=&n->payload.slug_aux.actor;transform_integer_copy(&b->transform,&a->transform);
 b->owner=a->owner;b->damage_owner=a->damage_owner;return 1;
}
static int follow(struct WeaponWorld*w,struct SlugAuxActor*q,struct SlugSharedState*s){
 const struct SlugMount*m=slug_mount(s);if(!m)return fail(w);struct WeaponActor*a=&q->actor,*p=&s->actor;
 a->damage_timer=p->damage_timer;a->transform.priority=p->transform.priority|1;
 transform_integer_offset(&a->transform,word((p->transform.x>>8)+m->turret_x-(a->transform.x>>8)),
  word(transform_y_up(&p->transform)+m->turret_y-transform_y_up(&a->transform)));
 if(a->damage_owner!=p->damage_owner){a->damage_owner=p->damage_owner;a->owner=p->owner;a->palette=owner_palette(a->damage_owner);}return 1;
}
static int read_input(struct WeaponWorld*w,struct SlugAuxActor*q){
 if((int8_t)q->actor.damage_owner<0){q->turret.input=0;q->turret.burst=0;return 1;}
 unsigned slot=q->actor.damage_owner==1?0:1;struct PlayerSourceInput in=w->inputs[slot];
 q->turret.input=in.held&15;
 if(in.actions&PB_FIRE){if(!sound(w,0x108c))return 0;if(!q->turret.burst)q->turret.cooldown=0;q->turret.burst=3;}return 1;
}
static int fire(struct WeaponWorld*w,struct NativeEntity*e,struct SlugSharedState*s){
 struct SlugAuxActor*q=&e->payload.slug_aux;struct WeaponActor*a=&q->actor;a->owner=s->actor.owner;
 struct PlayerEmission shot={.kind=EMIT_PISTOL,.transform=a->transform,.direction=(uint8_t)((q->turret.direction>>8)+4),
  .phase=q->turret.shot_phase,.spread=(int8_t)q->turret.sequence,.owner=a->owner,.damage_owner=a->damage_owner};
 if(!weapon_emit_slug_bullet(w,entity_ref(e),&shot))return 0;
 q->turret.sequence++;s->action_flags|=4;q->turret.shot_phase--;
 unsigned angle=(((q->turret.direction>>8)+4)&248)>>3;
 int16_t x=word((a->transform.x>>8)+slug_turret_offsets[angle][0]);
 int16_t y=word(transform_y_up(&a->transform)+slug_turret_offsets[angle][1]);uint8_t shape;
 if(!w->point_shape||!w->point_shape(w->scene,a,x,y,&shape))return fail(w);
 if(shape&&(shape==16||shape==32||shape==33||!(shape&0xf0))){
  struct NativeEntity*n=spawn(w,entity_ref(e),ENTITY_FIRST_CHILD,CH_SPARK,&a->transform);if(!n)return 0;
  n->payload.slug_aux.frame=(uint8_t)angle;
 }else a->animation.pose=5;
 return 1;
}
static int parent_live(const struct NativeEntity*p,const struct SlugSharedState*s){return !p->deleting&&s->present&&s->body_state!=SB_INACTIVE;}
static void turret_tick(struct WeaponWorld*w,struct NativeEntity*e,struct NativeEntity*parent,struct SlugSharedState*s){
 struct SlugAuxActor*q=&e->payload.slug_aux;struct WeaponActor*a=&q->actor;
 if(a->state==T_NEW){
  static const uint16_t directions[5]={0x4000,0x8000,0xc000,0,0};
  if(q->turret.variant>4){fail(w);return;}q->turret.direction=directions[q->turret.variant];
  a->palette=2;a->owner=a->damage_owner=255;q->turret.angular_speed=0;
  a->animation.pose=0;if(q->turret.variant<4)q->turret.sequence=0;
  else q->turret.target_direction=0;a->state=T_AIM;
 }
 if(a->state==T_AIM){
  if(!follow(w,q,s)||!read_input(w,q))return;
  int32_t target=slug_turret_aims[q->turret.variant][q->turret.input];int smooth=q->turret.variant==4;
  if(smooth){if(target>=0)q->turret.target_direction=(uint16_t)target;else target=q->turret.target_direction;}
  if(target>=0){
   int16_t delta=word(target-q->turret.direction);uint16_t magnitude=(uint16_t)(delta<0?word(-delta):delta);
   uint16_t acceleration=smooth?(magnitude>=0x5000?0x800:0x200):0x100;
   /* ASR.W retains the -32768 corner case; comparisons are unsigned. */
   uint16_t limit=(uint16_t)((int16_t)magnitude>>(smooth?1:2));
   uint16_t speed=(uint16_t)(q->turret.angular_speed+acceleration);if(speed>=limit)speed=limit;
   q->turret.angular_speed=speed;q->turret.direction=(uint16_t)(q->turret.direction+(delta<0?-speed:speed));
  }else q->turret.angular_speed=0;
  if(q->turret.cooldown)q->turret.cooldown--;
  else {q->turret.cooldown=1;if(q->turret.burst){q->turret.burst--;if(!fire(w,e,s))return;}}
  unsigned angle=(((q->turret.direction>>8)+4)&248)>>3,phase=a->animation.pose;
  if(phase>5){fail(w);return;}animation_drawing(&a->animation,&slug_child_program,slug_turret_drawings[angle][phase]);
  if(!animation_output_direct(&a->animation,AN_OUTPUT_DIRECT)){fail(w);return;}
  if(phase)a->animation.pose--;
  a->owner=s->actor.owner;
  if(s->input_masks[w->frame_phase&1]&8)a->state=T_DETACH;
  if(!parent_live(parent,s)||(s->actor.hit.flags&1)){if(throw_part(w,e))retire(w,e,1);}return;
 }
 if(a->state==T_DETACH){
  if(!sound(w,0x106a)||!throw_part(w,e))return;
  if(!spawn(w,entity_ref(e),ENTITY_LAST_CHILD,CH_FIRE,&a->transform))return;
  q->turret.travel=0;a->state=T_TRAVEL;
 }
 if(a->state==T_TRAVEL){
  int16_t dx=w->camera_delta_x;if(dx<0)dx=word(-dx);q->turret.travel=(uint16_t)(q->turret.travel+dx);
  if((int16_t)q->turret.travel>480)a->state=T_FLAG;
 }else if(a->state==T_FLAG){s->action_flags=(s->action_flags|8)&(uint8_t)~16u;a->state=T_WAIT;
  /* The FLAG entry falls through to WAIT in the same tick. */
 }
 if(a->state==T_WAIT&&(s->action_flags&16))a->state=T_RESTORE;
 else if(a->state==T_RESTORE){a->palette=owner_palette(a->damage_owner);if(!sound(w,0x106a))return;start(a,SCA_RESTORE);a->state=T_RESTORE_FRAME;}
 if(a->state==T_RESTORE_FRAME){
  if(!follow(w,q,s))return;(void)animate(w,a);if(w->fault)return;
  /* $045D1C loads the five-entry table but never jumps or writes a handler. */
  if(parent_live(parent,s)){if(throw_part(w,e))retire(w,e,1);return;}
 }
 s->input_masks[(~w->frame_phase)&1]|=16;
}
static void wheel_tick(struct WeaponWorld*w,struct NativeEntity*e,struct SlugSharedState*s){
 struct SlugAuxActor*q=&e->payload.slug_aux;struct WeaponActor*a=&q->actor;
 if(!a->state){q->wheel.palettes[0]=0x196;q->wheel.palettes[1]=0x197;q->wheel.palettes[2]=0x198;
  q->wheel.palettes[3]=65535; /* fourth request exceeds original allocator limit */
  a->palette=0x198;a->state=1;}
 if(!q->wheel.dead&&(s->actor.hit.flags&1)){q->wheel.dead=1;q->wheel.phase=0;}
 if((q->wheel.dead||(uint16_t)s->actor.hit.hp<=16)&&(s->actor.display_flags&8)){
  q->wheel.phase=(q->wheel.phase+2)&6;a->palette=q->wheel.palettes[q->wheel.phase>>1];
  /* The fourth historical word is1C=FFFF. Source palette WORD <<8
   * addresses fixed slotFF, not a nonexistent RGB record65535. */
  a->fixed_palette=a->palette==65535?255:0;
  a->animation.cursor=a->animation.last_draw_cursor=s->actor.animation.last_draw_cursor==65535?
   s->actor.animation.cursor:s->actor.animation.last_draw_cursor;
  transform_integer_offset(&a->transform,word((s->actor.transform.x>>8)-(a->transform.x>>8)),
   word(transform_y_up(&s->actor.transform)-transform_y_up(&a->transform)));
  a->transform.priority=(uint16_t)(s->actor.transform.priority-2);a->hit.flags|=64;
  const struct AnimationOwner owner={0};(void)animation_step(&a->animation,&slug_animation_program,&owner);
  if(a->animation.fault){fail(w);return;}
 }
 if(s->body_state==SB_DEATH_FADE&&s->body_enter)retire(w,e,0);
}
static void tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;struct SlugAuxActor*q=&e->payload.slug_aux;struct WeaponActor*a=&q->actor;
 static const int16_t bounds[4]={0,1,0,1};if(w->fault)return;
 if(q->kind==CH_THROWN){
  if(!a->state){a->palette=owner_palette(a->damage_owner);a->transform.priority=0xd010;
   a->vx=(a->transform.facing&1)?-56:56;a->vy=900;a->ay=-20;a->ax=0;a->hit.invulnerable=60;start(a,SCA_THROWN);a->state=1;}
  if(!move(w,a,WMOVE_FALL))return;(void)animate(w,a);if(w->fault)return;
  if(!a->hit.invulnerable){if(!sound(w,0x1022))return;q->kind=CH_FIRE;if(!fire_begin(w,a))return;
   if(outside(w,a,slug_child_fire_bounds)){retire(w,e,0);return;}
  }else {if(outside(w,a,bounds))retire(w,e,1);else entity_delete_children(p,e);return;}
 }
 if(q->kind==CH_FIRE){
  if(!a->state){if(!fire_begin(w,a))return;if(outside(w,a,slug_child_fire_bounds)){retire(w,e,0);return;}}
  if(animate(w,a)||outside(w,a,slug_child_fire_bounds))retire(w,e,0);return;
 }
 if(q->kind==CH_SPARK){
  if(!a->state){int16_t dx,dy;weapon_vector((uint8_t)(q->frame<<3),20,&dx,&dy);
   transform_integer_offset(&a->transform,dx,dy);a->palette=0x1d6;a->transform.priority=0xd080;start(a,SCA_SPARK);a->state=1;}
  if(move(w,a,WMOVE_SYNC)&&animate(w,a)&&!w->fault)retire(w,e,0);return;
 }
 struct NativeEntity*parent=entity_get(p,e->parent);
 if(!parent||parent->kind!=ENTITY_SLUG||!parent->payload.slug){fail(w);return;}struct SlugSharedState*s=parent->payload.slug;
 if(q->kind==CH_TURRET){turret_tick(w,e,parent,s);return;}
 if(q->kind==CH_WHEEL){wheel_tick(w,e,s);return;}
 if(q->kind!=CH_MARKER){fail(w);return;}
 if(a->state==2){retire(w,e,0);return;}
 if(!a->state){a->palette=0x1ad;a->attached_offset=40;a->transform.priority=65535;a->display_flags|=64;
  start(a,w->scene_number==5?SCA_ICON_ALT:SCA_ICON);a->state=1;}
 transform_integer_offset(&a->transform,word((s->actor.transform.x>>8)-(a->transform.x>>8)),
  word(transform_y_up(&s->actor.transform)+a->attached_offset-transform_y_up(&a->transform)));
 a->transform.priority=s->actor.transform.priority;
 if(w->slug_mount_enabled&&(s->status_flags&32)&&!outside(w,a,slug_child_icon_bounds))(void)animate(w,a);
 if(s->actor.hit.flags&1)a->state=2;
}
int slug_spawn_turret(struct SlugAnimationContext*c,unsigned variant){
 if(variant>4)return fail(c->weapons);struct NativeEntity*e=spawn(c->weapons,c->entity,ENTITY_FIRST_CHILD,CH_TURRET,&c->slug->actor.transform);
 if(!e)return 0;struct SlugAuxActor*q=&e->payload.slug_aux;
 /* $05DC1C fills these bytes with $FF. Variants 0..3 clear $81 only;
  * variant 4 retains its initial $FF spread sequence. $80 stays $FF. */
 q->turret.variant=(uint8_t)variant;q->turret.burst=q->turret.cooldown=255;
 q->turret.sequence=q->turret.shot_phase=255;q->turret.target_direction=65535;return 1;
}
int slug_spawn_wheel(struct SlugAnimationContext*c){return spawn(c->weapons,c->entity,ENTITY_FIRST_CHILD,CH_WHEEL,&c->slug->actor.transform)!=0;}
int slug_spawn_marker(struct SlugAnimationContext*c){return spawn(c->weapons,c->entity,ENTITY_FIRST_CHILD,CH_MARKER,0)!=0;}
