#include "interactions.h"
#include "items.h"
#include "slug_body.h"
#include "enemies.h"
#include "../generated/item-programs.h"
static int16_t word(int x){return (int16_t)(uint16_t)x;}
int interaction_fault(struct WeaponWorld*w){if(!w->fault)w->fault=29;return 0;}
unsigned interaction_rank(struct WeaponWorld*w,struct WeaponActor*a,uint8_t owner){
 struct InteractionWorld*r=w->interactions;
 if(!r||!r->rank_valid||r->rank>7||!a||!w->enemies||!w->enemies->slug){interaction_fault(w);return 0;}
 unsigned n=r->demo_mode?4:r->rank;if(w->enemies->slug->present){n++;a->owner=owner;}return n>7?7:n;
}
int interaction_sound(struct WeaponWorld*w,uint16_t s){return w->sound&&w->sound(w->scene,s,0)?1:interaction_fault(w);}
struct NativeEntity*interaction_allocate(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,const struct PlayerTransform*t,EntityTick tick){
 if(!w->interactions||!w->entities||!t){interaction_fault(w);return 0;}
 struct NativeEntity*e=entity_spawn(w->entities,parent,ENTITY_INTERACTION,order,tick);if(!e){interaction_fault(w);return 0;}
 struct InteractionActor*q=&e->payload.interaction;
 *q=(struct InteractionActor){.entry=1,.support_slope=-1,.sibling_drawing=65535,.scale_x=255,.scale_y=255,
  .params={255,255,255,255,255,255,255,255},.gun={.owner=-1,.sprite_template=65535},
  .actor={.active=1,.owner=255,.damage_owner=255,.score_owner=255,.palette=65535,.platform_ids=255,.ground_slope=-1,.ground_tag=-1,
   .hit={.id=e->index,.hp=-1,.impact_x=-1,.impact_y=-1,.last_attacker=65535,.last_kind=255,.flags=64},
   .animation={.cursor=65535,.drawing=65535,.last_draw_cursor=65535}}};
 q->actor.transform=*t;transform_integer_copy(&q->actor.transform,t);return e;
}
void interaction_start(struct InteractionActor*q,unsigned entry){animation_begin(&q->actor.animation,interaction_entries[entry]);q->actor.movement_flags|=32;}
void interaction_defense(struct InteractionActor*q,unsigned binding){
 q->actor.defense=interaction_defenses[binding].records;q->actor.defense_count=interaction_defenses[binding].count;
}
struct Context {struct WeaponWorld*w;struct NativeEntity*e;};
static uint32_t read_field(void*ctx,unsigned f,unsigned width){
 struct Context*c=ctx;if(f==INF_5C&&width==4)return c->e->payload.interaction.sibling_drawing;
 interaction_fault(c->w);return 0;
}
static int write_field(void*ctx,unsigned f,unsigned width,enum AnimationFieldOperation op,uint32_t value){
 struct Context*c=ctx;if(f!=INF_5C||width!=4||op!=AN_FIELD_SET||value>65535)return interaction_fault(c->w);
 c->e->payload.interaction.sibling_drawing=(uint16_t)value;return 1;
}
static int bind(void*ctx,unsigned kind,uint32_t v){
 struct Context*c=ctx;struct InteractionActor*q=&c->e->payload.interaction;
 if(kind==AN_ATTACK&&v==0xffffffffu){q->actor.attack=0;hit_begin_attack(&q->actor.hit);return 1;}
 if(kind!=AN_DEFENSE)return interaction_fault(c->w);
 if(v==0xffffffffu){q->actor.defense=0;q->actor.defense_count=0;return 1;}
 if(v>=sizeof(interaction_defenses)/sizeof(*interaction_defenses))return interaction_fault(c->w);interaction_defense(q,v);return 1;
}
static int call(void*ctx,unsigned id,struct Animation*animation){
 (void)animation;struct Context*c=ctx;struct InteractionActor*q=&c->e->payload.interaction;
 switch(id){
 case INC_3F626:transform_integer_offset(&q->actor.transform,0,36);return 1;
 case INC_3F62E:transform_integer_offset(&q->actor.transform,0,-36);return 1;
 case INC_3EFEC:return item_spawn_drop(c->w,&q->actor.transform,q->params[3],q->params[4],q->params[5],0,0);
 case INC_9B90A:return item_respawn_below(c->w,&q->actor.transform,q->params[0],q->params[1],q->params[2]);
 default:return interaction_fault(c->w);
 }
}
static int sound(void*ctx,uint16_t s,int stop){struct Context*c=ctx;return c->w->sound&&c->w->sound(c->w->scene,s,stop)?1:interaction_fault(c->w);}
int interaction_animate(struct WeaponWorld*w,struct NativeEntity*e){
 struct WeaponActor*a=&e->payload.interaction.actor;if(a->animation.cursor==65535)return 0;
 struct Context c={w,e};struct AnimationOwner o={.context=&c,.read=read_field,.write=write_field,.bind=bind,.call=call,.sound=sound};
 int done=animation_step(&a->animation,&interaction_program,&o);if(a->animation.fault)interaction_fault(w);return done;
}
int interaction_move(struct WeaponWorld*w,struct WeaponActor*a,enum WeaponMove m){
 struct WeaponContact hit;if(!w->move){interaction_fault(w);return -1;}int r=w->move(w->scene,a,m,&hit);if(r<0)interaction_fault(w);return r;
}
void interaction_priority(struct WeaponWorld*w,struct WeaponActor*a,uint16_t value,unsigned bits){
 if((uint16_t)(value-0x4000)<0x7fff)value=(uint16_t)((value&0xe01f)|((255-((transform_y_up(&a->transform)+w->camera_y)&255))<<5));
 a->transform.priority=(uint16_t)((value&0xffe3)|bits);
}
int interaction_outside(struct WeaponWorld*w,struct WeaponActor*a,const int16_t*b,int force){
 int16_t x=word((a->transform.x>>8)-w->camera_x),y=word(transform_y_up(&a->transform)+w->camera_y);
 if(force)a->hit.flags|=128;
 if(a->hit.flags&128)return x<b[0]||x>word(320+b[1])||y<word(256-b[3])||y>word(496-b[2]);
 if((x>0&&x<320&&y>256&&y<496)||x<=-128||x>=512||y<=0||y>=640)a->hit.flags|=128;return 0;
}
void climb_targets_frame(struct ClimbTargets*t,unsigned phase){t->phase=phase&1;t->count[t->phase]=0;}
int climb_targets_publish(struct ClimbTargets*t,int16_t x,int16_t y,uint8_t h,uint8_t key){
 unsigned n=t->count[t->phase];if(n==8)return 0;t->records[t->phase][n]=(struct ClimbRecord){x,y,h,key};t->count[t->phase]++;return 1;
}
static const struct ClimbRecord*nearest_climb(const struct ClimbTargets*t,const struct WeaponActor*a,int16_t*edge){
 const struct ClimbRecord*best=0;uint16_t distance=65535;int16_t x=word(a->transform.x>>8),y=transform_world_y(&a->transform);
 unsigned phase=t->phase^1;
 for(unsigned i=0;i<t->count[phase];i++){
  const struct ClimbRecord*r=&t->records[phase][i];uint16_t dy=(uint16_t)word(r->y-y);if(dy>r->height)continue;
  int16_t ex=word(r->x-(dy>>3)),dx=word(ex-x);uint16_t d=dx<0?(uint16_t)word(-dx):(uint16_t)dx;
  if(d<distance){distance=d;best=r;*edge=ex;}
 }
 return best;
}
int climb_target_nearest(const struct ClimbTargets*t,const struct WeaponActor*a,uint8_t*key,int16_t*x){
 const struct ClimbRecord*r=nearest_climb(t,a,x);if(!r)return 0;*key=r->key;return 1;
}
int climb_target_crossing(const struct ClimbTargets*t,const struct WeaponActor*a,uint8_t*key,int*ascending){
 int16_t edge=0,x=word(a->transform.x>>8),y=transform_world_y(&a->transform);const struct ClimbRecord*best=nearest_climb(t,a,&edge);
 if(!best)return 0;int16_t step=word(((uint32_t)a->transform.x&255)+a->vx);
 if((word(x+(step>>8)-edge)<0)==(word(x-edge)<0))return 0;
 *key=best->key;*ascending=word(y-word(best->y-(best->height>>1)))>=0;return 1;
}
int climb_target_follow(struct WeaponWorld*w,struct WeaponActor*a,uint8_t key,int16_t*height){
 struct ClimbTargets*t=&w->interactions->climb;unsigned phase=t->phase^1;const struct ClimbRecord*r=0;
 for(unsigned i=0;i<t->count[phase];i++)if(t->records[phase][i].key==key){r=&t->records[phase][i];break;}
 *height=-1;if(!r)return interaction_move(w,a,WMOVE_FALL)>=0;
 int16_t base=word(512-r->y+w->camera_y),step=word(((uint32_t)(131072-a->transform.y)&255)+word(a->vy+a->ay));
 int16_t delta=word(transform_y_up(&a->transform)+w->camera_y+(step>>8)-base);
 if(delta>=-8)*height=word(base+r->height+1);
 a->vx=word((word(-delta)>>3)+r->x-(a->transform.x>>8));a->vx=word(a->vx*256);a->ax=0;
 int ok=interaction_move(w,a,WMOVE_FALL)>=0;a->vx=0;return ok;
}
static void crate_tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;struct InteractionActor*q=&e->payload.interaction;struct WeaponActor*a=&q->actor;if(w->fault)return;
 if(q->entry){q->entry=0;
  if(a->state==0){a->palette=0x1ab;interaction_priority(w,a,0x8000,0);interaction_start(q,INAN_CRATE);a->state=1;}
  else if(a->state==2){if(!interaction_sound(w,0x102e))return;interaction_start(q,INAN_CRATE_BREAK);}
 }
 if(a->state==3){entity_delete(p,e);return;}
 if(interaction_move(w,a,WMOVE_SYNC)<0)return;
 if(a->state==1){
  if(q->params[3]==0||q->params[3]==255){int r=terrain_weapon_foot_contact(w->interactions->terrain,a);if(r<0){interaction_fault(w);return;}/* $09B8D8: source carry means air; the C foot probe returns 0 in air. */
   if(!r&&interaction_move(w,a,WMOVE_ITEM)<0)return;}
  struct TerrainMotion m={.platform_ids=a->platform_ids};terrain_platform_publish(w->interactions->terrain,&m,word((a->transform.x>>8)-16),word(transform_y_up(&a->transform)+16),32,0);a->platform_ids=m.platform_ids;
  crate_body_contact(w,a);a->transform.priority=0x8000;interaction_animate(w,e);a->hit.flags&=(uint8_t)~8u;
  if(a->hit.flags&1){a->state=2;q->entry=1;}
 }else if(interaction_animate(w,e)){a->state=3;q->entry=1;}
 if(interaction_outside(w,a,item_bounds,q->params[7]!=0))entity_delete(p,e);
}
int crate_spawn(struct WeaponWorld*w,struct EntityRef parent,enum EntityInsert order,const struct PlayerTransform*t,const uint8_t params[8]){
 struct NativeEntity*e=interaction_allocate(w,parent,order,t,crate_tick);if(!e)return 0;e->payload.interaction.kind=1;
 if(params)for(unsigned i=0;i<8;i++)e->payload.interaction.params[i]=params[i];return 1;
}
static void climb_actor_tick(struct EntityPool*p,struct NativeEntity*e,void*world){
 struct WeaponWorld*w=world;struct InteractionActor*q=&e->payload.interaction;if(w->fault)return;
 if(!w->secondary_view_valid){interaction_fault(w);return;}
 if(q->entry){q->entry=0;q->gun.offset_x=word((q->actor.transform.x>>8)-w->camera_x+w->secondary_x_offset);
  q->gun.offset_y=word(transform_world_y(&q->actor.transform)-w->camera_y+w->secondary_y_offset);
  q->key=++w->interactions->climb.last_key;return;}
 if((uint16_t)word(q->gun.offset_x-w->secondary_x_offset+64)>=896){entity_delete(p,e);return;}
 if(q->climb_watch){
  struct NativeEntity*parent=entity_get(p,e->parent);int stop=parent?q->climb_watch(parent):-1;
  if(stop<0){interaction_fault(w);return;}if(stop){entity_delete(p,e);return;}
 }
 /* Full lists intentionally ignore later publishers ($076FEA), without fault. */
 (void)climb_targets_publish(&w->interactions->climb,word(q->gun.offset_x-w->secondary_x_offset+w->camera_x),
  word(q->gun.offset_y-w->secondary_y_offset+w->camera_y),q->params[0],q->key);
}
int climb_actor_spawn(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,uint8_t height){
 return climb_actor_spawn_watched(w,p,o,t,height,0);
}
int climb_actor_spawn_watched(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,const struct PlayerTransform*t,uint8_t height,MissionWatchFn watch){
 struct NativeEntity*e=interaction_allocate(w,p,o,t,climb_actor_tick);if(!e)return 0;e->payload.interaction.kind=3;e->payload.interaction.params[0]=height;e->payload.interaction.climb_watch=watch;return 1;
}
int interaction_spawn_template(struct WeaponWorld*w,struct EntityRef p,enum EntityInsert o,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8]){
 if(id==143)return climb_actor_spawn(w,p,o,t,params?params[0]:255);
 if(id==172||id==173||id==192)return pow_spawn(w,p,o,id==172?POW_IDLE:id==173?POW_STACKED:POW_BOUND,t,params);
 if(id==283)return gun_spawn(w,p,o,t,0);if(id==293)return crate_spawn(w,p,o,t,params);return 0;
}
